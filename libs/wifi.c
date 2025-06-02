#include "wifi.h"
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"

#define WIFI_LED_PIN CYW43_WL_GPIO_LED_PIN
#define SUCCESS 0
#define TIMEOUT_WIFI_CONNECTION_MS 30000

static int _initialization = -1;

bool wifi_is_init_success() {
    return _initialization == SUCCESS;
}

void wifi_init() {
    _initialization = cyw43_arch_init();
    if (!wifi_is_init_success()) return;
    cyw43_arch_enable_sta_mode();
    cyw43_arch_gpio_put(WIFI_LED_PIN, false);
}

void wifi_deinit() {
    cyw43_arch_deinit();
}

void wifi_set_led(bool status) {
    cyw43_arch_gpio_put(WIFI_LED_PIN, status);
}

bool wifi_connect_to(char* ssid, char* password) {
    return cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, TIMEOUT_WIFI_CONNECTION_MS);
}

char* wifi_ip() {
    static char ip[15] = "IP Error";
    if (netif_default) {
        char* ipa = ipaddr_ntoa(&netif_default->ip_addr);
        strcpy(ip, ipa);
    }
    return ip;
}

void wifi_keep_active() {
    cyw43_arch_poll();
}
