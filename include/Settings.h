#ifndef Settings_h
#define Settings_h

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_Log.h>
#include <DevIQ_MQTT.h>
#include <DevIQ_Components.h>

using namespace DeviceIQ_FileSystem;
using namespace DeviceIQ_Log;
using namespace DeviceIQ_MQTT;
using namespace DeviceIQ_Components;

extern FileSystem *devFileSystem;
extern Log *devLog;
extern MQTT *devMQTT;

#include "Defaults.h"

class settings_t {
    private:
        static void sanitizeIpString(String& s) noexcept;
    public:
        class log_t {
            private:
                uint8_t pEndpoint;
                uint8_t pLogLevel;
                String pSyslogServerHost;
                uint16_t pSyslogServerPort;
            public:
                [[nodiscard]] uint8_t Endpoint() const noexcept { return pEndpoint; }
                void Endpoint(uint8_t value) noexcept { pEndpoint = value; }
                
                [[nodiscard]] uint8_t LogLevel() const noexcept { return pLogLevel; }
                void LogLevel(uint8_t value) noexcept { pLogLevel = value; }
                
                [[nodiscard]] const String& SyslogServerHost() const noexcept { return pSyslogServerHost; }
                void SyslogServerHost(String value) noexcept { value.trim(); value.toLowerCase(); pSyslogServerHost = std::move(value); }
                
                [[nodiscard]] uint16_t SyslogServerPort() const noexcept { return pSyslogServerPort; }
                void SyslogServerPort(uint16_t value) { pSyslogServerPort = (value == 0) ? 514 : value; }
        } Log;
        class network_t {
            private:
                bool pDHCPClient{};
                String pHostname;
                IPAddress pIP_Address{0,0,0,0};
                IPAddress pGateway{0,0,0,0};
                IPAddress pNetmask{255,255,255,0};
                IPAddress pDNS[2]{ IPAddress(0,0,0,0), IPAddress(0,0,0,0) };
                String pSSID;
                String pPassphrase;
                uint16_t pConnectionTimeout;
                bool pOnlineChecking{};
                uint16_t pOnlineCheckingTimeout;

                static bool isValidNetmask(const IPAddress& mask) noexcept;
                static void stripControlChars(String& s) noexcept;
                static bool isPrintableASCII(const String& s) noexcept;
                static bool isHex64(const String& s) noexcept;
            public:
                [[nodiscard]] bool DHCPClient() const noexcept { return pDHCPClient; }
                void DHCPClient(bool value) noexcept { pDHCPClient = value; }

                [[nodiscard]] String Hostname() const noexcept { return pHostname; }
                void Hostname(String value) noexcept;

                [[nodiscard]] const IPAddress& IP_Address() const noexcept { return pIP_Address; }
                void IP_Address(String value) noexcept;

                [[nodiscard]] const IPAddress& Gateway() const noexcept { return pGateway; }
                void Gateway(String value) noexcept;

                [[nodiscard]] const IPAddress& Netmask() const noexcept { return pNetmask; }
                void Netmask(String value) noexcept;

                [[nodiscard]] const IPAddress& DNS(uint8_t index) const noexcept { static IPAddress zero(0,0,0,0); return (index < 2) ? pDNS[index] : zero; }
                void DNS(uint8_t index, String value) noexcept;

                [[nodiscard]] const String& SSID() const noexcept { return pSSID; }
                void SSID(String value) noexcept;

                [[nodiscard]] const String& Passphrase() const noexcept { return pPassphrase; }
                void Passphrase(String value) noexcept;

                [[nodiscard]] uint16_t ConnectionTimeout() const noexcept { return pConnectionTimeout; }
                void ConnectionTimeout(uint16_t value) { pConnectionTimeout = (value == 0) ? 514 : value; }

                [[nodiscard]] bool OnlineChecking() const noexcept { return pOnlineChecking; }
                void OnlineChecking(bool value) noexcept { pOnlineChecking = value; }

                [[nodiscard]] uint16_t OnlineCheckingTimeout() const noexcept { return pOnlineCheckingTimeout; }
                void OnlineCheckingTimeout(uint16_t value) { pOnlineCheckingTimeout = (value == 0) ? 514 : value; }
        } Network;
        class update_t {
            private:
                String pManifestURL;
                bool pAllowInsecure{};
                bool pEnableLANOTA{};
                String pPasswordLANOTA;
                uint16_t pCheckInterval;
                bool pAutoReboot{};
                bool pDebug{};
                bool pCheckAtStartup{};
            public:
                [[nodiscard]] const String& ManifestURL() const noexcept { return pManifestURL; }
                void ManifestURL(String value) noexcept;

