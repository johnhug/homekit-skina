/*
* HomeKit interface for PWM control 12v IKEA Dioder leds.
* 
*/
#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "multipwm.h"

#define LED_INBUILT_GPIO    2
#define PWM_GPIO            4 // D2

#define PWM_MAX 1024

// pwm
pwm_info_t pwm_info;

// Home Kit variables
bool hk_on = false;
int hk_brightness = 100;

int map(int value, int in_lower, int in_upper, int out_lower, int out_upper) {
  return out_lower + ((out_upper - out_lower) / (in_upper - in_lower)) * (value - in_lower);
}

void update_pwm() {
    uint16_t pwm_pin_value = 0;

    if (hk_on) {
        pwm_pin_value = map(hk_brightness, 0, 100, 0, UINT16_MAX);
    }

    multipwm_set_duty(&pwm_info, 0, pwm_pin_value);
}

void init_pwm() {
    pwm_info.channels = 1;

    multipwm_init(&pwm_info);
    multipwm_set_pin(&pwm_info, 0, PWM_GPIO);

    multipwm_start(&pwm_info);
    
    update_pwm();
}

void identify_task(void *_args) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 9; j++) {
            gpio_write(LED_INBUILT_GPIO, 0);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_write(LED_INBUILT_GPIO, 1);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    xTaskCreate(identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t on_get() {
    return HOMEKIT_BOOL(hk_on);
}

void on_set(homekit_value_t value) {
    hk_on = value.bool_value;

    update_pwm();
}

homekit_value_t brightness_get() {
    return HOMEKIT_INT(hk_brightness);
}

void brightness_set(homekit_value_t value) {
    hk_brightness = value.int_value;

    update_pwm();
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Haku");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id = 1,
        .category = homekit_accessory_category_lightbulb,
        .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "John Hug"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "1302a6bab770"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Skina-Dimmer"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.5"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Haku"),
            HOMEKIT_CHARACTERISTIC(
                ON, false,
            .getter = on_get,
            .setter = on_set
                ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
            .getter = brightness_get,
            .setter = brightness_set
                ),
            NULL
        }),
        NULL
    }),
    NULL
};

void on_homekit_event(homekit_event_t event) {
    if (event == HOMEKIT_EVENT_CLIENT_VERIFIED) {
        identify(HOMEKIT_INT(1));    
    }
}

homekit_server_config_t config = {
    .accessories = accessories,
    .category = homekit_accessory_category_lightbulb,
    .password = "408-03-437",
    .setupId = "35A9",
    .on_event = on_homekit_event
};

void on_wifi_ready() {
    identify(HOMEKIT_INT(1));    
}

void user_init(void) {
    gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);
    init_pwm();

    wifi_config_init("Haku", NULL, on_wifi_ready);

    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = 4 + 1 + 6 + 1;
    char *name_value = malloc(name_len);
    snprintf(name_value, name_len, "Haku-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);
 
    homekit_server_init(&config);
}
