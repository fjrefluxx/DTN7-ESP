#include "Router.hpp"
#include <list>
#include <vector>
#include "CLA.hpp"
#include "dtn7-esp.hpp"
#include "statusReportCodes.hpp"

Router::Router() {}

Router::~Router() {}

/// @brief prepares bundle for forwarding, takes care off processing (cf. RFC 9171, 5.4 Bundle Forwarding Step 4)
/// @param bundle pointer to the bundle which shall be prepared
/// @return pointer to a new bundle on the heap, which is ready to be sent
Bundle* Router::prepareForSend(Bundle* bundle) {
    // first create a local copy of the original Bundle

    /* the original version of this code encoded the bundle to CBOR and then decoded it in order to create a copy
    uint8_t *cbor =nullptr;
    size_t cborSize;
    bundle->toCbor(&cbor ,cborSize);
    Bundle* result = Bundle::fromCbor(cbor ,cborSize);
    */

    // just use the copy constructor to copy the bundle
    Bundle* result = new Bundle(*bundle);

    // remove previous node block if present
    if (result->hasPreviousNode)
        result->removePreviousNode();

// if enabled, attach a previous node block containing information about this node
#if Config_AttachPreviousNodeBlock_TRUE
    PreviousNodeBlock prev = PreviousNodeBlock(
        DTN7::BPA->localEndpoint.localEID, CONFIG_canonicalCrcType);
    prev.setFlag(BLOCK_FLAG_DISCARD_CANT_BE_PROCESSED);
    result->insert(prev);
#else
#endif

    // if present, update the bundle age block
    if (result->hasBundleAge) {
        // get current time
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        // convert current time to ms
        uint64_t currentTime =
            ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);

        result->increaseAge(currentTime - (bundle->receivedAt));
    }

    // if present, update the hop count block
    if (result->hasHopCount) {
        result->increaseHopCount();
    }

    return result;
}

std::vector<ReceivedBundle*> Router::getNewBundles() {
    // create a vector for the result
    std::vector<ReceivedBundle*> result;

    // poll all CLAs
    for (CLA* cla : clas) {
        // poll the CLA
        std::vector<ReceivedBundle*> toAdd = cla->getNewBundles();

        // insert returned bundles to result vector
        result.insert(result.end(), toAdd.begin(), toAdd.end());
    }

    return result;
}