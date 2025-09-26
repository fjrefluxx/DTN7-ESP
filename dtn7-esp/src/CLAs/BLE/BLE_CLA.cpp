#include "sdkconfig.h"
#if CONFIG_USE_BLE_CLA

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
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_bt.h"

#include "nvs_flash.h"
}

uint8_t cbor[maxBleBundleSize];
size_t cborSize = 0;
bool transmissionComplete = false;

std::string BleCLA::getName() {
    return this->name;
}

bool BleCLA::checkCanAddress() {
    return true;
}

BleCLA::BleCLA(std::string localURI) {
    // write Node URI + length in the buffer to make them accessible fom c
    strncpy(nodeURI, localURI.c_str(), localURI.length());
    uriLength = localURI.length();

    // initialize the NVS flash, as this is required to use BLE
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // initialize Nimble
    nimbleInit();

    // create task to clean up ble peers
    xTaskCreate(cleanPeersTask, "BLE Advertise Switch Task", 4 * 1024, NULL, 5,
                NULL);
}

void nimbleInit() {
    nimble_port_init();
    nimble_host_config_init();
    ble_gatts_count_cfg(gatt_services);
    ble_gatts_add_svcs(gatt_services);
    nimble_host_config_init();

    // create Nimble host task
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
    return;
}

BleCLA::~BleCLA() {
    // TODO, delete tasks, deinitialize ble
    nimble_port_stop();
}

std::vector<ReceivedBundle*> BleCLA::getNewBundles() {
    // return empty vector, as the BLE CLA does not require polling
    return std::vector<ReceivedBundle*>();
}

/// @brief finds the ble address of a given peer from the list of known peers
/// @param node Node of which the BLE address is searched
/// @return ble addres of the requested peer, if it is not found the type field of ble_addr_t is set to 255 in order to indicate failure
ble_addr_t findPeer(Node* node) {
    // search the targeted peer in the list of known peers
    BlePeer bleDest = BlePeer();
    bool foundPeer = false;
    for (BlePeer peer : DTN7::bleCla->currentPeers) {
        if (peer.name == node->URI) {
            foundPeer = true;
            bleDest = peer;
            break;
        }
    }

    ble_addr_t peerAddress;

    peerAddress.type =
        255;  // set the type field of the peer address to an impossible value, used to indicate that the address was not found
    // return that the transmission failed if peer was not found
    if (!foundPeer) {
        ESP_LOGE("BLE CLA", "Failed to find requested Peer");
        return peerAddress;
    }

    // get the peers address into the format required by the BLE library
    peerAddress.type = bleDest.type;
    for (int i = 0; i < 6; i++)
        peerAddress.val[i] = bleDest.addr[i];

    return peerAddress;
}

/// @brief handles the actual transmission of the bundle to the ble peer
/// @param bundle bundle to be transmitted
/// @param destination ble address of the peer which shall receive the bundle
/// @return
bool internSend(Bundle* bundle, ble_addr_t destination, int attempt) {
    // if the size of the CBOR in the global buffer is not 0, this means a previous bundle has not jet been transmitted, therefore we need to wait, for now just do this by polling
    while (cborSize != 0) {
        if (cborSize > maxBleBundleSize) {
            ESP_LOGW("BLE send",
                     "previous bundle failed or an connection attempt was made "
                     "during bundle transmission, declaring previous "
                     "transmission complete");
            cborSize = 0;
            break;
        }

        vTaskDelay(100);
    }
    uint8_t* localCbor;
    bundle->toCbor(
        &localCbor,
        cborSize);  // write the encoded bundle to the global send buffer

    if (cborSize > maxBleBundleSize) {
        return false;
    }
    memcpy(cbor, localCbor, cborSize);
    delete localCbor;
    transmissionComplete = false;
    int success =
        1;  // zero indicates successful connection, therfore initialize as non zero, 1 arbitrarily chosen

    if (attempt > 1) {
        vTaskDelay(attempt *
                   pdMS_TO_TICKS(200));  // back off more with each attempt
    }
    success = connect_to_peer(&destination);

    if (success != 0)
        return false;

    int limiter = 0;
    while (!transmissionComplete && limiter < 20) {
        if (cborSize > maxBleBundleSize) {
            ESP_LOGE("BLE send", "transmission failed");
            break;
        }
        ESP_LOGI("BLE send", "transmission still ongoing... cborSize:%u",
                 cborSize);
        vTaskDelay(pdMS_TO_TICKS(500));
        limiter++;
    }

    if (limiter >= 20) {
        cborSize = maxBleBundleSize + 1;  // apparently it has failed
    }

    bool result = cborSize == 0;
    cborSize = 0;
    ESP_LOGI("intern send", "returning %s", result ? "true" : "false");
    return result;
}

