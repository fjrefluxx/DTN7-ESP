#include "SerialCLA.hpp"
#include "CLA.hpp"
#include "dtn7-esp.hpp"
#include "esp_timer.h"

//disable warnings due to missing initializers in UART config struct
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

std::string SerialCLA::getName() {
    return this->name;
}

bool SerialCLA::checkCanAddress() {
    return false;
}

SerialCLA::SerialCLA(int baud, uint rx, uint tx) {
    // initialize the UART port config structure
    uart_num = (uart_port_t)CONFIG_UART_PORT_NUM;
    int intr_alloc_flags = 0;
    uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // get the current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    // set the beginning of the transmission cycle
    startOfCycle =
        ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);

    // enable UART
    ESP_ERROR_CHECK(uart_driver_install(uart_num, CONFIG_UART_BUF_SIZE, 0, 0,
                                        NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx, rx, -1, -1));

    // clear the UART buffer
    uart_flush(uart_num);
}

SerialCLA::~SerialCLA() {
    uart_driver_delete(uart_num);
}

std::vector<ReceivedBundle*> SerialCLA::getNewBundles() {
    std::vector<ReceivedBundle*> result;

    // get the length of data in the buffer
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t*)&length));

    // create an appropriately sized array
    uint8_t data[length];

    // read the data from UART
    length = uart_read_bytes(uart_num, data, length, 1000);
    ESP_LOGI("SerialCLA::getNewBundles", "RX Buffer Length: %i", length);

    uint startindex = 0;
    uint cborLength = 0;

    // first search the beginning of a bundle
    for (int j = 0; j < length; j++) {
        if (data[j] == 0x9f)  // find cbor start byte
        {
            startindex = j;  // the index where the bundle starts is now found
            break;
        }
    }
    // now find bundle end. Assumed that after its end another starts
    for (int i = 0; i < length; i++) {
        if (data[i] == 0xff) {  // detect end bundle
            cborLength =
                i + 1 -
                startindex;  // we found the end of the bundle, this has to be included in the bytes, therfore +1
            ESP_LOGI("SerialCLA::getNewBundles", "received potential Bundle");

            // decode the bundle
            Bundle* b = Bundle::fromCbor(&data[startindex], cborLength);
            ReceivedBundle* recBundle =
                new ReceivedBundle(b, std::string("none"));

            if (b->valid)
                result.push_back(
                    recBundle);  // only add valid received bundles to result

            startindex = startindex + cborLength +
                         1;  // +1 needed to skip the end of message marker
            cborLength = 0;
        }
    }
    // clear UART buffer
    uart_flush(uart_num);

    ESP_LOGI("SerialCLA::getNewBundles", "number of received Bundles: %u",
             result.size());
    return result;
}

bool SerialCLA::send(Bundle* bundle, Node* destination) {
    ESP_LOGI("SerialCLA::send()", "sending Bundle, with ID:%s",
             bundle->getID().c_str());
    // serialize the bundle to CBOR
    uint8_t* cbor;
    size_t cborSize;
    bundle->toCbor(&cbor, cborSize);

    // get current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t currentTime =
        ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);

    // check whether to begin new reference time
    if ((currentTime - startOfCycle) >
        (uint64_t)CONFIG_TimeBetweenClaPoll * (uint64_t)1000000) {
        startOfCycle = currentTime;
        usedBytesInCycle = 0;
    }

    // check how many bytes were transmitted in this cycle
    uint64_t newUsedByteCount = usedBytesInCycle + cborSize + 1;
    if (newUsedByteCount > CONFIG_UART_BUF_SIZE) {
        ESP_LOGI("SerialCLA::send()",
                 "sending Bundle not Possible, too high chance the receive "
                 "Buffer is full");
        return false;
    }

    // update the amount of bytes send in this cycle
    usedBytesInCycle = newUsedByteCount;

    // send out the data
    int writtenBytes =
        uart_write_bytes_with_break(uart_num, (const char*)cbor, cborSize, 2);
    return writtenBytes == cborSize;
}

#pragma GCC diagnostic pop