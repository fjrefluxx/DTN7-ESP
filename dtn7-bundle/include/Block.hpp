#pragma once
#include "EID.hpp"
#include "cbor.h"
#include "esp_log.h"
#include "utils.hpp"

/**
 * @file Block.hpp
 * @brief This file contains all relevant definitions for the CanonicalBlock and PrimaryBlock classes.
 */


/// @brief calculates the CRC as specified in rfc9171
/// @param crcType the type of crc To calculate
/// @param data data to calculate CRC on
/// @param dataSize size of the input data
/// @return the calculated CRC
uint32_t calculateCRC(uint8_t crcType, const uint8_t* data, size_t dataSize);

/// @brief checks if the given data has a valid crc attached (either in the last 2 bytes for CRC type 1 or in the last 4 for CRC type 2).
///        This checking is implemented by replacing the bytes representing the CRC and then recalculating it and checking whether this calculation yields the expected result.
/// @param crcType type of the included CRC
/// @param data data to check
/// @param dataSize size of the data
/// @return whether the CRC check passed
bool checkCRC(uint8_t crcType, const uint8_t* data, size_t dataSize);

/// @brief represents a generic canonical block
class CanonicalBlock {
   public:
    /// @brief indicates whether the block is Valid
    bool valid;

    /// @brief block type code
    uint64_t blockTypeCode;

    /// @brief block number
    uint64_t blockNumber;

    /// @brief stores the blocks processing control flags
    uint64_t blockProcessingControlFlags;

    /// @brief stores the crc type of the block
    uint64_t crcType;

    /// @brief stores the block type specific data on the heap
    uint8_t* blockTypeSpecificData;

    /// @brief stores the CRC
    uint8_t* CRC;

    /// @brief stores the data size
    size_t dataSize;

    /// @brief stores the crc Size
    size_t crcSize;

    /// @brief print the block to the log
    void print() {
        ESP_LOGI(
            "canonical block Print",
            "Valid: %s, TypeCode:%llu, blockNumber: %llu, BlockFlags:%llu, "
            "BlockTypeSpecificData size: %u, CRC Type:%llu",
            valid ? "true" : "false", blockTypeCode, blockNumber,
            blockProcessingControlFlags, dataSize, crcType);
        ESP_LOGI("canonical block Print", "%.*s", dataSize,
                 blockTypeSpecificData);
        return;
    };

    /// @brief Deletes the canonical block
    ~CanonicalBlock() {
        delete[] blockTypeSpecificData;
        delete[] CRC;
    };

    /// @brief generates an empty canonical block (not valid)
    CanonicalBlock() {
        dataSize = 0;
        crcSize = 0;
        blockNumber = 0;  // default block number is 0; marks block is not valid
        blockProcessingControlFlags = 0;
        crcType = CRC_TYPE_NOCRC;
        blockTypeSpecificData = nullptr;
        CRC = nullptr;
        valid = false;
    }

