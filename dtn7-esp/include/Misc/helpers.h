#pragma once
#include <set>
#include <string>
#include <vector>
#include "Data.hpp"
#include "EID.hpp"
#include "cbor.h"

/**
 * @file helpers.h
 * @brief Provides helpers used for serializing BundleInfo and Node, a Hash function wrapper Class used if BPoL with BundleID hashes in advertisements is enabled
 */

/// @brief reads an std::string from a CborValue containing a CBOR Text string. The given CborValue is advanced to the next element after the String
/// @param value the CborValue to read string from. Will NOT be advanced to next element after the string, as this leads to decoding errors if the next element is a CBOR primitive type
/// @return the String Stored in the CborValue, or a string containing "error" if no string was stored
inline std::string stringFromCbor(CborValue* value) {
    // check if passed value is a CBOR text string
    if (cbor_value_is_text_string(value)) {
        // get Length of string and create char array of corresponding size
        size_t idLength = 0;
        cbor_value_calculate_string_length(value, &idLength);
        char idChars[idLength];

        // copy string into char array
        cbor_value_copy_text_string(value, idChars, &idLength, value);

        // return read string
        return std::string(idChars, idLength);
    }
    // if given CborValue was no text string, return "error"
    return std::string("error");
}

/// @brief reads an array of EIDs from a CBOR value, if it contains an array of EIDs in their standard CBOR encoding. Such an array can be created with encodeEidArray. The given CborValue is advanced to the next element after the Array of EIDs.
/// @param value the CborValue to read EIDs from. Must point to an CBOR Array containing EIDs! Will be advanced to next element after the array.
/// @return a Vector of EIDs, empty if none were decoded
inline std::vector<EID> decodeEidArray(CborValue* value) {
    // create vector for result
    std::vector<EID> result;

    // check weather CborValue points to array
    if (cbor_value_is_array(value)) {
        // calculate number of elements(EIDs) in the CBOR array
        size_t numElems = 0;
        cbor_value_get_array_length(value, &numElems);

        // enter the CBOR array
        CborValue ArrayValue;
        cbor_value_enter_container(value, &ArrayValue);

        // add all EIDs in the Array to the result vector
        for (int i = 0; i < numElems; i++) {
            result.push_back(EID::fromCbor(&ArrayValue));
        }
        // For some reason it is not necessary to leave the CBOR array, uncommenting the following line may lead to errors
        // cbor_value_leave_container(value ,&ArrayValue);
    }
    // return vector of EIDs
    return result;
}

/// @brief encodes a given std::string onto a given CBOR Encoder
/// @param encoder CborEncoder to use
/// @param toEncode string which shall be CBOR encoded
inline void stringToCbor(CborEncoder* encoder, std::string toEncode) {
    // get char array from std string
    const char* string = toEncode.c_str();

    // encode char array as CBOR text string
    cbor_encode_text_string(encoder, string, toEncode.size());
    return;
}

/// @brief encodes all EIDs contained in the given vector into an CBOR array on the given CBOR Encoder,
///  the CBOR encoding of the individual EIDs is as described in RFC9171. The resulting array can be decoded with decodeEidArray.
/// @param encoder encoder to be used
/// @param Eids EIDs to be encoded
inline void encodeEidArray(CborEncoder* encoder, std::vector<EID>& Eids) {
    // create local CBOR encoder for use in the array
    CborEncoder encoderInternal;

    // create CBOR array using the passed CBOR encoder
    cbor_encoder_create_array(encoder, &encoderInternal, Eids.size());

    // add all EIDs to the created array
    for (EID eid : Eids) {
        eid.toCbor(&encoderInternal);
    }

    // close the CBOR array
    cbor_encoder_close_container(encoder, &encoderInternal);
    return;
}

/// @brief encode all Nodes of the given Node Vector into a CBOR array of Byte Strings, the byte strings are equivalent to the CBOR encoding of the node object.
/// The resulting CBOR can be decoded with decodeNodeArray.
/// @param encoder the encoder to be used
/// @param nodes the nodes to encode in a vector
inline void encodeNodeArray(CborEncoder* encoder, std::vector<Node>& nodes) {
    // create local CBOR encoder for use in the array
    CborEncoder encoderInternal;

    // create CBOR array using the passed CBOR encoder
    cbor_encoder_create_array(encoder, &encoderInternal, nodes.size());

    // add all nodes to the created array
    for (Node n : nodes) {
        // serialize the node into a vector
        std::vector<uint8_t> bytes = n.serialize();

        // encode serialized node as CBOR byte string
        cbor_encode_byte_string(&encoderInternal, &bytes[0], bytes.size());
    }

    // close the CBOR array
    cbor_encoder_close_container(encoder, &encoderInternal);
    return;
}

