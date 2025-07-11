#include "Block.hpp"
#include "crc32c.h"
#include "esp_log.h"
#include "esp_rom_crc.h"

//array of zeros, used as source to default initialize CRC fields
static uint8_t zeros[4] = {0, 0, 0, 0};

void CanonicalBlock::toCbor(uint8_t** cbor, size_t& cborSize) {

    CborEncoder encoderExternal = CborEncoder();
    uint8_t buf[255];
    cbor_encoder_init(&encoderExternal, buf, sizeof(buf), 0);
    CborEncoder* encoderInternal = new CborEncoder();
    // required size: up to 7 byte for array + string + data size + 4*int
    uint8_t* buffer = new uint8_t[dataSize + 38];

    size_t arrayLength = 5;
    if (crcType != CRC_TYPE_NOCRC)
        arrayLength += 1;
    cbor_encoder_init(encoderInternal, buffer, sizeof(buffer), 0);
    cbor_encoder_create_array(&encoderExternal, encoderInternal, arrayLength);
    cbor_encode_uint(encoderInternal, this->blockTypeCode);
    cbor_encode_uint(encoderInternal, this->blockNumber);
    cbor_encode_uint(encoderInternal, this->blockProcessingControlFlags);
    cbor_encode_uint(encoderInternal, this->crcType);
    cbor_encode_byte_string(encoderInternal, this->blockTypeSpecificData,
                            this->dataSize);

    //if CRC is required we need to ecode this into the block
    if (this->crcType != CRC_TYPE_NOCRC) {
        //the CRC field must be filled with 0 before calculating the CRC
        cbor_encode_byte_string(encoderInternal, zeros, this->crcSize);
    }
    cbor_encoder_close_container_checked(&encoderExternal, encoderInternal);

    size_t cborSizeCanonical =
        cbor_encoder_get_buffer_size(&encoderExternal, buf);

    *cbor = new uint8_t[cborSizeCanonical];
    memcpy(*cbor, buf, cborSizeCanonical);

    cborSize = cborSizeCanonical;
    ESP_LOGD("canonicalToCbor", "canonical Block cborSize:%u",
             cborSizeCanonical);

    //calculate and insert CRC into CBOR
    if (this->crcType == CRC_TYPE_X25) {
        uint16_t crc = calculateCRC(this->crcType, *cbor, cborSizeCanonical);
        (*cbor)[cborSizeCanonical - 1] = crc & 0xFF;
        (*cbor)[cborSizeCanonical - 2] = (crc >> 8) & 0xFF;
    }
    if (this->crcType == CRC_TYPE_CRC32C) {
        uint32_t crc = calculateCRC(this->crcType, *cbor, cborSizeCanonical);
        (*cbor)[cborSizeCanonical - 1] = crc & 0xFF;
        (*cbor)[cborSizeCanonical - 2] = (crc >> 8) & 0xFF;
        (*cbor)[cborSizeCanonical - 3] = (crc >> 16) & 0xFF;
        (*cbor)[cborSizeCanonical - 4] = (crc >> 24) & 0xFF;
    }

    delete[] buffer;
    delete encoderInternal;
    return;
}

uint64_t CanonicalBlock::getHopCount() {
    if (blockTypeCode == 10) {
        uint64_t result;
        CborParser parser;
        CborValue value;
        cbor_parser_init(blockTypeSpecificData, dataSize, 0, &parser, &value);
        if (cbor_value_is_array(&value)) {
            CborValue value2;
            cbor_value_enter_container(
                &value, &value2);  // enter the array describing the block
            cbor_value_get_uint64(&value2, &result);  // first read hop limit
            cbor_value_advance(&value2);              // move to next value
            cbor_value_get_uint64(&value2, &result);  // read hop count
            return result;
        }
    }
    return 0;
}

uint64_t CanonicalBlock::getHopLimit() {
    if (blockTypeCode == 10) {
        uint64_t result;
        CborParser parser;
        CborValue value;
        cbor_parser_init(blockTypeSpecificData, dataSize, 0, &parser, &value);
        if (cbor_value_is_array(&value)) {
            CborValue value2;
            cbor_value_enter_container(
                &value, &value2);  // enter the array describing the block
            cbor_value_get_uint64(&value2, &result);  // first read hop limit
            return result;
        }
    }
    return 0;
}

void CanonicalBlock::setAge(uint64_t age) {
    if (blockTypeCode == 7) {
        CborEncoder* encoder = new CborEncoder();
        uint8_t buf[10];
        cbor_encoder_init(encoder, buf, sizeof(buf), 0);
        cbor_encode_uint(encoder, age);
        delete[] this->blockTypeSpecificData;
        size_t actualSize = cbor_encoder_get_buffer_size(encoder, buf);
        blockTypeSpecificData = new uint8_t[actualSize];
        memcpy(blockTypeSpecificData, buf, actualSize);
    }
}

