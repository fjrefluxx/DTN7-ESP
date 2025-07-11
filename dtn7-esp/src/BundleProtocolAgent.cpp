#include "BundleProtocolAgent.hpp"
#include <algorithm>
#include "Data.hpp"
#include "dtn7-esp.hpp"
#include "utils.hpp"

BundleProtocolAgent::BundleProtocolAgent(std::string URI, Storage* storage,
                                         Router* router) {
    // initialize the class variables
    this->storage = storage;
    this->router = router;

    // create forward and received queues
    forwardQueue = xQueueCreate(CONFIG_ForwardQueueSize, sizeof(BundleInfo*));
    receiveQueue =
        xQueueCreate(CONFIG_ReceiveQueueSize, sizeof(ReceivedBundle*));

    // setup the local node endpoint and register it with the BPA
    localEndpoint = new Endpoint(URI);
    this->registerEndpoint(localEndpoint);
}

BundleProtocolAgent::~BundleProtocolAgent() {
    // delete queues
    vQueueDelete(forwardQueue);
    vQueueDelete(receiveQueue);

    // cleanup objects from heap
    delete localEndpoint;
    delete storage;
    delete router;
}

void BundleProtocolAgent::registerEndpoint(Endpoint* endpoint) {
    // set the endpoint's BPA pointer to point to this object
    endpoint->BPA = this;

    // if the endpoint does not have a valid EID, it cannot be registered
    if (endpoint->localEID.valid == false) {
        ESP_LOGE("BundleProtocolAgent registerEndpoint",
                 "given endpoint has no valid EID");
        return;
    }

    // do not register the same endpoint multiple times
    if (std::find(registeredEndpoints.begin(), registeredEndpoints.end(),
                  endpoint) != registeredEndpoints.end()) {
        ESP_LOGE("BundleProtocolAgent registerEndpoint",
                 "given endpoint already registered");
    }
    else {
        // add the endpoint to the registered endpoints
        registeredEndpoints.push_back(endpoint);
        ESP_LOGI("BundleProtocolAgent registerEndpoint",
                 "registered endpoint with EID: %s",
                 endpoint->localEID.getURI().c_str());
    }
    return;
}

bool BundleProtocolAgent::unregisterEndpoint(Endpoint* endpoint) {
    // find the endpoint in the list of registered endpoints
    // iterate through the entire vector of endpoints
    int index = 0;
    while (index < registeredEndpoints.size()) {
        // compare the URI of each element
        if (registeredEndpoints[index]->localEID.getURI() ==
            endpoint->localEID.getURI()) {
            // Multiple occurrences of endpoints with the same URI are prevented by the register endpoint function and, therefore, are not accounted for
            break;
        }
        index++;
    }

    // if the found index is no the size of the vector (i.e., is not registered with the BPA), remove it from the registered endpoints via its index and set its BPA pointer to null
    if (index != registeredEndpoints.size()) {
        registeredEndpoints.erase(registeredEndpoints.begin() + index);
        endpoint->BPA = nullptr;  // remove reference to BPA from endpoint
        return true;
    }

    // the endpoint was not registered
    return false;
}

bool BundleProtocolAgent::bundleTransmission(Bundle* bundle) {
    bundle->getID();

    // set the relevant retention constraint
    bundle->retentionConstraint = RETENTION_CONSTRAINT_DISPATCH_PENDING;

    // create a received bundle object indication that the bundle originated from this node
    ReceivedBundle* recBundle =
        new ReceivedBundle(bundle, DTN7::localNode->URI);

    // send the bundle to the received queue
    return (xQueueSend(receiveQueue, (void*)&recBundle, portMAX_DELAY));
}

bool BundleProtocolAgent::cancelTransmission(std::string bundleID) {
    // attempt to remove the bundle from storage. Cancel all future retransmission attemps, unless the bundle is currently in received/forward queue or in processing.
    return storage->removeBundle(bundleID);
}

