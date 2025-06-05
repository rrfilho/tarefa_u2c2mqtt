#include "mqtt_client.h"
#include <stdbool.h>
#include "pico/cyw43_arch.h"        // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "pico/unique_id.h"         // Biblioteca com recursos para trabalhar com os pinos GPIO do Raspberry Pi Pico
#include "lwip/apps/mqtt.h"         // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h"    // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"               // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"         // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} mqtt_client;

#define MQTT_KEEP_ALIVE_SECONDS 60

// QoS - mqtt_subscribe: At most once (QoS 0); At least once (QoS 1); Exactly once (QoS 2);
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1
#define MQTT_DEVICE_NAME "pico"

static mqtt_client _state;
static bool _unique_topic = false; // Definir como true para adicionar o nome do cliente aos tópicos, para suportar vários dispositivos que utilizam o mesmo servidor
static mqtt_handler _handler;

static char* generate_card_id() {
    static char card_id[5];
    pico_get_unique_board_id_string(card_id, sizeof(card_id));
    for(int i = 0; i < sizeof(card_id) - 1; i++) card_id[i] = tolower(card_id[i]);
    return card_id;
}

static char* generate_identifier() {
    char* card_id = generate_card_id();
    static char identifier[sizeof(MQTT_DEVICE_NAME) + sizeof(card_id) - 1];
    memcpy(&identifier[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&identifier[sizeof(MQTT_DEVICE_NAME) - 1], card_id, sizeof(card_id) - 1);
    identifier[sizeof(identifier) - 1] = 0;
    return identifier;
}

static const char* full_topic(const char* name) {
    if (!_unique_topic) return name;
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", _state.mqtt_client_info.client_id, name);
    return full_topic;
}

static void set_client_info(char* username, char* password) {
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(MQTT_WILL_TOPIC), sizeof(will_topic));
    _state.mqtt_client_info.client_id = generate_identifier();
    _state.mqtt_client_info.client_user = username;
    _state.mqtt_client_info.client_pass = password;
    _state.mqtt_client_info.will_topic = will_topic;
    _state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_SECONDS;
    _state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    _state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    _state.mqtt_client_info.will_retain = true;
}

static void dns_found(const char* hostname, const ip_addr_t* ipaddr, __unused void* arg) {
    if (ipaddr) _state.mqtt_server_address = *ipaddr;
    else panic("dns request failed");
}

static err_t resolve_dns(char* host) {
    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(host, &_state.mqtt_server_address, dns_found, &_state);
    cyw43_arch_lwip_end();
    return err;
}

static void on_subscribe(__unused void* arg, err_t err) {
    if (err == ERR_OK) _state.subscribe_count++;
    else panic("subscribe request failed %d", err);
}

static void on_unsubscribe(__unused void* arg, err_t err) {
    if (err != ERR_OK) panic("unsubscribe request failed %d", err);
    _state.subscribe_count--;
    if (_state.subscribe_count <= 0 && _state.stop_client) mqtt_disconnect(_state.mqtt_client_inst);
}

static void on_publish(__unused void* arg, err_t err) {
    if (err != ERR_OK) {
        printf("pub_request_cb failed %d", err);
    }
}

void mqtt_client_publish(char* topic, char* content) {
    mqtt_publish(_state.mqtt_client_inst, full_topic(topic), content, strlen(content), MQTT_WILL_QOS, true, on_publish, NULL);
}

static void on_connection(mqtt_client_t* client, __unused void* arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        _state.connect_done = true;
        mqtt_client_publish((char*) _state.mqtt_client_info.will_topic, "1");
        return;
    }
    if (status == MQTT_CONNECT_DISCONNECTED && _state.connect_done) {
        return;
    }
    panic("Unexpected status or Failed to connect to mqtt server");
}

static void on_topic_incoming(__unused void* arg, const char* topic, u32_t tot_len) {
    strncpy(_state.topic, topic, sizeof(_state.topic));
}

static void on_data_incoming(__unused void* arg, const u8_t *data, u16_t len, u8_t flags) {
    const char *basic_topic = _unique_topic ? _state.topic + strlen(_state.mqtt_client_info.client_id) + 1 : _state.topic;
    strncpy(_state.data, (const char*) data, len);
    _state.len = len;
    _state.data[len] = '\0';
    _handler(basic_topic, _state.data);
}

static void start_client() {
    _state.mqtt_client_inst = mqtt_client_new();
    if (!_state.mqtt_client_inst) panic("MQTT client instance creation error");
    cyw43_arch_lwip_begin();
        if (mqtt_client_connect(_state.mqtt_client_inst, &_state.mqtt_server_address, MQTT_PORT, on_connection, NULL, &_state.mqtt_client_info) != ERR_OK) {
            panic("MQTT broker connection error");
        }
        mqtt_set_inpub_callback(_state.mqtt_client_inst, on_topic_incoming, on_data_incoming, NULL);
    cyw43_arch_lwip_end();
}

void mqtt_client_init(char* host, char* username, char* password, bool unique_topic, mqtt_handler handler) {
    _unique_topic = unique_topic;
    _handler = handler;
    set_client_info(username, password);
    err_t err = resolve_dns(host);
    if (err != ERR_OK && err != ERR_INPROGRESS) panic("dns request failed");
    if (err == ERR_OK) start_client();
}

void mqtt_client_subscribe(char* topic) {
    mqtt_sub_unsub(_state.mqtt_client_inst, full_topic(topic), MQTT_SUBSCRIBE_QOS, on_subscribe, NULL, true);
}

void mqtt_client_unsubscribe(char* topic) {
    mqtt_sub_unsub(_state.mqtt_client_inst, full_topic(topic), MQTT_SUBSCRIBE_QOS, on_subscribe, NULL, false);
}

void mqtt_client_stop() {
    _state.stop_client = true;
}

bool mqtt_is_active() {
    return !_state.connect_done || mqtt_client_is_connected(_state.mqtt_client_inst);
}