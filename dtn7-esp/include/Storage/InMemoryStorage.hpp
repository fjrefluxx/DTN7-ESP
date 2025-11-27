#pragma once
#include <list>
#include <set>
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

/**
 * @file Storage.hpp
 * @brief This file contains the definitions for the different versions of in-memory storage.
 *        These are:
 *              InMemoryStorage
 *              InMemoryStorageSerialized
 *              InMemoryStorageSerializedIA
 */

/// @brief stores bundles, nodes and bundle ids in memory
class InMemoryStorage : public Storage {
    /// @brief stores bundles, first value is BundleID
    std::list<BundleInfo> bundles;

    /// @brief stores known Nodes, key is node address/URI
    std::unordered_map<std::string, Node> nodes;

    /// @brief stores known Bundle Ids
    std::set<std::string> bundle_ids;

    /// @brief mutex to handle shared access to bundles
    SemaphoreHandle_t bundlesMutex;

    /// @brief mutex to handle shared access to nodes
    SemaphoreHandle_t nodesMutex;

    /// @brief mutex to handle shared access to bundleIDs
    SemaphoreHandle_t bundleIdMutex;

   private:
    /// @brief stores the limit of bundles allowed to be stored
    uint maxStoredBundles;

   public:
    InMemoryStorage() {
        ESP_LOGI("InMemoryStorage Setup", "Setup InMemoryStorage");
        maxStoredBundles = CONFIG_MaxStoredBundles;
        bundlesMutex = xSemaphoreCreateMutex();
        nodesMutex = xSemaphoreCreateMutex();
        bundleIdMutex = xSemaphoreCreateMutex();
    }
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

    /// @brief deletes the oldest stored Bundle. This function looks at all stored bundles and compares the time each bundle was received at. This is very slow.
    /// @return the Deleted Bundle
    BundleInfo deleteOldest() override;

    /// @brief internally stores what the number of bundles was when it was called
    void beginRetryCycle() override;

    /// @brief returns whether there are still bundles to be returned in this retry
    bool hasBundlesToRetry() override;
};

/// @brief Stores bundles, nodes and bundle IDs in memory.
///         Bundles and nodes are serialized for storage. This reduces the space required to store bundles.
///         The amount of memory which is used for bundle storage is limited indirectly, as a desired amount of heap which shall remain free can be set in menuconfig.
///         If this limit is surpassed by storing another bundle the oldest bundles are removed from storage. How many bundles are removed can be configured in menuconfig.
///         Note: removal is very slow due to some currently unknown problem.
class InMemoryStorageSerialized : public Storage {
    /// @brief stored bundles.  First value is the bundle ID, second value is the serialized bundle info (byte vector)
    std::list<std::pair<std::string, std::vector<uint8_t>>> bundles;

    /// @brief known nodes, key is node address/identifier, value is vector of bytes representing serialized node
    std::unordered_map<std::string, std::vector<uint8_t>> nodes;

    /// @brief stores known Bundle Ids
    std::set<std::string> bundle_ids;

    /// @brief mutex to handle shared access to bundles
    SemaphoreHandle_t bundlesMutex;

    /// @brief mutex to handle shared access to nodes
    SemaphoreHandle_t nodesMutex;

    /// @brief mutex to handle shared access to bundleIDs
    SemaphoreHandle_t bundleIdMutex;

    /// @brief defines how many bundles are maximally to be removed if there is not enough space to delay a bundle
    uint maxRemovedBundles = CONFIG_MaxRemovedBundles;

   public:
    InMemoryStorageSerialized() {
        ESP_LOGI("InMemoryStorageSerialized Setup",
                 "Setup InMemoryStorageSerialized");
        bundlesMutex = xSemaphoreCreateMutex();
        nodesMutex = xSemaphoreCreateMutex();
        bundleIdMutex = xSemaphoreCreateMutex();
    }

    /// @brief adds a node to the known nodes, if it is already present, it is overwritten
    /// @param node node to store
    void addNode(Node node) override;

    /// @brief removes a node from the list of known nodes
    /// @param address
    void removeNode(std::string address) override;

