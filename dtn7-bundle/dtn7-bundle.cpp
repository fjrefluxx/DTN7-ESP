//#pragma once
#include "dtn7-bundle.hpp"
#include <stdio.h>
#include <cstddef>
#include <vector>
#include "cbor.h"
#include "cborBlockDecode.hpp"
#include "esp_heap_caps.h"

/// @brief creates/updates the cbor representation of the bundle
void Bundle::toCbor(uint8_t** cbor, size_t& cborSize) {

    // the first element is the primary block
    uint8_t* primaryCbor;
    size_t primaryCborSize = 0;
    primaryBlock.toCbor(&primaryCbor, primaryCborSize);

    // followed by all optional canonical blocks

    std::vector<uint8_t*> cBlockCbor;
    std::vector<size_t> cBlockCborSizes;

    for (CanonicalBlock cBlock : (this->extensionBlocks)) {
        cborSize=0;
        uint8_t* cbor = nullptr;
        cBlock.toCbor(&cbor, cborSize);
        cBlockCbor.push_back(cbor);
        cBlockCborSizes.push_back(cborSize);
    }

    // last is the payload block
    uint8_t* payloadCbor = nullptr;
    size_t payloadCborSize = 0;
    payloadBlock.toCbor(&payloadCbor, payloadCborSize);

    // assemble everything into one CBOR array
    // first calculate the final CBOR size +2 for start and stop bit for CBOR array
    cborSize = primaryCborSize + payloadCborSize + 2;
    for (size_t cBlockSize : cBlockCborSizes)
        cborSize += cBlockSize;  // add the size of all encoded extension blocks
    ESP_LOGD("Bundle to cbor", "Size: %u", cborSize);

    //allocate array for cbor
    uint8_t* cborLoc = new uint8_t[cborSize];
    *cbor = cborLoc;

    // set the start byte
    cborLoc[0] = 0x9f;
    size_t currentIndex = 1;

    //copy the primary block to the result
    memcpy(cborLoc + currentIndex, primaryCbor, primaryCborSize);
    currentIndex += primaryCborSize;
    delete[] primaryCbor;

    //copy the extension block to the result
    for (int i = 0; i < cBlockCbor.size(); i++) {
        memcpy(cborLoc + currentIndex, cBlockCbor[i], cBlockCborSizes[i]);
        currentIndex += cBlockCborSizes[i];
        delete[] cBlockCbor[i];
    }

    //copy the payload block to the result
    memcpy(cborLoc + currentIndex, payloadCbor, payloadCborSize);
    currentIndex += payloadCborSize;
    delete[] payloadCbor;

    //set the final byte in the CBOR array
    cborLoc[currentIndex] = 0xff;

    return;
};

