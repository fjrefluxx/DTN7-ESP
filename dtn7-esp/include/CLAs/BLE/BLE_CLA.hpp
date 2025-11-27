#pragma once
#include <cstdlib>
#include <string>
#include <unordered_set>
#include "CLA.hpp"
#include "Data.hpp"
#include "dtn7-esp.hpp"

/**
 * @file BLE_CLA.hpp
 * @brief This file contains the BleCLA as well as a helper Class to store BLE peers.
 *        BLEhandling.hpp contains the actual interaction with the nimble GAP/GATT functions.
 * @note For the BLE-CLA to work, the following settings must be made in menuconfig:
 *           CONFIG_BT_ENABLED = y
 *           CONFIG_BT_NIMBLE_ENABLED = y
 */

/*
For the BLE-CLA to work, the following settings must be made in menuconfig:
CONFIG_BT_ENABLED = y
CONFIG_BT_NIMBLE_ENABLED = y
*/

// the minimal gap between BLE bundle send operations
#define minGapBetweenSendMS CONFIG_BLE_SEND_GAP

// the maximum size for a bundle transmitted via BLE, currently only used to set the size of the buffer
#define maxBleBundleSize 1024

extern uint8_t cbor[];
extern size_t cborSize;
extern bool transmissionComplete;

void nimbleInit();

/// @brief struct representing a BLE peer, used in order to keep track of currently known peers
extern "C" struct BlePeer {
    /// @brief The type of the address
    uint8_t type;
    /// @brief The value of the address as an array of 6 bytes
    uint8_t addr[6];

    /// @brief Name of the Peer
    std::string name;

    /// @brief Timestamp of last discovery, in us
    uint32_t last_seen;

    /// @brief equality function, required for unordered set
    /// @param compareTo blePeer to compare this to
    /// @return whether the peers have the same name
    bool operator==(const BlePeer& compareTo) const {
        return compareTo.name == this->name;
    }
};

/// @brief simple hasher class for the BLE peer, required to use unordered map of BlePeers
struct BlePeerHasher {
    size_t operator()(const BlePeer& k) const {
        return std::hash<std::string>()(k.name);
    }
};

/// @brief requires the following menuconfig settings:
///     CONFIG_BT_ENABLED = y
///     CONFIG_BT_NIMBLE_ENABLED = y
class BleCLA : public CLA {
   private:
    /// @brief the name of the CLA, here "BLE CLA"
    std::string name = "BLE CLA";

    /// @brief signal that the BLE CLA addresses specific peers
    bool canAddress = true;

    /// @brief stores the time at which the last bundle was sent via the BLE CLA
    uint64_t lastSendTime = 0;

   public:
    /// @brief store all currently known BLE peers, TODO age out of BLE peers
    std::unordered_set<BlePeer, BlePeerHasher> currentPeers;

    /// @brief returns the name of the CLA ("BLE CLA")
    /// @return std::string with the name of the CLA
    std::string getName() override;

    /// @brief returns a whether a CLA  is able to send message to a specific, singular node. Returns False if the CLA broadcasts messages
    /// @return whether a CLA can address specific nodes(true) or only broadcast messages(false)
    bool checkCanAddress() override;

    /// @brief setup the BLE CLA
    BleCLA(std::string localURI);

    /// @brief Destructs the BLE CLA, still TODO
    ~BleCLA();

    /// @brief if the CLA is not setup to use the received queue, this function can be used to get the bundles received since the last time it was called, must be safe to call from a different thread than send()
    /// @return new received bundles since this function was last called
    std::vector<ReceivedBundle*> getNewBundles() override;

    /// @brief send a bundle to a target node via the CLA, must be safe to call from a different thread than getNewBundles() / if the queue system is used thread safety to the thread receiving bundles has to be kept in mind
    /// @param bundle the Bundle to send
    /// @param destination destination node, not used as the Serial CLA is not able to address specific nodes
    /// @return true if the bundle was successfully sent
    virtual bool send(Bundle* bundle, Node* destination = nullptr);

    /// @brief handles the discovery of a new BLE peer, adds it to the list of known peers of the BLE CLA and the BundleProtocol agent, or adjusts the last seen time if already known
    /// @param type type of address of the peer
    /// @param val address of the peer
    /// @param name name of the peer
    /// @param nameLength length of the name of the peer
    void discoveredPeer(const uint type, const uint8_t val[6], char* name,
                        uint8_t nameLength);

    /// @brief Task to periodically remove BLE peers which have not been seen, still TODO
    /// @param arg
    void cleanUpBlePeers();
};
