#pragma once
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include "Data.hpp"
#include "Storage.hpp"
#include "dtn7-bundle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define BUNDLE_STORAGE_NAMESPACE "bundles"

/**
 * @file FlashStorage.hpp
 * @brief This file contains the definitions for the FlashStorage.
 */

/// @brief class which stores bundles in the ESP32's flash, a custom partition table is to be used
class FlashStorage : public Storage {
    /// @brief stores known Nodes, key is node address/identifier
    std::unordered_map<std::string, Node> nodes;

    /// @brief stores known Bundle Ids
    std::set<std::string> bundle_ids;

    /// @brief the handle used to access the nvs flash storage
    nvs_handle_t flashHandle;

    /// @brief stores the key of the oldest bundle stored in flash. This is update when bundles are removed/inserted and used in order to not require a search operation if the oldest bundle shall be removed for space reasons.
    uint32_t oldestKey = 0;

    /// @brief used to store the time the oldest bundle was received
    uint64_t oldestReceivedTime = 0;

    /// @brief stores the number of the highest key that has been used. This is required as the NVS storeage which is used to store the bundles only stores key/value pairs.
    ///        To use this principle to store a list of bundles, the key is simply set as an integer which is incremented for each newly stored bundle. For this reason the current range in which the keys are found has to be stored.
    uint32_t highestUsedKey = 0;

    /// @brief stores the first key that is used. See highestUsedKey. The lowest used key is required, as keys are not reused and the lowest key which is used is always incremented when a bundle is removed from storage.
    uint32_t lowestUsedKey = 0;

    /// @brief mutex to handle shared access to bundles
    SemaphoreHandle_t bundlesMutex;

    /// @brief mutex to handle shared access to nodes
    SemaphoreHandle_t nodesMutex;

    /// @brief mutex to handle shared access to bundleIDs
    SemaphoreHandle_t bundleIdMutex;

   public:
    FlashStorage() {
        ESP_LOGI("FlashStorage Setup",
                 "Setup FlashStorage\n It is strongly recommended to use "
                 "Custom Partition Table with Flash Storage!");
        // create the required mutexes for each type of stored data
        bundlesMutex = xSemaphoreCreateMutex();
        nodesMutex = xSemaphoreCreateMutex();
        bundleIdMutex = xSemaphoreCreateMutex();

        // initialize the flash
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // NVS partition was truncated and needs to be erased
            // Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        // If configured in menuconfig, the flash storage is able to keep its information between restarts.
        // In this case we need to find the key range in which the bundles are stored. This information is also stored in the flash and is read in the following.
        // The same goes for the information about which key belongs to the oldest stored bundle.
#if CONFIG_KeepBetweenRestart
        err = nvs_get_u32(flashHandle, "HighestKey", &highestUsedKey);
        if (err == ESP_ERR_NVS_NOT_FOUND)
            highestUsedKey = 0;
        err = nvs_get_u32(flashHandle, "LowestKey", &lowestUsedKey);
        if (err == ESP_ERR_NVS_NOT_FOUND)
            lowestUsedKey = 0;
        err = nvs_get_u32(flashHandle, "OldestKey", &oldestKey);
        if (err == ESP_ERR_NVS_NOT_FOUND)
            oldestKey = 0;

#else
        // if no information is to be kept in between restarts, we erase the flash
        nvs_flash_erase();
        err = nvs_flash_init();
#endif
        ESP_ERROR_CHECK(err);
        nvs_open(BUNDLE_STORAGE_NAMESPACE, NVS_READWRITE, &flashHandle);
    }

    ~FlashStorage() { nvs_close(flashHandle); }

    /// @brief adds a node to the known nodes, if it is already present, it is overwritten
    /// @param node node to store
    void addNode(Node node) override;

    /// @brief removes a node from the list of known nodes
    /// @param address
    void removeNode(std::string address) override;

    /// @brief gets a previously stored node object from a given address/identifier
    /// @param address
    /// @return the previously stored node if it exists, otherwise an empty node object
    Node getNode(std::string address) override;

    /// @brief gets all known nodes
    /// @return a vector containing all known nodes
    std::vector<Node> getNodes() override;

    /// @brief checks whether a bundleID was seen before
    /// @param bundleID std::string containing the BundleID to check
    /// @return whether the bundle Id was seen before
    bool checkSeen(std::string bundleID) override;

    /// @brief adds a BundleId to the known BundleIDs, if it is not already known, if it is already stored it is overridden
    /// @param bundleID bundleId to mark as seen
    /// @param node identifier of the node from which the Bundle was received
    void storeSeen(std::string bundleID) override;

    /// @brief removes a Bundle from storage
    /// @param bundleID BundleId of the bundle to remove
    /// @return true if the bundle was previously stored, otherwise false
    bool removeBundle(std::string bundleID) override;

    /// @brief stores a given bundle for later retransmission
    /// @param bundle bundle to store
    /// @return if other bundles were removed from storage in order to fit the new one, a vector of the removed bundles is returned
    std::vector<BundleInfo> delayBundle(BundleInfo* bundle) override;

    /// @brief returns a portion previously delayed bundles, exact size can be configured in menuconfig, as a vector, called repeatedly when retrying bundles, starts with the oldest batch of bundles, only returns bundles up until to the point where the last bundle was stored when beginRetryCycle() was called
    std::vector<BundleInfo> getBundlesRetry() override;

    /// @brief deletes the oldest stored Bundle
    /// @return the Deleted Bundle
    BundleInfo deleteOldest() override;

    /// @brief internally stores what the number of bundles was when it was called
    void beginRetryCycle() override;

    /// @brief returns whether there are still bundles to be returned in this retry
    bool hasBundlesToRetry() override;
};