#pragma once

#include "sdkconfig.h"
#if CONFIG_USE_LORA_CLA  //only compile if LoRa CLA is enabled, this allows the overall project to be installed on platforms which do not have the required spi resources for the lora CLA (ESP32-C3)

#include <list>
#include <string>
#include "CLA.hpp"
#include "Data.hpp"
#include "RadioLib.h"
#include "dtn7-bundle.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_DevKit_LoRa32V2
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_NSS 18
#define LORA_DIO0 26
#define LORA_NRST 14
#define LORA_BUSY 35
#elif CONFIG_DevKit_LiLygoLoRa32
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_NSS 18
#define LORA_DIO0 26
#define LORA_NRST 14
#define LORA_BUSY 33
#elif CONFIG_DevKit_LoRa32V3
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10
#define LORA_NSS 8
#define LORA_DIO0 14
#define LORA_NRST 12
#define LORA_BUSY 13
#else
#define LORA_SCK CONFIG_SCK_PIN
#define LORA_MISO CONFIG_MISO_PIN
#define LORA_MOSI CONFIG_MOSI_PIN
#define LORA_NSS CONFIG_NSS_PIN
#define LORA_DIO0 CONFIG_DIO0_PIN
#define LORA_NRST CONFIG_NRST_PIN
#define LORA_BUSY CONFIG_BUSY_PIN
#endif

/**
 * @file LoRaCLA.hpp
 * @brief This file contains the LoRa CLA, including its predefined devkits. If an additional predefined devkit is to be added, this can be used as reference, in conjunction with "Kconfig.projbuild".
 */


/// @brief the task handle of the loraReceiver Handler
extern TaskHandle_t loraRecHandle;
#if CONFIG_enableBPoL
/// @brief the task handle of the BPoL advertise Task
extern TaskHandle_t advertiseTaskHandle;

/// @brief the BPoL advertise Interval, can in theory be changed at runtime
extern uint32_t BPoLAdvertiseInterval;

#endif
/// @brief class representing the LoRa CLA
class LoraCLA : public CLA {
   private:
    RadioLibHal* hal;
#if CONFIG_RadioModel_SX1276 || CONFIG_DevKit_LoRa32V2 || \
    CONFIG_DevKit_LiLygoLoRa32
    SX1276 radio = NULL;
#elif CONFIG_RadioModel_SX1262 || CONFIG_DevKit_LoRa32V3
    SX1262 radio = NULL;
#elif CONFIG_RadioModel_LLCC68
    LLCC68 radio = NULL;
#endif
    std::string name = "LoRa CLA";
    bool canAddress = false;

    /// @brief used to handle radio thread safety, required as for example the BPoL advertise task could collide with a bundle forwarding attempt
    SemaphoreHandle_t radioMutex;

   public:
    /// @brief used to store the system time at which the current duty cycle period began  in us
    uint64_t startOfDutyCycleTime = 0;

    /// @brief the Amount of Airtime, in uS used in the current duty cycle period
    uint64_t usedAirtimeInTime = 0;

    /// @brief the configured duty cycle, in percent
    uint dutyCyclePercent = 0;

    /// @brief returns the name of the CLA ("LoRa CLA")
    /// @return std::string with the name of the CLA
    std::string getName() override;

    /// @brief returns a whether a CLA  is able to send message to a specific, singular node. Returns False if the CLA broadcasts messages
    /// @return whether a CLA can address specific nodes(true) or only broadcast messages(false)
    bool checkCanAddress() override;

    /// @brief setup the LoRa CLA, default parameters are pins defined in menuconfig
    /// @param sck
    /// @param miso
    /// @param mosi
    /// @param nss
    /// @param dio0
    /// @param nrst
    /// @param busy
    LoraCLA(int8_t sck = LORA_SCK, int8_t miso = LORA_MISO,
            int8_t mosi = LORA_MOSI, int8_t nss = LORA_NSS,
            int8_t dio0 = LORA_DIO0, int8_t nrst = LORA_NRST,
            int8_t busy = LORA_BUSY);

    /// @brief destruct LoraCLA, stops receive task, resets radio
    ~LoraCLA();

    /// @brief this function is not implemented for the LoRa CLA, as all bundles are handled via the received queue
    /// @return an empty vector
    std::vector<ReceivedBundle*> getNewBundles() override;

    /// @brief sends a Bundle using the LoRa CLA, is thread safe
    /// @param bundle the Bundle to send
    /// @param destination destination node, not used as the LoRa CLA is not able to address specific nodes
    /// @return true if the bundle was successfully sent
    virtual bool send(Bundle* bundle, Node* destination = nullptr);

    /// @brief set up the ISR needed to receive via Lora
    void setupIsr();

    /// @brief the ISR which notifies the reception tasks of incoming LoRa messages
    static void receivedHandler();

    /// @brief reads Data from the lora Radio, is thread safe
    /// @param data array into which the data should be read, will be allocated on the heap and should be deleted
    /// @param dataSize data size used for the array
    void readData(uint8_t** data, size_t& dataSize);

    /// @brief the task which does the actual LoRa packet reception, blocks until it is notified by the receiveHandler isr
    /// @param param
    static void loraRecTask(void* param);

    /// @brief transmits data using the LoRa radio
    /// @param bundleData pointer to data array, the array will be deleted
    /// @param dataSize size of the data
    /// @return whether the sending was successful
    bool transmitData(uint8_t* bundleData, size_t dataSize);

    /// @brief task which handles BPoL advertising
    /// @param param
    static void advertiseTask(void* param);

    /// @brief send an advertise Packet
    static void sendAdvertise();
};

#endif