    /// @brief generates a canonical block
    /// @param type block type code for the new block
    /// @param num block number for the new block
    /// @param size size of the data for the new block
    /// @param flags uint64_t representing the blockProcessingControlFlags of the new block
    /// @param data pointer to the data for the new block, needs to point to at least size amount of memory
    /// @param crcType type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    CanonicalBlock(uint64_t type, uint64_t num, size_t size, uint64_t flags,
                   uint8_t* data, uint8_t crcType = CRC_TYPE_NOCRC)
        : blockTypeCode(type),
          blockNumber(num),
          blockProcessingControlFlags(flags),
          dataSize(size) {
        blockTypeSpecificData = new uint8_t[size];
        memcpy(blockTypeSpecificData, data, size);
        crcSize = 0;
        this->crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            this->CRC = nullptr;
            this->crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            this->CRC = new uint8_t[2]{0, 0};
            this->crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            this->CRC = new uint8_t[4]{0, 0, 0, 0};
            this->crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }
        valid = true;
    }

    /// @brief generates a canonical block with out a block number, use this if Bundle::insertCanonicalBlock is to be used for automatic numbering
    /// @param type block type code for the new block
    /// @param size size of the data for the new block
    /// @param flags uint64_t representing the blockProcessingControlFlags of the new block
    /// @param data pointer to the data for the new block, needs to point to at least size amount of memory
    /// @param crcType type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    CanonicalBlock(uint64_t type, size_t size, uint64_t flags, uint8_t* data,
                   uint8_t crcType = CRC_TYPE_NOCRC)
        : blockTypeCode(type),
          blockProcessingControlFlags(flags),
          dataSize(size) {
        blockTypeSpecificData = new uint8_t[size];
        blockNumber = 0;
        memcpy(blockTypeSpecificData, data, size);
        crcSize = 0;
        this->crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            this->CRC = nullptr;
            this->crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            this->CRC = new uint8_t[2]{0, 0};
            this->crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            this->CRC = new uint8_t[4]{0, 0, 0, 0};
            this->crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }
        valid = true;
    }

    /// @brief generates canonical block without data
    /// @param type block type code for the new block
    /// @param num block number for the new block
    /// @param flags uint64_t representing the blockProcessingControlFlags of the new block
    /// @param crcType type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    CanonicalBlock(uint64_t type, uint64_t num, uint64_t flags,
                   uint8_t crcType = CRC_TYPE_NOCRC)
        : blockTypeCode(type),
          blockNumber(num),
          blockProcessingControlFlags(flags),
          dataSize(0) {
        blockTypeSpecificData = nullptr;
        crcSize = 0;
        this->crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            this->CRC = nullptr;
            this->crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            this->CRC = new uint8_t[2]{0, 0};
            this->crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            this->CRC = new uint8_t[4]{0, 0, 0, 0};
            this->crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }
        valid = true;
    }

    /// @brief generates canonical block without data and without a block number, use this if Bundle::insertCanonicalBlock is to be used for automatic numbering
    /// @param type block type code for the new block
    /// @param flags uint64_t representing the blockProcessingControlFlags of the new block
    /// @param crcType type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    CanonicalBlock(uint64_t type, uint64_t flags,
                   uint8_t crcType = CRC_TYPE_NOCRC)
        : blockTypeCode(type), blockProcessingControlFlags(flags), dataSize(0) {
        blockNumber = 0;
        blockTypeSpecificData = nullptr;
        crcSize = 0;
        this->crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            this->CRC = nullptr;
            this->crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            this->CRC = new uint8_t[2]{0, 0};
            this->crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            this->CRC = new uint8_t[4]{0, 0, 0, 0};
            this->crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }
        valid = true;
    }

    /// @brief canonical block copy constructor
    /// @param old
    CanonicalBlock(const CanonicalBlock& old) {
        blockTypeSpecificData = new uint8_t[old.dataSize];
        dataSize = old.dataSize;
        blockTypeCode = old.blockTypeCode;
        blockNumber = old.blockNumber;
        blockProcessingControlFlags = old.blockProcessingControlFlags;
        this->crcType = old.crcType;
        crcSize = old.crcSize;
        valid = old.valid;
        CRC = new uint8_t[crcSize];
        memcpy(blockTypeSpecificData, old.blockTypeSpecificData, old.dataSize);
        memcpy(CRC, old.CRC, old.crcSize);
    }

    /// @brief canonical block operator=
    /// @param old
    /// @return
    CanonicalBlock& operator=(const CanonicalBlock& old) {
        if (this == &old)
            return *this;

        delete[] blockTypeSpecificData;
        blockTypeSpecificData = new uint8_t[old.dataSize];
        dataSize = old.dataSize;
        blockTypeCode = old.blockTypeCode;
        blockNumber = old.blockNumber;
        blockProcessingControlFlags = old.blockProcessingControlFlags;
        crcType = old.crcType;
        crcSize = old.crcSize;
        valid = old.valid;
        delete[] CRC;
        CRC = new uint8_t[crcSize];
        memcpy(blockTypeSpecificData, old.blockTypeSpecificData, old.dataSize);
        memcpy(CRC, old.CRC, old.crcSize);
        return *this;
    }

    /// @brief generates a BlockProcessingFlags Object from the blocks encoded flags
    /// @return BlockProcessingFlags object representing the blocks control flags
    BlockProcessingFlags getFlags() {
        return BlockProcessingFlags(this->blockProcessingControlFlags);
    }

    /// @brief sets the encoded BlockProcessingFlags from a BlockProcessingFlags object
    /// @param flags the new flags for the block
    void setFlags(BlockProcessingFlags flags) {
        this->blockProcessingControlFlags = flags.getEncoded();
        return;
    }

    /// @brief sets a specific control flag for the block
    /// @param flag the position of the flag to set
    void setFlag(int flag) {
        this->blockProcessingControlFlags |= 1ULL << flag;
    }

    /// @brief clears a specific control flag for the block
    /// @param flag the position of the flag to clear
    void clearFlag(int flag) {
        this->blockProcessingControlFlags &= 1ULL << flag;
    }

    /// @brief encodes the canonical block to CBOR in a newly allocated array
    /// @param cbor pointer to a new array on the heap storing the encoded primary block
    /// @param cborSize size of the cbor for the primary block
    void toCbor(uint8_t** cbor, size_t& cborSize);

    /// @brief if the block is an HopCountBlock, read the stored HopCount
    /// @return HopCount stored in the block
    uint64_t getHopCount();

    /// @brief if the block is an HopCountBlock, read the stored HopLimit
    /// @return HopLimit stored in the block
    uint64_t getHopLimit();

    /// @brief if the block is an BundleAgeBlock, read the stored age
    /// @return age stored in the block
    uint64_t getAge() {
        if (blockTypeCode == 7) {
            uint64_t result;
            CborParser parser;
            CborValue value;
            cbor_parser_init(blockTypeSpecificData, dataSize, 0, &parser,
                             &value);
            cbor_value_get_uint64(&value, &result);
            return result;
        }
        else {
            return 0;
        }
    }

    /// @brief if the block is an BundleAgeBlock, set the stored age
    /// @param age new age to be stored in the block
    void setAge(uint64_t age);
};