                [[nodiscard]] bool AllowInsecure() const noexcept { return pAllowInsecure; }
                void AllowInsecure(bool value) noexcept { pAllowInsecure = value; }

                [[nodiscard]] bool EnableLANOTA() const noexcept { return pEnableLANOTA; }
                void EnableLANOTA(bool value) noexcept { pEnableLANOTA = value; }

                [[nodiscard]] const String& PasswordLANOTA() const noexcept { return pPasswordLANOTA; }
                void PasswordLANOTA(String value) noexcept;

                [[nodiscard]] uint16_t CheckInterval() const noexcept { return pCheckInterval; }
                void CheckInterval(uint16_t value) { pCheckInterval = value; }

                [[nodiscard]] bool AutoReboot() const noexcept { return pAutoReboot; }
                void AutoReboot(bool value) noexcept { pAutoReboot = value; }

                [[nodiscard]] bool Debug() const noexcept { return pDebug; }
                void Debug(bool value) noexcept { pDebug = value; }

                [[nodiscard]] bool CheckAtStartup() const noexcept { return pCheckAtStartup; }
                void CheckAtStartup(bool value) noexcept { pCheckAtStartup = value; }
        } Update;
        class general_t {
            private:
                bool pNTPUpdate{};
                String pNTPServer;
            public:
                [[nodiscard]] bool NTPUpdate() const noexcept { return pNTPUpdate; }
                void NTPUpdate(bool value) noexcept { pNTPUpdate = value; }

                [[nodiscard]] const String& NTPServer() const noexcept { return pNTPServer; }
                void NTPServer(String value) noexcept;
        } General;
        class orchestrator_t {
            private:
                bool pAssigned{};
                String pServerID;
                IPAddress pIP_Address{0,0,0,0};
                uint16_t pPort{};
            public:
                [[nodiscard]] bool Assigned() const noexcept { return pAssigned; }
                void Assigned(bool value) noexcept { pAssigned = value; }

                [[nodiscard]] const String& ServerID() const noexcept { return pServerID; }
                void ServerID(String value) noexcept;

                [[nodiscard]] const IPAddress& IP_Address() const noexcept { return pIP_Address; }
                void IP_Address(String value) noexcept;

                [[nodiscard]] uint16_t Port() const noexcept { return pPort; }
                void Port(uint16_t value) { pPort = (value == 0) ? 30300 : value; }
        } Orchestrator;
        class webhooks_t {
            private:
                uint16_t pPort{};
                bool pEnabled{};
                String pToken;
            public:
                [[nodiscard]] uint16_t Port() const noexcept { return pPort; }
                void Port(uint16_t value) { pPort = (value == 0) ? 80 : value; }

                [[nodiscard]] bool Enabled() const noexcept { return pEnabled; }
                void Enabled(bool value) noexcept { pEnabled = value; }

                [[nodiscard]] const String& Token() const noexcept { return pToken; }
                void Token(String value) noexcept;
        } WebHooks;
        class mqtt_t {
            private:
                bool pEnabled{};
                String pBroker;
                uint16_t pPort{};
                String pUser;
                String pPassword;
            public:
                [[nodiscard]] bool Enabled() const noexcept { return pEnabled; }
                void Enabled(bool value) noexcept { pEnabled = value; }

                [[nodiscard]] const String& Broker() const noexcept { return pBroker; }
                void Broker(String value) noexcept;

                [[nodiscard]] uint16_t Port() const noexcept { return pPort; }
                void Port(uint16_t value) { pPort = (value == 0) ? 80 : value; }

                [[nodiscard]] const String& User() const noexcept { return pUser; }
                void User(String value) noexcept;

                [[nodiscard]] const String& Password() const noexcept { return pPassword; }
                void Password(String value) noexcept;
        } MQTT;

        Collection Components;

        void LoadDefaults();
        bool Load(const String& configfilename = Defaults.ConfigFileName) noexcept;
        bool Save(const String& configfilename = Defaults.ConfigFileName) const noexcept;
        bool InstallComponents(const String& configfilename = Defaults.ConfigFileName) noexcept;
};

extern settings_t Settings;

#endif