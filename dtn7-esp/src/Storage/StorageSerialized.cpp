#include <iterator>
#include <sstream>
#include <unordered_map>
#include <vector>
#include "Data.hpp"
#include "InMemoryStorage.hpp"
#include "Storage.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void InMemoryStorageSerialized::addNode(Node node) {
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    nodes[node.URI] = node.serialize();
    xSemaphoreGive(nodesMutex);
    return;
}

void InMemoryStorageSerialized::removeNode(std::string address) {
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    // nodes.insert( {node.identifier ,node});
    nodes.erase(address);
    xSemaphoreGive(nodesMutex);
}

/// @brief gets the node object for a given address, if it was seen before. Otherwise returns an empty node object.
/// @param address address(unique identifier) to look up
/// @return
Node InMemoryStorageSerialized::getNode(std::string address) {
    Node result;
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    if (nodes.find(address) != nodes.end())
        result = Node(nodes.at(address));
    xSemaphoreGive(nodesMutex);
    return result;
}

std::vector<Node> InMemoryStorageSerialized::getNodes() {
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    std::vector<Node> result;
    for (std::unordered_map<std::string, std::vector<uint8_t>>::iterator it =
             nodes.begin();
         it != nodes.end(); ++it) {
        result.push_back(Node(it->second));
    }
    xSemaphoreGive(nodesMutex);
    return result;
}

/// @brief checks whether a bundle with a given ID was seen Before
/// @param bundleID
/// @return true if the bundle was seen before
bool InMemoryStorageSerialized::checkSeen(std::string bundleID) {
    ESP_LOGD("check Seen", "checking bundle ID: %s", bundleID.c_str());
    bool result = false;
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);
    result = !(
        bundle_ids.find(bundleID) ==
        bundle_ids
            .end());  // if the bundle id is found, i.e., the result of find is not a bundle's end, the bundle id is already contained
    xSemaphoreGive(bundleIdMutex);
    return result;
}

void InMemoryStorageSerialized::storeSeen(std::string bundleID) {
    ESP_LOGD("store Seen", "storing bundle ID: %s", bundleID.c_str());
    size_t estimatedSize = sizeof(std::string);
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);
    bundle_ids.insert(bundleID);
    ESP_LOGI("store Seen",
             "stored bundle ID: %s ,number of stored Ids: %u, estimated size "
             "of this Storage entry:%u",
             bundleID.c_str(), bundle_ids.size(), estimatedSize);
    xSemaphoreGive(bundleIdMutex);
    return;
}

bool InMemoryStorageSerialized::removeBundle(std::string bundleID) {
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    for (auto it = bundles.begin(); it != bundles.end();) {
        if (it->first == bundleID) {
            it = bundles.erase(it);
            break;
        }
        it++;
    }
    xSemaphoreGive(bundlesMutex);
    return false;
}

/// @brief Stores a bundle. If there is insufficient space, the oldest stored bundle is removed from storage, and returned in a vector.
/// @param bundle the bundle to be stored
/// @return a vector containing the bundle which was deleted to make space for the new bundle
std::vector<BundleInfo> InMemoryStorageSerialized::delayBundle(
    BundleInfo* bundle) {
    size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    std::vector<uint8_t> serialized = bundle->serialize();
    size_t estimatedSize =
        serialized.size() +
        sizeof(std::pair<std::string, std::vector<uint8_t>>) +
        bundle->bundle.getID()
            .size();  // rough estimate of additional storage size needed for the given bundle
    std::vector<BundleInfo> result;

    int i = 0;  // used to keep track of number of removed bundles
    while ((freeHeap - estimatedSize <= CONFIG_TargetFreeHeap) &&
           i < maxRemovedBundles) {
        freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        result.push_back(deleteOldest());
        i++;
    }
    freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    bundles.insert(bundles.end(), std::pair<std::string, std::vector<uint8_t>>(
                                      bundle->bundle.getID(), serialized));
    ESP_LOGI("delay Bundle",
             "free Heap:%u, estimate of bundles with this size that could "
             "still be stored: %u, num of Stored: %u",
             freeHeap, (freeHeap - CONFIG_TargetFreeHeap) / (estimatedSize + 4),
             bundles.size());
    xSemaphoreGive(bundlesMutex);
    return result;
}

std::vector<BundleInfo> InMemoryStorageSerialized::getBundlesRetry() {
    ESP_LOGI("getBundlesRetry", "getting bundles from storage");
    std::vector<BundleInfo> result;
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    for (int i = 0; i < CONFIG_RetryBatchSize; i++) {
        if (bundlesToReturn == 0)
            break;
        result.push_back(BundleInfo(bundles.front().second));
        bundles.pop_front();
        bundlesToReturn--;
    }

    xSemaphoreGive(bundlesMutex);
    return result;
}

