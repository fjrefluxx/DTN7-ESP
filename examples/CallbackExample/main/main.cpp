#include <stdio.h>
#include "dtn7-esp.hpp"
#include "esp_log.h"

/// @brief example callback function for use in an endpoint, simply prints payload, destination, source and sequence number of bundle using printf.
///        To differentiate between callbacks each one prints its name, e.g., callback 1, in front of the received data.
/// @param data argument with which the bundle payload is passed into callback
/// @param dest destination endpoint of bundle
/// @param source source node of bundle
/// @param primaryB Primary Block of Bundle
void callback1(std::vector<uint8_t> data, std::string dest, std::string source,
               PrimaryBlock primaryB) {
    printf(
        "Callback 1:\nreceived: %.*s \nfor: %s, from: %s, with sequenceNum: "
        "%llu\n",
        data.size(), data.data(), dest.c_str(), source.c_str(),
        primaryB.timestamp.sequenceNumber);
    return;
}

/// @brief example callback function for use in an endpoint, simply prints payload, destination, source and sequence number of bundle using printf.
///        To differentiate between callbacks each one prints its name:i.e. Callback 1 befor the received data
/// @param data argument with which the bundle payload is passed into callback
/// @param dest destination endpoint of bundle
/// @param source source node of bundle
/// @param primaryB Primary Block of Bundle
void callback2(std::vector<uint8_t> data, std::string dest, std::string source,
               PrimaryBlock primaryB) {
    printf(
        "Callback 2:\nreceived: %.*s \nfor: %s, from: %s, with sequenceNum: "
        "%llu\n",
        data.size(), data.data(), dest.c_str(), source.c_str(),
        primaryB.timestamp.sequenceNumber);
    return;
}

/// @brief example callback function for use in multiple endpoints, differentiates between endpoints according to the destination endpoint, and sends a simple answer bundle using the same Endpoint.
///        This serves as an example on how to share callbacks between endpoints and demonstrates that callbacks can also trigger the transmission of reply messages.
/// @param data argument with which the bundle payload is passed into callback
/// @param dest destination endpoint of bundle
/// @param source source node of bundle
/// @param primaryB Primary Block of Bundle
void replyingCallback(std::vector<uint8_t> data, std::string dest,
                      std::string source, PrimaryBlock primaryB) {
    // get theeEndpoint object corresponding to the destination endpoint. Checking wether it is registered is not required, as this callback is only triggered for registered endpoints.
    Endpoint* targetEndpoint = DTN7::BPA->getLocalEndpoint(dest);
    char answer[] = "Reply Message";

    // send a reply message with the original source as destination.
    targetEndpoint->send((uint8_t*)&answer, sizeof(answer), source);
    return;
}

/// @brief basic example, initializes BPA and shows how to transmit and receive messages, CLAs, routing strategy, etc... are chosen in menuconfig
/// @param
extern "C" void app_main(void) {
    // First, initialize the BPA(Bundle Protocol Agent). Here we use the basic version of te setup function, without a callback for the Node central endpoint.

    // As the setup function expects a valid dtn URI (ether dtn or ipn scheme see RFC9171), we use the included function to convert the nodes Wifi Mac addres to a dtn URI
    std::string uri = DTN7::uriFromMac();

    // now setup the BPA with the generated URI and store the returned pointer to the node central endpoint, setup the central endpoint with callback1().
    Endpoint* centralEndpoint = DTN7::setup(uri, callback1);

    // we can register additional endpoints with the following. Here, we add an Endpoint called "dtn://callback2/", which uses the "callback2" function as its callback
    Endpoint* c2 = DTN7::registerEndpoint("dtn://callback2/", callback2);

    // send message via node central endpoint to callback2 endpoint, not anonymous and with default lifetime

    // message as string
    std::string message = "Test";

    // for message transmission we have do have data as uint8_t array, we take c string from std string and cast it to unit8_t
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(),
                          "dtn://callback2/");

    // now we send the same message to the node central endpoint() (the central endpoint sends itself a message, this will trigger its callback)
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(), uri);

    // now callback1 and callback2 each have triggered once. To ensure this, we wait a short time to allow for message propagation.
    vTaskDelay(100);

    // Now we remove the callback from the "dtn://callback2" endpoint, as we are adding a different callback, this could be skipped as any currently present callback will be overriden, but removing it explicitly helps readability
    c2->clearCallback();

    // Now we add the replyingCallback
    c2->setCallback(replyingCallback);

    // we create additional Endpoint with replyingCallback
    Endpoint* replyingEndpoint =
        DTN7::registerEndpoint("dtn://replyingEndpoint/", replyingCallback);

    // now send a message from the central endpoint to both endpoints using the replyingCallback. They will both result in an answer message beeing send to the central endpoint, which will trigger its callback
    // both answer messages will have the same payload, but a different source URI, which will be visible in the output created by the central endpoints callback upon reception of the replies.

    // for message transmission we have do have data as uint8_t array, we take c string from std string and cast it to unit8_t
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(),
                          "dtn://callback2/");

    // now we send the same message to the node central endpoint() (the central endpoint sends itself a message, this will trigger its callback)
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(),
                          "dtn://replyingEndpoint/");
}