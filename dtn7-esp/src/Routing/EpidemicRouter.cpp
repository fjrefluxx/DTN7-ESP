#include "EpidemicRouter.hpp"
#include <list>
#include <vector>
#include "CLA.hpp"
#include "dtn7-esp.hpp"
#include "statusReportCodes.hpp"

EpidemicRouter::EpidemicRouter() {};

EpidemicRouter::EpidemicRouter(std::vector<CLA*> clas, Storage* storage) {
    this->clas = clas;
    this->storage = storage;
}

bool EpidemicRouter::handleForwarding(BundleInfo* bundle, uint& reasonCode) {
    // get known peers from storage
    std::vector<Node> peers = storage->getNodes();

    ESP_LOGI("EpidemicRouter",
             "handleForwarding, number of CLAs in Routers Cla list:%u, number "
             "of known Peers:%u",
             this->clas.size(), peers.size());

    // initialize reason code for return value
    uint reason = BundleStatusReportReasonCodes::
        NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE;

    // initialize vector of nodes in order to store to which nodes this bundle shall be forwarded to
    std::vector<Node> toForward;

    // check if we have peers which have not been forwarded this bundle
    for (Node n : peers) {
        if (!checkForwardedTo(n, bundle->forwardedTo, bundle->bundle.getID()))
            toForward.push_back(
                n);  // if this node has been not already forwarded this bundle, add it to the nodes which shall receive it

        // debug logging to check that all nodes are correctly checked
        ESP_LOGD("EpidemicRouter", "handleForwarding ,checked node:%s",
                 n.URI.c_str());
    }

    // only if there are known nodes which have not been forwarded this bundle a forwarding attempt is undertaken. Because of this, a neighbor discovery mechanism is required for this routing strategy
    if (toForward.size() != 0) {
        // initialize struct to later get the current time
        struct timeval tv_now;

        // initialize boolean in order to keep track whether any CLA has successfully broadcast the bundle
        bool successfulBroadcast = false;

        // prepare the bundle for transmission as late as possible
        Bundle* preparedBundle = prepareForSend(&bundle->bundle);

        // iterate through all CLAs
        for (CLA* cla : clas) {
            ESP_LOGI("Epidemic Router", "trying CLA: %s for Broadcast",
                     cla->getName().c_str());

            // use all broadcast CLAs to broadcast the bundle
            if (!cla->checkCanAddress()) {
                // task the CLA with sending the bundle, and if this is successful update the broadcast time and the status code of the bundle
                if (cla->send(preparedBundle)) {
                    // get the current time
                    gettimeofday(&tv_now, NULL);

                    // convert current time to ms
                    uint64_t currentTime = ((int64_t)tv_now.tv_sec * 1000L +
                                            (int64_t)tv_now.tv_usec / 1000);

                    // update the last broadcast time of the bundle, this updating has no direct use, but can be helpfull for debugging purposes
                    bundle->lastBroadcastTime = currentTime;

                    // update the status report reason code to reflect that the bundle was send via a broadcast CLA
                    reason = BundleStatusReportReasonCodes::
                        FORWARDED_OVER_UNIDIRECTIONAL_LINK;

                    // keep track that the bundle was successfully broadcast
                    successfulBroadcast = true;

                    // update the number of broadcasts of this bundle
                    bundle->numOfBroadcasts += 1;
                }
                else {
                    // update the reason code to indicate the failed broadcast
                    reason = BundleStatusReportReasonCodes::TRAFFIC_PARED;
                }
            }
            else {

                // let all non broadcast CLAs try to forward the bundle to nodes which have been determined to not have received it
                for (Node dest : toForward) {

                    // if the CLA could send the bundle to this node, add it to the forwarded to list
                    if (cla->send(preparedBundle, &dest))
                        bundle->forwardedTo.push_back(dest);
                }
            }

            // it is expected that all known nodes are in range and have received a broadcast, if one was carried out, therfore add them to the forwardedTo nodes
            if (successfulBroadcast)
                bundle->forwardedTo.insert(bundle->forwardedTo.end(),
                                           toForward.begin(), toForward.end());
        }

        // clean up heap
        delete preparedBundle;
    }
    else {
        // informational logging
        ESP_LOGI("Epidemic Router",
                 "no Peers which have not been Forwarded this bundle");
    }

    // write the reason code to the reference passed to this function
    reasonCode = reason;

    // informational logging
    ESP_LOGI("Epidemic Router", "Forwarded to %u nodes, with %u broadcasts",
             bundle->forwardedTo.size(), bundle->numOfBroadcasts);

    // declare the transmission a success if the desired the amount of receiving nodes has been received
    return bundle->forwardedTo.size() >= CONFIG_NumOfForwards;
}

bool EpidemicRouter::checkForwardedTo(Node toCheck,
                                      std::vector<Node>& forwardedTo,
                                      std::string bundleID) {
    // if the use of bundleID hashes is enabled, these are checked here
#if CONFIG_useReceivedSet

    size_t idHash = DTN7::hasher->hash(bundleID);

    // first check whether this bundles bundleID has was already marked as received by this node
    if (toCheck.receivedHashes.contains(idHash)) {
        toCheck.confirmedReception =
            true;  // bundle was apparently forwarded to this node, not by this node, but this is not relevant, therefore add to list of forwarded to nodes and set confirmed reception to true

        // update the forwarded to list of the bundle to include this node
        forwardedTo.push_back(toCheck);

        // remove the hash from the received hashes of this node, as this information is now conveyed by the confirmed reception field and the space used by the hash can be saved.
        toCheck.receivedHashes.erase(idHash);

        // re-add the modified node to storage
        DTN7::BPA->storage->addNode(toCheck);
        ESP_LOGI("checkForwardedTo", "already revived from checked node");
        // no need to check anything else, return true
        return true;
    }
#endif

    bool forwardedToThis = false;
    int index = 0;  // index only used if node must be removed from forwardedTo

    // iterate through all nodes in the forwardedTO vector
    for (Node& forwarded : forwardedTo) {
        // the bundle was forwarded to this node
        if (forwarded.URI == toCheck.URI) {
            forwardedToThis = true;
            // if enabled, we now have to check whether the entry in the forwarded to vector is justified
#if CONFIG_useReceivedSet

            // if it was already confirmed, do not check again
            if (forwarded.confirmedReception)
                return true;

            // if the node which is checked does not have this bundleID in its confirmed BundleIDs, it was not forwarded to this node, therefor remove from forwarded to and return false
            if (!(toCheck.receivedHashes.contains(idHash))) {
                forwardedTo.erase(forwardedTo.begin() + index);
                ESP_LOGI(
                    "checkForwardedTo",
                    "not forwarded to checked node, removing from forwardedTo");
                return false;
            }

            else {
                // the hash of this bundleId was contained, therfore mark it as confirmed, changes value in vector, as it is used as a reference
                forwarded.confirmedReception = true;

                // remove the hash from the received hashes of this node, as this information is now conveyed by the confirmed reception field and the space used by the hash can be saved.
                toCheck.receivedHashes.erase(idHash);

                // re-add the modified node to storage
                DTN7::BPA->storage->addNode(toCheck);
            }
#endif
            break;
        }
        index++;
    }
    return forwardedToThis;
}
