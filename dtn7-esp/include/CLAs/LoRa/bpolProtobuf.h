/**
 * @file bpolProtobuf.h
 * @brief This file contains all Protbuf de- and encoding required for BPoL
 */

#pragma once
#include <cstring>
#include <iostream>
#include <sstream>
#include "../proto-c/protocol.pb-c.h"

/// @brief local debugging function, prints an array of bytes as hex
/// @param array data to print
/// @param length length of data
void printByteArrayAsHex2(const unsigned char* array, uint& length) {
    ESP_LOGE("print Byte Array", "Size: %u", length);
    for (int i = 0; i < length; ++i) {
        printf("%02X ", array[i]);
    }
    printf("\n");
    return;
};

/// @brief decodes a Protobuf message as defined in BPoL, and handles the discovery of neighbors and the reception of bundles
/// @param data array containing the Protobuf message
/// @param datasize size of the array
void decodeProtobuf(uint8_t* data, size_t datasize) {
    ESP_LOGI("decode proto", "protoVersion %s", protobuf_c_version());

    // create overall packet Object from received data
    Lora__Protocol__Packet* packet =
        lora__protocol__packet__unpack(NULL, datasize, data);

    // check if decoding was successful
    if (!packet) {
        ESP_LOGE("decode proto", "failed");
        return;
    }

    // handle the different types of BPoL packets
    switch (packet->type) {
        case LORA__PROTOCOL__PACKET_TYPE__TYPE_BUNDLE_FORWARD: {
            // if packet is of type BundleForward, the contained Bundle must be extracted and send to the receiveQueue

            // first decode the bundle from its CBOR representation
            Bundle* received =
                Bundle::fromCbor(packet->bundle_forward->bundle_data.data,
                                 packet->bundle_forward->bundle_data.len);

            // check the Bundles validity
            if (received->valid) {
                // attempt to get the sending node from storage. If it is already known the corresponding node object is returned, otherwise one is created
                Node sender = DTN7::BPA->storage->getNode(
                    std::string(packet->bundle_forward->sender));

                // if the URI of the created or stored node is empty, update it to the now known URI
                if (sender.URI == "none")
                    sender.URI = std::string(packet->bundle_forward->sender);

                // set the last seen time for the sending node
                sender.setLastSeen();

                // store the sender node in the list of known nodes
                DTN7::BPA->storage->addNode(sender);

                // create a received bundle containing the bundle and the URI of the sender
                ReceivedBundle* recBundle =
                    new ReceivedBundle(received, sender.URI);

                // send the Received bundle to the receiveQueue for further processing
                xQueueSend(DTN7::BPA->receiveQueue, (void*)&recBundle,
                           portMAX_DELAY);
            }
            else {
                delete received;  // invalid bundles are discarded directly
                ESP_LOGW("LoraCLARecTask", "deleted Invalid Bundle");
            }
            break;
        }
        case LORA__PROTOCOL__PACKET_TYPE__TYPE_ADVERTISE: {
            // if packet is of type Advertise, the contained information about the node has to be updated/ added to the storage
            ESP_LOGW("LoraCLARecTask", "received advertise");

            // attempt to get the sending node from storage. If it is already known the corresponding node object is returned, otherwise one is created
            Node sender = DTN7::BPA->storage->getNode(
                std::string(packet->advertise->node_name));
            // if the URI of the created or stored node is empty, update it to the now known URI
            if (sender.URI == "none")
                sender.URI = std::string(packet->advertise->node_name);

#if CONFIG_useReceivedSet
            // if usage of hashes of bundleIDs for reception confirmation is enabled in menuconfig, these hashes have to be read from the data
            // field of the advertise message and inserted in the set of hashes received from this sender

            // first find the corresponding entry in the data field
            for (int i = 0; i < packet->advertise->n_data; i++) {
                if (strcmp(packet->advertise->data[i]->key, "BH") == 0) {
                    // read bundlesHash from data map, this is a sting oh hashes delimited by ';'
                    std::string bundlesHashes =
                        std::string(packet->advertise->data[i]->value);

                    // now split the string into the individual hashes for this a string stream is generated and split at the delimiter ';'
                    std::stringstream hashesStream(bundlesHashes);
                    std::string hashString;
                    while (std::getline(hashesStream, hashString, ';')) {
                        size_t hash = 0;
                        std::stoul(hashString.c_str(), &hash);
                        // add each hash to the received Hashes
                        sender.receivedHashes.insert(hash);
                    }
                }
            }
#endif
            // set the last seen time for the sending node
            sender.setLastSeen();

            // ToDo Position: The advertise message can potentially contain position information. This would have to be decoded here

            // store the sender node in the list of known nodes
            DTN7::BPA->storage->addNode(sender);
        } break;
        default:
            break;
    }

    // cleanup packet
    lora__protocol__packet__free_unpacked(packet, NULL);
    return;
}

