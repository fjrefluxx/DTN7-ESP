#pragma once
#include <algorithm>
#include "Block.hpp"
#include "EID.hpp"
#include "cbor.h"
#include "esp_log.h"

#define BUNDLE_FLAG_IS_FRAGMENT 0
#define BUNDLE_FLAG_IS_ADMIN_RECORD 1
#define BUNDLE_FLAG_DO_NOT_FRAGMENT 2
#define BUNDLE_FLAG_ACK_REQUEST 5
#define BUNDLE_FLAG_STATUS_TIME_REQUEST 6
#define BUNDLE_FLAG_REPORT_RECEPTION 14
#define BUNDLE_FLAG_REPORT_FORWARDING 16
#define BUNDLE_FLAG_REPORT_DELIVERY 17
#define BUNDLE_FLAG_REPORT_DELETION 18

#define BLOCK_FLAG_MUST_BE_REPLICATED 0
#define BLOCK_FLAG_REPORT_CANT_BE_PROCESSED 1
#define BLOCK_FLAG_DELETE_CANT_BE_PROCESSED 2
#define BLOCK_FLAG_DISCARD_CANT_BE_PROCESSED 4

#define CRC_TYPE_NOCRC 0
#define CRC_TYPE_X25 1
#define CRC_TYPE_CRC32C 2

/// @brief represents the creation timestamp
struct CreationTimestamp {
    uint64_t creationTime;
    uint64_t sequenceNumber;

    /// @brief Prints the Creation Timestamp to the Log
    void print() {
        ESP_LOGI("Creation Timestamp Print",
                 "Creation Time:%llu, Sequence Number: %llu", creationTime,
                 sequenceNumber);
    }

    /// @brief generates a creation timestamp from creation time and sequence number
    /// @param time Creation time for the timestamp
    /// @param sequence Sequence Number for the timestamp
    CreationTimestamp(uint64_t time, uint64_t sequence)
        : creationTime(time), sequenceNumber(sequence) {}

    /// @brief generates a (0|0) creation timestamp
    CreationTimestamp() {
        creationTime = 0;
        sequenceNumber = 0;
    }

    /// @brief encodes the creation timestamp to cbor with the given encoder
    /// @param encoder the encoder which is used to encode the block, its buffer will contain the resulting cbor
    void toCbor(CborEncoder* encoder) {
        CborEncoder* encoderInternal = new CborEncoder;
        uint8_t* buf = new uint8_t[19];  // up to 7 bytes overhead + 2 ints
        cbor_encoder_init(encoderInternal, buf, sizeof(buf), 0);
        cbor_encoder_create_array(
            encoder, encoderInternal,
            2);  // rcf9171: IPN scheme be represented as a CBOR array comprising two items
        cbor_encode_uint(encoderInternal, creationTime);
        cbor_encode_uint(encoderInternal, sequenceNumber);
        cbor_encoder_close_container_checked(encoder, encoderInternal);
        delete encoderInternal;
        delete[] buf;
        return;
    }

    /// @brief converts the timestamp to a std string, used for bundle Id creation
    /// @return string representing the creation timestamp, format : {creationTime-SequenceNumber}
    std::string toString() {
        return std::to_string(this->creationTime)
            .append("-")
            .append(std::to_string(this->sequenceNumber));
    }
};

/// @brief represents the bundle processing control flags, only used for encoding
class BundleProcessingFlags {
   private:
    /// @brief checks whether the bit at the specified location is set
    /// @param flags the uint64 of which to check the bit
    /// @param bit the bit position to check
    /// @return whether the bit is set
    bool getBitAtPos(uint64_t flags, int bit) {
        return ((flags >> bit) & 1ULL);
    }
    /// @brief sets the bit at the specified location
    /// @param flags the uint64 of which to set the bit
    /// @param bit the bit position to set
    /// @return the given flags uint64 with the new set bit
    uint64_t setBitAtPos(uint64_t flags, int bit) {
        return flags |= 1ULL << bit;
    }
    /// @brief clears the bit at the specified location
    /// @param flags the uint64 of which to clear the bit
    /// @param bit the bit position to clear
    /// @return the given flags uint64 with the new cleared bit
    uint64_t clearBitAtPos(uint64_t flags, int bit) {
        return flags &= 1ULL << bit;
    }

   public:
    /// @brief the encoded flags as a uint64
    uint64_t encoded;

    /// @brief creates a new bundle processing control flags
    /// @param flagsEncoded the encode flags in a uint64_t
    BundleProcessingFlags(uint64_t flagsEncoded) { encoded = flagsEncoded; }
    BundleProcessingFlags() : BundleProcessingFlags(0) {}
    uint64_t setFlag(int flagNum) {
        encoded = setBitAtPos(encoded, flagNum);
        return encoded;
    }
    bool getFlag(int flagNum) { return getBitAtPos(encoded, flagNum); }
    uint64_t getEncoded() { return encoded; }
    uint64_t clearFlag(int flagNum) {
        encoded = clearBitAtPos(encoded, flagNum);
        return encoded;
    }
};

/// @brief represents the block processing control flags, only used for encoding
class BlockProcessingFlags {
   private:
    bool getBitAtPos(uint64_t flags, int bit) {
        return ((flags >> bit) & 1ULL);
    }
    uint64_t setBitAtPos(uint64_t flags, int bit) {
        return flags |= 1ULL << bit;
    }
    uint64_t clearBitAtPos(uint64_t flags, int bit) {
        return flags &= 1ULL << bit;
    }

   public:
    uint64_t encoded;

    BlockProcessingFlags(uint64_t flagsEncoded) { encoded = flagsEncoded; }

    BlockProcessingFlags() : BlockProcessingFlags(0) {}
    uint64_t setFlag(int flagNum) {
        encoded = setBitAtPos(encoded, flagNum);
        return encoded;
    }

    bool getFlag(int flagNum) { return getBitAtPos(encoded, flagNum); }

    uint64_t getEncoded() { return encoded; }

    uint64_t clearFlag(int flagNum) {
        encoded = clearBitAtPos(encoded, flagNum);
        return encoded;
    }
};
