#include "Endpoint.hpp"
#include "dtn7-esp.hpp"
#include "sdkconfig.h"

bool Endpoint::send(uint8_t* data, size_t dataSize, std::string destination,
                    bool anonymous, uint64_t lifetime) {
    // first check whether Endpoint is registered with BundleProtocolAgent
    if (BPA == nullptr) {
        ESP_LOGI("Endpoint send",
                 "endpoint not registered with BPA, cannot send");
        return false;
    }
    else {
        ESP_LOGI("Endpoint Send", "sending...");

        // Create EID Object
        EID dest = EID::fromUri(destination);

        // create bundle age block, only used if no accurate clock configured, or the clock has not been successfully synchronized
        BundleAgeBlock ageBlock(0, CONFIG_canonicalCrcType);

        //indicates wether the age block must be attached
        bool attachAgeBlock = false;

        // create creation timestamp
        CreationTimestamp timestp(0, sequenceNum);

        // if node has accurate clock (node is synchronized to dtn time (See RFC 9171 Section 4.2.6), enabled in menuconfig),
        // the current time is included in the creationTimestamp. In this case, no bundle age block is required
#if CONFIG_HasAccurateClock
        ESP_LOGD("Endpoint send", "Accurate Clock configured!");

        //first, check whether node clock is actually synchronized
        if (DTN7::clockSynced) {
            // get current node time
            struct timeval tv_now;
            gettimeofday(&tv_now, NULL);

            // convert to milliseconds
            uint64_t currentTime = ((int64_t)tv_now.tv_sec * 1000L +
                                    (int64_t)tv_now.tv_usec / 1000);

            // if in the same millisecond a bundle was already created, increase sequence number of new bundle to allow for differentiation
            if (lastCreationTime == currentTime)
                sequenceNum++;

            // override creation timestamp
            timestp = CreationTimestamp(currentTime, sequenceNum);

            // update last creation time
            lastCreationTime = currentTime;
        }
        //accurate clock is configured, but not synchronized, act as if no accurate clock was enabled
        else {
            ESP_LOGW("Endpoint send",
                     "Accurate Clock enabled, but not synchronized! "
                     "Falling back "
                     "to non accurate clock operation");

            // increase sequence number
            sequenceNum++;

            attachAgeBlock = true;
        }

#else
        ESP_LOGD("Endpoint send", "No Accurate Clock configured");

        // increase sequence number
        sequenceNum++;

        attachAgeBlock = true;
#endif
        // create PrimaryBlock
        PrimaryBlock primary;

        // if Anonymous transmission was selected set fromURI to "dtn:none"
        if (anonymous) {
            EID anonym = EID::fromUri("dtn:none");

            // update primary block with relevant information
            primary = PrimaryBlock(dest, anonym, anonym, timestp, lifetime,
                                   CONFIG_primaryCrcType);
        }
        else {
            // update primary block with relevant information
            primary = PrimaryBlock(dest, localEID, localEID, timestp, lifetime,
                                   CONFIG_primaryCrcType);
        }

        // Create Payload Block
        PayloadBlock payload(data, dataSize);

        // Build Bundle with PrimaryBlock and payload dlock
        Bundle* b = new Bundle(&primary, &payload);

        // Add configured extension blocks to bundle

        //if requrired (no accurate clock configured or not yet synchronized) attach bundle age block
        if (attachAgeBlock) {
            ESP_LOGD("Endpoint send", "Adding BundleAgeBlock");
            b->insertCanonicalBlock(ageBlock);
        }

#if CONFIG_AttachHopCountBlock
        b->insertCanonicalBlock(
            HopCountBlock(hopLimit, 0, CONFIG_canonicalCrcType));
#endif
        // Transmit bundle via BPA
        return BPA->bundleTransmission(b);
    }
    return false;
}

bool Endpoint::sendText(std::string text, std::string destination,
                        bool anonymous, uint64_t lifetime) {
    uint8_t data[text.size()];
    memcpy(data, text.c_str(), text.size());
    return send(data, text.size(), destination, anonymous, lifetime);
}