/// @brief reads an array of Nodes from a CBOR value, if it contains a CBOR array of byte strings, as created by encodeNodeArray.
/// The Stored nodes are returned in a vector. The given CborValue is advanced to the next element after the Array of Nodes.
/// @param value The CborValue to read Nodes from. Must point to an CBOR Array containing serialized Nodes! Will be advanced to next element after the array.
/// @return the decoded nodes in a vector, or an empty vector if none were encoded.
inline std::vector<Node> decodeNodeArray(CborValue* value) {
    // create result vector
    std::vector<Node> result;

    // check whether given CBOR value is array
    if (cbor_value_is_array(value)) {
        // calculate size of array
        size_t numElems = 0;
        cbor_value_get_array_length(value, &numElems);

        // enter the CBOR array
        CborValue ArrayValue;
        cbor_value_enter_container(value, &ArrayValue);

        // read all nodes from array
        for (int i = 0; i < numElems; i++) {
            // check whether array element is CBOR byte string
            if (cbor_value_is_byte_string(&ArrayValue)) {
                // calculate length of byte string and create array of appropriate size
                size_t cborLength = 0;
                cbor_value_calculate_string_length(&ArrayValue, &cborLength);
                uint8_t cbor[cborLength];

                // copy CBOR byte string to array
                cbor_value_copy_byte_string(&ArrayValue, cbor, &cborLength,
                                            &ArrayValue);

                // decode node object from bytes and add to result
                result.push_back(
                    Node(std::vector<uint8_t>(cbor, cbor + cborLength)));
            }
        }

        // leave array
        cbor_value_leave_container(value, &ArrayValue);
    }
    return result;
}

/// @brief encodes the given Bundle into a CBOR Byte String, the byte string is equivalent to the CBOR encoding of the Bundle, as described in RFC9171.
/// Can be decoded using decodeBundle.
/// @param encoder the encoder onto which the Bundle is to be encoded
/// @param bundle the Bundle to be encoded
inline void encodeBundle(CborEncoder* encoder, Bundle* bundle) {
    // initialize empty byte string
    uint8_t* cbor = nullptr;
    size_t cborSize = 0;

    // generate CBOR representation of Bundle
    bundle->toCbor(&cbor, cborSize);

    // encode CBOR representation of bundle onto CBOR encoder
    cbor_encode_byte_string(encoder, cbor, cborSize);

    // clean up cbor array
    delete[] cbor;
    return;
}

/// @brief decodes a Bundle from a CBOR Byte String pointed to by the given CborValue, as created by encodeBundle.
/// @param value  CBOR value to read from
/// @return the Bundle Read from the Cbor Value, or an empty Bundle if reading was unsuccessful.
inline Bundle* decodeBundle(CborValue* value) {
    // check weather given CBOR value is byte string
    if (cbor_value_is_byte_string(value)) {
        // calculate length of byte string
        size_t cborLength = 0;
        cbor_value_calculate_string_length(value, &cborLength);

        // create appropriate size array
        uint8_t cbor[cborLength];

        // copy byte string from CBOR
        cbor_value_copy_byte_string(value, cbor, &cborLength, value);

        // decode and return Bundle
        return Bundle::fromCbor(cbor, cborLength);
    }

    // retrun empty bundle
    else
        return new Bundle();
}

/// @brief encodes a set of Hashes into a CBOR Array, can be decoded using decodeHashesSet.
/// @param encoder the encoder onto which the hashes are to be written
/// @param hashes the hashes to encode
inline void encodeHashesSet(CborEncoder* encoder, std::set<size_t>& hashes) {
    // create local CBOR encoder for use in the array
    CborEncoder encoderInternal;

    // create CBOR array
    cbor_encoder_create_array(encoder, &encoderInternal, hashes.size());

    // encode hashes
    for (size_t hash : hashes) {
        cbor_encode_uint(&encoderInternal, hash);
    }
    cbor_encoder_close_container(encoder, &encoderInternal);
    return;
}

/// @brief decodes a set of Hashes from CBOR Array pointed to by the given CborValue. Such a CBOR array can be created using encodeHashesSet.
/// @param value CBOR value to read from
/// @return the set of Hashes encode in the CBOR Array
inline std::set<size_t> decodeHashesSet(CborValue* value) {
    // create result set
    std::set<size_t> result;

    // check whether CBOR value points to array
    if (cbor_value_is_array(value)) {
        // calculate array size
        size_t numElems = 0;
        cbor_value_get_array_length(value, &numElems);

        // enter CBOR array
        CborValue ArrayValue;
        cbor_value_enter_container(value, &ArrayValue);

        // decode elements from array
        for (int i = 0; i < numElems; i++) {
            // check whether array element is integer and add to result
            if (cbor_value_is_unsigned_integer(&ArrayValue)) {
                uint64_t hash = 0;
                cbor_value_get_uint64(&ArrayValue, &hash);
                result.insert((size_t)hash);
            }
        }
        // For some reason it is not necessary to leave the CBOR array, uncommenting the following line may lead to errors
        // cbor_value_leave_container(value ,&ArrayValue);
    }
    return result;
}

/// @brief base class for different hash functions to use in BPoL advertisements, to use a custom hash function a Class derived from this one must be provided and instantiated in dtn7-esp::setupClasses
class HashWrapper {
   public:
    /// @brief calculates the hash for the given string
    /// @param string string to calculated hash for
    /// @return resulting hash
    virtual size_t hash(std::string string) = 0;
};

/// @brief This Class is just a Wrapper around std::hash
class StdHasher : public HashWrapper {
   public:
    /// @brief this function just calls std::hash for the given string
    /// @param string string to calculated hash for
    /// @return resulting hash
    size_t hash(std::string string) override {
        return std::hash<std::string>{}(string);
    }
};