#pragma once
#include "Data.hpp"
#include "Endpoint.hpp"
#include "Router.hpp"
#include "Storage.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "statusReportCodes.hpp"

/**
 * @file BundleProtocolAgent.hpp
 * @brief This file contains the definitions for the BundleProtocolAgent.
 */

/// @brief class representing a DTN endpoint
class Endpoint;

/// @brief The main BundleProtocolAgent class, may only be initialized via the setup() method from the DTN7 namespace
class BundleProtocolAgent {

   public:
    /// @brief queue handle of the forward queue
    QueueHandle_t forwardQueue;

    /// @brief queue handle of the receive queue
    QueueHandle_t receiveQueue;

    /// @brief pointer to the storage object used, pointer to Storage base class, for polymorphism.
    /// This is instantiated via the constructor and is not indented to be changed afterwards
    Storage* storage = nullptr;

    /// @brief pointer to the Router object used, pointer to Router base class, for polymorphism
    /// This is instantiated via the constructor and is not indented to be changed afterwards
    Router* router = nullptr;

    /// @brief pointer to the local Node Central Endpoint
    /// This is instantiated in the setup() method from the DTN7 namespace
    Endpoint* localEndpoint = nullptr;

    /// @brief stores list of locally registered endpoints
    std::vector<Endpoint*> registeredEndpoints;

    /// @brief BundleProtocolAgent default Constructor
    BundleProtocolAgent() {};

    /// @brief Constructs the BundleProtocolAgent
    /// @param URI URI to use for the node local endpoint
    /// @param storage storage the BundleProtocolAgent should use
    /// @param router router the BundleProtocolAgent should use
    BundleProtocolAgent(std::string URI, Storage* storage, Router* router);

    /// @brief destructs the BundleProtocolAgent
    ~BundleProtocolAgent();

    /// @brief registers the given endpoint with the BundleProtocolAgent
    /// @param endpoint Endpoint object to be registered
    void registerEndpoint(Endpoint* endpoint);

    /// @brief removes an endpoint from the locally registered endpoints
    /// @param endpoint the Endpoint to unregister
    /// @return whether the Endpoint was registered
    bool unregisterEndpoint(Endpoint* endpoint);

    /// @brief Handles transmission of a new, locally generated Bundle
    /// @param bundle the bundle which is to be transmitted, allocated on the heap, as this pointer will later on be deleted!! just create a bundle* with new Bundle(...) and pass that pointer
    /// @return whether the transmission of the bundle to the received queue was successful.
    bool bundleTransmission(Bundle* bundle);

    /// @brief if the bundle with the given ID is stored for later transmission, it will be remove from storage. This effectively cancels any future retransmission attempts.
    /// The bundle is not removed from the processing queues, therefore if it currently resides in either the received or forwarding queue, or if it is currently beeing processed, the cancellation will fail.
    /// @param bundleID the id of the bundle which should be removed
    /// @return true, if the bundle was in storage and has been removed, false if it was not stored, which could either mean it has already successfully concluded transmission, was never seen, or was currently being processed as this function was called and may be in storage later on
    bool cancelTransmission(std::string bundleID);

    /// @brief Handles the reception procedure described in RFC 9171 Section 5.6
    /// @param bundle the received bundle
    /// @param fromNode the sending node, if it is known
    /// @return whether the reception was successful, could be false if i.e. the hop count defined in an eventual hop count block is exceeded, or if the bundle is deleted because of some processing control flags
    bool bundleReception(Bundle* bundle, std::string fromNode = "none");

    /// @brief handles bundle Dispatching, as described in RFC9171 Section 5.3
    /// @param bundle bundle to dispatch
    /// @return success True if bundle was successfully send to forward Queue
    bool bundleDispatching(BundleInfo* bundle);

    /// @brief Performs the Local Bundle delivery Procedure as described in Section 5.7 of RFC 9171
    /// @param bundle the Bundle object to deliver locally
    /// @return true if the Destination endpoint was registered with the BPA
    bool localBundleDelivery(BundleInfo* bundle);

    /// @brief handles Bundle forwarding, as described in RFC9171 Section 5.4.
    /// The handleForwarding() of the Router class is used for the actual forwarding Process
    /// checkNoFailure from BundleStatusReportReasonCodes is used whether to declare a forwarding failure
    /// \todo { Implementation of Status reports would partially go here, is currently not implemented }
    /// @param bundle BundleInfo object which is to be forwarded
    void bundleForwarding(BundleInfo* bundle);

    /// @brief handles Bundle Deletion as described in RFC9171 5.10. Currently the passed Bundle object is just deleted and the function returns.
    /// \todo { Implementation of Status reports would partially go here, is currently not implemented }
    /// @param bundle bundle to delete
    /// @param reason reason Code for deletion
    void bundleDeletion(
        Bundle* bundle,
        uint reason = BundleStatusReportReasonCodes::NO_ADDITIONAL_INFORMATION);

    /// @brief handles Bundle Deletion as described in RFC9171 5.10. Currently the passed BundleInfo object ist just deleted and the function returns.
    /// \todo { Implementation of Status reports would partially go here, is currently not implemented }
    /// @param bundle bundle to delete
    /// @param reason reason Code for deletion
    void bundleDeletion(
        BundleInfo* bundle,
        uint reason = BundleStatusReportReasonCodes::NO_ADDITIONAL_INFORMATION);

    /// @brief checks whether a EID belongs to a locally registered endpoint
    /// @param destination the EID to Check
    /// @return whether the EID is locally registered
    bool isLocalDest(EID destination);

    /// @brief if the Endpoint with the given URI is registered with the BPA, this function returns a pointer to the Endpoint object. This function returns an nullpointer if the endpoint was not registered.
    /// @param URI of the requested endpoint
    /// @return pointer to the Endpoint object, NULL pointer if it is not registered with the BPA
    Endpoint* getLocalEndpoint(std::string URI);
};