BundleInfo InMemoryStorageSerialized::deleteOldest() {
    ESP_LOGW("InMemoryStorageSerialized::deleteOldest()",
             "searching oldest bundle");
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    auto oldest = bundles.begin();
    uint64_t oldestTime = BundleInfo(oldest->second).bundle.receivedAt;
    for (auto it = bundles.begin(); it != bundles.end(); it++) {
        uint64_t currentAge = BundleInfo(it->second).bundle.receivedAt;
        if (currentAge < oldestTime) {
            oldest = it;
            oldestTime = currentAge;
        }
        vTaskDelay(1);  // avoid watchdog
    }

    BundleInfo result(oldest->second);
    bundles.erase(oldest);
    ESP_LOGW("InMemoryStorageSerialized::deleteOldest()",
             "removed oldest bundle, num of StoredBundles:%u", bundles.size());
    xSemaphoreGive(
        bundlesMutex);  // give bundles mutex before calling remove bundle, as it is needed by remove bundle

    return result;
}
void InMemoryStorageSerialized::beginRetryCycle() {
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    bundlesToReturn = bundles.size();
    xSemaphoreGive(bundlesMutex);
    return;
}

bool InMemoryStorageSerialized::hasBundlesToRetry() {
    return (bundlesToReturn != 0);
}

inline bool isXolder(uint64_t xTime, uint64_t xNum, uint64_t yTime,
                     uint64_t yNum) {
    if (xTime == 0) {
        if (yTime == 0)
            return xNum < yNum;
        else
            return false;  // if x has no accurate clock it is preferred
    }
    else if (yTime == 0)
        return true;  // if y has no accurate clock it is preferred
    else if (xTime == yTime)
        return xNum < yNum;
    else
        return xTime < yTime;
}

std::vector<BundleInfo> InMemoryStorageSerializedIA::delayBundle(
    BundleInfo* bundle) {
    size_t freeHeap =
        heap_caps_get_free_size(MALLOC_CAP_8BIT);  // get current free heap
    std::vector<uint8_t> serialized =
        bundle->serialize();  // serialize the bundle to be stored
    size_t estimatedSize =
        serialized.size() +
        sizeof(
            std::pair<std::string, std::pair<std::vector<uint8_t>, uint64_t>>) +
        bundle->bundle.getID().size() +
        sizeof(
            uint64_t);  // rough estimate of additional storage size needed for the given bundle
    std::vector<BundleInfo>
        result;  // create a vector for old bundles which are potentially removed
    int i = 0;   // used to keep track of number of removed bundles

    // if still not enough heap is free: remove the oldest n (always maxRemovedBundles) from storage and add removed bundles to the result vector
    while ((freeHeap - estimatedSize <= CONFIG_TargetFreeHeap) &&
           i < maxRemovedBundles) {
        freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        result.push_back(deleteOldest());
        i++;
    }
    freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    xSemaphoreTake(bundlesMutex,
                   portMAX_DELAY);  // bundles are modified, mutex needed
    // then insert it into the map, remember, in this case the bundles list stores a pair consisting of the bundle id and a pair consisting of the serialized bundleInfo and the time this bundle was received
    bundles.insert(
        bundles.end(),
        std::pair<std::string, std::pair<std::vector<uint8_t>, uint64_t>>(
            bundle->bundle.getID(),
            std::pair<std::vector<uint8_t>, uint64_t>(
                serialized, bundle->bundle.receivedAt)));
    ESP_LOGI("delay Bundle",
             "free Heap:%u, estimate of bundles with this size that could "
             "still be stored: %u, num of Stored: %u",
             freeHeap, (freeHeap - CONFIG_TargetFreeHeap) / (estimatedSize + 4),
             bundles.size());
    xSemaphoreGive(bundlesMutex);
    return result;
}

std::vector<BundleInfo> InMemoryStorageSerializedIA::getBundlesRetry() {
    ESP_LOGI("getBundlesRetry", "getting Bundles From Storage");
    std::vector<BundleInfo> result;
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    for (int i = 0; i < CONFIG_RetryBatchSize; i++) {
        if (bundlesToReturn == 0)
            break;
        result.push_back(BundleInfo(bundles.front().second.first));
        bundles.pop_front();
        bundlesToReturn--;
    }
    xSemaphoreGive(bundlesMutex);
    return result;
}

BundleInfo InMemoryStorageSerializedIA::deleteOldest() {
    ESP_LOGW("InMemoryStorageSerializedIA::deleteOldest()",
             "searching oldest bundle");
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    auto oldest = bundles.begin();
    uint64_t oldestTime = oldest->second.second;
    for (auto it = bundles.begin(); it != bundles.end(); it++) {
        uint64_t currentAge = it->second.second;
        if (currentAge < oldestTime) {
            oldest = it;
            oldestTime = currentAge;
        }
        vTaskDelay(1);  // needed to avoid watchdog
    }

    BundleInfo result(oldest->second.first);
    bundles.erase(oldest);
    ESP_LOGI("InMemoryStorageSerialized::deleteOldest()",
             "removed oldest bundle, num of StoredBundles:%u", bundles.size());
    xSemaphoreGive(
        bundlesMutex);  // give bundles mutex before calling remove bundle, as it is needed by remove bundle
    return result;
}

void InMemoryStorageSerializedIA::beginRetryCycle() {
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    bundlesToReturn = bundles.size();
    xSemaphoreGive(bundlesMutex);
    return;
}

bool InMemoryStorageSerializedIA::hasBundlesToRetry() {
    return (bundlesToReturn != 0);
}
