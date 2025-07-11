#include "FlashStorage.hpp"
#include <sstream>
#include "Storage.hpp"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

void FlashStorage::addNode(Node node) {
    // just add node to the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    nodes[node.URI] = node;
    xSemaphoreGive(nodesMutex);
    return;
}

void FlashStorage::removeNode(std::string address) {
    // just remove node from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    ESP_LOGD("FlashStorage:removeNode", "removing node : %s", address.c_str());
    nodes.erase(address);
    xSemaphoreGive(nodesMutex);
    return;
}

Node FlashStorage::getNode(std::string address) {
    // get the node from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    Node result = nodes[address];

    // if the node is contained but does not have a URI, clean it up from storage
    if (result.URI == "none")
        nodes.erase(address);
    xSemaphoreGive(nodesMutex);
    return result;
}

std::vector<Node> FlashStorage::getNodes() {
    ESP_LOGI("FlashStorage::getNodes()", "getting nodes");
    // get all nodes from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    std::vector<Node> result;

    // use a map iterator to iterate through all stored elements
    for (std::unordered_map<std::string, Node>::iterator it = nodes.begin();
         it != nodes.end(); ++it) {
        result.push_back(it->second);
    }
    xSemaphoreGive(nodesMutex);
    return result;
}

bool FlashStorage::checkSeen(std::string bundleID) {
    ESP_LOGD("FlashStorage::checkSeen", "checking bundle ID: %s",
             bundleID.c_str());
    bool result = false;
    // use semaphore to ensure thread safety
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);

    // if the bundle id is found, i.e. the result of find is not bundles end, the bundle id is already contained
    result = !(bundle_ids.find(bundleID) == bundle_ids.end());
    xSemaphoreGive(bundleIdMutex);
    return result;
}

void FlashStorage::storeSeen(std::string bundleID) {
    ESP_LOGD("FlashStorage::storeSeen", "storing bundle ID: %s",
             bundleID.c_str());

    // insert bundleID into set, use semaphore to ensure thread safety
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);
    bundle_ids.insert(bundleID);
    ESP_LOGI("FlashStorage::storeSeen",
             "stored bundle ID: %s ,number of stored Ids: %u", bundleID.c_str(),
             bundle_ids.size());
    xSemaphoreGive(bundleIdMutex);
    return;
}

bool FlashStorage::removeBundle(std::string bundleID) {
    ESP_LOGE(" FlashStorage::removeBundle", "removeBundle not implemented!");
    return false;
}

std::vector<BundleInfo> FlashStorage::delayBundle(BundleInfo* bundle) {
    // bundles are stored serialized in the flash, thus, begin by serializing the BundleInfo object
    std::vector<uint8_t> serialized = bundle->serialize();

    // in case old bundles have to be removed to make space for this bundle, initialize a vector to store these potentially removed bundles in
    std::vector<BundleInfo> result;

    // take the bundlesMutex, as in the following the flash will be accessed and thread safety has to be ensured
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);

    // get statistics about the flash usage, and print them to the log
    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGI("DelayBundle FlashStorage",
             "Count: UsedEntries = (%u), FreeEntries = (%u), AvailableEntries "
             "= (%u), AllEntries = (%u), Required Entries For This:%u",
             nvs_stats.used_entries, nvs_stats.free_entries,
             nvs_stats.available_entries, nvs_stats.total_entries,
             (2 + (serialized.size() / 32) + 1));

    // Check that the flash has enough space to store the bundle. Total entries needed for blob: 2(overhead) + 1 pro 32 byte.
    // For some reason, the last 35 available entries are not used; this is checked here to, with some safety buffer
    while (nvs_stats.available_entries - 40 <
           (2 + (serialized.size() / 32) + 1)) {
        // as long as not enough space is available, we remove the oldest bundle from the storage. As this requires the mutex for bundles,
        // we have to release that here in order to avoid a deadlock
        xSemaphoreGive(bundlesMutex);

        // remove the oldest bundle and add it to the list of removed bundles
        result.push_back(deleteOldest());

        // retake the mutex and get the flash stats again
        xSemaphoreTake(bundlesMutex, portMAX_DELAY);
        nvs_get_stats(NULL, &nvs_stats);
    }

    // as the NVS flash storage is a key value storage, we need to asign a key to each bundle.
    // For this purpose an integer is used which has to be incremented for each ne stored bundle.
    highestUsedKey++;

    // write the bundle to flash
    nvs_set_blob(flashHandle, std::to_string(highestUsedKey).c_str(),
                 &serialized[0], serialized.size());

    // If this bundle is older than the currently stored oldest bundle, meaning it has been received by this node earlier, set it as the oldest bundle.
    // Therefore, update the information about the oldest stored bundle.
    // This will only happen during a retry cycle, which reads bundles in the order of their key and then potentially re-adds them to storage.
    // If bundles were not retried, the oldest bundle would always be the one with the lowest key.
    if (bundle->bundle.receivedAt < oldestReceivedTime) {
        oldestReceivedTime = bundle->bundle.receivedAt;
        oldestKey = highestUsedKey;
    }

    // If configured in menuconfig, the flash storage is able to keep its information between restarts.
    // To facilitate this it is required that information about the location of the bundles in flash is also stored in flash.
    // Therefore we need to store the information about the used keys.
#if CONFIG_KeepBetweenRestart
    nvs_set_u32(flashHandle, "HighestKey", highestUsedKey);
    nvs_set_u32(flashHandle, "LowestKey", lowestUsedKey);
    nvs_set_u32(flashHandle, "OldestKey", oldestKey);