/// @brief Represents the primary block
class PrimaryBlock {
   public:
    /// @brief indicates whether the block is valid
    bool valid;

    /// @brief stores the version stored in the primary block
    uint64_t version;

    /// @brief stores the bundleProcessingControlFlags of the primary block
    uint64_t bundleProcessingControlFlags;

    /// @brief stores the CRC type of the block
    uint64_t crcType;

    /// @brief EID representing the Destination Address(EID) stored in the primary block
    EID destEID;

    /// @brief EID representing the Source Address(EID) stored in the primary block
    EID sourceEID;

    /// @brief EID representing the ReportTo Address(EID) stored in the primary block
    EID reportToEID;

    /// @brief CreationTimestamp of the primary block/bundle
    CreationTimestamp timestamp;

    /// @brief stores the bundle's lifetime
    uint64_t lifetime;

    /// @brief Stores the bundle's fragment offset (if bundle is a fragment)
    uint64_t fragOffset;

    /// @brief Stores the total ADU length (if bundle is a fragment)
    uint64_t totalADULength;

    /// @brief stores the size of the primary block's CRC
    size_t crcSize;

    /// @brief stores the CRC for the primary block
    uint8_t* CRC;

    /// @brief prints the block to the log
    void print() {
        ESP_LOGI(
            "primary block Print",
            "Valid: %s, Version:%llu, BundleControlFlags: %llu, crcType:%llu",
            valid ? "true" : "false", version, bundleProcessingControlFlags,
            crcType);
        destEID.print();
        sourceEID.print();
        reportToEID.print();
        timestamp.print();
        ESP_LOGI("primary block Print",
                 "lifetime:%llu, fragOffset: %llu, totalADULength:%llu, CRC "
                 "Type:%llu",
                 lifetime, fragOffset, totalADULength, crcType);
        return;
    };