bool Endpoint::send(std::vector<uint8_t> data, std::string destination,
                    bool anonymous, uint64_t lifetime) {
    // just call send function with relevant arguments (pointer to data contained in vector and vector size)
    return send(data.data(), data.size(), destination, anonymous, lifetime);
};

Endpoint::Endpoint(std::string address,
                   void (*onReceive)(std::vector<uint8_t>, std::string,
                                     std::string, PrimaryBlock)) {
    hasCallback = true;
    this->onReceive = onReceive;
    this->localEID = EID::fromUri(address);
    sequenceNum = 0;
}

Endpoint::Endpoint(std::string address) {
    hasCallback = false;
    this->localEID = EID::fromUri(address);
    sequenceNum = 0;
}

void Endpoint::setCallback(void (*onReceive)(std::vector<uint8_t>, std::string,
                                             std::string, PrimaryBlock)) {
    this->hasCallback = true;
    this->onReceive = onReceive;
    return;
}

void Endpoint::clearCallback() {
    this->hasCallback = false;
    this->onReceive = nullptr;
}

void Endpoint::localBundleDelivery(Bundle bundle) {
    std::vector<uint8_t> data(bundle.payloadBlock.blockTypeSpecificData,
                              bundle.payloadBlock.blockTypeSpecificData +
                                  bundle.payloadBlock.dataSize);
    ESP_LOGI("Endpoint", "received Bundle");

    // if Endpoint has a callback, call this callback with the relevant arguments
    if (hasCallback) {
        onReceive(data, bundle.primaryBlock.destEID.getURI(),
                  bundle.primaryBlock.sourceEID.getURI(), bundle.primaryBlock);
    }
    // otherwise add the bundle to the buffer storing received bundles for this Endpoint
    else {
        bundleBuffer.push_back(bundle);
    }
    return;
}

bool Endpoint::poll(std::vector<uint8_t>& data, std::string& source,
                    std::string& destination, PrimaryBlock& primaryBlock) {
    // if the endpoint has a callback ther can be no bundles to be polled
    if (hasCallback)
        return false;

    // check whether bundles are available to be polled
    if (bundleBuffer.size() == 0) {
        return false;
    }
    else {
        // get first bundle from buffer
        Bundle b = bundleBuffer.front();
        bundleBuffer.erase(bundleBuffer.begin());

        // move bundle payload to data vector
        data.clear();
        data.reserve(b.payloadBlock.dataSize);
        std::copy(
            b.payloadBlock.blockTypeSpecificData,
            b.payloadBlock.blockTypeSpecificData + b.payloadBlock.dataSize,
            std::back_inserter(data));

        // set source of Bundle
        source = b.getSource().getURI();

        // set destination of Bundle
        destination = b.getDest().getURI();

        // set Primary Block
        primaryBlock = b.primaryBlock;
    }
    return true;
}

std::vector<uint8_t> Endpoint::poll() {
    // if the endpoint has a callback ther can be no bundles to be polled, return empty vector
    std::vector<uint8_t> result;
    if (hasCallback)
        return result;

    // check whether bundles are available to be polled, if not return empty vector
    if (bundleBuffer.size() == 0)
        return result;

    // get first bundle from buffer
    Bundle b = bundleBuffer.front();
    bundleBuffer.erase(bundleBuffer.begin());

    // move bundle payload to result vector
    result.clear();
    result.reserve(b.payloadBlock.dataSize);
    std::copy(b.payloadBlock.blockTypeSpecificData,
              b.payloadBlock.blockTypeSpecificData + b.payloadBlock.dataSize,
              std::back_inserter(result));

    // return result vector
    return result;
}

bool Endpoint::hasData() {
    return (bundleBuffer.size() != 0);
}

bool Endpoint::operator==(const Endpoint& endpoint) {
    // if the scheme codes are not the same we can save time and say that the Endpoints are not the same
    if (localEID.schemeCode != endpoint.localEID.schemeCode)
        return false;

    // same for the sspSize
    if (localEID.sspSize != endpoint.localEID.sspSize)
        return false;

    // in all other cases we compare the entire SSP, we do this by comparing the content od the memory of the SSPs
    int differences =
        memcmp(localEID.SSP, endpoint.localEID.SSP, endpoint.localEID.sspSize);
    return differences == 0;
}
