#pragma once
#include "CLA.hpp"
#include "Router.hpp"
#include "Storage.hpp"
#include "dtn7-bundle.hpp"

/**
 * @file EpidemicRouter.hpp
 * @brief This file contains the definitions for the EpidemicRouter.
 */

/// @brief  Epidemic router functionality.
///         The epidemic router checks whether any nodes are in range, to which a bundle has not been forwarded, before broadcasting.
///         This requires a peer discovery mechanism by the CLA, as provided by the LoRa CLA in BPoL-compatible mode.
///         By default, it is expected that all nodes which are currently in the list of known neighbors, as produced by the CLA's peer discovery mechanism, successfully receive a potential broadcast.
///
///         This assumption might be acceptable in some cases, but will not always be true. For this reason, an experimental feature is included in this routing strategy and the LoRa CLA which allows to check whether the transmissions were actually successful,
///         and to detect whether a bundle was forwarded to a neighboring node by any other node. This mechanism uses a set of hashes of bundle IDs which are transmitted in the advertisement messages ,see the LoRa CLA.
///         If this feature is enabled, the nodes which are assumed to have received a given bundle are reevaluated at the start of the transmission attempt of the bundle and potentially removed from the list of nodes which have received the bundle,
///         if the hash does not indicate a successful reception. The intervals in which bundles are evaluated for transmission, nodes advertise their presence and the time after which not recently seen nodes are removed from the known peers need to be adjusted to each other in order to have full benefit of this feature.
///         Furthermore, the overall benefit of this feature should be evaluated for the desired application.
class EpidemicRouter : public Router {
   private:
    /// @brief pointer to the storage instance used
    Storage* storage;

   public:
    /// @brief default constructor
    EpidemicRouter();

    /// @brief generates an epidemic router
    /// @param clas CLAs the router should use
    /// @param storage storage class the router uses
    EpidemicRouter(std::vector<CLA*> clas, Storage* storage);

    /// @brief handles the forwarding of bundles as described in section 5.4 step 2 of RFC9171
    /// @param bundle the bundle to forward
    /// @param reasonCode a reference to a uint in which to store the reason code if the forwarding was unsuccessful
    /// @return whether the forwarding was successful
    bool handleForwarding(BundleInfo* bundle, uint& reasonCode)
        override;  // only place router has to enact its strategy

    /// @brief checks whether a node is in a vector of forwarded to nodes and, if enabled in menuconfig: checks whether the Node has confirmed the reception of the given bundle ID in its last advertisement, if not, removes node from forwardedTO and returns false
    /// @param toCheck the node which should be checked
    /// @param forwardedTo the list of nodes which the bundle has been forwarded to
    /// @param bundleID the ID of the bundle
    /// @return true if the Node is in the forwarded to Vector and, if enabled, the reception of the BundleID was confirmed
    bool checkForwardedTo(Node toCheck, std::vector<Node>& forwardedTo,
                          std::string bundleID);
};