    /// @brief generates a complete primary block
    /// @param version the bundleProtocol version of the block, this field is ignored, as only version 7 is supported
    /// @param bundleProcessingControlFlags uint64_t representing the bundleProcessingControlFlags of the new block
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    /// @param destEID Endpoint ID of the bundle Destination
    /// @param sourceEID Endpoint ID of the bundle Source
    /// @param reportToEID Endpoint ID of the bundle Report To node
    /// @param timestamp creation timestamp of the bundle
    /// @param lifetime the lifetime of the bundle
    /// @param fragOffset Fragment offset, Fragmentation is not supported, but if the information exists it is not discarded
    /// @param totalADULength Total ADU lengthFragmentation is not supported, but if the information exists it is not discarded
    /// @param CRC pointer to the CRC for the new block, needs to point to atleast CRCsize amount of memory
    /// @param crcSize size of the CRC of the block, defaults to 0, as CRC is not supported
    PrimaryBlock(uint64_t version, uint64_t bundleProcessingControlFlags,
                 uint64_t crcType, EID destEID, EID sourceEID, EID reportToEID,
                 CreationTimestamp timestamp, uint64_t lifetime,
                 uint64_t fragOffset, uint64_t totalADULength, uint8_t* CRC,
                 size_t crcSize = 0)
        : bundleProcessingControlFlags(bundleProcessingControlFlags),
          crcType(crcType),
          destEID(destEID),
          sourceEID(sourceEID),
          reportToEID(reportToEID),
          timestamp(timestamp),
          lifetime(lifetime),
          fragOffset(fragOffset),
          totalADULength(totalADULength) {
        this->version = version;
        this->version = 7;
        this->CRC = new uint8_t[crcSize];
        this->crcSize = crcSize;
        this->valid = true;
        ESP_LOGD("Primary Block", "CRC type: %llu", crcType);
        memcpy(this->CRC, CRC, crcSize);
    };

    /// @brief generates a minimal valid primary block
    /// @param destEID Endpoint ID of the bundle Destination
    /// @param sourceEID Endpoint ID of the bundle Source
    /// @param reportToEID Endpoint ID of the bundle Report To node
    /// @param timestamp creation timestamp of the bundle
    /// @param lifetime the lifetime of the bundle
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    PrimaryBlock(EID destEID, EID sourceEID, EID reportToEID,
                 CreationTimestamp timestamp, uint64_t lifetime,
                 uint8_t crcType = CRC_TYPE_NOCRC)
        : destEID(destEID),
          sourceEID(sourceEID),
          reportToEID(reportToEID),
          timestamp(timestamp),
          lifetime(lifetime) {
        version = 7;
        bundleProcessingControlFlags = 0;
        this->crcType = crcType;
        fragOffset = 0;
        totalADULength = 0;
        valid = true;
        ESP_LOGD("Primary Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            this->CRC = nullptr;
            this->crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            this->CRC = new uint8_t[2]{0, 0};
            this->crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            this->CRC = new uint8_t[4]{0, 0, 0, 0};
            this->crcSize = 4;
        }
        else {
            ESP_LOGE("Primary Block", "Unsupported CRC type");
        }
    };

    ~PrimaryBlock() { delete[] CRC; }

    /// @brief generates an empty primary block
    PrimaryBlock() {
        version = 7;
        bundleProcessingControlFlags = 0;
        crcType = CRC_TYPE_NOCRC;
        destEID = EID();
        sourceEID = EID();
        reportToEID = EID();
        timestamp = CreationTimestamp();
        lifetime = 0;
        fragOffset = 0;
        totalADULength = 0;
        crcSize = 0;
        valid = false;
        CRC = nullptr;
    }

