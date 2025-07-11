#include "sdkconfig.h"
#if CONFIG_USE_BLE_CLA
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

//generate error message if BLE CLA is enabled without NimBLE beeing enabled
#ifndef CONFIG_BT_NIMBLE_ENABLED
#error "The BLE CLA requires NimBLE to be enabled in menuconfig!"
#endif

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "BLE_CLA.hpp"
#include "BLEhandling.hpp"

extern "C" {
#include "esp_bt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "nvs_flash.h"
}

static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};

char nodeURI[MAX_URI_BUFFER_SIZE] = {0};  // Global buffer
uint8_t uriLength = 0;

/// @brief used to keep track of the node is currently advertising or scanning, required to perform switching between the two procedures
bool isAdvertising = false;

/// @brief this callback is called upon completion of the ble write procedure
/// @param conn_handle
/// @param error
/// @param attr
/// @param arg
/// @return
static int write_callback(uint16_t conn_handle,
                          const struct ble_gatt_error* error,
                          struct ble_gatt_attr* attr, void* arg) {
    if ((error->status == 0) |
        (error->status ==
         7)) {  // apparently 7 is also an acceptable return code

        ESP_LOGI("BLE CLA", "Write successful");
        cborSize = 0;
    }
    else {
        ESP_LOGE("BLE CLA",
                 "Write (bundle transmission) failed, error code: %d",
                 error->status);
        cborSize =
            maxBleBundleSize +
            1;  // indicate unsuccessful bundle transmission by setting the size to a value larger than actually possible
    }
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    transmissionComplete = true;

    return 0;
}

extern "C" void cPeerDiscovery(const ble_addr_t& addr, char* name,
                               uint8_t nameLength) {
    if (DTN7::bleCla != NULL)
        DTN7::bleCla->discoveredPeer(addr.type, addr.val, name, nameLength);
    return;
}

void handleBLEReception(Bundle* bundle, ble_addr_t* senderAddr) {
    std::string fromUri = "none";
    for (BlePeer peer : DTN7::bleCla->currentPeers) {
        if (peer.addr[0] == senderAddr->val[0] &&
            peer.addr[1] == senderAddr->val[1] &&
            peer.addr[2] == senderAddr->val[2] &&
            peer.addr[3] == senderAddr->val[3] &&
            peer.addr[4] == senderAddr->val[4] &&
            peer.addr[5] == senderAddr->val[5]) {
            fromUri = peer.name;
            break;
        }
    }
    ReceivedBundle* recBundle = new ReceivedBundle(bundle, fromUri);
    xQueueSend(DTN7::BPA->receiveQueue, (void*)&recBundle, portMAX_DELAY);
    return;
}

static void format_addr(char* addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
            addr[2], addr[3], addr[4], addr[5]);
}

bool connectionActive() {
    struct ble_gap_conn_desc desc;
    for (uint16_t conn_handle = 0;
         conn_handle < MYNEWT_VAL(BLE_MAX_CONNECTIONS); ++conn_handle) {
        if (ble_gap_conn_find(conn_handle, &desc) == 0) {
            return true;  // Active connection found
        }
    }
    return false;
}

extern "C" int connect_to_peer(ble_addr_t* addr) {
    if (transmissionComplete)
        return 0;
    ESP_LOGI("BLE system",
             "Attempting Conncetion To: %02X:%02X:%02X:%02X:%02X:%02X",
             addr->val[5], addr->val[4], addr->val[3], addr->val[2],
             addr->val[1], addr->val[0]);
    // wait for potentially present connections to finish
    while (connectionActive()) {
        ESP_LOGI("BLE system", "Other Connection in progress, waiting...");
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 0x0016,
        .scan_window = 0x0016,
        .itvl_min = 0x0020,
        .itvl_max = 0x0040,
        .latency = 0,
        .supervision_timeout = 100,
    };
    ble_gap_disc_cancel();
    ble_gap_adv_stop();
    int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, addr, 4000, &conn_params,
                             gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE system", "Connection failed: %d", rc);
    }
    else {
        ESP_LOGD("BLE system", "Connecting to device...");
    }
    return rc;
}

