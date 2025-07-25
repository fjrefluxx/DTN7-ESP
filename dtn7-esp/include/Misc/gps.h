/**
 * @file gps.h
 * @brief this code is oriented on the example provided with the nmea component, found here: https://github.com/igrr/libnmea-esp32/tree/main/example
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#pragma once
#include <sys/time.h>
#include <time.h>
#include "driver/uart.h"
#include "dtn7-esp.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gprmc.h"
#include "nmea.h"
#include "sdkconfig.h"

#if CONFIG_UseGPS

#define UART_NUM (uart_port_t) CONFIG_GpsUart
#define UART_RX_PIN CONFIG_GpsUartRX
#define UART_RX_BUF_SIZE (1024)

static char buf[UART_RX_BUF_SIZE + 512];
static size_t total_bytes;
static char* last_buf_end;

TaskHandle_t gpsUpdaterHandel = NULL;

/// @brief set up GPS UART as defined in menuconfig
void initUart() {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(
        uart_driver_install(UART_NUM, UART_RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    return;
}

/// @brief reads an NMEA message from the UART port
/// @param out_line_buf pointer to the beginning of the read message
/// @param out_line_len length of nmea message
void readNmeaLine(char** out_line_buf, size_t* out_line_len) {
    ESP_LOGI("readNmeaLine", "reading nmea");
    *out_line_buf = NULL;
    *out_line_len = 0;

    if (last_buf_end != NULL) {
        // if not all data has been read from the buffer previously, move this data to the beginning of the buffer
        ESP_LOGI("readNmeaLine", "moving previous data");
        size_t len_remaining = total_bytes - (last_buf_end - buf);
        ESP_LOGI("readNmeaLine", "len remaining:%u", len_remaining);
        memmove(buf, last_buf_end, len_remaining);
        last_buf_end = NULL;
        total_bytes = len_remaining;
    }

    int read_bytes = uart_read_bytes(
        UART_NUM, (uint8_t*)buf + total_bytes, UART_RX_BUF_SIZE - total_bytes,
        pdMS_TO_TICKS(100));  // read only as much data as fits into buffer
    if (read_bytes <= 0) {
        // no data has been read, return
        return;
    }
    total_bytes += read_bytes;  // length of data in buffer

    char* start =
        (char*)memchr(buf, '$', total_bytes);  // find start of nmea message
    if (start == NULL) {
        // no start symbol found, not a nmea message, return
        total_bytes = 0;
        return;
    }

    char* end = (char*)memchr(
        start, '\r', total_bytes - (start - buf));  // search end of message
    if (end == NULL || *(++end) != '\n') {
        // end is not in buffer or not followed by newline, therefore not valid
        return;
    }
    end++;

    end[-2] = NMEA_END_CHAR_1;
    end[-1] = NMEA_END_CHAR_2;

    *out_line_buf = start;
    *out_line_len = end - start;

    if (end < buf + total_bytes) {
        // is data left after the message? if yes, save relevant information to read it later
        last_buf_end = end;
    }
    else {
        total_bytes = 0;
    }
    return;
}

/// @brief task which periodically updates the nodes position using the gps module
/// @param param
void gpsUpdater(void* param) {
    ESP_LOGI("gpsUpdater", "Task Started");

    while (true) {
        // wait for the configured interval before updating position
        vTaskDelay((CONFIG_GpsUpdateInterval * 1000) / portTICK_PERIOD_MS);

        // We dont want old GPS messages from the buffer.
        // Remove messages older than 1 second. vTaskDelay allows the scheduler to run other tasks in the meantime.
        uart_flush(UART_NUM);
        vTaskDelay((1 * 1000) / portTICK_PERIOD_MS);

        // prepare required data structures
        char buf[64];
        nmea_s* data;
        struct tm time;
        char* start;
        size_t length;
        int counter = 0;

        // read from GPS until required message has been read
        while (true) {
            readNmeaLine(&start, &length);
            // if the line is empty, try again
            if (length == 0) {
                continue;
            }

            data = nmea_parse(start, length, 0);
            if (data == NULL)
                continue;

            // check if message is a GPRMC message, containing time and position
            if (NMEA_GPRMC == data->type) {
                nmea_gprmc_s* pos = (nmea_gprmc_s*)data;
                time = pos->date_time;
                // now set the position of the node, if it is contained in nmea sentence
                if (pos->latitude.degrees != 0 || pos->latitude.minutes != 0 ||
                    pos->longitude.degrees != 0 ||
                    pos->longitude.minutes != 0) {
                    DTN7::localNode->hasPos = true;
                    if (pos->latitude.cardinal == 'S')
                        DTN7::localNode->position.first =
                            -(pos->latitude.degrees +
                              (pos->latitude.minutes) / 60);
                    else
                        DTN7::localNode->position.first =
                            pos->latitude.degrees +
                            (pos->latitude.minutes) / 60;
                    if (pos->longitude.cardinal == 'W')
                        DTN7::localNode->position.second =
                            -(pos->longitude.degrees +
                              (pos->longitude.minutes) / 60);
                    else
                        DTN7::localNode->position.first =
                            pos->longitude.degrees +
                            (pos->longitude.minutes) / 60;
                }

                // position update finished, exit loop
                break;
            }
            counter++;
            if (counter > 10)
                break;  // used to limit number of messages read if no gprmc is received
        }
        // cleanup data
        nmea_free(data);
        uart_flush(UART_NUM);
        memset(buf, 0, sizeof(buf));

        // the following would allow to resynchronize time, due to potentially large deviations. because of the way the messages are read from the GPS, this is disabled
        // time_t timeSeconds =mktime(&time); // time since 1970 in seconds
        // if(timeSeconds<0||lastTime == timeSeconds)continue;
        // ESP_LOGI("gpsUpdater" ,"Time: %lli", timeSeconds);
        // lastTime = timeSeconds;
        // DTN time is the time since 2000-01-01, we need to transform this, by adding 946 ,684 ,800 seconds
        // struct timeval timeval;
        // timeval.tv_sec = timeSeconds+946684800;
        // timeval.tv_usec = 0;
        // settimeofday(&timeval ,NULL);
    }
}

/// @brief Sets up GPS hardware, task to read position and synchronize node clock to DTN time.
/// Reads NMEA sentences until a position and time is found; therefore, potentially blocks for a long time!
void initializeGPS() {
    ESP_LOGI("initializeGPS", "initialising");
    initUart();
    struct tm time;
    while (true) {
        // prepare required data structures
        char buf[32];
        nmea_s* data;

        char* start;
        size_t length;

        // if the line is empty, try again
        readNmeaLine(&start, &length);
        if (length == 0) {
            continue;
        }
        data = nmea_parse(start, length, 0);

        // check if message is a GPRMC message, containing time and position
        if (NMEA_GPRMC == data->type) {
            ESP_LOGI("initializeGPS", "GPRMC sentence");
            nmea_gprmc_s* pos = (nmea_gprmc_s*)data;
            time = pos->date_time;
            strftime(buf, sizeof(buf), "%d %b %T %Y", &pos->date_time);
            ESP_LOGI("initializeGPS", "Date & Time: %s", buf);

            // now set the position of the node, if it is already contained in nmea sentence
            if (pos->latitude.degrees != 0 || pos->latitude.minutes != 0 ||
                pos->longitude.degrees != 0 || pos->longitude.minutes != 0) {
                DTN7::localNode->hasPos = true;
                if (pos->latitude.cardinal == 'S')
                    DTN7::localNode->position.first =
                        -(pos->latitude.degrees + (pos->latitude.minutes) / 60);
                else
                    DTN7::localNode->position.first =
                        pos->latitude.degrees + (pos->latitude.minutes) / 60;
                if (pos->longitude.cardinal == 'W')
                    DTN7::localNode->position.second = -(
                        pos->longitude.degrees + (pos->longitude.minutes) / 60);
                else
                    DTN7::localNode->position.first =
                        pos->longitude.degrees + (pos->longitude.minutes) / 60;
            }
            break;
        }
        vTaskDelay(1);  // needed to avoid watchdog
        nmea_free(data);
    }

    time_t timeSeconds = mktime(&time);  // time since 1970 in seconds

    // DTN time is the time since 2000-01-01, we need to transform this, by adding 946'684'800 seconds
    struct timeval timeval;
    timeval.tv_sec = timeSeconds + 946684800;
    timeval.tv_usec = 0;
    settimeofday(&timeval, NULL);
    xTaskCreate(&gpsUpdater, "GpsUpdater", 3000, NULL, 2, &gpsUpdaterHandel);
    uart_flush(UART_NUM);
    return;
}

/// @brief deletes the GPS position updating Task
void deinitializeGPS() {
    vTaskDelete(gpsUpdaterHandel);
    return;
}

#endif

#pragma GCC diagnostic pop
