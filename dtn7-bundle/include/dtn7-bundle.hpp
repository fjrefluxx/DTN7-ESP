#pragma once
#include <sys/time.h>
#include <set>
#include <vector>
#include "Block.hpp"
#include "time.h"

#define RETENTION_CONSTRAINT_DISPATCH_PENDING 2
#define RETENTION_CONSTRAINT_FORWARD_PENDING 1
#define RETENTION_CONSTRAINT_NONE 0

/**
 * @file dtn7-bundle.hpp
 * @brief This file contains the definition of the DTN7 bundle in accordance to RFC9171.
*/

/// @brief Representation of a DTN7 bundle in accordance to RFC9171
class Bundle {
   private:
    /// @brief the ID of the bundle, updated by calling setBundleID
    std::string bundleID;

    /// @brief stores the already used block numbers, used for automatic block numbering
    std::set<uint64_t> usedBlockNums;

   public:
    /// @brief primary block of the bundle
    PrimaryBlock primaryBlock;

    /// @brief primary block of the bundle
    PayloadBlock payloadBlock;

    /// @brief stores the canonical blocks of the bundle
    std::vector<CanonicalBlock> extensionBlocks;

    /// @brief indicates whether the bundle is valid
    bool valid;

    /// @brief stores the system time in milliseconds at which the bundle was received/created
    uint64_t receivedAt = 0;

    /// @brief indicates whether the bundle contains a PreviousNodeBlock
    bool hasPreviousNode = false;

    /// @brief indicates whether the bundle contains a BundleAgeeBlock
    bool hasBundleAge = false;

    /// @brief indicates whether the bundle contains a HopCountBlock
    bool hasHopCount = false;

    /// @brief used to store the bundle retention Constraint
    uint8_t retentionConstraint = 0;

    /// @brief generate a new bundle on the heap from the given CBOR encoded byte array.
    /// If Invalid cbor is given to this function, the created bundle valid flag is set to false.
    ///
    /// @param cbor pointer to the beginning of the CBOR byte array
    /// @param cbor_length length of the CBOR byte array
    /// @return pointer to a new bundle an the heap
    static Bundle* fromCbor(const uint8_t* cbor, size_t cbor_length);

    /// @brief Prints the bundle to the log
    void print();

    /// @brief generates a empty bundle
    Bundle() {
        // call the default constructors for all elemets of the bundle
        primaryBlock = PrimaryBlock();
        payloadBlock = PayloadBlock();
        extensionBlocks = std::vector<CanonicalBlock>();
        usedBlockNums = std::set<uint64_t>();

        // important: set valid flag to false
        valid = false;

        // initialize bundleID as null
        bundleID = "null";

        // mark all extension blocks as not present
        hasPreviousNode = false;
        hasBundleAge = false;
        hasHopCount = false;
        usedBlockNums.insert(
            1);  // primary block always has blockNumber 1, 1 is always used

        // set the received time of the new bundle to the current time
        setReceivedTime();
    }

    /// @brief Generates a bundle with the given primary block and payload, this is the minimum valid Bundle
    /// @param primary primary block for the new bundle
    /// @param payload primary block for the new bundle
    Bundle(PrimaryBlock* primary, PayloadBlock* payload);

    /// @brief deletes the bundle
    ~Bundle() {}

    /// @brief Generates a bundle with primary, payload, and extension blocks. It is expected that all elements passed to this function are valid
    /// @param primary primary block for the new bundle
    /// @param payload primary block for the new bundle
    /// @param extensionBlocks canonical blocks for the new bundle, need to have valid block numbering, i.e., every block needs a unique number
    Bundle(PrimaryBlock* primary, PayloadBlock* payload,
           std::vector<CanonicalBlock> extensionBlocks) {
        primaryBlock = *primary;
        payloadBlock = *payload;
        usedBlockNums = std::set<uint64_t>();

        // if all Blocks are valid and numbered correctly, which is expected here, the bundle is valid
        valid = true;

        // set the bundleID of the bundle
        bundleID = primaryBlock.sourceEID.getURI().append("-").append(
            primaryBlock.timestamp.toString());

        usedBlockNums.insert(
            1);  // primary block always has blockNumber 1, 1 is always used

        // TODO
        for (CanonicalBlock block : extensionBlocks) {
            // check if block number of each canonical block exists once in given vector
            if (usedBlockNums.find(block.blockNumber) != usedBlockNums.end()) {
                ESP_LOGE("Bundle Constructor",
                         "given canonical block vector has invalid block "
                         "numbering, pleas use insert block function for "
                         "automatically corrected block numbering");
                valid = false;
            }
            else {
                usedBlockNums.insert(
                    block.blockNumber);  // update list of used block numbers
                if (block.blockTypeCode == 6)
                    hasPreviousNode = true;
                if (block.blockTypeCode == 7)
                    hasBundleAge = true;
                if (block.blockTypeCode == 10)
                    hasHopCount = true;
            }
        }
        this->extensionBlocks = extensionBlocks;
        setReceivedTime();
    };

