#include "Data.hpp"
#include "cbor.h"
#include "esp_log.h"
#include "helpers.h"

BundleInfo::BundleInfo(std::vector<uint8_t> serialized) {
    // write to debug log
    ESP_LOGD("BundleInfo::deserialize", "deserializing BundleInfo");
    // initialize CBOR parser and value pointer
    CborParser parser;
    CborValue value;
    cbor_parser_init(&serialized[0], serialized.size(), 0, &parser, &value);

    // check whether the outer CBOR structure is an array
    if (cbor_value_is_array(&value)) {
        // enter the array
        CborValue ArrayValue;
        cbor_value_enter_container(&value, &ArrayValue);

        // Fixed CBOR structure: check that each element is of the expected type and decode it.
        // Cannot handle if the expected structure is not followed!
        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            uint64_t retentionConstraintLocal = 0;

            cbor_value_get_uint64(&ArrayValue,
                                  &retentionConstraintLocal);  // read value
            cbor_value_advance(&ArrayValue);  // move to next value
            this->setRetentionConstraint((uint)retentionConstraintLocal);
        }

        if (cbor_value_is_boolean(&ArrayValue)) {
            cbor_value_get_boolean(&ArrayValue,
                                   &locallyDelivered);  // read value
            cbor_value_advance(&ArrayValue);            // move to next value
        }

        if (cbor_value_is_array(&ArrayValue)) {
            forwardedTo = decodeNodeArray(
                &ArrayValue);  // decode the array of nodes with corresponding function
        }
        else {
            cbor_value_advance(&ArrayValue);
        }

        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            uint64_t numOfBroadcastsLocal = 0;
            cbor_value_get_uint64(&ArrayValue,
                                  &numOfBroadcastsLocal);  // read value
            cbor_value_advance(&ArrayValue);               // move to next value
            numOfBroadcasts = (uint)numOfBroadcastsLocal;
        }

        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue,
                                  &lastBroadcastTime);  // read value
            cbor_value_advance(&ArrayValue);            // move to next value
        }

        uint64_t receivedTime = 0;
        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue, &receivedTime);  // read value
            cbor_value_advance(&ArrayValue);  // move to next value
        }

        Bundle* intermediate = decodeBundle(
            &ArrayValue);  // decode the Bundle from the byte string it is stored as
        bundle = *intermediate;
        bundle.receivedAt =
            receivedTime;  // update the bundles receivedAt time to the correct value
        delete intermediate;
    }
}

BundleInfo::BundleInfo(Bundle* bundle) {
    this->bundle = *bundle;
}

std::vector<uint8_t> BundleInfo::serialize() {
    ESP_LOGD("BundleInfo::serialize", "serializing BundleInfo");

    // initialize encoder and buffer for encoder
    CborEncoder encoder;
    uint8_t buf[1000];
    cbor_encoder_init(&encoder, buf, sizeof(buf), 0);

    // initialize an encoder for use in the array
    CborEncoder encoderInternal;

    // create the array, with a fixed size of 7 elements and encode the values into it
    cbor_encoder_create_array(&encoder, &encoderInternal, 7);
    cbor_encode_uint(&encoderInternal, bundle.retentionConstraint);
    cbor_encode_boolean(&encoderInternal, locallyDelivered);
    encodeNodeArray(&encoderInternal, forwardedTo);
    cbor_encode_uint(&encoderInternal, numOfBroadcasts);
    cbor_encode_uint(&encoderInternal, lastBroadcastTime);
    cbor_encode_uint(&encoderInternal, bundle.receivedAt);
    encodeBundle(&encoderInternal, &bundle);

    // finish the array
    cbor_encoder_close_container(&encoder, &encoderInternal);

    // get actual used data size
    size_t dataSize = cbor_encoder_get_buffer_size(&encoder, buf);
    return std::vector<uint8_t>(buf, buf + dataSize);
}

void BundleInfo::setRetentionConstraint(uint constraint) {
    // just update the contained bundle's retention constraint
    bundle.retentionConstraint = constraint;
    return;
}

ReceivedBundle::ReceivedBundle(Bundle* bundle, std::string fromIdentifier) {
    this->bundle = bundle;
    this->fromAddr = fromIdentifier;
}

ReceivedBundle::~ReceivedBundle() {}

Node::Node(std::string URI) {
    // set node URI
    this->URI = URI;

    // add Node to list of EIDs associated with this node
    this->Eids.push_back(EID::fromUri(URI));
}

Node::Node() {
    // create default node, only none endpoint
    this->URI = "none";
    this->Eids.push_back(EID::fromUri("dtn:none"));
}