    /// @brief primary block copy constructor
    /// @param old
    PrimaryBlock(const PrimaryBlock& old) {
        version = old.version;
        bundleProcessingControlFlags = old.bundleProcessingControlFlags;
        crcType = old.crcType;
        destEID = old.destEID;
        sourceEID = old.sourceEID;
        reportToEID = old.reportToEID;
        timestamp = old.timestamp;
        lifetime = old.lifetime;
        fragOffset = old.fragOffset;
        totalADULength = old.totalADULength;
        crcSize = old.crcSize;
        valid = old.valid;
        CRC = new uint8_t[crcSize];
        memcpy(this->CRC, old.CRC, crcSize);
    }

    /// @brief PrimaryBlock operator=
    /// @param old
    /// @return
    PrimaryBlock& operator=(const PrimaryBlock& old) {
        if (this == &old)
            return *this;
        // ESP_LOGI("PrimaryBlock operator= ", "called");
        version = old.version;
        bundleProcessingControlFlags = old.bundleProcessingControlFlags;
        crcType = old.crcType;
        destEID = old.destEID;
        sourceEID = old.sourceEID;
        reportToEID = old.reportToEID;
        timestamp = old.timestamp;
        lifetime = old.lifetime;
        fragOffset = old.fragOffset;
        totalADULength = old.totalADULength;
        crcSize = old.crcSize;
        valid = old.valid;
        delete[] CRC;
        CRC = new uint8_t[crcSize];
        memcpy(this->CRC, old.CRC, crcSize);
        return *this;
    }

    /// @brief encodes the primary block to Cbor in a newly allocated array
    /// @param cbor pointer to a new array on the heap storing the encoded primary block
    /// @param cborSize size of the cbor for the primary block
    void toCbor(uint8_t** cbor, size_t& cborSize);

    /// @brief generates a BundleProcessingFlags Object from the blocks encoded flags
    /// @return BundleProcessingFlags object representing the blocks control flags
    BundleProcessingFlags getFlags() {
        return BundleProcessingFlags(this->bundleProcessingControlFlags);
    }

    /// @brief sets the encoded BundleProcessingFlags from a BundleProcessingFlags object
    /// @param flags the new flags for the bundle
    void setFlags(BundleProcessingFlags flags) {
        this->bundleProcessingControlFlags = flags.getEncoded();
        return;
    }

    /// @brief sets a specific control flag for the block
    /// @param flag the position of the flag to set
    void setFlag(int flag) {
        this->bundleProcessingControlFlags |= 1ULL << flag;
    }

    /// @brief sets a specific control flag for the block
    /// @param flag the position of the flag to set
    void clearFlag(int flag) {
        this->bundleProcessingControlFlags &= 1ULL << flag;
    }
};

/// @brief this object represents the previous node block. Used for creation of the bundle, not decoding.
class PreviousNodeBlock : public CanonicalBlock {
   public:
    /// @brief the EID of the previous node
    EID previousNode;

    /// @brief constructs the previous node block
    /// @param previous the previous nodes EID
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    /// @param blockNumber the block number the block should have, optional if insertCanonicalBlock is to be used
    PreviousNodeBlock(EID previous, uint8_t crcType = CRC_TYPE_NOCRC,
                      uint64_t blockNumber = 0)
        : previousNode(previous) {
        CanonicalBlock::blockNumber = blockNumber;
        CanonicalBlock::blockTypeCode = 6;

        CanonicalBlock::crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            CanonicalBlock::CRC = nullptr;
            CanonicalBlock::crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            CanonicalBlock::CRC = new uint8_t[2]{0, 0};
            CanonicalBlock::crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            CanonicalBlock::CRC = new uint8_t[4]{0, 0, 0, 0};
            CanonicalBlock::crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }
        valid = true;
        CborEncoder* encoder = new CborEncoder();
        uint8_t buf[20 + previous.sspSize];
        cbor_encoder_init(encoder, buf, sizeof(buf), 0);
        previous.toCbor(encoder);
        CanonicalBlock::dataSize = cbor_encoder_get_buffer_size(encoder, buf);
        CanonicalBlock::blockTypeSpecificData =
            new uint8_t[CanonicalBlock::dataSize];
        memcpy(blockTypeSpecificData, buf, CanonicalBlock::dataSize);