/// @brief Converts a cbor-encoded bundle to a bundle data structure
/// @param cbor pointer to the beginning of the cbor data
/// @param cbor_length length of the cbor Data
/// @return the created bundle
Bundle* Bundle::fromCbor(const uint8_t* cbor, size_t cbor_length) {
    CborParser parser;
    CborValue value;
    bool valid = true;
    cbor_parser_init(cbor, cbor_length, 0, &parser, &value);
    Bundle* result = new Bundle();

    if (cbor_value_is_array(&value)) {
        if (cbor_value_is_length_known(&value)) {
            // a bundle MUST be a cbor array of indefinite length
            ESP_LOGE("Bundle from cbor", "invalid bundle");
            valid = false;
            result->valid = false;
            return result;
        }
        else {
            CborValue ArrayValue;
            cbor_value_enter_container(&value, &ArrayValue);

            while ((!cbor_value_at_end(&ArrayValue)) && valid) {
                // each element in the outer array must be a cbor array (block)
                if (cbor_value_is_array(&ArrayValue)) {

                    // a block MUST be a cbor array of definite length
                    if (!cbor_value_is_length_known(&ArrayValue)) {
                        ESP_LOGE("Bundle from cbor",
                                 "invalid bundle, individual blocks must be of "
                                 "definite length");
                        valid = false;
                        result->valid = false;
                        return result;
                    }

                    // the block type (Primary Block/Canonical Block) can be deduced from the array length
                    size_t blockSize;
                    cbor_value_get_array_length(&ArrayValue, &blockSize);

                    // canonical blocks have a length of 5 or 6 with CRC
                    if (blockSize == 5 || blockSize == 6) {
                        CanonicalBlock cBlock =
                            fromCborCanonical(&ArrayValue, blockSize);
                        valid = cBlock.valid && valid;
                        if (!valid) {
                            result->valid = false;
                            return result;
                        }

                        // check if decoded block is the payload block.
                        if (cBlock.blockTypeCode == 1) {
                            // Yes, store as payload block
                            result->payloadBlock = PayloadBlock(cBlock);
                        }
                        else {
                            // No, store as generic canonical block
                            result->extensionBlocks.push_back(cBlock);
                            result->usedBlockNums.insert(cBlock.blockNumber);
                            ESP_LOGD("Bundle from cbor",
                                     "Read canonical block");
                            if (cBlock.blockTypeCode == 6)
                                result->hasPreviousNode = true;
                            if (cBlock.blockTypeCode == 7)
                                result->hasBundleAge = true;
                            if (cBlock.blockTypeCode == 10)
                                result->hasHopCount = true;
                        }
                    }
                    // primary blocks have a length between 8 and 11, depending on CRC/fragmentation
                    else if (blockSize >= 8 && blockSize <= 11) {
                        // decode the primary block
                        result->primaryBlock =
                            fromCborPrimary(&ArrayValue, blockSize);
                        ESP_LOGD("Bundle from cbor", "Read primary block");
                        valid = result->primaryBlock.valid && valid;
                        if (!valid) {
                            result->valid = false;
                            return result;
                        }
                    }
                    else {
                        ESP_LOGE("Bundle from cbor",
                                 "invalid bundle, individual blocks must have "
                                 "5, 6, or 8-11 elements");
                        valid = false;
                        result->valid = false;
                        return result;
                    }
                }
                else {
                    ESP_LOGE(
                        "Bundle from cbor",
                        "invalid bundle, individual blocks must be arrays");
                    valid = false;
                    result->valid = false;
                    return result;
                }
            }
        }
    }
    else {
        ESP_LOGE("Bundle from cbor", "invalid bundle, bundle must be array");
        valid = false;
        if (!valid) {
            result->valid = false;
            return result;
        }
    }
    result->valid = valid;
    if (result->valid)
        result->setBundleID();
    return result;  //return finished bundle data structure
}

void Bundle::print() {
    ESP_LOGE("Bundle Print", "Valid: %s, extensionBlocks:%u",
             valid ? "true" : "false", extensionBlocks.size());
    primaryBlock.print();
    for (CanonicalBlock cBlock : (this->extensionBlocks)) {
        ESP_LOGE("Bundle Print", "ExtensionBlock:");
        cBlock.print();
    }
    ESP_LOGE("Bundle Print", "PayloadBlock:");
    payloadBlock.print();
    return;
}

Bundle::Bundle(PrimaryBlock* primary, PayloadBlock* payload) {
    //ESP_LOGI("Bundle","Copy expected");
    primaryBlock = *primary;
    payloadBlock = *payload;
    usedBlockNums = std::set<uint64_t>();
    valid = true;

    bundleID = primaryBlock.sourceEID.getURI().append("-").append(
        primaryBlock.timestamp.toString());
    usedBlockNums.insert(
        1);  // Payload block always has blockNumber 1, 1 is always used
    this->extensionBlocks = std::vector<CanonicalBlock>();

    hasPreviousNode = false;
    hasBundleAge = false;
    hasHopCount = false;
    setReceivedTime();
};