#ifndef Settings_h
#define Settings_h

#pragma once

#include <Arduino.h>

class settings_t {
    private:
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

                static void sanitizeIpString(String& s) noexcept;
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
};

extern settings_t Settings;

#endif