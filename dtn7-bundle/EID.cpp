#include "EID.hpp"
#include <sstream>
#include "cbor.h"
#include "esp_log.h"

void EID::print() {
    ESP_LOGI("EID Print", "Valid: %s, SchemeCode:%llu",
             valid ? "true" : "false", schemeCode);
    switch (schemeCode) {
        // detect the URI scheme and print accordingly
        case URI_SCHEME_DTN_ENCODED:
            if (sspSize == 0) {
                // check whether is null endpoint
                ESP_LOGI("EID Print", "Endpoint: %s",
                         NONE_ENDPOINT_SPECIFIC_PART_NAME);
            }
            else {
                ESP_LOGI("EID Print", "Endpoint: %.*s", sspSize, SSP);
            }
            break;
        case URI_SCHEME_IPN_ENCODED:
            uint64_t node;
            uint64_t service;
            memcpy(&node, SSP, sizeof(uint64_t));
            memcpy(&service, SSP + sizeof(uint64_t), sizeof(uint64_t));
            ESP_LOGI("EID Print", "Endpoint: %llu.%llu", node, service);
            break;
        default:
            break;
    }
    return;
}

std::string EID::getURI() {
    switch (schemeCode) {
        // detect the URI scheme and decode accordingly
        case URI_SCHEME_DTN_ENCODED:
            // check if is null endpoint
            if (sspSize == 0)
                return std::string("dtn:").append(
                    NONE_ENDPOINT_SPECIFIC_PART_NAME);
            return std::string("dtn:").append(SSP, sspSize);
        case URI_SCHEME_IPN_ENCODED:
            uint64_t node;
            uint64_t service;
            memcpy(&node, SSP, sizeof(uint64_t));
            memcpy(&service, SSP + sizeof(uint64_t), sizeof(uint64_t));
            return std::string("ipn:")
                .append(std::to_string(node))
                .append(".")
                .append(std::to_string(service));
        default:
            return std::string("Invalid EID");
    }
}

EID EID::fromUri(std::string URI) {
    std::string scheme = URI.substr(0, 4);

    if (scheme == URI_SCHEME_DTN_NAME) {
        std::string SSPLocal = URI.substr(4);
        uint64_t sspLength = SSPLocal.size();
        if (SSPLocal.substr(0, 6) == NONE_ENDPOINT_SPECIFIC_PART_NAME)
            sspLength = 0;
        // ESP_LOGI("EID fromURI", "ssp Length: %llu, SSP: %s", sspLength, SSPLocal.c_str());
        return EID(URI_SCHEME_DTN_ENCODED, SSPLocal.c_str(), sspLength);
    }
    else if (scheme == URI_SCHEME_IPN_NAME) {
        std::string SSPLocal = URI.substr(4);
        std::istringstream SSPstream(SSPLocal);
        std::string part;
        uint64_t node = 0;
        uint64_t service = 0;

        if (std::getline(SSPstream, part, '.'))
            node = std::stoull(part);

        if (std::getline(SSPstream, part, '.'))
            service = std::stoull(part);
        return EID(URI_SCHEME_IPN_ENCODED, node, service);
    }
    else {
        ESP_LOGE("EID fromURI", "Invalid URI");
        return EID();
    }
}

void EID::toCbor(CborEncoder* encoder) {
    CborEncoder encoderInternal;
    // string and array each have up to 7 bytes overhead + size of actual data (1*int + sspSize bytes)
    uint8_t* buf = new uint8_t[20 + sizeof(SSP)];
    cbor_encoder_init(&encoderInternal, buf, sizeof(buf), 0);
    cbor_encoder_create_array(
        encoder, &encoderInternal,
        2);  // rcf9171: Each BP endpoint ID (EID) be represented as a cbor array comprising two items.
    cbor_encode_uint(&encoderInternal, schemeCode);

    if (isNone) {
        cbor_encode_uint(&encoderInternal, 0);
    }
    else {
        if (schemeCode == URI_SCHEME_DTN_ENCODED) {
            cbor_encode_text_string(&encoderInternal, SSP, sspSize);
        }
        else {
            CborEncoder encoderInternal2;
            uint8_t* buf2 = new uint8_t
                [19];  // array has up to 7 bytes overhead + size of actual data(2*int)
            cbor_encoder_init(&encoderInternal2, buf2, sizeof(buf2), 0);
            cbor_encoder_create_array(
                &encoderInternal, &encoderInternal2,
                2);  // rcf9171: ipn scheme be represented as a cbor array comprising two items
            uint64_t value0;
            uint64_t value1;
            memcpy(&value0, SSP, sizeof(uint64_t));
            cbor_encode_uint(&encoderInternal2, value0);
            memcpy(&value1, SSP + sizeof(uint64_t), sizeof(uint64_t));
            cbor_encode_uint(&encoderInternal2, value1);
            cbor_encoder_close_container_checked(&encoderInternal,
                                                 &encoderInternal2);
            delete[] buf2;
        }
    }
    cbor_encoder_close_container(encoder, &encoderInternal);
    delete[] buf;
    return;
}