    /// @brief gets a previously stored node object from a given address/identifier
    /// @param address
    /// @return the previously stored node if it exists, otherwise a empty node object
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

    /// @brief Returns n previously delayed bundles as a vector.
    ///         The Exact size can be configured in menuconfig. Called repeatedly when retrying bundles. Starts with the oldest batch of bundles.
    ///         Only returns bundles up until to the point where the last bundle was stored when beginRetryCycle() was called.
    std::vector<BundleInfo> getBundlesRetry() override;

    /// @brief deletes the oldest stored bundle. This function de-serializes all stored bundles and comares all received at times, very slow, maybe due to some programming error
    /// @return the Deleted Bundle
    BundleInfo deleteOldest() override;

    /// @brief internally stores what the number of bundles was when it was called
    void beginRetryCycle() override;

    /// @brief returns whether there are still bundles to be returned in this retry
    bool hasBundlesToRetry() override;
};

/// @brief Stores bundles, nodes and bundle IDs in memory.
///        Bundles and nodes are serialized for storage. This reduces the space required to store bundles.
///        The amount of memory which is used for bundle storage is limited indirectly, as a desired amount of heap which shall remain free can be set in menuconfig.
///        If this limit is surpassed by storing another bundle the oldest bundles are removed from storage. How many bundles are removed can be configured in menuconfig.
///        If this occurs it is very slow, maybe due to some unknown error.
///        The improvement compared to InMemoryStorageSerialized is that the bundle age is stored unserialized together with their serialized form,
///        to not require de-serialization to find the oldest bundle, at the cost of some space efficiency.
///        This leads to a minor speed improvement if old bundles are removed.
class InMemoryStorageSerializedIA : public InMemoryStorage {
    /* 
     * Bundle list
     * First value is the bundle ID.
     * Second value is a pair, consisting of the serialized bundle info (byte vector) and the bundle reception time.
     */
    std::list<std::pair<std::string, std::pair<std::vector<uint8_t>, uint64_t>>>
        bundles;

    // stores known nodes, key is node address/identifier, value is node object
    std::unordered_map<std::string, Node> nodes;

    // stores known bundle IDs and node addresses from which bundle was last received
    std::unordered_map<std::string, std::string> bundle_ids;

    /// @brief defines how many bundles are maximally to be removed if there is not enough space to delay a bundle
    uint maxRemovedBundles = CONFIG_MaxRemovedBundles;

    /// @brief mutex to handle shared access to bundles
    SemaphoreHandle_t bundlesMutex;

    /// @brief mutex to handle shared access to nodes
    SemaphoreHandle_t nodesMutex;

    /// @brief mutex to handle shared access to bundleIDs
    SemaphoreHandle_t bundleIdMutex;

   public:
    InMemoryStorageSerializedIA() {
        ESP_LOGI("InMemoryStorageSerialized Setup",
                 "Setup InMemoryStorageSerializedIA");
        bundlesMutex = xSemaphoreCreateMutex();
        nodesMutex = xSemaphoreCreateMutex();
        bundleIdMutex = xSemaphoreCreateMutex();
    }

    /// @brief stores a given bundle for later retransmission
    /// @param bundle bundle to store
    /// @return if other bundles were removed from storage in order to fit the new one, a vector of the removed bundles is returned
    std::vector<BundleInfo> delayBundle(BundleInfo* bundle) override;

    /// @brief returns a portion previously delayed bundles, exact size can be configured in menuconfig, as a vector, called repeatedly when retrying bundles, starts with the oldest batch of bundles, only returns bundles up until to the point where the last bundle was stored when beginRetryCycle() was called
    std::vector<BundleInfo> getBundlesRetry() override;

    /// @brief deletes the oldest stored Bundle. This function iterates through all stored bundles and comares all received at times, very slow, maybe due to some programming error
    /// @return the Deleted Bundle
    BundleInfo deleteOldest() override;

    /// @brief internally stores what the number of bundles was when it was called
    void beginRetryCycle() override;

    /// @brief returns whether there are still bundles to be returned in this retry
    bool hasBundlesToRetry() override;
};