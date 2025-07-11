#pragma once
#include <vector>
#include "BundleProtocolAgent.hpp"
#include "EID.hpp"
#include "dtn7-bundle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class BundleProtocolAgent;

/// @brief this class represents an DTN endpoint
class Endpoint {
   private:
    /// @brief last time a bundle was created from this endpoint, in ms
    uint64_t lastCreationTime = 0;

    /// @brief indicates whether the endpoint has a callback
    bool hasCallback;

    /// @brief the callback of the Endpoint arguments are std::vector<uint8_t> payload  ,std::string destination URI ,std::string Source URI, PrimaryBlock PrimaryBlock
    void (*onReceive)(std::vector<uint8_t>, std::string, std::string,
                      PrimaryBlock);

    /// @brief stores bundles to be polled from the endpoint, if nod callback is present
    std::vector<Bundle> bundleBuffer;

#if CONFIG_AttachHopCountBlock
    /// @brief stores the hop limit used if a hop count block is configured to be attached
    uint16_t hopLimit = CONFIG_HopLimit;
#endif
   public:
    /// @brief Stores the EID of the endpoint
    EID localEID;

    /// @brief Pointer to BundleProtocolAgent
    BundleProtocolAgent* BPA = nullptr;

    /// @brief stores the Last used sequence number of this endpoint
    uint64_t sequenceNum;

    /// @brief constructs an empty Endpoint object
    Endpoint() { sequenceNum = 0; };

    /// @brief creates an endpoint with a specified address and the specified callback
    /// @param address URI of the new endpoint
    /// @param onReceive  callback for the Endpoint first argument of callback is bundle Payload as a byte vector, second is the destination endpoint URI, third is the Source Endpoint URI, forth is the Primary Block of the corresponding bundle, WARNING: Callback is called from a different task! I must be thread safe!
    Endpoint(std::string address,
             void (*onReceive)(std::vector<uint8_t>, std::string, std::string,
                               PrimaryBlock));

    /// @brief creates an endpoint with a specified address without a callback
    /// @param address URI of the new endpoint
    Endpoint(std::string address);

    /// @brief adds a callback to the Endpoint, if it has none, otherwise overrides the Callback, if there are still received bundles stored which hve not been read using poll, these will be lost!
    /// @param onReceive new callback for the Endpoint first argument of callback is bundle Payload as a byte vector, second is the destination endpoint URI, third is the Source Endpoint URI, forth is the Primary Block of the corresponding bundle, WARNING: Callback is called from a different task! I must be thread safe!
    void setCallback(void (*onReceive)(std::vector<uint8_t>, std::string,
                                       std::string, PrimaryBlock));

    /// @brief removes the callback from an endpoint
    void clearCallback();

    /// @brief Function called by the BPA if it receives a Bundle for the endpoint
    /// @param bundle
    void localBundleDelivery(Bundle bundle);

    /// @brief Function for sending data via the BundleProtocolAgent, the actual Bundle is created here, attaches BundleAgeBlock if Node does not have accurate Clock, if CONFIG_AttachHopCountBlock is true the hop count block is added here
    /// @param data the Payload to send, stored in a byte Vector
    /// @param destination destination EID of the Bundle
    /// @param anonymous optional ,whether to send the bundle anonymously, i.e. with dtn:// none as sender id, defaults to false
    /// @param lifetime lifetime assigned to the generated bundle in ms, defaults to the value set in menuconfig
    /// @return whether the sending was successful
    bool send(std::vector<uint8_t> data, std::string destination,
              bool anonymous = false, uint64_t lifetime = CONFIG_BundleTTL);

    /// @brief Function for sending data via the BundleProtocolAgent, the actual Bundle is created here, attaches BundleAgeBlock if Node does not have accurate Clock, if CONFIG_AttachHopCountBlock is true the hop count block is added here
    /// @param data the Payload to send, should be a pointer to a array of uint8_t with the size passed in dataSize
    /// @param dataSize size of the Payload
    /// @param destination destination EID of the Bundle
    /// @param anonymous optional ,whether to send the bundle anonymously, i.e. with dtn:// none as sender id, defaults to false
    /// @param lifetime lifetime assigned to the generated bundle in ms, defaults to the value set in menuconfig
    /// @return whether the sending was successful
    bool send(uint8_t* data, size_t dataSize, std::string destination,
              bool anonymous = false, uint64_t lifetime = CONFIG_BundleTTL);

    /// @brief Function for sending text via the BundleProtocolAgent, the actual Bundle is created here, attaches BundleAgeBlock if Node does not have accurate Clock, if CONFIG_AttachHopCountBlock is true the hop count block is added here
    /// @param text text to send
    /// @param destination  destination EID of the Bundle
    /// @param anonymous optional ,whether to send the bundle anonymously, i.e. with dtn:// none as sender id, defaults to false
    /// @param lifetime lifetime assigned to the generated bundle in ms, defaults to the value set in menuconfig
    /// @return whether the sending was successful
    bool sendText(std::string text, std::string destination,
                  bool anonymous = false, uint64_t lifetime = CONFIG_BundleTTL);

    /// @brief poll the endpoint for newly received data, the data is written in to the corresponding fields, only the Payload of the first Bundle which was received since the last Poll is returned, call multiple times to receive multiple bundles
    /// @param data vector the received payload is written to
    /// @param source string to which the source URI is written to
    /// @param destination string to which the destination URI is written to
    /// @param PrimaryBlock Primary Block which will be overriden with the primary block of the received Bundle
    /// @return true if data has been returned
    bool poll(std::vector<uint8_t>& data, std::string& source,
              std::string& destination, PrimaryBlock& primaryBlock);

    /// @brief poll the endpoint for newly received data, the first received payload is returned, only the Payload of the first Bundle which was received since the last Poll is returned, call multiple times to receive multiple bundles
    /// @return a std::vector containing the bytes of the payload
    std::vector<uint8_t> poll();

    /// @brief check whether data, which can be read using poll() is present
    /// @return true whether data can be read from this endpoint
    bool hasData();

    /// @brief operator== for endpoints, just compares the EID's scheme+SSP
    /// @param end
    /// @return
    bool operator==(const Endpoint& endpoint);
};
