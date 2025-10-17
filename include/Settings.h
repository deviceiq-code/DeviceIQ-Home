#ifndef Settings_h
#define Settings_h

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_Log.h>
#include <DevIQ_MQTT.h>
#include <DevIQ_Components.h>
#include <base64.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

using namespace DeviceIQ_FileSystem;
using namespace DeviceIQ_Log;
using namespace DeviceIQ_MQTT;
using namespace DeviceIQ_Components;

extern FileSystem *devFileSystem;
extern Log *devLog;
extern MQTT *devMQTT;

#include "Defaults.h"

#define PASS_PBKDF2_ITERATIONS  10000
#define PASS_SALTLEN            16    // 128-bit salt
#define PASS_HASHLEN            32    // SHA-256 size
#define MAX_USERS               3

enum class UserError : uint8_t { OK = 0, UserExists, UserNotFound, MaxUsersReached, NoAdminRemaining, PasswordError, InvalidCredentials };

class user_t {
    private:
        String pUsername;
        bool pAdmin;
    public:
        uint8_t Salt[PASS_SALTLEN] = {0};
        uint8_t Hash[PASS_HASHLEN] = {0};

        void Username(const String& username) { pUsername = username; pUsername.toLowerCase(); }
        const String& Username() const noexcept { return pUsername; }
        bool Admin() const noexcept { return pAdmin; }
        void Admin(bool value) noexcept { pAdmin = value; }
        
        bool SetPassword(const String& password);
        bool Authenticate(const String& password) const;
};

class users_t {
    private:
        user_t pUsers[MAX_USERS];
        size_t userCount = 0;
        bool hasAdmin() const { for (size_t i = 0; i < userCount; ++i) if (pUsers[i].Admin()) return true; return false; }
    public:
        users_t() = default;
        
        inline size_t Count() const noexcept { return userCount; }
        inline size_t CountAdmins() const noexcept { size_t count = 0; for (size_t i = 0; i < userCount; ++i) if (pUsers[i].Admin()) count++; return count; }
        UserError Add(const String& username, const String& password, bool admin = false);
        UserError Remove(const String& username);
        UserError Authenticate(const String& username, const String& password, user_t** outUser = nullptr);
        UserError Find(const String& username, user_t** outUser = nullptr);
};

class settings_t {
    private:
        bool pFirstRun = false;
        bool pSaveComponentsStateFlag = false;
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
                uint16_t pSaveStatePooling;
            public:
                [[nodiscard]] bool NTPUpdate() const noexcept { return pNTPUpdate; }
                void NTPUpdate(bool value) noexcept { pNTPUpdate = value; }

                [[nodiscard]] const String& NTPServer() const noexcept { return pNTPServer; }
                void NTPServer(String value) noexcept;

                [[nodiscard]] uint16_t SaveStatePooling() const noexcept { return pSaveStatePooling; }
                void SaveStatePooling(uint16_t value) { pSaveStatePooling = (value <= 1) ? Defaults.General.SaveStatePooling : value; }
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
                void Port(uint16_t value) { pPort = (value == 0) ? Defaults.Orchestrator.Port : value; }
        } Orchestrator;
        class webserver_t {
            private:
                uint16_t pPort{};
                bool pEnabled{};
                String pWebHooksToken;
            public:
                [[nodiscard]] uint16_t Port() const noexcept { return pPort; }
                void Port(uint16_t value) { pPort = (value == 0) ? 80 : value; }

                [[nodiscard]] bool Enabled() const noexcept { return pEnabled; }
                void Enabled(bool value) noexcept { pEnabled = value; }

                [[nodiscard]] const String& WebHooksToken() const noexcept { return pWebHooksToken; }
                void WebHooksToken(String value) noexcept;
        } WebServer;
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

        [[nodiscard]] bool FirstRun() const noexcept { return pFirstRun; }
        [[nodiscard]] bool SaveComponentsStateFlag() const noexcept { return pSaveComponentsStateFlag; }
        void SetSaveComponentsState() noexcept { pSaveComponentsStateFlag = true; }

        Collection Components;
        users_t Users;

        void LoadDefaults();
        void RestoreToFactoryDefaults();
        bool Load(const String& configfilename = Defaults.ConfigFileName) noexcept;
        bool Save(const String& configfilename = Defaults.ConfigFileName) const noexcept;
        bool InstallComponents(const String& configfilename = Defaults.ConfigFileName) noexcept;
        bool SaveComponentsState(const String& configfilename = Defaults.ConfigFileName) noexcept;
};

extern settings_t Settings;

#endif