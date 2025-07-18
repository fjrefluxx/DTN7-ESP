#ifndef ESP_HAL_H
#define ESP_HAL_H

/**
 * @file EspHal.h
 * @brief The content of this file is taken from https://github.com/jgromes/RadioLib/blob/master/examples/NonArduino/ESP-IDF/main/EspHal.h
 */

// include RadioLib
#include <RadioLib.h>

// this example works on ESP32 and maybe on on ESP32S3 etc. but not tested, will probably not work on esp32s2
// if you need high portability, you should probably use Arduino anyway ...

// include all the dependencies
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "hal/clk_gate_ll.h"
#include "hal/gpio_hal.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc.h"
#include "soc/spi_reg.h"
#include "soc/spi_struct.h"

#include "esp_intr_alloc.h"
#include "esp_system.h"

#if CONFIG_IDF_TARGET_ESP32  // ESP32/PICO-D4
#include "esp32/rom/ets_sys.h"
#include "esp32/rom/gpio.h"
#include "soc/dport_reg.h"
#elif CONFIG_IDF_TARGET_ESP32S2
#include "esp32s2/rom/ets_sys.h"
#include "esp32s2/rom/gpio.h"
#include "soc/dport_reg.h"
#elif CONFIG_IDF_TARGET_ESP32S3
#include "esp32s3/rom/ets_sys.h"
#include "esp32s3/rom/gpio.h"
#include "soc/dport_reg.h"
#elif CONFIG_IDF_TARGET_ESP32C2
#include "esp32c2/rom/ets_sys.h"
#include "esp32c2/rom/gpio.h"
#elif CONFIG_IDF_TARGET_ESP32C3
#include "esp32c3/rom/ets_sys.h"
#include "esp32c3/rom/gpio.h"
#elif CONFIG_IDF_TARGET_ESP32C6
#include "esp32c6/rom/ets_sys.h"
#include "esp32c6/rom/gpio.h"
#elif CONFIG_IDF_TARGET_ESP32H2
#include "esp32h2/rom/ets_sys.h"
#include "esp32h2/rom/gpio.h"
#else
#error Target CONFIG_IDF_TARGET is not supported
#endif

struct spi_struct_t {
    spi_dev_t* dev;
    uint8_t num;
    int8_t sck;
    int8_t miso;
    int8_t mosi;
    int8_t ss;
};

// define Arduino-style macros
#define LOW (0x0)
#define HIGH (0x1)
#define INPUT (0x01)
#define OUTPUT (0x03)
#define RISING (0x01)
#define FALLING (0x02)
#define NOP() asm volatile("nop")

#define MATRIX_DETACH_OUT_SIG (0x100)
#define MATRIX_DETACH_IN_LOW_PIN (0x30)

// all of the following is needed to calculate SPI clock divider
#define ClkRegToFreq(reg) \
    (apb_freq / (((reg)->clkdiv_pre + 1) * ((reg)->clkcnt_n + 1)))

#elif CONFIG_IDF_TARGET_ESP32S3
// ESP32S3
#define SPI_COUNT (2)

#define SPI_CLK_IDX(p) \
    ((p == 0) ? FSPICLK_OUT_IDX : ((p == 1) ? SPI3_CLK_OUT_IDX : 0))
#define SPI_MISO_IDX(p) \
    ((p == 0) ? FSPIQ_OUT_IDX : ((p == 1) ? SPI3_Q_OUT_IDX : 0))
#define SPI_MOSI_IDX(p) \
    ((p == 0) ? FSPID_IN_IDX : ((p == 1) ? SPI3_D_IN_IDX : 0))

#define SPI_HSPI_SS_IDX(n) \
    ((n == 0) ? SPI3_CS0_OUT_IDX : ((n == 1) ? SPI3_CS1_OUT_IDX : 0))
#define SPI_FSPI_SS_IDX(n) \
    ((n == 0) ? FSPICS0_OUT_IDX : ((n == 1) ? FSPICS1_OUT_IDX : 0))
#define SPI_SS_IDX(p, n) \
    ((p == 0) ? SPI_FSPI_SS_IDX(n) : ((p == 1) ? SPI_HSPI_SS_IDX(n) : 0))

#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
// ESP32C3
#define SPI_COUNT (1)

#define SPI_CLK_IDX(p) FSPICLK_OUT_IDX
#define SPI_MISO_IDX(p) FSPIQ_OUT_IDX
#define SPI_MOSI_IDX(p) FSPID_IN_IDX