bool BundleProtocolAgent::bundleReception(Bundle* bundle,
                                          std::string fromNode) {
    ESP_LOGI("bundleReception", "handling reception");
    // add relevant retention constraint
    bundle->retentionConstraint = RETENTION_CONSTRAINT_DISPATCH_PENDING;

    // TODO report reception if requested TODO

    // cycle through canonical blocks
    for (CanonicalBlock block : bundle->extensionBlocks) {
        // if canonical block is not of a supported type, check action space
        // list all explicitly supported block types
        if ((block.blockTypeCode != 6) && (block.blockTypeCode != 7) &&
            (block.blockTypeCode != 10)) {
            // the block flags indicate what must happen in case the block is unsupported
            BlockProcessingFlags flags = block.getFlags();

            // if the node is setup to send status reports, we check if this has been requested
#if CONFIG_SendStatusReport
            if (flags.reportCantBeProcessed) {
                // TODO send status Report
            }
#endif
            // if the flags indicate that the bundle must be deleted if this block is unsupported, delete
            if (flags.getFlag(BLOCK_FLAG_DELETE_CANT_BE_PROCESSED)) {
                bundleDeletion(
                    bundle, BundleStatusReportReasonCodes::BLOCK_UNSUPPORTED);
                return false;
            }

            // if the flags indicate that the block must be removed if this block is unsupported, remove
            else if (flags.getFlag(BLOCK_FLAG_DISCARD_CANT_BE_PROCESSED)) {
                ESP_LOGE("BundleProtocolAgent bundleReception",
                         "removing block with type: %llu, Number: %llu",
                         block.blockTypeCode, block.blockNumber);
                bundle->removeBlock(block.blockNumber);
            }
        }
    }

    // check whether the hop limit has been surpassed (if present)
    if (bundle->hasHopCount) {
        if (bundle->getHopCount() >= bundle->getHopLimit()) {
            bundleDeletion(bundle,
                           BundleStatusReportReasonCodes::HOP_LIMIT_EXCEEDED);
            return false;
        }
    }

    // check the bundles age against its lifetime
    uint64_t lifetime = bundle->primaryBlock.lifetime;

    // if configured in menuconfig, ignore lifetime of primary block and use overiding lifetime instead
#if CONFIG_IgnoreBundleTTL
    lifetime = OverrideBundleTTL;
#endif

    // if the bundle has a bundle age block use this to get the bundles age and delete the bundle if required
    if (bundle->hasBundleAge) {
        if (bundle->getAge() >= lifetime) {
            bundleDeletion(bundle,
                           BundleStatusReportReasonCodes::LIFETIME_EXPIRED);
            return false;
        }
    }

    // if the node is setup with an accurate, synchronized clock, and the bundle has a non zero creation time, the bundle age can be checked against the node clock
#if CONFIG_HasAccurateClock
    if (bundle->primaryBlock.timestamp.creationTime != 0) {
        ESP_LOGD(
            "Bundle Reception",
            "Accurate Clock configured, checking bundle age using own clock");
        // calculate the time by which the bundle must be deleted
        uint64_t expirationTime =
            bundle->primaryBlock.timestamp.creationTime + lifetime;

        // get the current time
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        // convert current time to ms
        uint64_t currentTime =
            ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);

        // delete bundle if required
        if (expirationTime < currentTime) {
            bundleDeletion(bundle,
                           BundleStatusReportReasonCodes::LIFETIME_EXPIRED);
            return false;
        }
    }
#endif

    // create bundle info object
    BundleInfo* bundleInf = new BundleInfo(bundle);
    delete bundle;
    if (fromNode !=
        "none")  // check if the sender node is known, i.e. the fromNode string is not none
    {
        // get node with corresponding URI from storage, it should be stored, as the reception task updates each nodes last seen variable if a bundle is received from the node
        Node from = storage->getNode(fromNode);
        if (from.URI != "none") {
            // if the use of a received set per bundle is enable in menuconfig, we add the sender of the bundle to this set
#if CONFIG_useReceivedSet
            // the node from which the bundle was received is implicitly already confirmed to have received it
            from.confirmedReception = true;
#endif
            // we add the node from which the bundle was received to the nodes the bundle was forwarded to, so that no attempt to send the bundle back tzo the original sender is made
            bundleInf->forwardedTo.push_back(from);
        }
    }

    // dispatch the bundle
    return bundleDispatching(bundleInf);
}

bool BundleProtocolAgent::bundleDispatching(BundleInfo* bundle) {
    // check whether bundle must be delivered to a local endpoint
    if (isLocalDest(bundle->bundle.getDest())) {
        // handle local delivery
        localBundleDelivery(bundle);
    }
    ESP_LOGI("bundleDispatching", "dispatched Bundle:");
    // send bundle to forward queue
    return (xQueueSend(forwardQueue, (void*)&bundle, portMAX_DELAY));
}

bool BundleProtocolAgent::localBundleDelivery(BundleInfo* bundle) {
    ESP_LOGI("local Bundle delivery", "delivering Bundle");
    bool locallyDelivered = false;

    // find the endpoint(s) to deliver the bundle to
    for (Endpoint* endp : registeredEndpoints) {
        // check that endpoint URI is equal to destination URI
        if (endp->localEID.getURI() == bundle->bundle.getDest().getURI()) {
            // deliver the bundle
            endp->localBundleDelivery(
                (bundle->bundle));  // pass bundle by value

            // check whether this node is already listed in the nodes the bundle was forwarded to
            bool alreadyContained = false;
            for (Node n : bundle->forwardedTo) {
                if (n.URI == DTN7::localNode->URI) {
                    alreadyContained = true;
                    break;
                }
            }
            // if this is not the case, add the node to this list
            if (!alreadyContained)
                bundle->forwardedTo.push_back(*DTN7::localNode);

            // keep track that the bundle was locally delivered
            locallyDelivered = true;
        }
    }
    // return whether the bundle was delivered locall
    return locallyDelivered;
}

