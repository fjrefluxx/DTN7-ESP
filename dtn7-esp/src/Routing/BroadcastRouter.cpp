#include "BroadcastRouter.hpp"
#include <list>
#include <vector>
#include "CLA.hpp"
#include "Router.hpp"
#include "dtn7-esp.hpp"
#include "statusReportCodes.hpp"

SimpleBroadcastRouter::SimpleBroadcastRouter() {}

SimpleBroadcastRouter::SimpleBroadcastRouter(std::vector<CLA*> _clas,
                                             Storage* _storage) {
    this->clas = _clas;
    this->storage = _storage;
}

bool Router::sendToPreviousNode(Bundle* bundle) {
    return false;  // not implemented/used
}

bool SimpleBroadcastRouter::handleForwarding(BundleInfo* bundleInf,
                                             uint& reasonCode) {
    ESP_LOGI("SimpleBroadcastRouter",
             "handleForwarding, number of CLAs in Routers Cla list:%u",
             this->clas.size());

    // prepare bundle for transmission
    Bundle* preparedBundle = prepareForSend(&bundleInf->bundle);

    uint reason = BundleStatusReportReasonCodes::
        NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE;

    // check all nodes which are known and try to send the bundle directly to them
    for (Node node : storage->getNodes()) {
        // first check whether forwarding to a node has already happened
        bool alreadyForwarded = false;
        for (Node forwardedTo : bundleInf->forwardedTo) {
            if (forwardedTo.URI == node.URI)
                alreadyForwarded = true;
        }

        // if yes, do not forward again
        if (alreadyForwarded)
            continue;

        // otherwise try all CLAs which can address nodes
        for (CLA* cla : clas) {
            ESP_LOGI("SimpleBroadcastRouter",
                     "handleForwarding, Checking if %s can address Nodes",
                     cla->getName().c_str());
            if (cla->checkCanAddress()) {
                ESP_LOGI("SimpleBroadcastRouter",
                         "found Clas which can address");
                // attempt to send to the node
                bool success = cla->send(preparedBundle, &node);

                // if successful, add node to forwarded to and move to next node
                if (success) {
                    bundleInf->forwardedTo.push_back(node);
                    break;  // bundle is now forwarded to this node, no additional CLAs need to be tried
                }
                else {
                    reason = BundleStatusReportReasonCodes::
                        NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE;
                }
            }
        }
    }
    // get current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    // convert time to ms
    uint64_t currentTime =
        ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);

    // check whether the bundle was broadcasted to recently, if not, attempt to broadcast it
    if ((currentTime - (bundleInf->lastBroadcastTime)) >
            (this->msBetweenBroadcast) ||
        bundleInf->lastBroadcastTime == 0) {
        ESP_LOGI("SimpleBroadcastRouter", "Broadcast bundle");

        // task all broadcasting CLAs with sending this bundle
        for (CLA* cla : clas) {
            ESP_LOGI("SimpleBroadcastRouter", "trying CLA: %s for Broadcast",
                     cla->getName().c_str());
            if (!cla->checkCanAddress()) {
                bool success = cla->send(preparedBundle);
                // if the transmission was successful update last broadcast time
                if (success) {
                    reason = BundleStatusReportReasonCodes::
                        FORWARDED_OVER_UNIDIRECTIONAL_LINK;
                    bundleInf->numOfBroadcasts += 1;
                    bundleInf->lastBroadcastTime = currentTime;
                }
                else {
                    reason = BundleStatusReportReasonCodes::TRAFFIC_PARED;
                }
            }
        }
    }
    else {
        ESP_LOGI("SimpleBroadcastRouter",
                 "cannot Broadcast this bundle at the moment, last Broadcast "
                 "to recent");
    }

    delete preparedBundle;
    reasonCode = reason;

    ESP_LOGI("SimpleBroadcastRouter", "number of Broadcast for this bundle:%u",
             bundleInf->numOfBroadcasts);
    // as in dtn7zero: whether the number of broadcast was reached
    return (bundleInf->forwardedTo.size() >= this->minNumberOfForwards) ||
           (bundleInf->numOfBroadcasts >= this->numOfBroadcastAttempts);
}