extern "C" void ble_advertise() {
    /* Local variables */
    int rc = 0;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    // Stop advertising if already active
    rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {  // Ignore "already stopped" error
        ESP_LOGE("BLE system", "Failed to stop advertising: %d", rc);
    }

    /* Set advertising flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // include service uuid instead of name, name should be done by mac address
    // adv_fields.uuids16 = &serviceUUID;
    // adv_fields.num_uuids16 = 1;

    /* Set device name */
    adv_fields.name = (uint8_t*)nodeURI;
    if (uriLength > MAX_URI_LENGTH_ADVERTISED) {
        // uri to long, gets truncated
        adv_fields.name_len = MAX_URI_LENGTH_ADVERTISED;
        adv_fields.name_is_complete = 0;
    }
    else {
        adv_fields.name_len = uriLength;
        adv_fields.name_is_complete = 1;
    }

    /* Set device appearance */
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;

    /* Set device LE role */
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL_CENTRAL;
    adv_fields.le_role_is_present = 1;

    /* Set advertiement fields */
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE("BLE system", "failed to set advertising data, error code: %d",
                 rc);
        return;
    }

    /* Set non-connetable and general discoverable mode to be a beacon */
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    /* Set advertising interval */
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    /* Start advertising */
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE system", "failed to start advertising, error code: %d",
                 rc);
        return;
    }
    ESP_LOGD("BLE system", "advertising started!");
}

/* Public functions */
extern "C" void adv_init(void) {
    /* Local variables */
    int rc = 0;
    char addr_str[18] = {0};

    /* Make sure we have proper BT identity address set (random preferred) */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE("BLE system",
                 "device does not have any available bt address!");
        return;
    }

    /* Figure out BT address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE("BLE system", "failed to infer address type, error code: %d",
                 rc);
        return;
    }

    /* Printing ADDR */
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE system", "failed to copy device address, error code: %d",
                 rc);
        return;
    }
    format_addr(addr_str, addr_val);
    ESP_LOGD("BLE system", "device address: %s", addr_str);

    /* Start advertising. */
    ble_advertise();
}