void BundleProtocolAgent::bundleForwarding(BundleInfo* bundle) {
    // the main functionality of forwarding is handled by the router class.
    // the router returns information about the forwarding of each bundle via a reason code and a boolean which indicates overall success

    uint reasonCode = 0;

    /*
    RFC 9171 5.4 Step 1: "The retention constraint "Forward pending" be added to the bundle, and the bundle's "Dispatch pending" retention constraint be removed."
    */
    // the "Dispatch pending" retention constraint is implicitly removed, as the bundle is only able to have one retention constraint at a time
    bundle->setRetentionConstraint(RETENTION_CONSTRAINT_FORWARD_PENDING);

    /*
    RFC 9171, 5.4 Bundle Forwarding[…] Step 2: The BPA MUST determine whether or not
    forwarding is contraindicated (that is, rendered inadvisable)[…] * […] If the BPA elects
    to forward the bundle to some other node(s) for further forwarding but finds it impossible
    to select any node(s) to forward the bundle to, then forwarding is contraindicated.
    The BPA MAY choose to either forward the bundle directly to its destination node(s)
    (if possible) or forward the bundle to some other node(s) for further forwarding.

    The manner in which this decision is made may depend on the scheme name in the destination endpoint
    ID and/or on other state -> this is a routing choice, handled by router class
    */
    bool success = this->router->handleForwarding(bundle, reasonCode);

    // ->this function will return whether the bundle shall be stored for later reattempt of forwarding. This happens when the forwarding was not successful, but the reason codes do not indicate an overall failure
    // ->the CLA send functions are also called by router, no need to do anything with CLAs here

    // if the forwarding was successful, the router has decided that the bundle shall not be re-evaluated for transmission in the future, otherwise it might be required
    if (!success) {
        ESP_LOGI("bundleForwarding", "No forwarding success");

        // if the reason code indicates that the forwarding was no failure, the bundle will be reevaluated later on
        if (BundleStatusReportReasonCodes::checkNoFailure(reasonCode)) {
            ESP_LOGI("bundleForwarding",
                     "No forwarding failure, delaying bundle");

            // To enable this re-evaluation, we store the bundle using the storage class.
            // This might potentially remove other bundles from storage because of space constraints, they will be returned and processed
            std::vector<BundleInfo> removed = storage->delayBundle(bundle);

            delete bundle;  // clean up bundle from heap, now is stored in storage

            // if bundles have been removed, we handel bundle deletion for each of them
            if (removed.size() != 0) {
                for (BundleInfo b : removed) {
                    // handle bundle deletion with appropriate reason code
                    BundleProtocolAgent::bundleDeletion(
                        &b, BundleStatusReportReasonCodes::DEPLETED_STORAGE);
                }
            }
        }
        else {
            // forwarding failure, RFC9171 Section 5.4.2
            ESP_LOGI("bundleForwarding", "Forwarding failure");
            bool wasForLocalEndpoint = false;

            // RFC9171 makes a distinction whether the bundle was for a local endpoint or not.
            // This distinctions is also made here, however, it has no effect as no status reports are implemented
            for (Endpoint* end : registeredEndpoints) {
                if (end->localEID.getURI() ==
                    bundle->bundle.getDest().getURI()) {
                    bundle->setRetentionConstraint(RETENTION_CONSTRAINT_NONE);
                    wasForLocalEndpoint = true;
                    delete bundle;  // clean up bundle from heap, now is no longer needed
                }
            }
            if (!wasForLocalEndpoint) {
                // Delete the bundle with the reason given from the router. This would generate a status report if it were implemented and enabled
                bundleDeletion(&bundle->bundle, reasonCode);
            }
        }
    }
    else {
        ESP_LOGI("bundleForwarding", "Forwarding Success");

        // forwarding was successful, we no longer need the bundle
        bundle->setRetentionConstraint(RETENTION_CONSTRAINT_NONE);
        delete bundle;
    }

    // TODO handle status report, maybe this would be better placed in the routers handleForwarding function, as information about specific CLA transmission processes might be required
    return;
}

void BundleProtocolAgent::bundleDeletion(Bundle* bundle, uint reason) {
    ESP_LOGI("BundleProtocolAgent Bundle Deletion", "deleting bundle: ");
    delete bundle;  // clean up bundle from heap, now is no longer needed
    // TODO status report
    return;
}

void BundleProtocolAgent::bundleDeletion(BundleInfo* bundle, uint reason) {
    ESP_LOGI("BundleProtocolAgent Bundle Deletion", "deleting bundle: ");
    // TODO status report
    return;
}

bool BundleProtocolAgent::isLocalDest(EID destination) {
    // check whether the give EID belongs to a Endpoint which is registered with the BPA
    for (Endpoint* endp : registeredEndpoints)
        if (endp->localEID.getURI() == destination.getURI())
            return true;
    return false;
}

Endpoint* BundleProtocolAgent::getLocalEndpoint(std::string URI) {
    // find the Endpoint object in the registered endpoints
    for (Endpoint* endp : registeredEndpoints)
        // if found, return pointer to the Endpoint
        if (endp->localEID.getURI() == URI)
            return endp;
    // if not found, return null pointer
    return nullptr;
}
