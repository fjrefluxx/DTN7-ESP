#include "sdkconfig.h"
#if CONFIG_USE_BLE_CLA
#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

//generate error message if BLE CLA is enabled without NimBLE beeing enabled
#ifndef CONFIG_BT_NIMBLE_ENABLED
#error "The BLE CLA requires NimBLE to be enabled in menuconfig!"
#endif

extern "C" {
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_random.h"
#include "esp_system.h"

#include "nvs_flash.h"
}

/**
 * @file BLEhandling.hpp
 * @brief This file contains the helper functions to interact with the nimble GAP/GATT functions.
 * @note For the BLE-CLA to work, the following settings must be made in menuconfig:
 *           CONFIG_BT_ENABLED = y
 *           CONFIG_BT_NIMBLE_ENABLED = y
 */


// define the times required for advertising/scanning
#define ADVERTISE_TIME_MS CONFIG_BLE_ADVERTISE_TIME
#define SCAN_TIME_MS CONFIG_BLE_SCAN_TIME
#define MaxRandomOffsetScanSwitch CONFIG_BLE_MAX_RANDOM_OFFSET

// ble device appearance Tags
#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00
#define BLE_GAP_LE_ROLE_PERIPHERAL_CENTRAL 0x02

// size of buffer to initialize for own node id
#define MAX_URI_BUFFER_SIZE 128

// maximum length of node id to advertise as name, anything longer is cut of
#define MAX_URI_LENGTH_ADVERTISED 18

/// @brief global buffer for own Node URI
extern char nodeURI[MAX_URI_BUFFER_SIZE];
/// @brief length of own URI
extern uint8_t uriLength;

/* Library function declarations */
extern "C" void ble_store_config_init(void);

/// @brief UUID of the GATT service offered for message upload
static const ble_uuid16_t serviceUUID =
    BLE_UUID16_INIT(CONFIG_BLE_SERVICE_UUID);

/// @brief UUID of the GATT characteristic offered for message upload
static const ble_uuid16_t writeUUID = BLE_UUID16_INIT(CONFIG_BLE_WRITE_UUID);

/// @brief Device Name Characteristic UUID, required because the device name is offered as a GATT service
static const ble_uuid16_t deviceNameUUID = BLE_UUID16_INIT(0x2A00);

/// @brief Generic Access Service UUID, required because the device name is offered as a GATT service
static const ble_uuid16_t genericAccessUUID = BLE_UUID16_INIT(0x1800);

/// @brief used to keep track whether the node is currently in an advertising or scanning phase
extern bool isAdvertising;

extern "C" {
/// @brief starts BLE advertising, stops scanning
void ble_advertise();

/// @brief starts BLE scanning, stops advertising
void start_ble_scan();

/// @brief handle to react to GATT events
/// @param event event descriptor
/// @param arg additional arguments, unused
/// @return potential error code
int gap_event_handler(struct ble_gap_event* event, void* arg);

/// @brief used to print a connection by its descriptor
/// @param desc a BLE connection descriptor
void print_conn_desc(struct ble_gap_conn_desc* desc);

/// @brief handle BLE peer discovery, wraps the corresponding function of the BLE CLA, but is callable from C
/// @param addr address of the peer
/// @param name name of the peer
/// @param nameLength length of the name of the peer
void cPeerDiscovery(const ble_addr_t& addr, char* name, uint8_t nameLength);

/// @brief handles further processing of bundles received via BLE
/// @param bundle bundle which was received
/// @param senderAddr address of node from which it was received
void handleBLEReception(Bundle* bundle, ble_addr_t* senderAddr);

/// @brief callback which is called when BLE stack is fully setup
void ble_on_sync();

/// @brief callback which is called when BLE stack is reset
/// @param reason reason code for reset
void on_stack_reset(int reason);

/// @brief callback which is called when a GATT characteristic/service has bee registered, just does logging
/// @param ctxt
/// @param arg
void gatt_svr_register_cb(struct ble_gatt_register_ctxt* ctxt, void* arg);

/// @brief function to connect to a BLE peer, used in order to transmit a bundle to this peer
/// @param addr address of peer to connect to
/// @return response code of ble_gap_connect
int connect_to_peer(ble_addr_t* addr);

/// @brief task which handles switching between advertising and scanning
/// @param arg
void switchScanAdvertiseTask(void* arg);

/// @brief task which periodically checks if ble peers have exceeded their age
/// @param arg
void cleanPeersTask(void* arg);
}

