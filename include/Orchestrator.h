#ifndef Orchestrator_H
#define Orchestrator_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_Network.h>
#include <DevIQ_Log.h>
#include <DevIQ_Network.h>
#include <DevIQ_DateTime.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AsyncUDP.h>
#include <WiFiClient.h>

#include "Settings.h"

using namespace DeviceIQ_Log;
using namespace DeviceIQ_DateTime;
using namespace DeviceIQ_Network;
using namespace DeviceIQ_DateTime;

enum DiscoveryMode { DISCOVERY_NONE, DISCOVERY_ALL, DISCOVERY_UNMANAGED, DISCOVERY_MANAGED };

static JsonDocument s_doc;
static AsyncUDP s_udp;
static SemaphoreHandle_t s_sem;
static String s_rx;
static IPAddress s_rip;

class Orchestrator {
    public:
        const String mManagerName = "Orchestrator";
        const String mManagerVersion = "1.0.0";

        static Orchestrator& getInstance();

        void begin();

        bool ClearLog(const JsonVariantConst& cmd);
        bool Discover(const JsonVariantConst& cmd);
        bool GetLog(const JsonVariantConst& cmd);
        bool Pull(const JsonVariantConst& cmd);
        bool Push(const JsonVariantConst& cmd);
        bool Refresh(const JsonVariantConst& cmd);
        bool Add(const JsonVariantConst& cmd);
        bool Remove(const JsonVariantConst& cmd);
        bool Restart(const JsonVariantConst& cmd);
        bool Restore(const JsonVariantConst& cmd);
        bool Update(const JsonVariantConst& cmd);
        
        const JsonObjectConst SendUDP(const String &target, const uint16_t port, const JsonObjectConst &payload);

        bool FindOrchestratorServer();

    private:
        Orchestrator() = default;
        Orchestrator(const Orchestrator&) = delete;
        Orchestrator& operator=(const Orchestrator&) = delete;

        void handleUdpPacket(AsyncUDPPacket& packet);

        bool connectAndExchangeJson(IPAddress remoteIp, uint16_t port, std::function<void(WiFiClient&)> exchange);
        
        bool CheckOrchestratorAssignedAndServerID(const JsonObjectConst &cmd);

        const String &NamagerName() const { return mManagerName; }
        const String &NamagerVersion() const { return mManagerVersion; }

        AsyncUDP udp;
};

#endif
