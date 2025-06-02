#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "libs/buttons.h"
#include "libs/leds.h"
#include "libs/wifi.h"
#include "libs/mqtt_client.h"
#include "libs/display.h"
#include "libs/led_matrix.h"
#include "libs/buzzer.h"

// Credenciais WIFI - Tome cuidado ao publicar no github!
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MQTT_SERVER "192.168.0.100"
#define MQTT_USERNAME "ronaldo"
#define MQTT_PASSWORD "ronaldo"

bool entrance_light = false;
bool room_light = false;
bool kitchen_light = false;
bool bedroom_light = false;
unsigned int temperature = 20;
bool alarm = false;

void alarm_turn_on() {
    alarm = true;
    buzzer_set(true);
}

void init() {
    stdio_init_all();
    buttons_init(alarm_turn_on);
    leds_init();
    led_matrix_init();
    buzzer_init();
    wifi_init();
    display_init();
}

char* get_state(bool status) {
    return status ? "On" : "Off";
}

void mqtt_request_handler(const char* topic, const char* data) {
    if (strstr(topic, "/lights/entrance") != NULL) {
        entrance_light = !entrance_light;
        led_matrix_lights(entrance_light, room_light, kitchen_light, bedroom_light);
        mqtt_client_publish("/lights/entrance/state", get_state(entrance_light));
        return;
    }
    if (strstr(topic, "/lights/bedroom") != NULL) {
        bedroom_light = !bedroom_light;
        led_matrix_lights(entrance_light, room_light, kitchen_light, bedroom_light);
        mqtt_client_publish("/lights/bedroom/state", get_state(bedroom_light));
        return;
    }
    if (strstr(topic, "/lights/room") != NULL) {
        room_light = !room_light;
        led_matrix_lights(entrance_light, room_light, kitchen_light, bedroom_light);
        mqtt_client_publish("/lights/room/state", get_state(room_light));
        return;
    }
    if (strstr(topic, "/lights/kitchen") != NULL) {
        kitchen_light = !kitchen_light;
        led_matrix_lights(entrance_light, room_light, kitchen_light, bedroom_light);
        mqtt_client_publish("/lights/kitchen/state", get_state(kitchen_light));
        return;
    }
    if (strstr(topic, "/temperature") != NULL) {
        int temperature = atoi(data);
        if (temperature > 35)  temperature = 35;
        if (temperature < 15)  temperature = 15;
        leds_set_red(temperature);
        return;
    }
    if (strstr(topic, "alarm") != NULL) {
        alarm = false;
        buzzer_set(false);
        return;
    }
};

int main() {
    init();
    if (!wifi_is_init_success()) {
        display_set_message("Falha ao inicializar Wi-Fi");
        sleep_ms(1000);
        return -1;
    }
    display_set_message("Conectando ao Wi-Fi...");
    if (wifi_connect_to(WIFI_SSID, WIFI_PASSWORD)) {
        display_set_message("Falha ao conectar ao Wi-Fi");
        sleep_ms(1000);
        return -1;
    }
    display_set_message("Conectado ao Wi-Fi");
    sleep_ms(1000);
    display_set_message(wifi_ip());
    
    mqtt_client_init(MQTT_SERVER, MQTT_USERNAME, MQTT_PASSWORD, false, mqtt_request_handler);
    mqtt_client_subscribe("/lights/+");
    mqtt_client_subscribe("/temperature");
    mqtt_client_subscribe("/alarm");

    while (mqtt_is_active()) 
        wifi_keep_active();
    wifi_deinit();
    return 0;
}