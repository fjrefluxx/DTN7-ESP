#pragma once
#include "CLA.hpp"
#include "Storage.hpp"
#include "dtn7-bundle.hpp"

class Router {
   private:
    Storage* storage;

   public:
    Router();

    /// @brief Pointers to all stored CLA instances
    std::vector<CLA*> clas;

    virtual ~Router();

    /// @brief prepares bundle for forwarding, takes care off processing (cf. RFC 9171, 5.4 Bundle Forwarding Step 4)
    /// @param bundle pointer to the bundle which shall be prepared
    /// @return pointer to a new bundle on the heap, which is ready to be sent
    Bundle* prepareForSend(Bundle* bundle);

    /// @brief a method to poll all non-push CLAs for received bundles.
    /// @return a list of pointers to bundles which need to be handled, must be empty vector if bundles have been sent to queue
    std::vector<ReceivedBundle*> getNewBundles();

    /// @brief Main bundle forwarding handling, cf. RFC9171Section 5.4 Step 2.
    ///         Decides to forward a bundle to potential receivers or to store the bundle.
    ///         Can use bundle deletion provided by BPA.
    /// @param bundle the bundle to forward
    /// @param reasonCode a reference to uint in which a reason reason code will be stored if the forwarding is not successful
    /// @return whether the bundle was forwarded
    virtual bool handleForwarding(BundleInfo* bundle, uint& reasonCode) = 0;

    /// @brief Send a bundle back to its previous node, if it is known. As this is not useful for broadcast, they always returns false.
    /// @param Bundle the bundle to send back
    /// @return whether the operation was successful
    virtual bool sendToPreviousNode(Bundle* bundle);
};