void PrimaryBlock::toCbor(uint8_t** cbor, size_t& cborSize) {
    //for some inexplicable reason, encoding cbor only functions correctly with the encoder and the buffer on the stack

    //get the BundleProcessingControlFlags for this bundle
    BundleProcessingFlags flags = this->getFlags();

    CborEncoder encoderExternal = CborEncoder();
    uint8_t buf[255];
    cbor_encoder_init(&encoderExternal, buf, sizeof(buf), 0);
    CborEncoder* encoderInternal = new CborEncoder();
    // required size: up to 7 bytes for the external array, 4*6 bytes for the ints, + size of EID (20 + sspsize) and timestamp å(19), plus the size of the crc
    uint8_t* buffer =
        new uint8_t[7 + 24 + 3 * 20 + destEID.sspSize + sourceEID.sspSize +
                    reportToEID.sspSize + 19 + crcSize];
    cbor_encoder_init(encoderInternal, buffer, sizeof(buffer), 0);
    // rfc9171: array size = 8 because the primary block has 8 elements if no CRC or fragmentation is present

    uint8_t blockElementNumber =
        8;  //base size of the primary block is 8 elements
    if (this->crcType != CRC_TYPE_NOCRC)
        blockElementNumber +=
            1;  //if crc is present the block is 1 element larger
    if (flags.getFlag(BUNDLE_FLAG_IS_FRAGMENT))
        blockElementNumber +=
            2;  //if the bundle is fragmented, to additional elements are needed in the Block

    cbor_encoder_create_array(&encoderExternal, encoderInternal,
                              blockElementNumber);

    cbor_encode_uint(encoderInternal, this->version);
    cbor_encode_uint(encoderInternal, this->bundleProcessingControlFlags);
    cbor_encode_uint(encoderInternal, this->crcType);
    destEID.toCbor(encoderInternal);
    sourceEID.toCbor(encoderInternal);
    reportToEID.toCbor(encoderInternal);
    timestamp.toCbor(encoderInternal);
    cbor_encode_uint(encoderInternal, this->lifetime);

    //if the bundle is fragmented, we need two additional elements
    if (flags.getFlag(BUNDLE_FLAG_IS_FRAGMENT)) {
        cbor_encode_uint(encoderInternal, this->fragOffset);
        cbor_encode_uint(encoderInternal, this->totalADULength);
    }

    //if CRC is required we need to ecode this into the block
    if (this->crcType != CRC_TYPE_NOCRC) {
        //the CRC field must be filled with 0 before calculating the CRC
        cbor_encode_byte_string(encoderInternal, zeros, this->crcSize);
    }

    cbor_encoder_close_container(&encoderExternal, encoderInternal);

    size_t cborSizePrimary =
        cbor_encoder_get_buffer_size(&encoderExternal, buf);

    *cbor = new uint8_t[cborSizePrimary];
    memcpy(*cbor, buf, cborSizePrimary);

    cborSize = cborSizePrimary;
    ESP_LOGD("primaryToCbor", "primary block cborSize:%u", cborSizePrimary);

    //calculate and insert CRC into CBOR
    if (this->crcType == CRC_TYPE_X25) {
        uint16_t crc = calculateCRC(this->crcType, *cbor, cborSizePrimary);
        (*cbor)[cborSizePrimary - 1] = crc & 0xFF;
        (*cbor)[cborSizePrimary - 2] = (crc >> 8) & 0xFF;
    }
    if (this->crcType == CRC_TYPE_CRC32C) {
        uint32_t crc = calculateCRC(this->crcType, *cbor, cborSizePrimary);
        (*cbor)[cborSizePrimary - 1] = crc & 0xFF;
        (*cbor)[cborSizePrimary - 2] = (crc >> 8) & 0xFF;
        (*cbor)[cborSizePrimary - 3] = (crc >> 16) & 0xFF;
        (*cbor)[cborSizePrimary - 4] = (crc >> 24) & 0xFF;
    }
    delete[] buffer;
    delete encoderInternal;
    return;
}

uint32_t calculateCRC(uint8_t crcType, const uint8_t* data, size_t dataSize) {
    uint32_t result = 0;
    if (crcType == CRC_TYPE_X25) {
        ESP_LOGD("CRC calculation", "CRC Type: %u", crcType);
        //using the little endian version of the crc16 function, as this seems to be consistent with the expected behavior for x25 crc
        result = (uint32_t)esp_rom_crc16_le(0, data, dataSize);
    }
    else if (crcType == CRC_TYPE_CRC32C) {
        ESP_LOGD("CRC calculation", "CRC Type: %u", crcType);
        result = crc32c(0, data, dataSize);
    }
    else {
        ESP_LOGE("CRC calculation", "Invalid CRC Type: %u", crcType);
    }
    ESP_LOGD("CRC calculation", "Calculated CRC: %lu", result);
    return result;
}

bool checkCRC(uint8_t crcType, const uint8_t* data, size_t dataSize) {
    //create a copy of the data
    uint8_t* dataWithoutCRC = new uint8_t[dataSize];
    memcpy(dataWithoutCRC, data, dataSize);

    uint32_t includedCRC = 0;

    //copy the CRC into a single uint32_t and replace it with 0 in the data
    switch (crcType) {
        case CRC_TYPE_X25:
            includedCRC = data[dataSize - 1];
            includedCRC |= (data[dataSize - 2]) << 8;
            dataWithoutCRC[dataSize - 1] = 0;
            dataWithoutCRC[dataSize - 2] = 0;
            break;

        case CRC_TYPE_CRC32C:
            includedCRC = data[dataSize - 1];
            includedCRC |= (data[dataSize - 2]) << 8;
            includedCRC |= (data[dataSize - 3]) << 16;
            includedCRC |= (data[dataSize - 4]) << 24;
            dataWithoutCRC[dataSize - 1] = 0;
            dataWithoutCRC[dataSize - 2] = 0;
            dataWithoutCRC[dataSize - 3] = 0;
            dataWithoutCRC[dataSize - 4] = 0;
            break;
        default:
            ESP_LOGE("CRC check", "unsupported CRC type");
            break;
    }

    //calculate the CRC on the input data, with its original CRC replaced with 0
    uint32_t calcuatedCRC = calculateCRC(crcType, dataWithoutCRC, dataSize);

    //check if the calculated CRC matches the expected CRC
    bool passed = includedCRC == calcuatedCRC;

    if (passed) {
        ESP_LOGD("CRC check", "CRC OK!");
    }
    else {
        ESP_LOGW("CRC check", "CRC mismatch");
    }
    return passed;
}
