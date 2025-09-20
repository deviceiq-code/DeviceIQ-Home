#ifndef Tools_h
#define Tools_h

#pragma once

#include <Arduino.h>
#include <mbedtls/base64.h>
#include <ESPAsyncWebServer.h>
#include <DevIQ_Configuration.h>
#include <DevIQ_Log.h>
#include <vector>

using namespace DeviceIQ_Log;
using namespace DeviceIQ_Configuration;

#define CHECK_BIT(var, pos) ((var) & (1 << (pos - 1)))

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
void registerEndpoint(AsyncWebServer* server, TComponent component, const char* className, FillFunc fillJson, Configuration* devConfiguration, Log* devLog) {
    server->on(String("/" + component->Name()).c_str(), HTTP_GET, [=](AsyncWebServerRequest *request) {
        JsonDocument reply;
        String json;

        if (hasValidHeaderToken(request, devConfiguration->Setting["Webhooks"]["Token"].as<String>())) {
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

bool StreamFileAsBase64Json(String &fileName, WiFiClient &client, File &f, size_t fileSize, uint32_t crc32 = 0);
uint32_t CRC32_Update(uint32_t crc, const uint8_t* data, size_t len);
uint32_t CRC32_File(File& f);

#endif