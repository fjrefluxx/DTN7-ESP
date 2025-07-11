#pragma once
#include "CLA.hpp"
#include "Router.hpp"
#include "Storage.hpp"
#include "dtn7-bundle.hpp"

/// @brief  This class represents the functionality of the simple broadcast router.
///         This router is similar to the SimpleEpidemicRouter from dtn7zero (https:// github.com/dtn7/dtn7zero/blob/main/dtn7zero/routers/simple_epidemic_router.py) with some adaptations.
///         A delay between broadcast attempts of each bundle and the ability to declare a transmission success after a set number of broadcasts has been added.
///         For use with non-broadcast CLAs, this router attempts to forward each bundle to each known node by consecutive unicasts.
///         The BundleInfo class offers the ability to keep track of all nodes the bundle has been forwarded to. This function is used by adding all nodes a bundle was successfully sent to this list and then not sending the same bundle to these nodes again.
class SimpleBroadcastRouter : public Router {
   private:
    /// @brief pointer to the instance of the storage class used
    Storage* storage;

   public:
    /// @brief stores the number of broadcast attempts to be made per bundle, is initialized to a default value set in menuconfig, can be overridden at runtime
    uint numOfBroadcastAttempts = CONFIG_NumOfBroadcasts;

    /// @brief stores the minimal number of times a bundle should be forwarded to other nodes to declare the forwarding a success, broadcast are not counted toward this value, is initialized to a default value set in menuconfig, can be overridden at runtime
    uint minNumberOfForwards = CONFIG_MinNodesToForward;

    /// @brief minimal number of time between to broadcast attempts in ms, is initialized to a default value set in menuconfig, can be overridden at runtime
    uint64_t msBetweenBroadcast = CONFIG_BroadcastGap;

    /// @brief default constructor
    SimpleBroadcastRouter();

    /// @brief generates a Simple epidemic router
    /// @param clas CLAs the router should use
    /// @param storage storage class the router uses
    SimpleBroadcastRouter(std::vector<CLA*> clas, Storage* storage);

    /// @brief handles the forwarding of bundles as described in section 5.4 step 2 of RFC9171
    /// @param bundle the bundle to forward
    /// @param reasonCode a reference to a uint in which to store the reason code if the forwarding was unsuccessful
    /// @return whether the forwarding was successful
    bool handleForwarding(BundleInfo* bundle, uint& reasonCode)
        override;  // only place router has to enact its strategy
};