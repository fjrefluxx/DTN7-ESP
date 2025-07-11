#include "sdkconfig.h"

#if CONFIG_USE_LORA_CLA  // only compile if LoRa CLA is enabled, this allows the overall project to be installed on platforms which do not have the required spi resources for the lora CLA (ESP32-C3)

#include <RadioLib.h>
#include "EspHal2.h"
#include "LoRaCLA.hpp"
#include "bpolProtobuf.h"
#include "dtn7-esp.hpp"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t BPoLAdvertiseInterval =
    CONFIG_AdvertiseInterval * 1000;  // convert the advertise interval to ms

TaskHandle_t loraRecHandle = NULL;
#if CONFIG_enableBPoL
TaskHandle_t advertiseTaskHandle = NULL;
#endif

std::string LoraCLA::getName() {
    return this->name;
}

bool LoraCLA::checkCanAddress() {
    return this->canAddress;
}

LoraCLA::LoraCLA(int8_t sck, int8_t miso, int8_t mosi, int8_t nss, int8_t dio0,
                 int8_t nrst, int8_t busy) {
    // initialize the semaphore
    radioMutex = xSemaphoreCreateMutex();

    // get the configure duty cycle
    this->dutyCyclePercent = CONFIG_LoRa_DutyCycle;

    // if the LoRa CLA is not already set up, handle the setup process
    if (DTN7::loraCLA == NULL) {
        ESP_LOGI("LoraCLA", "Lora initializing ... ");

        // First create an instance of the appropriate HAL class.
        // This HAL class MUST fit to the ESP version which is used - for everything except the classic ESP32, additional work will be required
        hal = new EspHal2(sck, miso, mosi);

        // create the RadioLib radio module
        radio = new Module(hal, nss, dio0, nrst);
        // for SX1262 Module(hal, NSS, DIO1, RST, BUSY)

        ESP_LOGI("Lora", "begin:");

        // start the LoRa radio with all configuration from menuconfig and the default private syncword
        int state =
            radio.begin(CONFIG_LoRa_Frequency * 0.001, CONFIG_LoRa_Bandwidth,
                        CONFIG_LoRa_SpreadingFactor, CONFIG_LoRa_CodingRate,
                        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, CONFIG_LoRa_TXPower,
                        CONFIG_LoRa_PreambleLength);

        // if an error occurred print a error message and block further execution for a while
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGE("Lora", "failed begin, code %d\n", state);
            while (true) {
                hal->delay(1000);
            }
        }

        // create the LoRa receiver Task
        xTaskCreate(
            loraRecTask, "LoraReceiver", 6000, NULL, 3,
            &loraRecHandle);  // start receive handler task with high priority (3)

        // setup the ISR required for LoRa reception
        setupIsr();

        // get the current time
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);

        // convert the current time to us
        uint64_t currentTime =
            ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);

        // define the start point of the duty cycle as the current time. This time will be increased after the duty cycle reference time interval to the then current time
        startOfDutyCycleTime = currentTime;

        // enable LoRa reception
        radio.startReceive();
#if CONFIG_enableBPoL
        // if BPoL is enabled, create the advertise task
        xTaskCreate(advertiseTask, "BPoL Advertiser", 3000, NULL, 2,
                    &advertiseTaskHandle);
#endif
        // log that the LoRa CLA has been setup and with which parameters
        ESP_LOGI("LoraCLA",
                 "success!: Config:  Tx power: %idBm, Frequency: %f, "
                 "Bandwidth: %i, SpreadingFactor: %i",
                 CONFIG_LoRa_TXPower, (float)(CONFIG_LoRa_Frequency * 0.001),
                 CONFIG_LoRa_Bandwidth, CONFIG_LoRa_SpreadingFactor);
    }
    // if the LoRa CLA is already setup DO NOT set it up again
    else {
        ESP_LOGE("LoraCLA",
                 "Only One Instance Of the Lora CLA is allowed at any time");
    }
}

LoraCLA::~LoraCLA() {
    // delete the receive task
    vTaskDelete(loraRecHandle);
#if CONFIG_enableBPoL
    // delete the advertise task
    vTaskDelete(advertiseTaskHandle);
#endif

    // clear the radio configuration
    radio.reset();
    radio.clearPacketReceivedAction();
}

std::vector<ReceivedBundle*> LoraCLA::getNewBundles() {
    // return the correct data type, but no data, as this CLA does not use the polling mechanism
    return std::vector<ReceivedBundle*>();
}

