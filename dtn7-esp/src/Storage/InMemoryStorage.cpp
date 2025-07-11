#include "InMemoryStorage.hpp"
#include <iterator>
#include <sstream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void InMemoryStorage::addNode(Node node) {
    // just add node to the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    nodes[node.identifier] = node;
    xSemaphoreGive(nodesMutex);
    return;
}

void InMemoryStorage::removeNode(std::string address) {
    // just remove node from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    nodes.erase(address);
    xSemaphoreGive(nodesMutex);
}

/// @brief gets the nodes object to a given Address, if it was seen before, otherwise returns a empty node object
/// @param address
/// @return
Node InMemoryStorage::getNode(std::string address) {
    // get the node from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    Node result = nodes[address];

    // if the node is contained but does not have a URI, clean it up from storage
    if (result.URI == "none")
        nodes.erase(address);
    xSemaphoreGive(nodesMutex);
    return result;
}

std::vector<Node> InMemoryStorage::getNodes() {
    ESP_LOGI("InMemoryStorage::getNodes()", "getting nodes");
    // get all nodes from the nodes map, use semaphore to ensure thread safety
    xSemaphoreTake(nodesMutex, portMAX_DELAY);
    std::vector<Node> result;

    // use a map iterator to iterate through all stored elements
    for (std::unordered_map<std::string, Node>::iterator it = nodes.begin();
         it != nodes.end(); ++it) {
        result.push_back(it->second);
    }

    // release mutex
    xSemaphoreGive(nodesMutex);
    return result;
}

bool InMemoryStorage::checkSeen(std::string bundleID) {
    ESP_LOGD("InMemoryStorage::checkSeen", "checking bundle ID: %s",
             bundleID.c_str());
    bool result = false;
    // use semaphore to ensure thread safety
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);

    // if the bundle id is found, i.e. the result of find is not bundles end, the bundle id is already contained
    result = !(bundle_ids.find(bundleID) == bundle_ids.end());

    // release mutex
    xSemaphoreGive(bundleIdMutex);
    return result;
}

void InMemoryStorage::storeSeen(std::string bundleID) {
    ESP_LOGD("InMemoryStorage::storeSeen", "storing bundle ID: %s",
             bundleID.c_str());
    size_t estimatedSize = sizeof(std::string);
    // insert bundleID into set, use semaphore to ensure thread safety
    xSemaphoreTake(bundleIdMutex, portMAX_DELAY);
    bundle_ids.insert(bundleID);
    ESP_LOGI("InMemoryStorage::storeSeen",
             "stored bundle ID: %s ,number of stored Ids: %u, estimated size "
             "of this Storage entry:%u",
             bundleID.c_str(), bundle_ids.size(), estimatedSize);
    xSemaphoreGive(bundleIdMutex);
    return;
}

bool InMemoryStorage::removeBundle(std::string bundleID) {
    // get the mutex tro ensure that no other thread operates on the stored bundles
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);

    // iterate through all stored bundles and compare the bundleIDs
    for (auto it = bundles.begin(); it != bundles.end();) {
        if ((*it).bundle.getID() == bundleID) {
            // if the desired bundle is found, remove it
            it = bundles.erase(it);
            break;
        }
        it++;
    }

    // release the mutex
    xSemaphoreGive(bundlesMutex);
    return false;
}

/// @brief Stores a bundle, if there isn't sufficient space, the oldest stored bundle is removed from Storage, and returned in a Vector
/// @param bundle the Bundle to be stored
/// @return a Vector containing the bundle which was deleted to make space for the new bundle
std::vector<BundleInfo> InMemoryStorage::delayBundle(BundleInfo* bundle) {
    std::vector<BundleInfo> result;
    ESP_LOGI("delay Bundle", "stored Bundles: %u, max Stored Bundles:%u",
             bundles.size(), maxStoredBundles);

    // take bundles mutex because the size() method of bundles is used
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);

    // check if it is allowed to store an additional bundle and if not delete the oldest one
    if (bundles.size() >= maxStoredBundles) {
        // we have to give bundles mutex, as delete oldest needs it to function
        xSemaphoreGive(bundlesMutex);
        result.push_back(deleteOldest());

        // retake bundles Mutex after delete oldest has finished
        xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    }
    // insert the bundle into the list of stored bundles
    bundles.insert(bundles.end(), *bundle);

    // release the mutex
    xSemaphoreGive(bundlesMutex);
    return result;
}

std::vector<BundleInfo> InMemoryStorage::getBundlesRetry() {
    std::vector<BundleInfo> result;
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    for (int i = 0; i < CONFIG_RetryBatchSize; i++) {
        if (bundlesToReturn == 0)
            break;
        result.push_back(bundles.front());
        bundles.pop_front();
        bundlesToReturn--;
    }
    xSemaphoreGive(bundlesMutex);
    return result;
}

BundleInfo InMemoryStorage::deleteOldest() {
    ESP_LOGI("InMemoryStorage::deleteOldest()", "taking bundlesMutex");
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    ESP_LOGI("InMemoryStorage::deleteOldest()", "got bundlesMutex");
    auto oldest = bundles.begin();
    uint64_t oldestTime = (*oldest).bundle.receivedAt;
    for (auto it = bundles.begin(); it != bundles.end(); it++) {
        uint64_t currentAge = (*it).bundle.receivedAt;
        if (currentAge < oldestTime) {
            oldest = it;
            oldestTime = currentAge;
        }
        vTaskDelay(1);  // needed to avoid watchdog
    }
    BundleInfo result = *oldest;
    bundles.erase(oldest);
    xSemaphoreGive(bundlesMutex);  // give bundles mutex
    ESP_LOGI("InMemoryStorage::deleteOldest()", "released bundlesMutex");
    return result;
}

void InMemoryStorage::beginRetryCycle() {
    xSemaphoreTake(bundlesMutex, portMAX_DELAY);
    bundlesToReturn = bundles.size();
    xSemaphoreGive(bundlesMutex);
    return;
}

bool InMemoryStorage::hasBundlesToRetry() {
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