/// @brief encodes a BPoL advertise Packet containing the current node information
/// @param result pointer to an array in which the result is to be saved, this will be allocated on the heap and should be deleted
/// @param size size of the resulting array
void encodeAdvertisePacket(uint8_t** result, size_t* size) {
    // Initialize Packet and Advertise message
    Lora__Protocol__Packet packet = LORA__PROTOCOL__PACKET__INIT;
    Lora__Protocol__Advertise advertise = LORA__PROTOCOL__ADVERTISE__INIT;

    // Set type
    packet.type = LORA__PROTOCOL__PACKET_TYPE__TYPE_ADVERTISE;

    // Set node name
    char nodeNameBuffer[DTN7::localNode->identifier.length() + 1];
    strncpy(nodeNameBuffer, DTN7::localNode->identifier.c_str(),
            sizeof(nodeNameBuffer));
    nodeNameBuffer[sizeof(nodeNameBuffer) - 1] = '\0';  // ensure 0 termination
    advertise.node_name = nodeNameBuffer;

    // if configured in menuconfig, include the position of the node in the advertise message
#if CONFIG_IncludePosition
    if (DTN7::localNode->hasPosition) {
        // initialize position data structure
        Lora__Protocol__LatLngPos pos = LORA__PROTOCOL__LAT_LNG_POS__INIT;

        // Set position  field to current position
        pos.lat = DTN7::localNode->position.first;
        pos.lng = DTN7::localNode->position.second;

        // set position case filed to indicate the type of included position
        advertise.position_case = LORA__PROTOCOL__ADVERTISE__POSITION_LAT_LNG;

        // add position data structure to message
        advertise.lat_lng = &pos;
    }
    else {
        // node does not know own position, do not include a position

        // initialize position data structure
        Lora__Protocol__NoPos pos = LORA__PROTOCOL__NO_POS__INIT;

        // set position case filed to indicate no included position
        advertise.position_case = LORA__PROTOCOL__ADVERTISE__POSITION_NO_POS;

        // add position data structure to message
        advertise.no_pos = &pos;
    }
#else
    // initialize position data structure
    Lora__Protocol__NoPos pos = LORA__PROTOCOL__NO_POS__INIT;

    // set position case filed to indicate no included position
    advertise.position_case = LORA__PROTOCOL__ADVERTISE__POSITION_NO_POS;

    // add position data structure to message
    advertise.no_pos = &pos;
#endif
    // initialize data map
    advertise.n_data = 1;
    advertise.data = (Lora__Protocol__Advertise__DataEntry**)malloc(
        sizeof(Lora__Protocol__Advertise__DataEntry*) * advertise.n_data);
    advertise.data[0] = (Lora__Protocol__Advertise__DataEntry*)malloc(
        sizeof(Lora__Protocol__Advertise__DataEntry));
    lora__protocol__advertise__data_entry__init(advertise.data[0]);

    // add bundle hash key
    advertise.data[0]->key = (char*)"BH";

#if CONFIG_useReceivedSet
    // if usage of hashes of bundleIDs for reception confirmation is enabled in menuconfig, these hashes have to be stored in the data field of the advertise message

    // first initialize string to store hashes
    std::string bundlesHashes;

    // insert hashes into string, separated by ';'
    for (size_t hash : DTN7::localNode->receivedHashes) {
        bundlesHashes.append(std::to_string(hash)).append(";");
    }

    // ad string to corresponding field of advertise message
    advertise.data[0]->value = (char*)bundlesHashes.c_str();

    // clear set of received hashes of the local node
    DTN7::localNode->receivedHashes.clear();
#else
    advertise.data[0]->value = (char*)"";
#endif

    // Set content to advertise message
    packet.content_case = LORA__PROTOCOL__PACKET__CONTENT_ADVERTISE;
    packet.advertise = &advertise;

    // size of message
    size_t packet_length = lora__protocol__packet__get_packed_size(&packet);
    *result = new uint8_t[packet_length];
    *size = packet_length;

    // Serialize  message
    lora__protocol__packet__pack(&packet, *result);
    // printByteArrayAsHex2(*result ,*size);
    *size = packet_length;
    return;
}