bool BleCLA::send(Bundle* bundle, Node* destination) {
    ESP_LOGI("BLE CLA", "CLA Handling transmission to:%s",
             destination->URI.c_str());

    // ensure that a sufficient gap exists between ble transmissions
    // get current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    // convert current time to us
    uint64_t currentTime =
        ((uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec);

    ESP_LOGI("BLE CLA", "Time since Last send operation: %llu ms",
             (uint64_t)(currentTime - lastSendTime) / 1000);
    if (currentTime - lastSendTime < minGapBetweenSendMS * 1000) {
        // delay if last send operation to recent
        ESP_LOGW("BLE CLA", "Last Send to recent, delaying a bit");
        vTaskDelay(pdMS_TO_TICKS(minGapBetweenSendMS -
                                 (currentTime - lastSendTime) / 1000));
    }

    // get current time again,  in case it was delayed
    gettimeofday(&tv_now, NULL);

    lastSendTime =
        ((uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec);

    // get ble address of requested node
    ble_addr_t peerAddress = findPeer(destination);
    if (peerAddress.type == 255)
        return false;

    for (int i = 0; i < CONFIG_BLE_SEND_ATTEMPTS; i++) {
        if (internSend(bundle, peerAddress, i))
            return true;
    }
    cborSize =
        0;  // here we do this again to make sure that we are ready for the next bundle
    return false;
}

void BleCLA::discoveredPeer(const uint type, const uint8_t val[6], char* name,
                            uint8_t nameLength) {
    /*
    ESP_LOGI("BLE CLA", "CLA Handling Discovery of: %02X:%02X:%02X:%02X:%02X:%02X, node Name: %.*s",
             val[5], val[4], val[3],
             val[2], val[1], val[0], nameLength, name);*/

    // get current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    // convert current time to us
    uint64_t currentTime =
        ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);

    // convert name of discovered peer to std string
    std::string peerName = std::string(name, name + nameLength);

    // create peer object
    BlePeer peer;
    // copy in address type
    peer.type = type;
    // copy address value to peer object
    for (int i = 0; i < 6; i++)
        peer.addr[i] = val[i];
    peer.last_seen = currentTime;
    peer.name = peerName;
    currentPeers.erase(peer); //remove old entry of peer, if it exists, in order to update its last seen time
    this->currentPeers.insert(peer);
    Node dtnNode = DTN7::BPA->storage->getNode(peerName);
    if (dtnNode.identifier == "empty")

#if CONFIG_NOTIFY_RETRY_TASK
        // the discovered node is new, notify the bundle retry task in order to check if we have bundles which should be delivered to it
        xTaskNotifyGive(DTN7::storageRetryHandle);
#endif

    dtnNode.identifier = std::string(val, val + 6);
    if (dtnNode.URI == "none")
        dtnNode.URI = peerName;
    dtnNode.setLastSeen();
    DTN7::BPA->storage->addNode(dtnNode);
    ESP_LOGI("BLE CLA", "Added Node: %s to known Nodes", dtnNode.URI.c_str());
    return;
};

void BleCLA::cleanUpBlePeers() {
    ESP_LOGI("BLE CLA cleanUpBlePeers", "Cleaning up old peers ...");
    std::vector<BlePeer> toRemove;
    for (BlePeer peer : this->currentPeers) {
        // get current time
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        // convert current time to us
        uint64_t currentTime =
            ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);
        ESP_LOGI("BLE CLA cleanUpBlePeers", "peers age :%llu limit: %i",
                 (currentTime - peer.last_seen) / 1000,
                 CONFIG_BLE_MAX_PEER_AGE);

        if (currentTime - peer.last_seen > CONFIG_BLE_MAX_PEER_AGE * 1000) {
            toRemove.push_back(peer);
        }
    }
    for (BlePeer remove : toRemove)
        currentPeers.erase(remove);
    return;
}

#endif