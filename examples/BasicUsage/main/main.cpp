#include <stdio.h>
#include <string>
#include "dtn7-esp.hpp"

/// @brief example callback function for use in an endpoint, simply prints payload, destination, source and sequence number of bundle
/// @param data argument with which the bundle payload is passed into callback
/// @param dest destination endpoint of bundle
/// @param source source node of bundle
/// @param primaryB Primary Block of Bundle
void callback(std::vector<uint8_t> data, std::string dest, std::string source,
              PrimaryBlock primaryB) {
    printf(
        "Callback:\nreceived: %.*s \nfor: %s, from: %s, with sequenceNum: "
        "%llu\n",
        data.size(), data.data(), dest.c_str(), source.c_str(),
        primaryB.timestamp.sequenceNumber);
    return;
}

/// @brief basic example, initializes BPA and shows how to transmit and receive messages, CLAs, routing strategy, etc... are chosen in menuconfig
/// @param
extern "C" void app_main(void) {
    // First, initialize the BPA(Bundle Protocol Agent). Here we use the basic version of te setup function, without a callback for the Node central endpoint.

    // As the setup function expects a valid dtn URI (ether dtn or ipn scheme see RFC9171), we use the included function to convert the nodes Wifi Mac addres to a dtn URI
    std::string uri = DTN7::uriFromMac();

    // now setup the BPA with the generated URI and store the returned pointer to the node central endpoint
    Endpoint* centralEndpoint = DTN7::setup(uri);

    // we can register additional endpoints with the following. Here, we add a callback to the endpoint with a function pointer to the "callback" function defined above
    Endpoint* target = DTN7::registerEndpoint("dtn://target", callback);

    // send message via node central endpoint to other endpoint, here "dtn://target", not anonymous and with default lifetime

    // message as string
    std::string message = "Test";

    // for message transmission we have do have data as uint8_t array, we take c string from std string and cast it to unit8_t
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(),
                          "dtn://target");

    // in the following can be seen how to remove a callback from an endpoint, this is equivalent to switching the Endpoint to a passive state, see RFC9171
    target->clearCallback();

    // if we now receive a message, we have to poll the endpoint manually

    // send message again
    centralEndpoint->send((uint8_t*)message.c_str(), message.size(),
                          "dtn://target");

    // poll endpoint for message
    while (true) {
        // check if endpoint has data to read
        if (target->hasData()) {
            // get data, the same data is available as with the callback, we need references to objects into which the result can be written
            std::vector<uint8_t> data;
            std::string dest;
            std::string source;
            PrimaryBlock primaryB;

            // poll the endpoint for the first received bundle
            target->poll(data, source, dest, primaryB);

            // print received informaiton
            printf(
                "Polling:\nreceived: %.*s \nfor: %s, from: %s, with "
                "sequenceNum: %llu\n",
                data.size(), data.data(), dest.c_str(), source.c_str(),
                primaryB.timestamp.sequenceNumber);

            // check if further messages are available and begin loop again if yes, otherwise exit loop and move to next example
            if (target->hasData())
                continue;

            break;
        }
        else {
            // no data available, wait until data is available
            vTaskDelay(20);
        }
    }

    // re-add callback
    target->setCallback(callback);
}