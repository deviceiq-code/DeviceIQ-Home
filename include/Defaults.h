#ifndef Defaults_h
#define Defaults_h

#pragma once

#include <Arduino.h>

struct defaults {
    const char ConfigFileName[254] = "/config.json";
    const uint32_t InitialTimeAndDate = 1708136755;
    
    struct log {
        const char LogFileName[254] = "/config.log";
        const Endpoints EndPoint = ENDPOINT_SERIAL;
        const LogLevels LogLevel = LOGLEVEL_ALL;
        const char SyslogServerHost[254] = "";
        const uint16_t SyslogServerPort = 514;
    } Log;

    struct network {
        const bool DHCP_Client = true;
        const char IP_Address[16] = "192.168.4.1";
        const char Gateway[16] = "192.168.4.1";
        const char Netmask[16] = "255.255.255.0";
        String Hostname() const { uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA); char buf[12]; snprintf(buf, sizeof(buf), "DEV-%02X%02X%02X", mac[3], mac[4], mac[5]); return String(buf); }
        const uint16_t ConnectionTimeout = 15;
        const bool OnlineChecking = true;
        const uint16_t OnlineCheckingTimeout = 60;
    } Network;

    struct components {
        const bool Enable = true;
    } Components;

    struct ntp {
        const bool Update = true;
        const char Server[254] = "pool.ntp.org";
    } NTP;

    struct orchestrator {
        const bool Assigned = false;
    } Orchestrator;

    struct webhooks {
        const uint16_t Port = 80;
        const bool Enable = true;
        String Token() const { uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA); char buf[12]; snprintf(buf, sizeof(buf), "TOKEN-%02X%02X%02X", mac[3], mac[4], mac[5]); return String(buf); }
    } WebHooks;

    struct mqtt {
        const bool Enable = false;
        const char Broker[254] = "mqttbroker";
        const uint16_t Port = 1883;
        const char User[31] = "deviceiq";
        const char Password[31] = "deviceiq";
    } MQTT;

    struct update {
        const char ManifestURL[254] = "https://server.dts-network.com:8081/update.json";
        const bool AllowInsecure = true;
        const bool Debug = true;
        const uint32_t CheckUpdateInterval = 21600; // 6h
        const bool AutoReboot = true;
        const bool CheckOnBoot = true;
    } Update;
};

inline constexpr defaults Defaults{};

#endif