/// @brief Callback when the characteristic discovery is finished. If the required characteristic is present, this writes the bundle to the destination node.
/// @param conn_handle
/// @param error
/// @param chr
/// @param arg
/// @return
static int ble_gattc_disc_chrc_cb(uint16_t conn_handle,
                                  const struct ble_gatt_error* error,
                                  const struct ble_gatt_chr* chr, void* arg) {
    if (error->status != 0) {
        ESP_LOGW("BLE characteristic Discovery",
                 " Characteristic discovery failed, error: %u", error->status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return error->status;
    }

    ESP_LOGI("BLE characteristic Discovery",
             " Found Characteristic UUID: %04X, Handle: %d",
             chr->uuid.u16.value, chr->val_handle);

    // Store the handle if it's a write characteristic
    if (chr->uuid.u16.value == writeUUID.value) {
        ESP_LOGI("BLE characteristic Discovery",
                 " Found Write Characteristic!");
        int success = ble_gattc_write_flat(conn_handle, chr->val_handle, cbor,
                                           cborSize, write_callback, NULL);

        if (success == 0) {
            cborSize = 0;
        }
        else {
            cborSize =
                maxBleBundleSize +
                1;  // indicate unsuccessful bundle transmission by setting the size to a value larger than actually possible
        }
        return success;
    }

    return 0;
}

extern "C" int ble_gattc_disc_svc_cb(uint16_t conn_handle,
                                     const struct ble_gatt_error* error,
                                     const struct ble_gatt_svc* svc,
                                     void* arg) {
    int rc = 0;
    if (error->status != 0) {
        ESP_LOGW("BLE service Discovery", " Service discovery failed, code: %i",
                 error->status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return error->status;
    }

    ESP_LOGI(
        "BLE service Discovery",
        " Service discovered! UUID: %04X, Start Handle: %d, End Handle: %d",
        svc->uuid.u16.value, svc->start_handle, svc->end_handle);

    // Discover characteristics within this service
    if (svc->uuid.u16.value == serviceUUID.value)
        rc = ble_gattc_disc_chrs_by_uuid(conn_handle, svc->start_handle,
                                         svc->end_handle, &writeUUID.u,
                                         ble_gattc_disc_chrc_cb, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE service Discovery",
                 " Failed to start characteristic discovery, error: %i", rc);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        cborSize =
            maxBleBundleSize +
            1;  // indicate unsuccessful bundle transmission by setting the size to a value larger than actually possible
    }

    return 0;
}

/// @brief this boolean is used to keep track whether a connection is currently present, in order to prevent reenabling scanning/advertising during this connection
bool bleConnection = false;

extern "C" int gap_event_handler(struct ble_gap_event* event, void* arg) {
    /* Local variables */
    int rc = 0;
    struct ble_gap_conn_desc desc;

    /* Handle different GAP event */
    switch (event->type) {

        /* Connect event */
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            ble_gap_disc_cancel();  // stop scanning, periodic switching task will reenable
            bleConnection = true;
            ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (desc.role == BLE_GAP_ROLE_SLAVE) {
                ESP_LOGI("BLE system", "connected as slave");
                return rc;
            }
            if (event->connect.status == 0) {
                ble_gap_adv_stop();  // stop advertising, periodic switching task will reenable
                ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
                ESP_LOGI("BLE system", "connected as master, conn_handle %d",
                         event->connect.conn_handle);
                rc = ble_gattc_disc_svc_by_uuid(event->connect.conn_handle,
                                                &serviceUUID.u,
                                                ble_gattc_disc_svc_cb, NULL);
                return rc;
            }
            ESP_LOGW("BLE system", "connection unsuccessful as master");
            ESP_LOGE("BLE system", "connection unsuccessful");
            return rc;

        /* Disconnect event */
        case BLE_GAP_EVENT_DISCONNECT:
            /* A connection was terminated, print connection descriptor */
            ESP_LOGI("BLE system", "disconnected from peer; reason = %d",
                     event->disconnect.reason);
            bleConnection = false;
            /* Restart advertising */
            // ble_advertise();
            return rc;

        /* Advertising complete event */
        case BLE_GAP_EVENT_ADV_COMPLETE:
            /* Advertising completed, restart scanning */
            ESP_LOGI("BLE system", "advertise complete; reason = %d",
                     event->adv_complete.reason);
            // start_ble_scan();
            return rc;

        /* Scanning complete event */
        case BLE_GAP_EVENT_DISC_COMPLETE:
            /* Scanning completed, restart advertising */
            ESP_LOGI("BLE system", "advertise complete; reason = %d",
                     event->adv_complete.reason);
            // ble_advertise();
            return rc;

        case BLE_GAP_EVENT_DISC:
            // Extract device name from advertisement
            struct ble_hs_adv_fields fields;
            ble_hs_adv_parse_fields(&fields, event->disc.data,
                                    event->disc.length_data);
            ESP_LOGD("BLE system", "checking possible peer");
            if (fields.name != NULL) {
                ESP_LOGI("BLE system", "possible peer name:%.*s",
                         fields.name_len, fields.name);
                if ((fields.name[0] == 'd' && fields.name[1] == 't' &&
                     fields.name[2] == 'n' && fields.name[3] == ':') ||
                    (fields.name[0] == 'i' && fields.name[1] == 'p' &&
                     fields.name[2] == 'n' && fields.name[3] == ':')) {
                    ESP_LOGI("BLE system", "discovered peer");
                    cPeerDiscovery(event->disc.addr, (char*)fields.name,
                                   fields.name_len);
                    return rc;
                }
            }
            return rc;
    }
    return rc;
}

extern "C" void start_ble_scan(void) {
    ble_gap_disc_cancel();
    vTaskDelay(pdMS_TO_TICKS(100));  // Let the stack clean up
    struct ble_gap_disc_params disc_params = {0};
    disc_params.passive =
        1;  // when using active scanning, the timeout does not work, same on esp32-s3, requires task to switch scan /advertise
    disc_params.filter_duplicates = 1;
    disc_params.itvl = 0x0010;
    disc_params.window = 0x0010;
    disc_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;

    int rc = ble_gap_disc(BLE_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params,
                          gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE("BLE system", "Failed to start scan: %d", rc);
    }
    ESP_LOGD("BLE system", "BLE started scanning");
}

extern "C" void ble_on_sync() {
    ESP_LOGI("BLE system", "BLE stack initialized!");
    adv_init();
    xTaskCreate(switchScanAdvertiseTask, "BLE Advertise Switch Task", 4 * 1024,
                NULL, 5, NULL);
}

extern "C" void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI("BLE system", "nimble stack reset, reset reason: %d", reason);
}

