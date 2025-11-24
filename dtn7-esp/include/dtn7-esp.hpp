#pragma once
#include <string>
#include <vector>
#include "BLE_CLA.hpp"
#include "BundleProtocolAgent.hpp"
#include "CLA.hpp"
#include "Endpoint.hpp"
#include "LoRaCLA.hpp"
#include "SerialCLA.hpp"
#include "dtn7-bundle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "helpers.h"

/**
 * \mainpage dtn7-esp
 * Most relevant documentation can be found in the DTN7 namespace. \n
 * The BundleProtocolAgent as well as the Endpoint documentation may also be of interest.
 * For details on CBOR encoding and the representation of the bundle view the Bundle class and Block.hpp.
 * For details on usage and features read the readme of the Project.
 * @file dtn7-esp.hpp
 * @brief Defines the DTN7 namespace containing most vital functionality.
 */

// for some reason i need to define that here
class BleCLA;

/// @brief This is the main dtn7 namespace. It contains all functionality regarding setup and basic operation of the DTN implementation.
namespace DTN7 {
/// @brief stores the task handle of the Bundle Receiver Task
extern TaskHandle_t bundleReceiverHandle;

/// @brief stores the task handle of the storage Retry Task
extern TaskHandle_t storageRetryHandle;

/// @brief stores the task handle of the CLA Poll Task
extern TaskHandle_t claPollHandle;

/// @brief stores the task handle of the Bundle Dispatch Task
extern TaskHandle_t bundleForwardHandel;

/// @brief stores the Maximum Age Peers are allowed to have and not be removed from the list of known Peers
extern int32_t maxPeerAge;

/// @brief stores the HashWrapper Class used if BPoL is configured to use BundleID hashes in advertisements
extern HashWrapper* hasher;

/// @brief stores the global pointer to the BundleProtocolAgent Object, there may always be at most one
extern BundleProtocolAgent* BPA;

/// @brief pointer to the local node object, used for i.e. storing that a Bundle was locally delivered, as needed to comply with Section 5.3 Step 1 of RFC9171:
///         "even though the node is a member of the bundle's destination endpoint, the node shall not undertake to forward the bundle to itself"
extern Node* localNode;

#if CONFIG_USE_LORA_CLA

/// @brief pointer to the LoRa CLA, used to ensure that there is always at most one and for easy access from receive task
extern LoraCLA* loraCLA;

#endif

#if CONFIG_USE_BLE_CLA
/// @brief pointer to the BLE CLA, used to ensure that there is always at most one and for easy access from bluethooth stack
extern BleCLA* bleCla;
#endif

#if CONFIG_HasAccurateClock

/// @brief indicates whether the nodes clock has successfully been synchronized to DTN time.
///         Only present if the "Accurate Clock" option is enabled in menuconfig.
///         Any clock synchronization mechansm which is implemented must set this field to true once the clock is synchronized.
extern bool clockSynced;

#endif

/// @brief task which takes new Bundles, either received via the router and one of its CLA's or locally generated, from the received queue and processes it accordingly, handles deletion of duplicate bundles and updates list of received Bundles if enabled menuconfig
void bundleReceiver(void* param);

/// @brief task which handles bundle forwarding, reads bundle from forward queue and calls handleForwarding() method of router Class
void bundleForwarder(void* param);

/// @brief tasks which periodically tries to dispatch previously stored bundles
void retryBundles(void* param);

/// @brief task which periodically polls CLA's not using the queue system
void pollClas(void* param);

/// @brief Initialise BundleProtocolAgent with the given URI as the local nodes URI, returns the central Endpoint of this node
/// @param URI URI to be set as the local nodes URI
/// @param onReceive Function Pointer to the function to be called when data for the local Endpoint is received, first argument of callback is bundle Payload as a byte vector, second is the destination endpoint URI, third is the Source Endpoint URI, forth is the Primary Block of the corresponding bundle, WARNING: Callback is called from a different task! I must be thread safe!
/// @return The Endpoint object of the local node
Endpoint* setup(std::string URI,
                void (*onReceive)(std::vector<uint8_t>, std::string,
                                  std::string, PrimaryBlock));

/// @brief Initialise BundleProtocolAgent with the given URI as the local nodes URI, returns the central Endpoint of this node, this function does not set up a callback
/// @param URI URI to be set as the local nodes URI
/// @return The Endpoint object of the local node
Endpoint* setup(std::string URI);

/// @brief Adds an endpoint to the BundleProtocolAgent with the given URI, this function allocates memory, remember to delete the created endpoint, after unregistering it!!
/// @param URI URI to be set as the endpoint URI
/// @param onReceive Function Pointer to the function to be called when data for the  Endpoint is received, WARNING: Callback is called from a different task! It must be thread safe!
/// @return The Endpoint object that was created
Endpoint* registerEndpoint(std::string URI,
                           void (*onReceive)(std::vector<uint8_t>, std::string,
                                             std::string, PrimaryBlock) = NULL);

/// @brief unregisters an endpoint from the BUndleProtocolAgent
/// @param URI URI of the Endpoint to unregister
/// @return pointer to the unregistered endpoint, should be deleted if not intended to be registered again, if endpoint was not registered, it returns a null pointer
Endpoint* unregisterEndpoint(std::string URI);

/// @brief unregisters an endpoint from the BundleProtocolAgent
/// @param endpoint pointer to the Endpoint object which shall be unregistered
/// @return pointer to the unregistered endpoint, should be deleted if not intended to be registered again, if endpoint was not registered, it returns a null pointer
Endpoint* unregisterEndpoint(Endpoint* endpoint);

/// @brief checks for all known Peers if they have been seen recently and removes all which have not been seen recently, maximum age is set in menuconfig, but can be changed at runtime
void clearOldPeers();

/// @brief checks whether a given bundle has exceeded its lifetime
/// @param bundle bundle to check
/// @return whether the bundle can be kept
bool checkExpiration(BundleInfo* bundle);

/// @brief stops all tasks of the BPA, clears all objects from memory
void deinitializeBPA();

/// @brief creates a URI for the current node from its MAC address, useful for easily giving nodes unique node ID's
/// @return string containing URI, format: "dtn://xx.xx.xx.xx/"
std::string uriFromMac();

/// @brief adds the Node to the list of known Peers, with UINT64MAX as last seen value
/// @param node node to add
void addStaticPeer(Node node);
}  // namespace DTN7