/// @brief encodes A bundle Into A protobuf Packet, as described in BPoL
/// @param result array which shall hold the encoded data
/// @param size size_t reference to store the size of the encoded data into
/// @param bundle Bundle to encode
/// @param destination destination Node
void encodeForwardPacket(uint8_t** result, size_t* size, Bundle* bundle,
                         Node* destination) {
    ESP_LOGD("BPoL", "Encoding Forward Packet");

    // Initialize Packet and BundleForward message
    Lora__Protocol__Packet packet = LORA__PROTOCOL__PACKET__INIT;
    Lora__Protocol__BundleForward forward =
        LORA__PROTOCOL__BUNDLE_FORWARD__INIT;

    // create CBOR representation of bundle
    uint8_t* cbor;
    size_t cborSize;
    bundle->toCbor(&cbor, cborSize);
    if (cbor == nullptr || cborSize == 0 || cborSize > 8192) {  // sanity limit
        ESP_LOGE("ProtoEncode", "CBOR data invalid or too large: size = %d",
                 cborSize);
        return;
    }
    // Set packet type
    packet.type = LORA__PROTOCOL__PACKET_TYPE__TYPE_BUNDLE_FORWARD;

    // Set fields

    // copy own identifier
    char senderBuffer[DTN7::localNode->identifier.length() + 1];
    strncpy(senderBuffer, DTN7::localNode->identifier.c_str(),
            sizeof(senderBuffer));
    senderBuffer[sizeof(senderBuffer) - 1] = '\0';  // ensure 0 termination
    forward.sender = senderBuffer;

    // if given a destination, include it in the message, not used, could be used in the future for single cast transmission
    if (destination != nullptr) {
        char destBuffer[destination->identifier.length()];
        strncpy(destBuffer, destination->identifier.c_str(),
                sizeof(destBuffer));
        destBuffer[sizeof(destBuffer) - 1] = '\0';  // ensure 0 termination
        forward.destination = destBuffer;
    }
    else {
        char destBuffer[5];
        strncpy(destBuffer, "none", sizeof(destBuffer));
        destBuffer[sizeof(destBuffer) - 1] = '\0';  // ensure 0 termination
        forward.destination = destBuffer;
    }
    // add bundle id
    std::string bundleID = bundle->getID();
    char bundleIDBuffer[bundleID.length() + 1];
    strncpy(bundleIDBuffer, bundleID.c_str(), sizeof(bundleIDBuffer));
    bundleIDBuffer[sizeof(bundleIDBuffer - 1)] = '\0';
    forward.bundle_id = bundleIDBuffer;

    // add actual bundle
    forward.bundle_data.data = cbor;
    forward.bundle_data.len = cborSize;

    // set packet type
    packet.content_case = LORA__PROTOCOL__PACKET__CONTENT_BUNDLE_FORWARD;
    packet.bundle_forward = &forward;

    // calculate the size of serialized message and allocate relevant buffer
    size_t packet_length = lora__protocol__packet__get_packed_size(&packet);
    *result = new uint8_t[packet_length];
    *size = packet_length;
    if (!*result) {
        ESP_LOGE("ProtoEncode", "Failed to allocate memory for result");

        return;
    }

    // Serialize message
    lora__protocol__packet__pack(&packet, *result);
    ESP_LOGD("encode proto", "encoded");
    return;
}