    /// @brief Bundle copy constructor
    /// @param old
    Bundle(const Bundle& old) {
        primaryBlock = old.primaryBlock;
        payloadBlock = old.payloadBlock;
        usedBlockNums = old.usedBlockNums;
        this->extensionBlocks = old.extensionBlocks;
        valid = old.valid;
        bundleID = old.bundleID;
        hasBundleAge = old.hasBundleAge;
        hasPreviousNode = old.hasPreviousNode;
        hasHopCount = old.hasHopCount;
        receivedAt = old.receivedAt;
    }

    /// @brief operator= for the bundle
    /// @param old
    /// @return
    Bundle& operator=(const Bundle& old) {
        if (this == &old)
            return *this;

        primaryBlock = old.primaryBlock;
        payloadBlock = old.payloadBlock;
        usedBlockNums = old.usedBlockNums;
        valid = old.valid;
        bundleID = old.bundleID;
        hasBundleAge = old.hasBundleAge;
        hasPreviousNode = old.hasPreviousNode;
        receivedAt = old.receivedAt;
        hasHopCount = old.hasHopCount;
        this->extensionBlocks = old.extensionBlocks;
        return *this;
    }

    /// @brief returns a std::string which represents the bundle ID
    /// @return bundleId in format {SourceURI-CreationTime-sequenceNumber}
    std::string getID() {
        if (this->valid)
            this->bundleID = primaryBlock.sourceEID.getURI().append("-").append(
                primaryBlock.timestamp.toString());
        else
            this->bundleID = "null";

        if (this->primaryBlock.getFlags().getFlag(BUNDLE_FLAG_IS_FRAGMENT))
            this->bundleID.append("-").append(
                std::to_string(this->primaryBlock.fragOffset));

        return this->bundleID;
    }

