#include <stdbool.h>

bool wifi_is_init_success();
void wifi_init();
void wifi_deinit();
void wifi_set_led(bool status);
bool wifi_connect_to(char* ssid, char* password);
char* wifi_ip();
void wifi_keep_active();