        delete encoder;
    }

    /// @brief previous node default constructor
    PreviousNodeBlock() {
        dataSize = 0;
        crcSize = 0;
        crcType = CRC_TYPE_NOCRC;
        blockNumber = 0;
        valid = false;
        previousNode = EID();
        blockTypeSpecificData = nullptr;
        crcType = CRC_TYPE_NOCRC;
        CRC = nullptr;
    }
};

/// @brief this object represents the bundle age block. Used for creation of the bundle, not decoding.
class BundleAgeBlock : public CanonicalBlock {
    /// @brief the age stored in the block
    uint64_t age;

   public:
    /// @brief constructs the bundleAgeBlock, block number can be ignored if insertCanonicalBlock is to be used
    /// @param age the age to store in the block
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    /// @param blockNumber the block number of the previous block
    BundleAgeBlock(uint64_t age, uint8_t crcType = CRC_TYPE_NOCRC,
                   uint64_t blockNumber = 0) {
        age = age;
        CanonicalBlock::blockNumber = blockNumber;
        CanonicalBlock::blockTypeCode = 7;
        CanonicalBlock::valid = true;

        CanonicalBlock::crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            CanonicalBlock::CRC = nullptr;
            CanonicalBlock::crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            CanonicalBlock::CRC = new uint8_t[2]{0, 0};
            CanonicalBlock::crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            CanonicalBlock::CRC = new uint8_t[4]{0, 0, 0, 0};
            CanonicalBlock::crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }

        CborEncoder* encoder = new CborEncoder();
        uint8_t buf[sizeof(uint64_t) + 2];
        cbor_encoder_init(encoder, buf, sizeof(buf), 0);
        cbor_encode_uint(encoder, age);

        size_t actualSize = cbor_encoder_get_buffer_size(encoder, buf);
        CanonicalBlock::blockTypeSpecificData = new uint8_t[actualSize];
        memcpy(CanonicalBlock::blockTypeSpecificData, buf, actualSize);
        dataSize = actualSize;
        delete encoder;
    }

    /// @brief default constructor
    BundleAgeBlock() {
        dataSize = 0;
        crcSize = 0;
        crcType = CRC_TYPE_NOCRC;
        age = 0;
        valid = false;
        CanonicalBlock::blockTypeSpecificData = nullptr;
        CRC = nullptr;
    }
};

/// @brief Class representing the HopCountBlock, used for bundle encoding
class HopCountBlock : public CanonicalBlock {
   public:
    uint64_t hopLimit;
    uint64_t hopCount;

    /// @brief creates a hop count block
    /// @param hopLimit hopLimit to include in the HopCountBlock
    /// @param hopCount hopCount to include in the HopCountBlock, default 0
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    /// @param blockNumber block number of the HopCountBlock, default 0, leave 0 to use automatic numbering of insertCanonicalBlock
    HopCountBlock(uint64_t hopLimit, uint64_t hopCount = 0,
                  uint8_t crcType = CRC_TYPE_NOCRC, uint64_t blockNumber = 0)
        : hopLimit(hopLimit), hopCount(hopCount) {
        blockTypeCode = 10;
        blockNumber = blockNumber;

        CanonicalBlock::crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            CanonicalBlock::CRC = nullptr;
            CanonicalBlock::crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            CanonicalBlock::CRC = new uint8_t[2]{0, 0};
            CanonicalBlock::crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            CanonicalBlock::CRC = new uint8_t[4]{0, 0, 0, 0};
            CanonicalBlock::crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }

        valid = true;
        CborEncoder* encoder = new CborEncoder();
        uint8_t buf[7 + 12];
        CborEncoder* encoder2 = new CborEncoder();
        uint8_t buf2[12];
        cbor_encoder_init(encoder, buf, sizeof(buf), 0);
        cbor_encoder_init(encoder2, buf2, sizeof(buf2), 0);
        cbor_encoder_create_array(encoder, encoder2, 2);
        cbor_encode_uint(encoder2, hopLimit);
        cbor_encode_uint(encoder2, hopCount);
        cbor_encoder_close_container(encoder, encoder2);
        size_t actualSize = cbor_encoder_get_buffer_size(encoder, buf);
        blockTypeSpecificData = new uint8_t[actualSize];
        memcpy(blockTypeSpecificData, buf, actualSize);
        dataSize = actualSize;
        delete encoder;
        delete encoder2;
    }

    /// @brief default constructor of HopCount Block, not recommended to use, except you know what you are doing
    HopCountBlock() {
        blockTypeCode = 10;
        blockNumber = 0;
        crcSize = 0;
        crcType = CRC_TYPE_NOCRC;  // CRC is not supported
        dataSize = 0;
        valid = false;
        blockTypeSpecificData = nullptr;
        CRC = nullptr;
    }
};

/// @brief Class representing the PayloadBlock, used when decoding and encoding Bundle
class PayloadBlock : public CanonicalBlock {
   public:
    /// @brief
    /// @param data
    /// @param size
    /// @param crcType  type of CRC for this block, 0 = no CRC, 1 = CRC16, 2 = CRC32C, see RFC9171 for more information on the different CRC types
    PayloadBlock(uint8_t* data, size_t size, uint8_t crcType = CRC_TYPE_NOCRC)
        : CanonicalBlock(1, 1, size, 0, data) {

        CanonicalBlock::crcType = crcType;
        ESP_LOGD("Canonical Block", "CRC type:%u", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            CanonicalBlock::CRC = nullptr;
            CanonicalBlock::crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            CanonicalBlock::CRC = new uint8_t[2]{0, 0};
            CanonicalBlock::crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            CanonicalBlock::CRC = new uint8_t[4]{0, 0, 0, 0};
            CanonicalBlock::crcSize = 4;
        }
        else {
            ESP_LOGE("Canonical Block", "Unsupported CRC type: %u", crcType);
        }

        valid = true;
    }

    /// @brief primary block constructor from generic canonical block
    /// @param canonicalBlock
    PayloadBlock(CanonicalBlock& canonicalBlock)
        : CanonicalBlock(canonicalBlock.blockTypeCode,
                         canonicalBlock.blockNumber,
                         canonicalBlock.blockProcessingControlFlags) {
        crcType = canonicalBlock.crcType;
        ESP_LOGD("Canonical Block", "CRC type:%llu", crcType);
        if (crcType == CRC_TYPE_NOCRC) {
            CanonicalBlock::CRC = nullptr;
            CanonicalBlock::crcSize = 0;
        }
        else if (crcType == CRC_TYPE_X25) {
            CanonicalBlock::CRC = new uint8_t[2]{0, 0};
            CanonicalBlock::crcSize = 2;
        }
        else if (crcType == CRC_TYPE_CRC32C) {
            CanonicalBlock::CRC = new uint8_t[4]{0, 0, 0, 0};
            CanonicalBlock::crcSize = 4;
        }
        else {
            ESP_LOGE("Payload Block", "Unsupported CRC type: %llu", crcType);
        }

        delete[] CanonicalBlock::blockTypeSpecificData;
        CanonicalBlock::dataSize = canonicalBlock.dataSize;
        CanonicalBlock::blockTypeSpecificData =
            new uint8_t[canonicalBlock.dataSize];
        memcpy(blockTypeSpecificData, canonicalBlock.blockTypeSpecificData,
               canonicalBlock.dataSize);
    }

    /// @brief Default constructor, not recommended to use, except you know what you are doing
    PayloadBlock() {
        dataSize = 0;
        blockTypeCode = 1;
        blockNumber = 1;
        crcSize = 0;
        valid = 0;
        blockProcessingControlFlags = 0;
        crcType = CRC_TYPE_NOCRC;  // CRC is not supported
        blockTypeSpecificData = nullptr;
        CRC = nullptr;  // crc not Supported
    }
};