#define SPI_SPI_SS_IDX(n)                   \
    ((n == 0) ? FSPICS0_OUT_IDX             \
              : ((n == 1) ? FSPICS1_OUT_IDX \
                          : ((n == 2) ? FSPICS2_OUT_IDX : FSPICS0_OUT_IDX)))
#define SPI_SS_IDX(p, n) SPI_SPI_SS_IDX(n)

#else
// ESP32
#define SPI_COUNT (4)

#define SPI_CLK_IDX(p)                                  \
    ((p == 0) ? SPICLK_OUT_IDX                          \
              : ((p == 1) ? SPICLK_OUT_IDX              \
                          : ((p == 2) ? HSPICLK_OUT_IDX \
                                      : ((p == 3) ? VSPICLK_OUT_IDX : 0))))
#define SPI_MISO_IDX(p)                               \
    ((p == 0) ? SPIQ_OUT_IDX                          \
              : ((p == 1) ? SPIQ_OUT_IDX              \
                          : ((p == 2) ? HSPIQ_OUT_IDX \
                                      : ((p == 3) ? VSPIQ_OUT_IDX : 0))))
#define SPI_MOSI_IDX(p)                              \
    ((p == 0) ? SPID_IN_IDX                          \
              : ((p == 1) ? SPID_IN_IDX              \
                          : ((p == 2) ? HSPID_IN_IDX \
                                      : ((p == 3) ? VSPID_IN_IDX : 0))))

#define SPI_SPI_SS_IDX(n)                  \
    ((n == 0) ? SPICS0_OUT_IDX             \
              : ((n == 1) ? SPICS1_OUT_IDX \
                          : ((n == 2) ? SPICS2_OUT_IDX : SPICS0_OUT_IDX)))
#define SPI_HSPI_SS_IDX(n)                  \
    ((n == 0) ? HSPICS0_OUT_IDX             \
              : ((n == 1) ? HSPICS1_OUT_IDX \
                          : ((n == 2) ? HSPICS2_OUT_IDX : HSPICS0_OUT_IDX)))
#define SPI_VSPI_SS_IDX(n)                  \
    ((n == 0) ? VSPICS0_OUT_IDX             \
              : ((n == 1) ? VSPICS1_OUT_IDX \
                          : ((n == 2) ? VSPICS2_OUT_IDX : VSPICS0_OUT_IDX)))
#define SPI_SS_IDX(p, n)                                   \
    ((p == 0) ? SPI_SPI_SS_IDX(n)                          \
              : ((p == 1) ? SPI_SPI_SS_IDX(n)              \
                          : ((p == 2) ? SPI_HSPI_SS_IDX(n) \
                                      : ((p == 3) ? SPI_VSPI_SS_IDX(n) : 0))))

#endif

typedef union {
    uint32_t value;
    struct {
        uint32_t clkcnt_l : 6;
        uint32_t clkcnt_h : 6;
        uint32_t clkcnt_n : 6;
        uint32_t clkdiv_pre : 13;
        uint32_t clk_equ_sysclk : 1;
    };
} spiClk_t;

uint32_t getApbFrequency() {
    rtc_cpu_freq_config_t conf;
    rtc_clk_cpu_freq_get_config(&conf);

    if (conf.freq_mhz >= 80) {
        return (80 * MHZ);
    }

    return ((conf.source_freq_mhz * MHZ) / conf.div);
}

uint32_t spiFrequencyToClockDiv(uint32_t freq) {
    uint32_t apb_freq = getApbFrequency();
    if (freq >= apb_freq) {
        return SPI_CLK_EQU_SYSCLK;
    }

    const spiClk_t minFreqReg = {0x7FFFF000};
    uint32_t minFreq = ClkRegToFreq((spiClk_t*)&minFreqReg);
    if (freq < minFreq) {
        return minFreqReg.value;
    }

    uint8_t calN = 1;
    spiClk_t bestReg = {0};
    int32_t bestFreq = 0;
    while (calN <= 0x3F) {
        spiClk_t reg = {0};
        int32_t calFreq;
        int32_t calPre;
        int8_t calPreVari = -2;

        reg.clkcnt_n = calN;

        while (calPreVari++ <= 1) {
            calPre =
                (((apb_freq / (reg.clkcnt_n + 1)) / freq) - 1) + calPreVari;
            if (calPre > 0x1FFF) {
                reg.clkdiv_pre = 0x1FFF;
            }
            else if (calPre <= 0) {
                reg.clkdiv_pre = 0;
            }
            else {
                reg.clkdiv_pre = calPre;
            }
            reg.clkcnt_l = ((reg.clkcnt_n + 1) / 2);
            calFreq = ClkRegToFreq(&reg);
            if (calFreq == (int32_t)freq) {
                memcpy(&bestReg, &reg, sizeof(bestReg));
                break;
            }
            else if (calFreq < (int32_t)freq) {
                if (RADIOLIB_ABS(freq - calFreq) <
                    RADIOLIB_ABS(freq - bestFreq)) {
                    bestFreq = calFreq;
                    memcpy(&bestReg, &reg, sizeof(bestReg));
                }
            }
        }
        if (calFreq == (int32_t)freq) {
            break;
        }
        calN++;
    }
    return (bestReg.value);
}

