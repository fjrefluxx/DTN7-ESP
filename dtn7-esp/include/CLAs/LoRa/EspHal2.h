#pragma once

#include "sdkconfig.h"
#if CONFIG_USE_LORA_CLA  //only compile if LoRa CLA is enabled, this allows the overall project to be installed on platforms which do not have the required spi resources for the lora CLA (ESP32-C3)

/** 
 * @file EspHal2.h
 * @brief The content of this file is a modification of https://github.com/jgromes/RadioLib/blob/master/examples/NonArduino/ESP-IDF/main/EspHal.h
 *        Additional inspiration was drawn from: https://github.com/IanBurwell/DynamicLRS/blob/main/components/DLRS_LoRadio/radiolib_esp32s3_hal.hpp
*         The goal is to allow radiolib to work not only with the ESP32 but also its variants, for example the ESP32-S3. It has been tested on ESP32 and ESP32-S3.
*         This is achieved by using the SPI high level drivers of the ESP-IDF
*/

// include RadioLib
#include <RadioLib.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_hal.h"
#include "soc/dport_reg.h"
#include "soc/rtc.h"

#include <rom/ets_sys.h>

// define the SPI port to be used, SPI2_HOST is the default as it should be present on most devices, on ESP32/ESP32-S3 and maybe others SPI3_HOST can also be used. Do not use SPI1 as this is connected to the onboard flash
#define SPIPort SPI2_HOST

// define Arduino-style macros
#define NOP() asm volatile("nop")

// create a new ESP-IDF hardware abstraction layer
// the HAL must inherit from the base RadioLibHal class
// and implement all of its virtual methods
// this is pretty much just copied from Arduino ESP32 core, as RadioLib uses arduino functions
class EspHal2 : public RadioLibHal {
   public:
    // default constructor - initializes the base HAL and any needed private members
    EspHal2(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, 0, 1,
                      GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE),
          spiSCK(sck),
          spiMISO(miso),
          spiMOSI(mosi) {}

    void init() override {
        // we only need to init the SPI here
        spiBegin();
    }

    void term() override {
        // we only need to stop the SPI here
        spiEnd();
    }

    // GPIO-related methods (pinMode, digitalWrite etc.) should check
    // RADIOLIB_NC as an alias for non-connected pins
    void pinMode(uint32_t pin, uint32_t mode) override {
        if (pin == RADIOLIB_NC) {
            return;
        }

        gpio_hal_context_t gpiohal;
        gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);

        gpio_config_t conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = (gpio_mode_t)mode,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = (gpio_int_type_t)gpiohal.dev->pin[pin].int_type,
        };
        gpio_config(&conf);
        return;
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == RADIOLIB_NC) {
            return;
        }

        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == RADIOLIB_NC) {
            return (0);
        }

        return (gpio_get_level((gpio_num_t)pin));
    }

    void attachInterrupt(uint32_t interruptNum, void (*interruptCb)(void),
                         uint32_t mode) override {
        if (interruptNum == RADIOLIB_NC) {
            return;
        }

        // Only install the isr service once
        if (!isr_initialized) {
            gpio_install_isr_service((int)ESP_INTR_FLAG_IRAM);
            isr_initialized = true;
        }
        gpio_set_intr_type((gpio_num_t)interruptNum,
                           (gpio_int_type_t)(mode & 0x7));

        // this uses function typecasting, which is not defined when the functions have different signatures
        // untested and might not work
        // TODO
        gpio_isr_handler_add((gpio_num_t)interruptNum,
                             (void (*)(void*))interruptCb, NULL);
        return;
    }

    void detachInterrupt(uint32_t interruptNum) override {
        if (interruptNum == RADIOLIB_NC) {
            return;
        }

        gpio_isr_handler_remove((gpio_num_t)interruptNum);
        gpio_wakeup_disable((gpio_num_t)interruptNum);
        gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
        gpio_uninstall_isr_service();
        isr_initialized = false;
        return;
    }

    void delay(unsigned long ms) override {
        vTaskDelay(ms / portTICK_PERIOD_MS);
        return;
    }

    void delayMicroseconds(unsigned long us) override {
        ets_delay_us(us);
        return;
    }

    unsigned long millis() override {
        return ((unsigned long)(esp_timer_get_time() / 1000ULL));
    }

    unsigned long micros() override {
        return ((unsigned long)(esp_timer_get_time()));
    }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
        if (pin == RADIOLIB_NC) {
            return (0);
        }

        this->pinMode(pin, GPIO_MODE_INPUT);
        uint32_t start = this->micros();
        uint32_t curtick = this->micros();

        while (this->digitalRead(pin) == state) {
            if ((this->micros() - curtick) > timeout) {
                return (0);
            }
        }

        return (this->micros() - start);
    }

    /// @brief this function initializes the SPI hardware used for the LoRa modem
    void spiBegin() {
        //if SPI is already initialized, return
        if (spi_initialized)
            return;

        spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = spiMISO;
        buscfg.mosi_io_num = spiMOSI;
        buscfg.sclk_io_num = spiSCK;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 0;

        spi_device_interface_config_t devcfg = {};
        devcfg.mode = 0;
        devcfg.clock_speed_hz = 2 * 1000 * 1000;  // 2MHz
        devcfg.spics_io_num = -1;
        devcfg.queue_size = 1;

        // Initialize SPI bus
        esp_err_t ret = spi_bus_initialize(SPIPort, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE("SPI", "Failed to initialize SPI bus: %s",
                     esp_err_to_name(ret));
        }

        // initialize SPI device
        ret = spi_bus_add_device(SPIPort, &devcfg, &spi);
        if (ret != ESP_OK) {
            ESP_LOGE("SPI", "Failed to add SPI device: %s",
                     esp_err_to_name(ret));
        }

        spi_initialized = true;
        return;
    }

    /// @brief not needed - in ESP32 Arduino core, this function repeats clock div, mode and bit order configuration
    void spiBeginTransaction() {
        // not needed - in ESP32 Arduino core, this function
        // repeats clock div, mode and bit order configuration
        return;
    }

    /// @brief this function transfers multiple bytes via SPI
    /// @param out
    /// @param len
    /// @param in
    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
        spi_transaction_t t = {};
        uint8_t inBuffer
            [len +
             3];  // Spi transactions are 4 byte aligned, this is needed to avoid DMA writing over the end of the buffer
        memset(&t, 0, sizeof(t));      // Zero out the transaction
        t.length = len * 8;            // Length is in bits
        t.tx_buffer = out;             // The data to send
        t.rx_buffer = inBuffer;        // The data to receive
        spi_device_transmit(spi, &t);  //cary out SPI transaction
        memcpy(in, inBuffer, len);  //copy the received data to the input buffer
        return;
    }

    void spiEndTransaction() {
        // nothing needs to be done here
        return;
    }

    void spiEnd() {
        spi_bus_remove_device(this->spi);
        spi_bus_free(SPIPort);
        spi_initialized = false;
        return;
    }

   private:
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_device_handle_t spi;
    bool spi_initialized = false;
    bool isr_initialized = false;
};

#endif