extern "C" void gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt,
                                     void* arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

        /* Service register event */
        case BLE_GATT_REGISTER_OP_SVC:
            ESP_LOGD("BLE system", "registered service %s with handle = %d",
                     ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                     ctxt->svc.handle);
            break;

        /* Characteristic register event */
        case BLE_GATT_REGISTER_OP_CHR:
            ESP_LOGD("BLE system",
                     "registering characteristic %s with "
                     "def_handle = %d val_handle = %d",
                     ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                     ctxt->chr.def_handle, ctxt->chr.val_handle);
            break;

        /* Descriptor register event */
        case BLE_GATT_REGISTER_OP_DSC:
            ESP_LOGD("BLE system", "registering descriptor %s with handle = %d",
                     ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                     ctxt->dsc.handle);
            break;

        /* Unknown event */
        default:
            assert(0);
            break;
    }
}

extern "C" void switchScanAdvertiseTask(void* arg) {
    while (true) {
        uint32_t randomOffset = esp_random() % (MaxRandomOffsetScanSwitch + 1);
        if (bleConnection)
            vTaskDelay(pdMS_TO_TICKS(
                2000));  // if we are currently connected, wait a bit to not interrupt service discovery
        int limit = 0;
        while ((cborSize != 0) && limit < 10) {
            vTaskDelay(pdMS_TO_TICKS(200));
            ESP_LOGI("switchScanAdvertiseTask",
                     "waiting for bundle transmission to finish...");
            limit++;
        }
        cborSize = 0;
        if (isAdvertising) {
            isAdvertising = !isAdvertising;
            // if(DTN7::bleCla != NULL)DTN7::bleCla->cleanUpBlePeers(); // before scanning for new peers, we check if old peers are no longer present and remove them
            start_ble_scan();
            vTaskDelay(pdMS_TO_TICKS(SCAN_TIME_MS + randomOffset));
        }
        else {
            isAdvertising = !isAdvertising;
            ble_advertise();
            vTaskDelay(pdMS_TO_TICKS(ADVERTISE_TIME_MS + randomOffset));
        }
    }
}

extern "C" void cleanPeersTask(void* arg) {
    while (true) {

        if (DTN7::bleCla != NULL)
            DTN7::bleCla
                ->cleanUpBlePeers();  // before scanning for new peers, we check if old peers are no longer present and remove them
        vTaskDelay(pdMS_TO_TICKS(
            CONFIG_BLE_MAX_PEER_AGE >>
            1));  // wait for halve the time a peer is maximally allowed to be not seen
    }
    return;
}

#pragma GCC diagnostic pop

#endif