    /// @brief sets the bundles receivedAt time to now
    void setReceivedTime() {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        this->receivedAt =
            ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);
    }

    /// @brief generates the cbor representation of the bundle
    /// @param cbor Pointer to an uninitialized byte array (for example defined as: uint8_t* cbor = nullptr; then function call with: toCbor(&cbor, cborSize); ).
    /// a new byte array will be created on the heap; the calling method should delete it when no longer needed!
    /// @param cborSize size_t reference in which the size of the allocated array is stored
    void toCbor(uint8_t** cbor, size_t& cborSize);

    /// @brief adds the given canonical block to the bundle, if it is a primary block and the bundles primary block is empty it is set as the bundles primary block.
    /// If it is a different block type, it is added to the canonicalBlocks. Its Block number is checked that it only occurs once in the bundle and, if necessary, adjusted
    /// @param block the tlock to be inserted in the bundle
    /// @return the tlock number of the inserted block
    uint64_t insertCanonicalBlock(CanonicalBlock block) {
        uint64_t newNum = block.blockNumber;
        PayloadBlock oldPayload = payloadBlock;
        // check if primary block
        if (block.blockTypeCode == 1) {
            if (this->payloadBlock.dataSize == 0) {
                // if the current payload is empty, add given block as payload
                this->payloadBlock = PayloadBlock(block);
                ESP_LOGE("Bundle insert cBlock", "Payload set!");
            }
            else {
                ESP_LOGE("Bundle insert cBlock",
                         "Bundle already has payload, if it should be "
                         "overriden use setPayload()");
            }
        }
        else {
            if (newNum == 0 ||
                usedBlockNums.find(newNum) != usedBlockNums.end()) {
                // if the given block has number 0 or a number which is already in use in the bundle, it needs to be given a new number
                newNum =
                    2;  // lowest block number for a canonical block can be 2, because primary block is always 1
                while (usedBlockNums.find(newNum) != usedBlockNums.end()) {
                    newNum += 1;
                }
            }
            block.blockNumber = newNum;
            extensionBlocks.push_back(block);
            usedBlockNums.insert(newNum);
            if (block.blockTypeCode == 6)
                hasPreviousNode = true;
            if (block.blockTypeCode == 7)
                hasBundleAge = true;
            if (block.blockTypeCode == 10)
                hasHopCount = true;
        }
        return newNum;
    }

    /// @brief removes the canonicalBlock with the given number from the bundle. THIS FUNCTION IS NOT EFFICIENT, ONLY USE IF REALLY NECESSARY
    /// @param blockNumber the number of the block to remove
    /// @return the removed block, or empty block if bundle did not contain the requested block number
    CanonicalBlock removeBlock(uint64_t blockNumber) {
        CanonicalBlock result;
        if (usedBlockNums.find(blockNumber) != usedBlockNums.end()) {
            int index = 0;
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockNumber == blockNumber) {
                    result = block;
                    if (block.blockTypeCode == 6)
                        hasPreviousNode = false;
                    if (block.blockTypeCode == 7)
                        hasBundleAge = false;
                    if (block.blockTypeCode == 10)
                        hasHopCount = false;
                    extensionBlocks.erase(extensionBlocks.begin() + index);
                    break;
                }
                index += 1;
            }
        }
        else
            ESP_LOGE("Bundle remove Block",
                     "Bundle does not contain block with this number");
        return result;
    }

    /// @brief removes the PreviousNodeBlock from the bundle, if present, THIS FUNCTION IS NOT EFFICIENT ONLY USE IF REALLY NECESSARY
    /// @return the removed block, or empty block if bundle did not contain the requested Block number
    CanonicalBlock removePreviousNode() {
        CanonicalBlock result;
        if (hasPreviousNode) {
            int index = 0;
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 6) {
                    result = block;
                    extensionBlocks.erase(extensionBlocks.begin() + index);
                    hasPreviousNode = false;
                    break;
                }
                index += 1;
            }
        }
        else
            ESP_LOGE("Bundle remove Block",
                     "Bundle does not contain PreviousNodeBlock");

        hasPreviousNode = false;
        return result;
    }

    /// @brief if the bundle has a bundle age block, its stored age is increased by the given value
    /// @param difference the amount the bundle age is increased
    void increaseAge(uint64_t difference) {
        int index = 0;
        if (hasBundleAge) {
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 7) {
                    uint64_t newAge = block.getAge() + difference;
                    extensionBlocks.at(index) = BundleAgeBlock(
                        newAge, block.crcType, block.blockNumber);
                }
                index += 1;
            }
        }
        else {
            ESP_LOGE("Bundle increase Age",
                     "Bundle does not contain BundleAgeBlock");
        }
    }

    /// @brief if the bundle has ha hop count block, its stored hopcount is increased by 1
    void increaseHopCount() {
        int index = 0;
        if (hasHopCount) {
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 10) {
                    uint64_t newHopCount = block.getHopCount() + 1;
                    extensionBlocks.at(index) =
                        HopCountBlock(block.getHopLimit(), newHopCount,
                                      block.crcType, block.blockNumber);
                }
                index += 1;
            }
        }
        else {
            ESP_LOGE("Bundle increase HopCount",
                     "Bundle does not contain HopCountBlock");
        }
    }

    /// @brief returns the source endpoint ID of the bundle
    /// @return source endpoint ID of the bundle
    EID getSource() { return this->primaryBlock.sourceEID; }

    /// @brief returns the Destination Endpoint ID of the bundle
    /// @return Destination Endpoint ID of the bundle
    EID getDest() { return this->primaryBlock.destEID; }

    /// @brief returns the report to endpoint field ID of the bundle
    /// @return report to endpoint field ID of the bundle
    EID getReportTo() { return this->primaryBlock.reportToEID; }

    /// @brief sets the source endpoint of the bundle
    /// @param source new source endpoint of the bundle
    void setSource(EID source) {
        this->primaryBlock.sourceEID = source;
        return;
    }
    /// @brief sets the Destination Endpoint of the bundle
    /// @param Dest new Destination Endpoint of the bundle
    void setDest(EID Dest) {
        this->primaryBlock.destEID = Dest;
        return;
    }

    /// @brief sets the report to endpoint field of the bundle
    /// @param reportTo new report to endpoint field of the bundle
    void setReportTo(EID reportTo) {
        this->primaryBlock.reportToEID = reportTo;
        return;
    }

    /// @brief updates the bundles BundleID
    void setBundleID() {
        bundleID = primaryBlock.sourceEID.getURI().append("-").append(
            primaryBlock.timestamp.toString());
        if (this->primaryBlock.getFlags().getFlag(BUNDLE_FLAG_IS_FRAGMENT))
            this->bundleID.append("-").append(
                std::to_string(this->primaryBlock.fragOffset));
    }

    /// @brief gets the hop count stored in the hop count block, if one is present
    /// @return the hop count stored in the hop count block, returns 0 if none is present
    uint64_t getHopCount() {
        int index = 0;
        if (hasHopCount) {
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 10) {
                    return block.getHopCount();
                }
                index += 1;
            }
        }
        ESP_LOGE("Bundle get HopCount",
                 "Bundle does not contain HopCountBlock");
        return 0;
    }

    /// @brief gets the hop limit stored in the HopCountBlock, if one is present
    /// @return the hop limit stored in the HopCountBlock, returns 0 if none is present
    uint64_t getHopLimit() {
        int index = 0;
        if (hasHopCount) {
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 10) {
                    return block.getHopLimit();
                }
                index += 1;
            }
        }
        ESP_LOGE("Bundle get hop limit",
                 "Bundle does not contain HopCountBlock");
        return 0;
    }

    /// @brief gets the bundle age stored in the bundleAgeBlock, if one is present
    /// @return the bundleAge stored in the bundleAgeBlock, returns 0 if none is present
    uint64_t getAge() {
        int index = 0;
        if (hasBundleAge) {
            for (CanonicalBlock block : extensionBlocks) {
                if (block.blockTypeCode == 7) {
                    return block.getAge();
                }
                index += 1;
            }
        }
        ESP_LOGE("Bundle increase getAge",
                 "Bundle does not contain BundleAgeBlock");
        return 0;
    }
};