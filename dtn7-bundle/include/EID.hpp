#pragma once
#include <string>
#include "cbor.h"
#include "esp_log.h"

#define URI_SCHEME_DTN_NAME "dtn:"
#define URI_SCHEME_DTN_ENCODED 1
#define URI_SCHEME_IPN_NAME "ipn:"
#define URI_SCHEME_IPN_ENCODED 2

#define NONE_ENDPOINT_SPECIFIC_PART_NAME "none"
#define NONE_ENDPOINT_SPECIFIC_PART_ENCODED 0

/**
 * @file EID.hpp
 * @brief This file contains the implementation of the EID struct
*/

/// @brief Struct representing the EID
struct EID {
    /// @brief scheme code used by the EID
    uint64_t schemeCode;

    /// @brief the SchemeSpecificPart of the EID
    char* SSP;

    /// @brief stores whether the EID is the none endpoint specific name
    bool isNone;

    /// @brief indicates whether the EID is valid
    bool valid;

    /// @brief stores the size of the SSP
    size_t sspSize;

    /// @brief prints the EID to the log
    void print();

    /// @brief converts the EID object to a URI
    /// @return std::string containing the URI
    std::string getURI();

    /// @brief generates an EID object from a std::String containing a URI. Do NOT pass a IPN URI which is not INT.INT, this will lead to runtime abort
    /// @param URI std::String containing a URI
    /// @return EID representing th given URI
    static EID fromUri(std::string URI);

    /// @brief encodes the EID to cbor with the given encoder
    /// @param encoder the encoder which is used to encode the block, its buffer will contain the resulting cbor
    void toCbor(CborEncoder* encoder);

    /// @brief Creates an EID from a given cbor parser
    /// @param value the cbor parsers value object, it has to point to an valid cbor array with fixed Length
    /// @return the EID encoded in the given cbor value
    static EID fromCbor(CborValue* value);

    /// @brief constructs an EID with DTN scheme from a char array
    /// @param dtn_scheme_code must be URI_SCHEME_DTN_ENCODED
    /// @param string char array containing the scheme specific part of the DTN EID
    /// @param length length of the scheme specific part of the DTN EID
    EID(uint64_t dtn_scheme_code, const char* string, int length);

    /// @brief constructs an EID with IPN scheme from two uint64_t
    /// @param dtn_scheme_code must be URI_SCHEME_IPN_ENCODED
    /// @param node node of the IPN endpoint
    /// @param service service of the IPN endpoint
    EID(uint64_t dtn_scheme_code, uint64_t node, uint64_t service);

    /// @brief constructs an empty EID
    EID() {
        SSP = nullptr;
        sspSize = 0;
        isNone = true;
        schemeCode = 0;
        valid = false;
    }

    ~EID() { delete[] SSP; }

    /// @brief EID copy constructor
    /// @param old
    EID(const EID& old) {
        // ESP_LOGI("Eid Copy Constructor", "called");
        SSP = new char[old.sspSize];
        sspSize = old.sspSize;
        isNone = old.isNone;
        schemeCode = old.schemeCode;
        valid = old.valid;
        memcpy(SSP, old.SSP, old.sspSize);
    }

    /// @brief EID operator=
    /// @param old
    /// @return
    EID& operator=(const EID& old) {
        if (this == &old)
            return *this;
        // ESP_LOGI("Eid operator = ", "called");
        delete[] SSP;
        SSP = new char[old.sspSize];
        sspSize = old.sspSize;
        isNone = old.isNone;
        schemeCode = old.schemeCode;
        valid = old.valid;
        memcpy(SSP, old.SSP, old.sspSize);
        return *this;
    }
};