Node::Node(std::vector<uint8_t> serialized) {
    // write to debug log
    ESP_LOGD("Node", "deserializing Node");
    // initialize CBOR Parser and Value Pointer
    CborParser parser;
    CborValue value;
    cbor_parser_init(&serialized[0], serialized.size(), 0, &parser, &value);

    // check whether the outer CBOR structure is an array
    if (cbor_value_is_array(&value)) {
        // enter the array
        CborValue ArrayValue;
        cbor_value_enter_container(&value, &ArrayValue);

        // decode the individual elements using custom functions
        identifier = stringFromCbor(&ArrayValue);
        size_t arraySize=0;

        //if the EID array is empty, just advance the value pointer
        cbor_value_get_array_length(&ArrayValue, &arraySize); 
        if(arraySize==0) {
            cbor_value_advance(&ArrayValue);
        }
        else { 
            Eids=decodeEidArray(&ArrayValue);
            cbor_value_advance(&ArrayValue);
        }
        
        URI = stringFromCbor(&ArrayValue);

        // read the simple parameters using simple get value methods of the cbor libary
        lastSeen = 0;
        if (cbor_value_is_unsigned_integer(&ArrayValue)) {
            cbor_value_get_uint64(&ArrayValue, &lastSeen);
        }
        if (cbor_value_is_boolean(&ArrayValue))
            cbor_value_get_boolean(&ArrayValue, &hasPos);
        if (hasPos) {
            if (cbor_value_is_float(&ArrayValue))
                cbor_value_get_float(&ArrayValue, &(position.first));
            if (cbor_value_is_float(&ArrayValue))
                cbor_value_get_float(&ArrayValue, &(position.second));
        }

        // if the use of a set of hashes for reception confirmation is enabled, this set has also to be decoded, together with an additional boolean, see node definition
#if CONFIG_useReceivedSet
        if (cbor_value_is_boolean(&ArrayValue))
            cbor_value_get_boolean(&ArrayValue, &confirmedReception);
        receivedHashes = decodeHashesSet(&ArrayValue);
#endif
    }
}

// this array maps the array index to the corresponding character
constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string Node::idFromBytes(uint8_t* data, size_t dataSize) {
    // create string
    std::string s(dataSize * 2, ' ');

    // iterate through data array
    for (int i = 0; i < dataSize; ++i) {
        // first half of byte is first hex character
        s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];

        // second half of byte is second hex character
        s[2 * i + 1] = hexmap[data[i] & 0x0F];
    }
    return s;
}

std::vector<uint8_t> Node::serialize() {
    CborEncoder encoder;
    uint8_t buf[1000];
    cbor_encoder_init(&encoder, buf, sizeof(buf), 0);
    CborEncoder encoderInternal;
    uint8_t arraySize =
        hasPos
            ? 7
            : 5;  // array size should be 5 if no position is stored, otherwise 7
#if CONFIG_useReceivedSet
    arraySize += 2;
#endif
    cbor_encoder_create_array(&encoder, &encoderInternal, arraySize);
    stringToCbor(&encoderInternal, identifier);
    encodeEidArray(&encoderInternal, Eids);
    stringToCbor(&encoderInternal, URI);
    cbor_encode_uint(&encoderInternal, lastSeen);
    cbor_encode_boolean(&encoderInternal, hasPos);
    if (hasPos) {
        cbor_encode_float(&encoderInternal, position.first);
        cbor_encode_float(&encoderInternal, position.second);
    }
#if CONFIG_useReceivedSet
    cbor_encode_boolean(&encoderInternal, confirmedReception);
    encodeHashesSet(&encoderInternal, receivedHashes);
#endif
    cbor_encoder_close_container(&encoder, &encoderInternal);
    size_t dataSize = cbor_encoder_get_buffer_size(
        &encoder,
        buf);  // due to a bug in the tinycbor implementation 'get size' does not work with arrays on the heap
    std::vector<uint8_t> result(buf, buf + dataSize);
    return result;
}

void Node::print() {
    printf(
        "Node::print Identifier: %s, URI: %s, LastSeen: %llu, Num of EIDs: "
        "%u\n",
        identifier.c_str(), URI.c_str(), lastSeen, Eids.size());
    if (hasPos)
        printf("Node has Position: Lat:%f, Lng:%f\n", position.first,
               position.second);
#if CONFIG_useReceivedSet
    printf("Node Has the Following received Hashes:\n");
    for (size_t hash : receivedHashes) {
        printf("%u\n", hash);
    }
#endif
    for (EID e : Eids) {
        printf("Endpoint:\n");
        e.print();
    }
    return;
}

void Node::setLastSeen() {
    if (lastSeen == UINT64_MAX)
        return;  // if node is added staticly, do not override age

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    lastSeen =
        ((int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000);
    return;
}

void Node::setPosition(float lat, float lng) {
    hasPos = true;
    position.first = lat;
    position.second = lng;
    return;
}

bool Node::removePosition() {
    if (hasPos) {
        hasPos = false;
        return true;
    }
    return false;
}