/// @brief Callback to return the device name when read from GATT characteristic
/// @param conn_handle
/// @param attr_handle
/// @param ctxt
/// @param arg
/// @return
static int gatt_svr_chr_access_device_name(uint16_t conn_handle,
                                           uint16_t attr_handle,
                                           struct ble_gatt_access_ctxt* ctxt,
                                           void* arg) {
    return os_mbuf_append(ctxt->om, nodeURI, uriLength);
}

/// @brief this callback is called when data is written to the write service, meaning if another node has sent a bundle to this node, bundle reception via ble must be handled here
/// @param conn_handle
/// @param attr_handle
/// @param ctxt
/// @param arg
/// @return
static int ble_write_callback(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg) {
    if (!(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR))
        return 0;
    ESP_LOGI("BLE Receiver", "Received data with length: %u", ctxt->om->om_len);
    uint8_t recdata[ctxt->om->om_len];
    ble_hs_mbuf_to_flat(ctxt->om, recdata, ctxt->om->om_len, NULL);
    /* uncomment to print received data
    for (size_t i = 0; i < sizeof(recdata); i++) {
        printf("%02X ", recdata[i]);  // %02X prints each byte in two-digit hex format
    }
    printf("\n");
    */
    ble_gap_terminate(
        conn_handle,
        BLE_ERR_REM_USER_CONN_TERM);  // terminate the connection upon reception of the data

    if (recdata[0] == 0x9f) {
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(conn_handle, &desc);
        if (rc == 0) {
            // here could be filtered by rssi
            int8_t rssi;
            ble_gap_conn_rssi(conn_handle, &rssi);
            ESP_LOGI("BLE Receiver",
                     "Received data from conn_handle = %d (RSSI = %d dBm)",
                     conn_handle, rssi);
            Bundle* received = Bundle::fromCbor(recdata, sizeof(recdata));
            handleBLEReception(received, &desc.peer_id_addr);
        }
    }
    // printf("recdata[0]:%u ,recdata[sizeof(recdata-3)]:%u\n" ,recdata[0], recdata[sizeof(recdata-3)]);
    return 0;
}

/// @brief this struct defines the GATT serviced offered by the node
static const struct ble_gatt_svc_def gatt_services[] = {
    {// first the service which offers the ability to upload bundles
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &serviceUUID.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {
                 // characteristic which offers the data upload
                 .uuid = &writeUUID.u,
                 .access_cb = ble_write_callback,
                 .flags = BLE_GATT_CHR_F_WRITE,
             },
             {
                 0, /* No more characteristics in this service. */
             }}},
    {
        // then the service which offers the ability to read the device name
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &genericAccessUUID.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // Device Name Characteristic
                    .uuid = &deviceNameUUID.u,
                    .access_cb = gatt_svr_chr_access_device_name,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    0,  // No more characteristics
                },
            },
    },

    {
        0, /* No more services. */
    },
};

/// @brief initializes the nimble stack
/// @param
static void nimble_host_config_init(void) {
    // set the preferred MTU to a larger size
    ble_att_set_preferred_mtu(512);

    // add all the callbacks
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Store host configuration
    ble_store_config_init();
}

/// @brief task to run the nimble host
/// @param param
static void nimble_host_task(void* param) {
    ESP_LOGI("Nimble Host Task", "nimble host task has been started!");

    // This function won't return until nimble_port_stop() is executed
    nimble_port_run();

    // Clean up at exit
    vTaskDelete(NULL);
}

#pragma GCC diagnostic pop

#endif