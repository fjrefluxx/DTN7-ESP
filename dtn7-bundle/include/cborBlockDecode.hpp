#pragma once
#include "Block.hpp"
#include <vector>
#include "cbor.h"
#include "esp_log.h"
#include "utils.hpp"
#include "esp_heap_caps.h"

/**
 * @file cborBlockDecode.h
 * @brief This file contains the implementation of CBOR decoding for individual Blocks of a Bundle
*/


/// @brief Creates a Canonical Block from a given Cbor parser
/// @param value the Cbor Parsers value object, it has to point to an valid Cbor Array with fixed Length
/// @param size the size of the Cbor array Pointed to by the value Object
/// @return the Canonical Block encoded in the given CBOR value
CanonicalBlock fromCborCanonical(CborValue * valueExt, size_t size)
{   
    bool valid = true;
    CanonicalBlock result;
    uint64_t type;
    uint64_t number;
    uint64_t flags;
    uint64_t crcType;
    uint8_t* data = nullptr;
    size_t dataSize;
    uint8_t* CRClocal = nullptr;
    size_t crcSize;
    CborValue value;

    //store internal pointer to the data in the CBOR value
    const uint8_t * startIndex= valueExt->source.ptr;

    // we expect a fixed structure, an array with 5, or 6 if crc is present
    if (!cbor_value_is_array(valueExt)) {
        valid = false;
    } else {
        if (cbor_value_validate_basic(valueExt)!= CborNoError) {
            ESP_LOGE("CanonicalBlockFromCbor", "CBOR malformed");
            valid = false;
            return result;
        }
        cbor_value_enter_container(valueExt, &value); // enter the array describing the block
        
        // first 4 elements must be unsigned int, they represent: Version, BundleProcessing ControlFlags, CRCType
        if (cbor_value_is_unsigned_integer(&value)&&valid) {
            cbor_value_get_uint64(&value, &type); // read value
            cbor_value_advance(&value); // move to next value
        } else {
            ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
            valid = false;
            result.valid = false;
            return result;
        }

        if (cbor_value_is_unsigned_integer(&value)&&valid) {
            cbor_value_get_uint64(&value, &number); // read value
            cbor_value_advance(&value); // move to next value
        } else {
            valid = false;
            ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
            result.valid = false;
            return result;
        }

        if (cbor_value_is_unsigned_integer(&value)&&valid) {
            cbor_value_get_uint64(&value, &flags); // read value
            cbor_value_advance(&value); // move to next value
            // ESP_LOGI("CanonicalBlockFromCbor", "Flags:%llu", flags);
        } else {
            ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
            valid = false;
            result.valid = false;
            return result;
        }

        if (cbor_value_is_unsigned_integer(&value) && valid) {
            cbor_value_get_uint64(&value, &crcType); // read value
            cbor_value_advance(&value); // move to next value
            // ESP_LOGI("CanonicalBlockFromCbor", "crcType:%llu", crcType);
            if (crcType != CRC_TYPE_NOCRC) {
                ESP_LOGD("CanonicalBlockFromCbor", "Detected CRC, type: %llu", crcType); 
            }
        }
        else {
            ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
            valid = false;
            result.valid = false;
            return result;
        }

        // Next block type: specific data, as a CBOR byte string
        if (cbor_value_is_byte_string(&value) && valid) {
            cbor_value_get_string_length(&value, &dataSize);
            data = new uint8_t[dataSize];
            cbor_value_copy_byte_string(&value, data, &dataSize, &value);
        } else {
            ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
            valid = false;
            result.valid = false;
            return result;
        }
            
        // lastly, if CRC exists read CRC
        switch(crcType) {
            case 0:
                // no CRC
                CRClocal = nullptr;
                break;
            case CRC_TYPE_X25:
                // 2 Byte CRC
                CRClocal = new uint8_t[2];
                if (cbor_value_is_byte_string(&value) && valid) {
                    crcSize = 2;
                    cbor_value_copy_byte_string(&value, CRClocal, &crcSize, &value);
                } else {
                    ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor"); 
                    valid = false;
                    result.valid = false;
                    return result;
                    break;
                }
                break;
            case CRC_TYPE_CRC32C:
                // 4 Byte CRC
                CRClocal = new uint8_t[4];
                if (cbor_value_is_byte_string(&value) && valid) {
                    crcSize = 4;
                    cbor_value_copy_byte_string(&value, CRClocal, &crcSize, &value);
                } else {
                    ESP_LOGE("CanonicalBlockFromCbor", "Invalid cbor");
                    valid = false;
                    result.valid = false;
                    return result;
                    break;
                }
                break;
        }
    }

    if (valid) {
        cbor_value_leave_container(valueExt, &value); // leave array containing creation timestamp
        result = CanonicalBlock(type, number, dataSize, flags, data, crcType);
    } else {
        result = CanonicalBlock();
    }

    //if CRC is present, and the block is otherwise valid, we now check CRC
    if(valid && crcType != CRC_TYPE_NOCRC) {

        //using the internal pointers of the CBOR value, we can access the raw input data in order to check its CRC 
        const uint8_t * endIndex= valueExt->source.ptr;
        size_t length = endIndex-startIndex;

        //if the CRC check failed, we mark the block as invalid
        if(!checkCRC(crcType,startIndex,length)) valid = false;
    }

    result.valid = valid;


    if (data!= nullptr) delete[]data;
    if (CRClocal!= nullptr) delete[]CRClocal;

    return result;
}

