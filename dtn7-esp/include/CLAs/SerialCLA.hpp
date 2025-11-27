#pragma once
#include <list>
#include <string>
#include "CLA.hpp"
#include "Data.hpp"
#include "Hal.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "dtn7-bundle.hpp"
#include "sdkconfig.h"

/**
 * @file SerialCLA.hpp
 * @brief This file contains the SerialCLA.
 */


/// @brief A Simple Serial CLA, not using the queue system, does not use CTS/RTS. Mainly intended to serve as an example of a CLA not using the queue System
class SerialCLA : public CLA {
   private:
    /// @brief the name of the CLA, here "Serial CLA"
    std::string name = "Serial CLA";

    bool canAddress = false;

    /// @brief struct that stores the UART config
    uart_config_t uart_config;

    /// @brief stores the used UART port number
    uart_port_t uart_num;

    /// @brief used to store the system time at which the current transmit Cycle
    uint64_t startOfCycle = 0;

    /// @brief the Amount of data, in bytes used in the current cycle
    uint64_t usedBytesInCycle = 0;

   public:
    /// @brief returns the name of the CLA ("Serial CLA")
    /// @return std::string with the name of the CLA
    std::string getName() override;

    /// @brief returns a whether a CLA  is able to send message to a specific, singular node. Returns False if the CLA broadcasts messages
    /// @return whether a CLA can address specific nodes(true) or only broadcast messages(false)
    bool checkCanAddress() override;

    /// @brief setup the Serial CLA, default parameters are the ones set in menuconfig
    /// @param baud baudrate to use
    /// @param rx rx Pin number
    /// @param tx tx pin number
    SerialCLA(int baud = CONFIG_UART_BAUD_RATE, uint rx = CONFIG_UART_RXD,
              uint tx = CONFIG_UART_TXD);

    /// @brief Destructs the Serial CLA
    ~SerialCLA();

    /// @brief if the CLA is not setup to use the received queue, this function can be used to get the bundles received since the last time it was called, must be safe to call from a different thread than send()
    /// @return new received bundles since this function was last called
    std::vector<ReceivedBundle*> getNewBundles() override;

    /// @brief send a bundle to a target node via the CLA, must be safe to call from a different thread than getNewBundles() / if the queue system is used thread safety to the thread receiving bundles has to be kept in mind
    /// @param bundle the Bundle to send
    /// @param destination destination node, not used as the Serial CLA is not able to address specific nodes
    /// @return true if the bundle was successfully sent
    virtual bool send(Bundle* bundle, Node* destination = nullptr);
};
