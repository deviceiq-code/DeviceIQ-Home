#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DevIQ_Configuration.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_Network.h>
#include <DevIQ_Log.h>
#include <DevIQ_Network.h>
#include <DevIQ_DateTime.h>
#include <AsyncUDP.h>
#include <WiFiClient.h>

using namespace DeviceIQ_Configuration;
using namespace DeviceIQ_Log;
using namespace DeviceIQ_DateTime;
using namespace DeviceIQ_Network;
using namespace DeviceIQ_DateTime;

enum DiscoveryMode { DISCOVERY_NONE, DISCOVERY_ALL, DISCOVERY_UNMANAGED, DISCOVERY_MANAGED };

class ClientManager {
    public:
        const String mManagerName = "Orchestrator";
        const String mManagerVersion = "1.0.0";

        static ClientManager& getInstance();

        void begin();  // call in setup()

        bool UpdateOrchestrator();

    private:
        ClientManager() = default;                  // private ctor for singleton
        ClientManager(const ClientManager&) = delete;
        ClientManager& operator=(const ClientManager&) = delete;

        void handleUdpPacket(AsyncUDPPacket& packet);

        void handleDiscover(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handleRestart(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handleAdd(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handleRemove(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handleUpdate(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handlePull(const JsonVariantConst& cmd, IPAddress remoteIp);
        void handlePush(const JsonVariantConst& cmd, IPAddress remoteIp);

        bool connectAndExchangeJson(IPAddress remoteIp, uint16_t port, std::function<void(WiFiClient&)> exchange);

        const String &NamagerName() const { return mManagerName; }
        const String &NamagerVersion() const { return mManagerVersion; }

        AsyncUDP udp;
};

#endif