/// @brief Creates a primary block from a given cbor parser
/// @param valueExt the cbor parsers value object, points to a cbor array with fixed length
/// @param size the size of the cbor array 
/// @return the primary block encoded in the given CBOR value
PrimaryBlock fromCborPrimary(CborValue *valueExt, size_t size) {    
    /*
        length of array:
            8 if the bundle is not a fragment and has no CRC
            9 if the bundle is not a fragment and has a CRC
            10 if the bundle is a fragment and has no CRC
            11 if the bundle is a fragment and has a CRC
    */
    bool valid = true;
    uint64_t version = 0;
    uint64_t flags = 0;
    uint64_t crcType = 0;
    EID Dest;
    EID Source;
    EID ReportTo;
    CreationTimestamp time;
    uint64_t life = 0;
    uint64_t fragment = 0;
    uint64_t TotalADU = 0;
    uint8_t* CRClocal = nullptr;
    CborValue value;

    //store internal pointer to the data in the CBOR value
    const uint8_t * startIndex= valueExt->source.ptr;

    // we expect a fixed cbor structure
    if (!cbor_value_is_array(valueExt)) {
        valid = false; 
    } else {
        cbor_value_enter_container(valueExt, &value); // enter the array describing the block
    }




    // step through each element
    // first 3 elements must be unsigned int, they represent: Version, BundleProcessing ControlFlags, CRCType
    if (cbor_value_is_unsigned_integer(&value) && valid) {
        cbor_value_get_uint64(&value, &version); // read value
        cbor_value_advance(&value); // move to next value
        if (version!= 7) {
            ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, only Version 7 supported");
            valid = false;
            PrimaryBlock result;
            result.valid = false;
            return result;
        }
    } else {
        ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, first value not int");
        valid = false;
        PrimaryBlock result;
        result.valid = false;
        return result;
    }

    if (cbor_value_is_unsigned_integer(&value) && valid) {
        cbor_value_get_uint64(&value, &flags); // read value        
        cbor_value_advance(&value); // move to next value
    } else {
        ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor second value not int");
        PrimaryBlock result;
        result.valid = false;
        return result;
    }

    if (cbor_value_is_unsigned_integer(&value) && valid) {
        cbor_value_get_uint64(&value, &crcType); // read value
        cbor_value_advance(&value); // move to next value
        if (crcType != CRC_TYPE_NOCRC) {
            ESP_LOGD("PrimaryBlockFromCbor", "Block has CRC, type: %llu", crcType);
        }
    } else {
        ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor third value not int");
        valid = false;
        PrimaryBlock result;
        result.valid = false;
        return result;
    }

    // next 3 elements must be cbor arrays with length 2, each representing an EID
    // first for Destination EID
    if (valid) Dest = EID::fromCbor(&value);
    valid = Dest.valid;
    // Second for the Source EID
    if (valid) Source = EID::fromCbor(&value);
    valid = Source.valid;
    // third for report to EID
    if (valid) ReportTo = EID::fromCbor(&value);
    valid = ReportTo.valid;

    // creation timestamp. Array with 2 elements
    if (cbor_value_is_array(&value) && valid) {
        CborValue ArrayValue;
        cbor_value_enter_container(&value, &ArrayValue); // enter the array containing creation timestamp
        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue, &time.creationTime); // read creation time
            cbor_value_advance(&ArrayValue); // move to next value
        } else {
            ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, creation timestamp first value not int");
            valid = false;
            PrimaryBlock result;
            result.valid = false;
            return result;
        }

        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue, &time.sequenceNumber); // read sequence number
            cbor_value_advance(&ArrayValue); // move to next value
        } else {
            ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, creation timestamp second value not int");
            valid = false;
            PrimaryBlock result;
            result.valid = false;
            return result;
        }
        cbor_value_leave_container(&value, &ArrayValue); // leave array containing creation timestamp
    } else {
        ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, creation timestamp not array");
        valid = false;
        PrimaryBlock result;
        result.valid = false;
        return result;
    }

    // lifetime, represented as an unsigned Integer
    if (cbor_value_is_unsigned_integer(&value) && valid) {
        cbor_value_get_uint64(&value, &life); // read life time
        cbor_value_advance(&value); // move to next value
    } else {
        ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, lifetime not int");
        valid = false;
        PrimaryBlock result;
        result.valid = false;
        return result;
    }

    // fragment offset (fragmentation is unsupported, but fragmented bundles can be received)
    if ((size == 10 || size == 11) && valid) { // check whether it is possible to have fragment
        if (cbor_value_is_unsigned_integer(&value) && valid) {
            cbor_value_get_uint64(&value, &fragment); // read life time
            cbor_value_advance(&value); // move to next value
        } else {
            ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor fragment offset not int"); 
            valid = false;
            PrimaryBlock result;
            result.valid = false;
            return result;
        }
    
        // next TotalADU length
        if (cbor_value_is_unsigned_integer(&value) && valid) {
            cbor_value_get_uint64(&value, &TotalADU); // read life time
            cbor_value_advance(&value); // move to next value
        } else {
            ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor total ADU length not int"); 
            valid = false;
            PrimaryBlock result;
            result.valid = false;
            return result;
        }
    }
    size_t crcSize = 0;

    // last CRC (CRC is unsupported, but is decoded correctly if present)
    if ((size == 9 || size == 11) && valid) { // check whether it is possible to CRC
        switch(crcType) {
        case 0: // no crc
            delete[] CRClocal;
            CRClocal = nullptr;
            break;
        case CRC_TYPE_X25: // 2 Byte CRC
            delete[] CRClocal;
            CRClocal = new uint8_t[2];

            if (cbor_value_is_byte_string(&value)&&valid) {
                crcSize = 2;
                cbor_value_copy_byte_string(&value, CRClocal, &crcSize, &value);                
            } else {
                ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor, CRC not byte String");
                valid = false;
                PrimaryBlock result;
                result.valid = false;
                return result;
                }
            break;
        case CRC_TYPE_CRC32C://  4 Byte CRC
            delete[] CRClocal;
            CRClocal = new uint8_t[4];
            if (cbor_value_is_byte_string(&value)&&valid) {
                crcSize = 4;
                cbor_value_copy_byte_string(&value, CRClocal, &crcSize, &value);              
            } else {
                ESP_LOGE("PrimaryBlockFromCbor", "Invalid cbor CRC not byte String"); 
                valid = false;
                PrimaryBlock result;
                result.valid = false;
                return result;
            }
            break;
        }
    }

    //exit the CBOR array
    cbor_value_leave_container(valueExt, &value);

    //if CRC is present, and the block is otherwise valid, we now check CRC
    if(valid && crcType != CRC_TYPE_NOCRC) {

        //using the internal pointers of the CBOR value, we can access the raw input data in order to check its CRC 
        const uint8_t * endIndex= valueExt->source.ptr;
        size_t length = endIndex-startIndex;

        //if the CRC check failed, we mark the block as invalid
        if(!checkCRC(crcType,startIndex,length)) valid = false;
    }
    
    // all values have ben gathered, generate primary block Object
    PrimaryBlock result(version, flags, crcType, Dest, Source, ReportTo, time, life, fragment, TotalADU, CRClocal, crcSize);
    result.valid = valid;
    delete[] CRClocal;
    
    return result;
}