// create a new ESP-IDF hardware abstraction layer
// the HAL must inherit from the base RadioLibHal class
// and implement all of its virtual methods
// this is pretty much just copied from Arduino ESP32 core
class EspHal : public RadioLibHal {
   public:
    // default constructor - initializes the base HAL and any needed private members
    EspHal(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          spiSCK(sck),
          spiMISO(miso),
          spiMOSI(mosi) {
        gpio_install_isr_service((int)ESP_INTR_FLAG_IRAM);
    }

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

        gpio_set_intr_type((gpio_num_t)interruptNum,
                           (gpio_int_type_t)(mode & 0x7));

        // this uses function typecasting, which is not defined when the functions have different signatures
        // untested and might not work
        gpio_isr_handler_add((gpio_num_t)interruptNum,
                             (void (*)(void*))interruptCb, NULL);
    }

    void detachInterrupt(uint32_t interruptNum) override {
        if (interruptNum == RADIOLIB_NC) {
            return;
        }

        gpio_isr_handler_remove((gpio_num_t)interruptNum);
        gpio_wakeup_disable((gpio_num_t)interruptNum);
        gpio_set_intr_type((gpio_num_t)interruptNum, GPIO_INTR_DISABLE);
    }

    void delay(unsigned long ms) override {
        vTaskDelay(ms / portTICK_PERIOD_MS);
    }

    void delayMicroseconds(unsigned long us) override {
        uint64_t m = (uint64_t)esp_timer_get_time();
        if (us) {
            uint64_t e = (m + us);
            if (m > e) {  // overflow
                while ((uint64_t)esp_timer_get_time() > e) {
                    NOP();
                }
            }
            while ((uint64_t)esp_timer_get_time() < e) {
                NOP();
            }
        }
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

        this->pinMode(pin, INPUT);
        uint32_t start = this->micros();
        uint32_t curtick = this->micros();

        while (this->digitalRead(pin) == state) {
            if ((this->micros() - curtick) > timeout) {
                return (0);
            }
        }

        return (this->micros() - start);
    }

    void spiBegin() {

#if CONFIG_IDF_TARGET_ESP32S2
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI3_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI3_RST);
#elif CONFIG_IDF_TARGET_ESP32S3
        periph_ll_reset(PERIPH_SPI3_MODULE);
        periph_ll_enable_clk_clear_rst(PERIPH_SPI3_MODULE);
#elif CONFIG_IDF_TARGET_ESP32
        DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI2_CLK_EN);
        DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI2_RST);
#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        periph_ll_reset(PERIPH_SPI2_MODULE);
        periph_ll_enable_clk_clear_rst(PERIPH_SPI2_MODULE);
#endif

        // enable peripheral
        // DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI2_CLK_EN);
        // DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI2_RST);

        // reset the control struct
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
        this->spi->slave.trans_done = 0;
#endif
        this->spi->slave.val = 0;

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
    CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        this->spi->misc.val = 0;
#else
        this->spi->pin.val = 0;
#endif
        this->spi->user.val = 0;
        this->spi->user1.val = 0;
        this->spi->ctrl.val = 0;
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
        this->spi->ctrl1.val = 0;
        this->spi->ctrl2.val = 0;
#else
        this->spi->clk_gate.val = 0;
        this->spi->dma_conf.val = 0;
        this->spi->dma_conf.rx_afifo_rst = 1;
        this->spi->dma_conf.buf_afifo_rst = 1;
#endif

        this->spi->clock.val = 0;

#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C6 || \
    CONFIG_IDF_TARGET_ESP32H2
        this->spi->clk_gate.clk_en = 1;
        this->spi->clk_gate.mst_clk_sel = 1;
        this->spi->clk_gate.mst_clk_active = 1;
