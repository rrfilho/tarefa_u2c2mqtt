#include <stdbool.h>

typedef void (*mqtt_handler)(const char* topic, const char* data);

void mqtt_client_init(char* host, char* username, char* password, bool unique_topic, mqtt_handler handler);
void mqtt_client_subscribe(char* topic);
void mqtt_client_unsubscribe(char* topic);
void mqtt_client_publish(char* topic, char* content);
void mqtt_client_stop();
bool mqtt_is_active();