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

bool _entrance_light = false;
bool _room_light = false;
bool _kitchen_light = false;
bool _bedroom_light = false;
unsigned int _temperature = 20;
bool _alarm = false;

char* get_state(bool status);

void alarm_turn_on() {
    _alarm = true;
    buzzer_set(true);
    mqtt_client_publish("/alarm/state", get_state(_alarm));
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

char* to_string(int value) {
    static char value_str[5];
    itoa(value, value_str, 10);
    return value_str;
}

void mqtt_request_handler(const char* topic, const char* data) {
    if (strstr(topic, "/lights/entrance") != NULL) {
        _entrance_light = !_entrance_light;
        led_matrix_lights(_entrance_light, _room_light, _kitchen_light, _bedroom_light);
        mqtt_client_publish("/lights/entrance/state", get_state(_entrance_light));
        return;
    }
    if (strstr(topic, "/lights/bedroom") != NULL) {
        _bedroom_light = !_bedroom_light;
        led_matrix_lights(_entrance_light, _room_light, _kitchen_light, _bedroom_light);
        mqtt_client_publish("/lights/bedroom/state", get_state(_bedroom_light));
        return;
    }
    if (strstr(topic, "/lights/room") != NULL) {
        _room_light = !_room_light;
        led_matrix_lights(_entrance_light, _room_light, _kitchen_light, _bedroom_light);
        mqtt_client_publish("/lights/room/state", get_state(_room_light));
        return;
    }
    if (strstr(topic, "/lights/kitchen") != NULL) {
        _kitchen_light = !_kitchen_light;
        led_matrix_lights(_entrance_light, _room_light, _kitchen_light, _bedroom_light);
        mqtt_client_publish("/lights/kitchen/state", get_state(_kitchen_light));
        return;
    }
    if (strstr(topic, "/temperature") != NULL) {
        _temperature = atoi(data);
        if (_temperature > 35)  _temperature = 35;
        if (_temperature < 15)  _temperature = 15;
        leds_set_red(_temperature);
        mqtt_client_publish("/temperature/state", to_string(_temperature));
        return;
    }
    if (strstr(topic, "/alarm") != NULL) {
        _alarm = false;
        buzzer_set(false);
        mqtt_client_publish("/alarm/state", get_state(_alarm));
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
    mqtt_client_publish("/lights/entrance/state", get_state(_entrance_light));
    mqtt_client_publish("/lights/room/state", get_state(_room_light));
    mqtt_client_publish("/lights/kitchen/state", get_state(_kitchen_light));
    mqtt_client_publish("/lights/bedroom/state", get_state(_bedroom_light));
    mqtt_client_publish("/temperature/state", to_string(_temperature));
    mqtt_client_publish("/alarm/state", get_state(_alarm));
    mqtt_client_subscribe("/lights/+");
    mqtt_client_subscribe("/temperature");
    mqtt_client_subscribe("/alarm");

    while (mqtt_is_active()) {
        wifi_keep_active();
        sleep_ms(5000);
    }
    wifi_deinit();
    return 0;
}