#if !CONFIG_IDF_TARGET_ESP32C6 && !CONFIG_IDF_TARGET_ESP32H2
        this->spi->dma_conf.tx_seg_trans_clr_en = 1;
        this->spi->dma_conf.rx_seg_trans_clr_en = 1;
        this->spi->dma_conf.dma_seg_trans_en = 0;
#endif
#endif

        this->spi->user.usr_mosi = 1;
        this->spi->user.usr_miso = 1;
        this->spi->user.doutdin = 1;
        int i;
        for (i = 0; i < 16; i++) {
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
            this->spi->dev->data_buf[i].val = 0x00000000;
#else
            this->spi->data_buf[i] = 0x00000000;
#endif
        }

        // set SPI mode 0
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
    CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        this->spi->misc.ck_idle_edge = 0;
#else
        this->spi->pin.ck_idle_edge = 0;
#endif
        this->spi->user.ck_out_edge = 0;

        // set bit order to MSB first
        this->spi->ctrl.wr_bit_order = 0;
        this->spi->ctrl.rd_bit_order = 0;

        // set the clock
        this->spi->clock.val = spiFrequencyToClockDiv(2000000);

        // initialize pins
        this->pinMode(this->spiSCK, OUTPUT);
        this->pinMode(this->spiMISO, INPUT);
        this->pinMode(this->spiMOSI, OUTPUT);
#if CONFIG_IDF_TARGET_ESP32
        gpio_matrix_out(this->spiSCK, HSPICLK_OUT_IDX, false, false);
        gpio_matrix_in(this->spiMISO, HSPIQ_OUT_IDX, false);
        gpio_matrix_out(this->spiMOSI, HSPID_IN_IDX, false, false);
#elif CONFIG_IDF_TARGET_ESP32S3
        gpio_matrix_out(this->spiSCK, FSPICLK_OUT_IDX, false, false);
        gpio_matrix_in(this->spiMISO, FSPIQ_OUT_IDX, false);
        gpio_matrix_out(this->spiMOSI, FSPID_IN_IDX, false, false);
#elif CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        gpio_matrix_out(this->spiSCK, FSPICLK_OUT_IDX, false, false);
        gpio_matrix_in(this->spiMISO, FSPIQ_OUT_IDX, false);
        gpio_matrix_out(this->spiMOSI, FSPID_IN_IDX, false, false);
#endif
    }

    void spiBeginTransaction() {
        // not needed - in ESP32 Arduino core, this function
        // repeats clock div, mode and bit order configuration
    }

    uint8_t spiTransferByte(uint8_t data) {
#if CONFIG_IDF_TARGET_ESP32
        this->spi->mosi_dlen.usr_mosi_dbitlen = 7;
        this->spi->miso_dlen.usr_miso_dbitlen = 7;
#else
        this->spi->ms_dlen.ms_data_bitlen = 7;
        this->spi->ms_dlen.ms_data_bitlen = 7;
#endif
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        this->spi->dev->data_buf[0].val = data;
#else
        this->spi->data_buf[0] = data;
#endif
#if CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32C3 || \
    CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C6 || \
    CONFIG_IDF_TARGET_ESP32H2
        this->spi->cmd.update = 1;
        while (this->spi->cmd.update)
            ;
#endif
        this->spi->cmd.usr = 1;
        while (this->spi->cmd.usr)
            ;
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
        data = this->spi->dev->data_buf[0].val & 0xFF;
#else
        data = this->spi->data_buf[0] & 0xFF;
#endif
        return data;
    }

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) {
        for (size_t i = 0; i < len; i++) {
            in[i] = this->spiTransferByte(out[i]);
        }
    }

    void spiEndTransaction() {
        // nothing needs to be done here
    }

    void spiEnd() {
        // detach pins
        gpio_matrix_out(this->spiSCK, MATRIX_DETACH_OUT_SIG, false, false);
        gpio_matrix_in(this->spiMISO, MATRIX_DETACH_IN_LOW_PIN, false);
        gpio_matrix_out(this->spiMOSI, MATRIX_DETACH_OUT_SIG, false, false);
    }

   private:
    // the HAL can contain any additional private members
    int8_t spiSCK;
    int8_t spiMISO;
    int8_t spiMOSI;
    spi_dev_t* spi = (volatile spi_dev_t*)(DR_REG_SPI2_BASE);
};