#endif
    // the NVS storage might do some caching, with nvs_commit() it finishes the flash operations
    nvs_commit(flashHandle);

    // all flash operations finished, release the mutex
    xSemaphoreGive(bundlesMutex);

    return result;
}

std::vector<BundleInfo> FlashStorage::getBundlesRetry() {
    ESP_LOGI("getBundlesRetry", "getting bundles from flash");

    // create a vector to return bundles to be retired
    std::vector<BundleInfo> result;

    // take the mutex for bundles, as we will interact with the flash
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);

    // get as many bundles as configured in menuconfig
    for (int i = 0; result.size() < CONFIG_RetryBatchSize; i++) {
        // if no bundles which have not been retired in this retry cycle are present, stop the operation
        if (bundlesToReturn == 0)
            break;

        // Read the size of memory space required for blob
        size_t required_size = 0;
        nvs_get_blob(flashHandle, std::to_string(lowestUsedKey).c_str(), NULL,
                     &required_size);

        // Read previously saved blob if available
        uint8_t cbor[required_size];
        if (required_size > 0) {
            // read the data from flash
            nvs_get_blob(flashHandle, std::to_string(lowestUsedKey).c_str(),
                         cbor, &required_size);

            // remove the data from the flash, as it will be stored with a different key if it is re-inserted ore not at all if the forwarding is successful after this retry.
            nvs_erase_key(flashHandle, std::to_string(lowestUsedKey).c_str());

            // add the bundle to bundles read from flash
            result.push_back(
                BundleInfo(std::vector(cbor, cbor + required_size)));
        }

        if (lowestUsedKey == oldestKey) {
            // in case the now removed bundle was the oldest, the one following it becomes the oldest
            oldestKey++;
        }

        // the lowest used key now needs to point to the next stored bundle
        lowestUsedKey++;

        // decrease the number of bundles which needs to be read in this batch
        bundlesToReturn--;
    }

    // If configured in menuconfig, the flash storage is able to keep its information between restarts.
    // To facilitate this it is required that information about the location of the bundles in flash is also stored in flash.
    // Therefore we need to store the information about the used keys.
#if CONFIG_KeepBetweenRestart
    nvs_set_u32(flashHandle, "HighestKey", highestUsedKey);
    nvs_set_u32(flashHandle, "LowestKey", lowestUsedKey);
    nvs_set_u32(flashHandle, "OldestKey", oldestKey);
#endif
    // the NVS storage might do some caching, with nvs_commit() it finishes the flash operations
    nvs_commit(flashHandle);

    // all flash operations finished, release the mutex
    xSemaphoreGive(bundlesMutex);
    return result;
}

inline bool isXolder(uint64_t xTime, uint64_t xNum, uint64_t yTime,
                     uint64_t yNum) {
    // if the time of x is unknown
    if (xTime == 0) {
        // and the time of y is unknown
        if (yTime == 0) {
            // decide only according to sequence number. THis decision has nothing todo with the actual age
            return xNum < yNum;
        }
        else {
            // if x has no accurate clock it is preferred
            return false;
        }
    }
    else if (yTime == 0) {
        // if y has no accurate clock it is preferred
        return true;
    }
    // if both x and y have have the same  creation time, decide according to sequence number. This decision has nothing to do with the actual age
    else if (xTime == yTime) {
        return xNum < yNum;
    }
    // if both x and y have information about the creation time this decision is simple, just compare the creation times
    else
        return xTime < yTime;
}

BundleInfo FlashStorage::deleteOldest() {
    ESP_LOGI("DelayBundle FlashStorage", "Deleting oldest bundle from flash");
    // first get the size required to read the oldest bundle
    size_t required_size = 0;
    nvs_get_blob(flashHandle, std::to_string(oldestKey).c_str(), NULL,
                 &required_size);

    // create accordingly sized buffer
    uint8_t cbor[required_size];
    if (required_size > 0) {
        // read the bundle from flash
        nvs_get_blob(flashHandle, std::to_string(oldestKey).c_str(), cbor,
                     &required_size);

        // and remove the key
        nvs_erase_key(flashHandle, std::to_string(oldestKey).c_str());
    }

    // If this function is called during a retry cycle, we can detect this by checking whether there are bundles to be returned (bundlesToReturn != 0).
    // If the previously oldest key was not the lowest key (lowestUsedKey != oldestKey), this means that the oldest bundle had already been re-inserted into storage.
    // This means that the first bundle to have been retried, which is propably the oldest, will have the lowest key after the bundles beeing retried in this retry cycle (lowestUsedKey+bundlesToReturn)
    if (bundlesToReturn != 0 && lowestUsedKey != oldestKey &&
        lowestUsedKey + bundlesToReturn < highestUsedKey) {
        // we expect the oldest bundle to have already been retried, therefore, it should probably be the one after the end of the bundles beeing retried
        oldestKey = lowestUsedKey + bundlesToReturn;
    }
    else {
        oldestKey++;  // we expect the next oldest bundle just to be the next one
    }

    // retrun the bundles which have been read from storage
    return BundleInfo(std::vector(cbor, cbor + required_size));
}

void FlashStorage::beginRetryCycle() {
    // we want that the currently stored bundles are not change during the following operation, therfore take mutex
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);

    // all bundles which are stored at the moment should be retried, calculate how many this is
    bundlesToReturn = highestUsedKey - lowestUsedKey + 1;

    // release mutex
    xSemaphoreGive(bundlesMutex);
    return;
}

bool FlashStorage::hasBundlesToRetry() {
    // just check if there are still bundles to be returned in this retry cycle
    return (bundlesToReturn != 0);
}