bool LoraCLA::send(Bundle* bundle, Node* destination) {
    ESP_LOGI("LoraCLA::send", "Transmitting Bundle Via Lora");

#if CONFIG_enableBPoL
    // create the Protobuf packet for transmission
    uint8_t* protobuf;
    size_t protobufSize = 0;
    encodeForwardPacket(&protobuf, &protobufSize, bundle, destination);

    // use the transmitData function, as this handles duty cycle checking and thread safety
    bool result = transmitData(protobuf, protobufSize);

    // delete the encoded data from the heap
    delete[] protobuf;

    // return whether the transmission was a success
    return result;
#else
    // CBOR encode the bundle
    uint8_t* cbor;
    size_t cborSize = 0;
    bundle->toCbor(&cbor, cborSize);

    // use the transmitData function, as this handles duty cycle checking and thread safety
    bool result = transmitData(cbor, cborSize);

    // delete the encoded data from the heap
    delete[] cbor;

    // return whether the transmission was a success
    return result;
#endif
}

void LoraCLA::setupIsr() {
    radio.setPacketReceivedAction(receivedHandler);
    return;
}

// Notify the task which handles the actual transmission.
// Function is stored in IRAM, as required/recommended for ISRs (https:// docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/memory-types.html)
IRAM_ATTR void LoraCLA::receivedHandler() {
    BaseType_t HigherPriorityTaskWoken = pdFALSE;
    // notify the receive task
    vTaskNotifyGiveFromISR(loraRecHandle, &HigherPriorityTaskWoken);
    // if a higher priority task was woken by NotifyGive, explicitly schedule this task (should happen as receive task has a high priority)
    portYIELD_FROM_ISR(HigherPriorityTaskWoken);
    return;
}

void LoraCLA::readData(uint8_t** data, size_t& dataSize) {
    // take the mutex to ensure thread safety
    xSemaphoreTake(radioMutex, portMAX_DELAY);

    // read the data size from the radio
    dataSize = DTN7::loraCLA->radio.getPacketLength();

    // allocate appropriate buffer
    *data = new uint8_t[dataSize];

    // read the data from the radio
    DTN7::loraCLA->radio.readData(*data, dataSize);

    // release the mutex
    xSemaphoreGive(radioMutex);

    return;
}

void LoraCLA::loraRecTask(void* param) {
    while (true) {
        // wait until notified (by the receivedHandler ISR)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // log some debug information
        ESP_LOGD("LoraCLARecTask",
                 "Rec Task Notified, processing received Data...");
        ESP_LOGD("LoraCLARecTask",
                 "Free Heap: = %i ,minimal Free Stack since Task creation:%u",
                 heap_caps_get_free_size(MALLOC_CAP_8BIT),
                 uxTaskGetStackHighWaterMark(NULL));

        // initialize fields into which the received data will be read
        size_t dataSize;
        uint8_t* data = nullptr;

        // read the data
        DTN7::loraCLA->readData(&data, dataSize);

        // check if the data is too short to be anything usefull
        if (dataSize >= 5) {
            // Check for CBOR coding, i.e., dtn7-zero compatibility. The 5th byte (first data byte after header) MUST be 0x9f in a CBOR-encoded bundle.
            if (data[4] == 0x9f) {
                // acount for the header in the data size
                dataSize -= 4;

                // decode the bundle, account for the header and move the start index to the 5th byte
                Bundle* received = Bundle::fromCbor(data + 4, dataSize);
                if (received->valid) {
                    // only handle valid bundles
                    ReceivedBundle* recBundle = new ReceivedBundle(
                        received,
                        "none");  // transmitting node is not known in simple, non protobuf, case

                    // send bundle to receive queue
                    xQueueSend(DTN7::BPA->receiveQueue, (void*)&recBundle,
                               portMAX_DELAY);
                }
                else {
                    // invalid bundles are discarded directly
                    delete received;
                    ESP_LOGW("LoraCLARecTask", "deleted invalid bundle");
                }
            }
            else {
                ESP_LOGI("LoraCLARecTask", "recognized protobuf");

                // decode protobuf, account for header when passing the data pointer and size argument
                decodeProtobuf(data + 4, dataSize - 4);
            }
        }
        else
            ESP_LOGI("LoraCLARecTask", "Data to small to be any valid packet");

        // clean up the data from heap
        delete[] data;
        ESP_LOGI("LoraCLARecTask", "Read incoming packet with data size:%u",
                 dataSize);
    }
}

