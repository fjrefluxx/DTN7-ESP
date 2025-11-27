#pragma once
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include "Data.hpp"
#include "dtn7-bundle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

/**
 * @file Storage.hpp
 * @brief This file contains the definitions for the Storage base class.
 */

/// @brief base class for storage implementations, access to derived classes needs to be thread safe!
class Storage {
   protected:
    /// @brief stores how many bundle still need to be retried
    uint16_t bundlesToReturn = 0;

   public:
    /// @brief base class representing Some kind of storage for bundles, all methods have to be thread safe!
    Storage() {};
    virtual ~Storage() {};

    /// @brief adds a node to the known nodes, if it is already present, it is overwritten
    /// @param node node to store
    virtual void addNode(Node node) = 0;

    /// @brief removes a node from the list of known nodes
    /// @param address
    virtual void removeNode(std::string address) = 0;

    /// @brief gets a previously stored node object from a given address/identifier
    /// @param address
    /// @return the previously stored node if it exists, otherwise a empty node object
    virtual Node getNode(std::string address) = 0;

    /// @brief gets all known nodes
    /// @return a vector containing all known nodes
    virtual std::vector<Node> getNodes() = 0;

    /// @brief checks whether a bundleID was seen before
    /// @param bundleID std::string containing the BundleID to check
    /// @return whether the bundle Id was seen before
    virtual bool checkSeen(std::string bundleID) = 0;

    /// @brief adds a BundleId to the known BundleIDs, if it is not already known, if it is already stored it is overridden
    /// @param bundleID bundleId to mark as seen
    /// @param node identifier of the node from which the Bundle was received
    virtual void storeSeen(std::string bundleID) = 0;

    /// @brief removes a Bundle from storage
    /// @param bundleID BundleId of the bundle to remove
    /// @return true if the bundle was previously stored, otherwise false
    virtual bool removeBundle(std::string bundleID) = 0;

    /// @brief stores a given bundle for later retransmission
    /// @param bundle bundle to store
    /// @return if other bundles were removed from storage in order to fit the new one, a vector of the removed bundles is returned
    virtual std::vector<BundleInfo> delayBundle(BundleInfo* bundle) = 0;

    /// @brief returns a portion previously delayed bundles, exact size can be configured in menuconfig, as a vector, called repeatedly when retrying bundles, starts with the oldest batch of bundles, only returns bundles up until to the point where the last bundle was stored when beginRetryCycle() was called
    virtual std::vector<BundleInfo> getBundlesRetry() = 0;

    /// @brief deletes the oldest stored Bundle
    /// @return the Deleted Bundle
    virtual BundleInfo deleteOldest() = 0;

    /// @brief internally stores what the number of bundles was when it was called
    virtual void beginRetryCycle() = 0;

    /// @brief returns whether ther are still bundles to be returned in this retry
    /// @return
    virtual bool hasBundlesToRetry() = 0;
};

/// @brief dummy class for storage implementations, nothing is actually stored, but all functions can safely be called
class DummyStorage : public Storage {
   protected:
    /// @brief stores how many bundle still need to be retried
    uint16_t bundlesToReturn = 0;

   public:
    /// @brief base class representing a storage for bundles. All methods have to be thread safe!
    DummyStorage() {};
    ~DummyStorage() {};

    /// @brief adds a node to the known nodes
    /// @param node node to store
    void addNode(Node node) override { return; };

    /// @brief removes a node from the list of known nodes
    /// @param address
    void removeNode(std::string address) override { return; };

    /// @brief gets a previously stored node object from a given address/identifier
    /// @param address
    /// @return the previously stored node if it exists, otherwise a empty node object
    Node getNode(std::string address) override { return Node(); };

    /// @brief gets all known nodes
    /// @return a vector containing all known nodes
    std::vector<Node> getNodes() override { return std::vector<Node>(); };

    /// @brief checks whether a bundleID was seen before
    /// @param bundleID std::string containing the BundleID to check
    /// @return whether the bundle Id was seen before
    bool checkSeen(std::string bundleID) override { return false; };

    /// @brief adds a BundleID to the known BundleIDs, if it is not already known. If already stored, but the node from which it was received has identifier "none", overwrites identifier ("none").
    /// @param bundleID bundleId to mark as seen
    /// @param node identifier of the node from which the Bundle was received
    void storeSeen(std::string bundleID) override { return; };

    /// @brief removes a Bundle from storage
    /// @param bundleID BundleId of the bundle to remove
    /// @return true if the bundle was previously stored, otherwise false
    bool removeBundle(std::string bundleID) override { return false; };

    /// @brief Stores a given bundle for later retransmission.
    /// @param bundle bundle to store
    /// @return if other bundles were removed from storage in order to fit the new one, a vector of the removed bundles is returned
    std::vector<BundleInfo> delayBundle(BundleInfo* bundle) override {
        return std::vector<BundleInfo>();
    };

    /// @brief Returns n previously delayed bundles as a vector.
    ///         Exact size can be configured in menuconfig. Called repeatedly when retrying bundles. Starts with the oldest batch of bundles.
    ///         Only returns bundles up until to the point where the last bundle was stored when beginRetryCycle() was called.
    std::vector<BundleInfo> getBundlesRetry() override {
        return std::vector<BundleInfo>();
    };

    /// @brief deletes the oldest stored Bundle
    /// @return the Deleted Bundle
    BundleInfo deleteOldest() override { return BundleInfo(); };

    /// @brief internally stores what the number of bundles was when it was called
    void beginRetryCycle() override { return; };

    /// @brief returns whether ther are still bundles to be returned in this retry
    /// @return
    bool hasBundlesToRetry() override { return false; };
};
