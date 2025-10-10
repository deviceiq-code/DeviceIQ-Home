#ifndef Defaults_h
#define Defaults_h

#pragma once

#include <Arduino.h>

#pragma once
#include <Arduino.h>
#include <IPAddress.h>

struct defaults_t {
    struct log_t {
        const uint8_t Endpoint = 0b00000101; // Serial + File
        const uint8_t Level = 0b11111111; // All
        const char* SyslogServer = "syslog.svr";
        const uint16_t SyslogPort = 514;
    } Log;
    struct network_t {
        const bool DHCPClient = true;
        String Hostname() const { uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA); char buf[12]; snprintf(buf, sizeof(buf), "DEV-%02X%02X%02X", mac[3], mac[4], mac[5]); return String(buf); }
        const char* IP_Address = "0.0.0.0";
        const char* Gateway = "0.0.0.0";
        const char* Netmask = "255.255.255.0";
        const char* DNS[2] = { "8.8.8.8", "8.8.4.4" };
        const char* SSID = "";
        const char* Passphrase = "";
        const uint16_t ConnectionTimeout = 30;
        const bool OnlineChecking = true;
        const uint16_t OnlineCheckingTimeout = 10;
    } Network;
    struct update_t {
        const char* ManifestURL = "https://server.dts-network.com:8081/update.json";
        const bool AllowInsecure = false;
        const bool EnableLANOTA = false;
        const char* PasswordLANOTA = "";
        const uint16_t CheckInterval = 3600;
        const bool AutoReboot = true;
        const bool Debug = false;
        const bool CheckAtStartup = true;
    } Update;
    struct general_t {
        const bool NTPUpdate = true;
        const char* NTPServer = "pool.ntp.org";
    } General;
    struct orchestrator_t {
        const bool Assigned = false;
        const char* ServerID = "";
        const char* IP_Address = "";
        const uint16_t Port = 30030;
    } Orchestrator;
    struct webhooks_t {
        const uint16_t Port = 80;
        const bool Enabled = false;
        const char* Token = "";
    } WebHooks;
    struct mqtt_t {
        const bool Enabled = false;
        const char* Broker = "";
        const uint16_t Port = 1883;
        const char* User = "";
        const char* Password = "";
    } MQTT;
    struct components_t {
        const bool Enabled = true;
    } Components;
    const char* ConfigFileName = "/config.json";
    const char* LogFileName = "/device.log";
    const uint32_t InitialTimeAndDate = 1708136755;
};

inline const defaults_t Defaults{};

#endif