EID EID::fromCbor(CborValue* value) {
    EID result;
    result.valid = true;

    if (cbor_value_is_array(value)) {
        if (cbor_value_validate_basic(value) != CborNoError) {
            ESP_LOGE("EIDFromcbor", "cbor malformed");
            result.valid = false;
            return result;
        }
        CborValue ArrayValue;
        cbor_value_enter_container(
            value, &ArrayValue);  // enter the array containing the EID
        uint64_t scheme = 0;
        // the first value of the array must be a uint and denotes the scheme of the EID
        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue, &scheme);  // read value
            cbor_value_advance(&ArrayValue);              // move to next value
        }
        else {
            ESP_LOGE("EIDFromcbor", "Invalid cbor");
            result.valid = false;
            return result;
        }

        // depending on the scheme a different EID object is created
        switch (scheme) {
            case URI_SCHEME_DTN_ENCODED: {

                size_t length = 0;
                char* string = nullptr;
                if (cbor_value_is_unsigned_integer(
                        &ArrayValue)) {  // second element is only int if endpoint name is non specific
                    length = 0;
                    // printf("%02X \n", *cbor_value_get_next_byte(&ArrayValue));
                    cbor_value_advance(&ArrayValue);  // move to next value
                }
                if (cbor_value_is_text_string(&ArrayValue)) {
                    cbor_value_get_string_length(&ArrayValue, &length);
                    // ESP_LOGI("EIDFromcbor", "SSP size in cbor: %u", length);
                    string = new char[length + 1];
                    cbor_value_copy_text_string(&ArrayValue, string, &length,
                                                &ArrayValue);
                    // cbor_value_advance(&ArrayValue);// move to next value
                }
                else {
                    result.valid = false;
                    ESP_LOGE("EIDFromcbor",
                             "Invalid cbor, SSP not Integer nor Text String");
                    return result;
                }
                result = EID(scheme, string, length);
                if (string != nullptr)
                    delete[] string;
                break;
            }

            case URI_SCHEME_IPN_ENCODED: {
                uint64_t node = 0;
                uint64_t service = 0;
                if (cbor_value_is_array(&ArrayValue)) {
                    if (cbor_value_validate_basic(&ArrayValue) != CborNoError) {
                        ESP_LOGE("EIDFromcbor", "cbor malformed");
                        result.valid = false;
                        return result;
                    }
                    CborValue ArrayValue2;
                    cbor_value_enter_container(
                        &ArrayValue,
                        &ArrayValue2);  // enter Array containing SSP
                    cbor_value_get_uint64(&ArrayValue2, &node);  // read value
                    cbor_value_advance(&ArrayValue2);  // move to next value
                    cbor_value_get_uint64(&ArrayValue2,
                                          &service);   // read value
                    cbor_value_advance(&ArrayValue2);  // move to next value
                    cbor_value_leave_container(
                        &ArrayValue,
                        &ArrayValue2);  // leave Array Containing SSP
                }
                result = EID(scheme, node, service);
                break;
            }

            default: {
                ESP_LOGE("EIDFromcbor", "Unknown URI SCheme");
                result.valid = false;
                return result;
            }
        }

        // leave array containing EID
        cbor_value_leave_container(value, &ArrayValue);
    }
    else {
        ESP_LOGE("EIDFromcbor", "Invalid cbor, not an Array");
        result.valid = false;
    }

    return result;
}

EID::EID(uint64_t dtn_scheme_code, const char* string, int length) {
    if (dtn_scheme_code != URI_SCHEME_DTN_ENCODED)
        ESP_LOGE("Eid Creation", "Wrong scheme code");

    schemeCode = dtn_scheme_code;
    sspSize = length;
    SSP = new char[length];

    if (length == 0 && URI_SCHEME_DTN_ENCODED) {
        isNone = true;
    }
    else {
        isNone = false;
    }

    if (dtn_scheme_code == URI_SCHEME_DTN_ENCODED) {
        strncpy(SSP, string, length);  // copy string to ssp
    }
    valid = true;
}

EID::EID(uint64_t dtn_scheme_code, uint64_t node, uint64_t service) {
    if (dtn_scheme_code != URI_SCHEME_IPN_ENCODED)
        ESP_LOGE("Eid Creation", "Wrong scheme code");

    schemeCode = dtn_scheme_code;
    sspSize = 2 * sizeof(uint64_t);
    isNone = false;
    SSP = new char[2 * sizeof(uint64_t)];
    if (dtn_scheme_code == URI_SCHEME_IPN_ENCODED) {
        memcpy(SSP, &node, sizeof(uint64_t));
        memcpy(SSP + sizeof(uint64_t), &service, sizeof(uint64_t));
    }
    valid = true;
}