bool LoraCLA::transmitData(uint8_t* bundleData, size_t dataSize) {
    ESP_LOGI("LoraCLA", "Transmitting data with size:%u", dataSize);

    if (dataSize == 0)
        return false;  // return false if packet is empty
    if (dataSize > 250)
        return false;  // return false if packet is to large

    // take the mutex to calculate time on air
    xSemaphoreTake(radioMutex, portMAX_DELAY);
    unsigned long timeOnAir = radio.getTimeOnAir(
        dataSize +
        4);  // get time on air in microseconds, +4 is required due to header
    xSemaphoreGive(radioMutex);

    // get current time
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    // convert current time to us
    uint64_t currentTime =
        ((int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec);

    // check whether to begin new reference time
    if ((currentTime - startOfDutyCycleTime) >
        (uint64_t)CONFIG_LoRa_DutyCycleTime * (uint64_t)60 *
            (uint64_t)1000000) {
        startOfDutyCycleTime = currentTime;
        usedAirtimeInTime = 0;  // reset used airtime in the interval
    }
    // calculated new used duty cycle
    float newDutyCycle =
        (((float)(usedAirtimeInTime + timeOnAir) / 1000.0) /
         (CONFIG_LoRa_DutyCycleTime * 60 * 1000)) *
        100;  // *100 is required to convert from a fraction to a percentage

    // log some info
    ESP_LOGI("LoraCLA", "dutyCycle: %f, time on Air %lu, bandwidth:%f",
             newDutyCycle, timeOnAir, radio.getDataRate());

    // check if duty cycle would be violated if the data were to be transmit
    if (newDutyCycle > CONFIG_LoRa_DutyCycle) {
        ESP_LOGW("LoraCLA",
                 "Sending Data would violate duty cycle, time until duty cycle "
                 "reset: %u s",
                 CONFIG_LoRa_DutyCycleTime * 60 -
                     (uint)((currentTime - startOfDutyCycleTime) / 1000000));
        return false;  // transmission cannot be conducted at the moment
    }

    // update used time on air
    usedAirtimeInTime += timeOnAir;

    // create a local array to store the data with header, this is not particularly elegant, but works
    uint8_t dataLocal[dataSize + 4];

    // attach a header to the LoRa message, for compatibility with dtn7zero and BPoL 4 Bytes are needed, any bytes could be used, we use the same default as dtn7zero
    dataLocal[0] = 0xff;
    dataLocal[1] = 0xff;
    dataLocal[2] = 0x00;
    dataLocal[3] = 0x00;

    // copy the data to the local array with header, again not elegant
    for (uint index = 4; index < dataSize + 4; index++) {
        dataLocal[index] = bundleData[index - 4];
    }

    // take the mutex for transmission
    xSemaphoreTake(radioMutex, portMAX_DELAY);

    // disable reception and the interrupt
    radio.standby();
    radio.clearPacketReceivedAction();
    vTaskDelay(1);  // needed to avoid watchdog

    // transmit the data
    int16_t status = radio.transmit(dataLocal, dataSize + 4);

    // re-enable reception and interrupt
    radio.setPacketReceivedAction(receivedHandler);
    radio.startReceive();

    // release mutex
    xSemaphoreGive(radioMutex);

    // check whether an error occurred
    if (status != RADIOLIB_ERR_NONE) {
        ESP_LOGE("LoraCLA", "sending Failed, errorCode:%i", status);
        return false;
    }
    return true;
}

void LoraCLA::advertiseTask(void* param) {
#if CONFIG_enableBPoL
    vTaskDelay(
        (5 * 1000) /
        portTICK_PERIOD_MS);  // wait 5 seconds before doing the initial advertise
    while (true) {
        ESP_LOGI("LoraCLA::advertiseTask", "sending advertise packet");
        sendAdvertise();
        vTaskDelay(
            BPoLAdvertiseInterval /
            portTICK_PERIOD_MS);  // wait the configured time between advertisements
    }
#endif
    return;
}

void LoraCLA::sendAdvertise() {
    uint8_t* protobuf;
    size_t protobufSize = 0;
    encodeAdvertisePacket(&protobuf, &protobufSize);
    if (DTN7::loraCLA == NULL)
        ESP_LOGE("LoraCLA::advertiseTask", "no CLA");
    else
        DTN7::loraCLA->transmitData(protobuf, protobufSize);
    return;
}

#endif