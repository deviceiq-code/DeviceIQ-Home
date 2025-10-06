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
                [[nodiscard]] const uint8_t Endpoint() const noexcept { return pEndpoint; }
                void Endpoint(uint8_t value) noexcept { pEndpoint = value; }
                
                [[nodiscard]] const uint8_t LogLevel() const noexcept { return pLogLevel; }
                void LogLevel(uint8_t value) noexcept { pLogLevel = value; }
                
                [[nodiscard]] const String SyslogServerHost() const noexcept { return pSyslogServerHost; }
                
                [[nodiscard]] const uint16_t SyslogServerPort() const noexcept { return pSyslogServerPort; }
                void SyslogServerPort(uint16_t value) { pSyslogServerPort = value; }
        } Log;
        class network_t {
            private:
                bool pDHCPClient{};
                String pHostname;
                IPAddress pIP_Address{0,0,0,0};
                IPAddress pGateway{0,0,0,0};
                IPAddress pNetmask{255,255,255,0};

                static void sanitizeIpString(String& s) noexcept;
                static bool isValidNetmask(const IPAddress& mask) noexcept;
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
        } Network;
} Settings;

#endif