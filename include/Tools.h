#ifndef Tools_h
#define Tools_h

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <ESPAsyncWebServer.h>
#include <DevIQ_Network.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_Log.h>
#include <vector>

#include "Settings.h"

using namespace DeviceIQ_Log;
using namespace DeviceIQ_Network;

#define CHECK_BIT(var, pos) ((var) & (1 << (pos - 1)))

inline bool IsEmpty(const char* s) { return (!s || *s == '\0'); }
inline void BuildDefaultHostname(char* out, size_t outSize) { if (!out || outSize == 0) return; uint8_t mac[6]; esp_read_mac(mac, ESP_MAC_WIFI_STA); snprintf(out, outSize, "DEV-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); }

class SString : public String {
    public:
        SString() : String() {}
        SString(uint16_t n) : String(n) {}
        SString(String str) : String(str) {}
        SString(String str, size_t sz);
        SString(const char* str, size_t sz);
        SString(const byte* str, size_t sz);

        // String LimitString(size_t sz, bool fill = false);
        String LowerCase();
        String UpperCase();
        std::vector<String> Tokenize(const char separator);
};
String CharArrayPointerToString(char* Text, uint32_t length);
String LimitString(String text, uint16_t sz, bool fill);

bool hasValidHeaderToken(AsyncWebServerRequest *request, String api_token);

template<typename TComponent, typename FillFunc>
void registerEndpoint(AsyncWebServer* server, TComponent component, const char* className, FillFunc fillJson, String token, Log* devLog) {
    server->on(String("/" + component->Name()).c_str(), HTTP_GET, [=](AsyncWebServerRequest *request) {
        JsonDocument reply;
        String json;

        if (hasValidHeaderToken(request, token)) {
            reply["Class"] = className;
            reply["Name"] = component->Name();
            fillJson(reply, component);
            serializeJson(reply, json);

            request->send(200, "application/json", json.c_str());
            devLog->Write("Granted HTTP request from " + request->client()->remoteIP().toString() + " to /" + component->Name(), LOGLEVEL_INFO);
        } else {
            reply["Error"] = "Unauthorized";
            serializeJson(reply, json);

            request->send(401, "application/json", json.c_str());
            devLog->Write("Unauthorized HTTP request from " + request->client()->remoteIP().toString() + " to /" + component->Name(), LOGLEVEL_WARNING);
        }
    });
}

bool StreamFileAsBase64Json(String fileName, String macAddress, String command, WiFiClient &client, File &f, size_t fileSize, uint32_t crc32);
uint32_t CRC32_Update(uint32_t crc, const uint8_t* data, size_t len);
uint32_t CRC32_File(File& f);

void Web_Content(String content, String mimetype, AsyncWebServerRequest *request, bool requires_authentication, bool static_content = false);
String urlEncode(const String &str);

#endif