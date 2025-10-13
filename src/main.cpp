#include <Arduino.h>
#include <DevIQ_Network.h>
#include <DevIQ_Components.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_MQTT.h>
#include <DevIQ_DateTime.h>
#include <DevIQ_Log.h>
#include "DevIQ_Update.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>

#include "AsyncTelnetServer.h"
#include "Orchestrator.h"
#include "Version.h"
#include "Tools.h"

using namespace DeviceIQ_Components;
using namespace DeviceIQ_Log;
using namespace DeviceIQ_MQTT;
using namespace DeviceIQ_DateTime;
using namespace DeviceIQ_Network;
using namespace DeviceIQ_Update;

#include "Defaults.h"
#include "Settings.h"

FileSystem *devFileSystem;
Clock *devClock;
Log *devLog;
MQTT *devMQTT;
Network *devNetwork;
AsyncWebServer *devWebServer;
AsyncTelnetServer *devTelnetServer;
UpdateClient *devUpdateClient;

Timer *devSaveState;

settings_t Settings;
orchestrator Orchestrator;

volatile bool g_cmdCheckNow = false;

void setup() {
    Serial.begin(115200);

    // General Settings
    esp_log_level_set("*", ESP_LOG_NONE);

    // FileSystem
    devFileSystem = new FileSystem();
    if (devFileSystem->Exists(String(Defaults.ConfigFileName) + ".tmp")) {
        devFileSystem->DeleteFile(String(Defaults.ConfigFileName) + ".tmp"); // Delete any old temporary config file
    }

    // Settings
    Settings.Load();

    // Clock
    devClock = new Clock();
    devClock->EpochUpdate(Defaults.InitialTimeAndDate);

    // Log
    devLog = new Log(devFileSystem, devClock);
    devLog->SerialPort(&Serial);
    devLog->LogFileName(Defaults.LogFileName);
    devLog->Endpoint(Settings.Log.Endpoint());
    devLog->LogLevel(Settings.Log.LogLevel());
    devLog->SyslogServerHost(Settings.Log.SyslogServerHost());
    devLog->SyslogServerPort(Settings.Log.SyslogServerPort());

    devLog->Write(Version.ProductFamily + " " + Version.Software.Info(), LOGLEVEL_INFO);

    // MQTT
    devMQTT = new MQTT();

    // Network
    devNetwork = new Network();
    devNetwork->DHCP_Client(Settings.Network.DHCPClient());
    if (!devNetwork->DHCP_Client()) {
        devNetwork->IP_Address(Settings.Network.IP_Address());
        devNetwork->Gateway(Settings.Network.Gateway());
        devNetwork->Netmask(Settings.Network.Netmask());
    }

    devNetwork->Hostname(Settings.Network.Hostname());
    devNetwork->SSID(Settings.Network.SSID());
    devNetwork->Passphrase(Settings.Network.Passphrase());
    devNetwork->ConnectionTimeout(Settings.Network.ConnectionTimeout());
    devNetwork->OnlineChecking(Settings.Network.OnlineChecking());
    devNetwork->OnlineCheckingTimeout(Settings.Network.OnlineCheckingTimeout());

    // Update
    devUpdateClient = new UpdateClient([&]{
        DeviceIQ_Update::UpdateConfig c;
        c.model = Version.ProductName;
        c.currentVersion = Version.Software.Info();
        c.manifestUrl = Settings.Update.ManifestURL();
        c.rootCA_PEM = nullptr;
        c.allowInsecure = Settings.Update.AllowInsecure();
        c.enableLanOta = Settings.Update.EnableLANOTA();

        c.lanHostname = Settings.Network.Hostname();
        c.lanPassword = Settings.Update.PasswordLANOTA();
        c.checkInterval = Settings.Update.CheckInterval();
        c.httpTimeout = 15;
        c.streamBufSize = size_t(4096);
        c.autoReboot = Settings.Update.AutoReboot();

        return c;
    }());

    devUpdateClient->OnError([&](DeviceIQ_Update::Error e, const String& d) {
        if (Settings.Update.Debug()) devLog->Write("Update Client: Error [" + String((int)e) + "] Detail [" + d + "]", LOGLEVEL_INFO);
    });

    devUpdateClient->OnEvent([&](DeviceIQ_Update::Event e) {
        const String v = devUpdateClient->LatestVersion();
        const String min = devUpdateClient->LatestMinVersion();
        const String url = devUpdateClient->LatestUrl();
        const bool forced = DeviceIQ_Update::UpdateClient::IsNewer(min, Version.Software.Info());

        switch (e) {
            case Event::NewVersion: {
                devLog->Write("Update Client: New version " + v + " available (MIN " + min + ", FORCED " + (forced ? "Yes" : "No") + ")", forced ? LOGLEVEL_WARNING : LOGLEVEL_INFO);
            } break;
            case Event::Downloading: {
                if (Settings.Update.Debug()) devLog->Write("Update Client: Downloading " + url, LOGLEVEL_INFO);
            } break;
            case Event::Verifying: {
                if (Settings.Update.Debug()) devLog->Write("Update Client: Verifying new firmware integrity", LOGLEVEL_INFO);
            } break;
            case Event::Applying: {
                if (Settings.Update.Debug()) devLog->Write("Update Client: Applying update", LOGLEVEL_INFO);
            } break;
            case Event::Rebooting: {
                if (Settings.Update.Debug()) devLog->Write("Update Client: Rebooting to apply update", LOGLEVEL_WARNING);
            } break;
            default: break;
        }
    });

    // Components
    Settings.InstallComponents();
    devLog->Write("Components: " + String(Settings.Components.Count()) + " component(s) installed", LOGLEVEL_INFO);

    // Components callbacks
    static bool interfacesRegistered = false;

    devNetwork->OnModeChanged([&] {
        APMode t = devNetwork->ConnectionMode();

        switch (t) {
            case WifiClient: {
                devLog->Write("Network: " + devNetwork->Hostname() + " MAC " + devNetwork->MAC_Address() + " connected to " + devNetwork->SSID() + " IP " + devNetwork->IP_Address().toString(), LOGLEVEL_INFO);

                // NTP
                if (Settings.General.NTPUpdate()) {
                    if (devClock->NTPUpdate(Settings.General.NTPServer())) {
                        devLog->Write("Date and Time: Updated from NTP server " + Settings.General.NTPServer(), LOGLEVEL_INFO);
                    } else {
                        devLog->Write("Date and Time: Failed to update from NTP server " + Settings.General.NTPServer(), LOGLEVEL_ERROR);
                    }
                } else {
                    devLog->Write("Date and Time: Using local settings", LOGLEVEL_INFO);
                }

                // Orchestrator
                Orchestrator.Begin();
                if (Settings.Orchestrator.Assigned() && !Settings.Orchestrator.ServerID().isEmpty()) {
                    devLog->Write("Orchestrator: Device assigned to server ID " + Settings.Orchestrator.ServerID(), LOGLEVEL_WARNING);
                    
                    JsonDocument cmd;
                    cmd["Server ID"] = Settings.Orchestrator.ServerID();
                    Orchestrator.Refresh(cmd);
                } else {
                    devLog->Write("Orchestrator: Device is not assigned to an Orchestrator server", LOGLEVEL_WARNING);
                }

                // WebServer/MQTT
                if (!interfacesRegistered) {
                    devWebServer = new AsyncWebServer(Settings.WebServer.Port());
                    
                    devWebServer->onNotFound([](AsyncWebServerRequest *request) { request->send(404); });
                    devWebServer->on("/res/css/styles.css", HTTP_GET, [&](AsyncWebServerRequest *request) { Web_Content("/res/css/styles.css", "text/css", request, false, true); });
                    devWebServer->on("/res/img/logo.png", HTTP_GET, [&](AsyncWebServerRequest *request) { Web_Content("/res/img/logo.png", "image/png", request, false, true); });
                    devWebServer->on("/login.html", HTTP_GET, [&](AsyncWebServerRequest *request) { Web_Content("/login.html", "text/html", request, false); });

                    // WebServer
                    if (Settings.WebServer.Enabled()) {
                        for (auto m : Settings.Components) {
                            switch (m->Class()) {

                                case CLASS_RELAY: {
                                    devWebServer->on(String("/" + m->Name()).c_str(), HTTP_GET, [=](AsyncWebServerRequest *request) {
                                        JsonDocument reply;
                                        String json;

                                        if (hasValidHeaderToken(request, Settings.WebServer.WebHooksToken())) {
                                            bool setnewvalue = false;
                                            String Arg, Val;

                                            if (request->args() > 0) {
                                                for (uint8_t i = 0; i < (uint8_t)request->args(); i++) {
                                                    Arg = request->argName(i);
                                                    Val = request->arg(i);

                                                    if (Arg.equalsIgnoreCase("state")) {
                                                        if (Val == "on" || Val == "true" || Val == "1") {
                                                            m->as<Relay>()->State(true);
                                                            setnewvalue = true;
                                                        } else if (Val == "off" || Val == "false" || Val == "0") {
                                                            m->as<Relay>()->State(false);
                                                            setnewvalue = true;
                                                        } else if (Val == "toggle" || Val == "invert" || Val == "~") {
                                                            m->as<Relay>()->Invert();
                                                            setnewvalue = true;
                                                        }
                                                    }
                                                }
                                            }

                                            reply["Class"] = "Relay";
                                            reply["State"] = m->as<Relay>()->State();
                                            reply["Name"] = m->Name();

                                            serializeJson(reply, json);

                                            request->send(200, "application/json", json.c_str());

                                            devLog->Write("Granted HTTP request " + String(setnewvalue ? "(SET '" + Arg + "=" + Val + "')"  : "(GET)") + " from " + request->client()->remoteIP().toString() + " to /" + m->Name(), LOGLEVEL_INFO);
                                        } else {
                                            reply["Error"] = "Unauthorized";
                                            serializeJson(reply, json);

                                            request->send(401, "application/json", json.c_str());
                                            devLog->Write("Unauthorized HTTP request from " + request->client()->remoteIP().toString() + " to /" + m->Name(), LOGLEVEL_WARNING);
                                        }
                                    });
                                } break;

                                case CLASS_PIR : {
                                    registerEndpoint(devWebServer, m, "PIR", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Motion"] = comp->as<PIR>()->State();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_BUTTON: {
                                    registerEndpoint(devWebServer, m, "Button", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Pressed"] = comp->as<Button>()->IsPressed();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_CURRENTMETER: {
                                    registerEndpoint(devWebServer, m, "Currentmeter", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Current AC"] = comp->as<Currentmeter>()->CurrentAC();
                                        reply["Current DC"] = comp->as<Currentmeter>()->CurrentDC();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_THERMOMETER: {
                                    registerEndpoint(devWebServer, m, "Thermometer", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Humidity"] = comp->as<Thermometer>()->Humidity();
                                        reply["Temperature"] = comp->as<Thermometer>()->Temperature();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_BLINDS: {
                                    registerEndpoint(devWebServer, m, "Blinds", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Position"] = comp->as<Blinds>()->Position();
                                        reply["State"] = comp->as<Blinds>()->State() == BlindsStates::BLINDSSTATE_DECREASING ? "Decreasing" : (comp->as<Blinds>()->State() == BlindsStates::BLINDSSTATE_INCREASING ? "Increasing" : "Stopped");
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_DOORBELL: {
                                    registerEndpoint(devWebServer, m, "Doorbell", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["State"] = comp->as<Doorbell>()->State();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;

                                case CLASS_CONTACTSENSOR: {
                                    registerEndpoint(devWebServer, m, "ContactSensor", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["State"] = comp->as<ContactSensor>()->State();
                                    }, Settings.WebServer.WebHooksToken(), devLog);
                                } break;
                            }
                        }

                        devWebServer->begin();
                        devLog->Write("Web Server: Enabled on port " + String(Settings.WebServer.Port()), LOGLEVEL_INFO);
                    } else {
                        devLog->Write("Web Server: Disabled", LOGLEVEL_INFO);
                    }

                    // MQTT
                    if (Settings.MQTT.Enabled()) {
                        devMQTT->Broker(Settings.MQTT.Broker());
                        devMQTT->Port(Settings.MQTT.Port());
                        devMQTT->User(Settings.MQTT.User());
                        devMQTT->Password(Settings.MQTT.Password());

                        devMQTT->Subscribe(devNetwork->Hostname() + "/Set/#", [&](const String& topic, const String& payload) {
                            const String prefixSet = devNetwork->Hostname() + "/Set/";

                            if (topic.startsWith(prefixSet) == true) {
                                String tmpClass, tmpName, tmpPayload;

                                String rest = topic.substring(prefixSet.length());
                                int slash = rest.indexOf('/');
                                if (slash >= 0) rest = rest.substring(0, slash);

                                int colon = rest.indexOf(':');
                                if (colon <= 0 || colon == rest.length()-1) { 
                                    devLog->Write("MQTT: Data received does not have expected structure", LOGLEVEL_WARNING);
                                    return;
                                }

                                tmpClass = rest.substring(0, colon);
                                tmpName  = rest.substring(colon + 1);
                                tmpPayload = payload;
                                
                                tmpClass.trim();
                                tmpName.trim();
                                tmpPayload.trim();

                                tmpPayload.toLowerCase();

                                auto comp =  Settings.Components[tmpName];

                                if (!comp) {
                                    devLog->Write("MQTT: Target not found [" + tmpClass + ":" + tmpName + "]", LOGLEVEL_WARNING);
                                } else {
                                    switch (comp->Class()) {

                                        case CLASS_RELAY: {
                                            if (!tmpClass.equalsIgnoreCase("relay")) {
                                                devLog->Write("MQTT: Class mismatch - topic says [" + tmpClass + "], actual is Relay:" + tmpName, LOGLEVEL_WARNING);
                                            } else {
                                                if (tmpPayload == "on" || tmpPayload == "true" || tmpPayload == "1")
                                                    comp->as<Relay>()->State(true);
                                                else if (tmpPayload == "off" || tmpPayload == "false" || tmpPayload == "0")
                                                    comp->as<Relay>()->State(false);
                                                else if (tmpPayload == "toggle" || tmpPayload == "invert" || tmpPayload == "~")
                                                    comp->as<Relay>()->Invert();
                                            }
                                        } break;

                                        case CLASS_BLINDS: {
                                            if (!tmpClass.equalsIgnoreCase("blinds")) {
                                                devLog->Write("MQTT: Class mismatch - topic says [" + tmpClass + "], actual is Blinds:" + tmpName, LOGLEVEL_WARNING);
                                            } else { 
                                                // handleBlinds(comp, data);
                                            }
                                        } break;

                                        case CLASS_BUTTON: {
                                            if (!tmpClass.equalsIgnoreCase("button")) {
                                                devLog->Write("MQTT: Class mismatch - topic says [" + tmpClass + "], actual is Button:" + tmpName, LOGLEVEL_WARNING);
                                            } else { 
                                                // handleButton(comp, data);
                                            }
                                        } break;

                                        case CLASS_THERMOMETER: {
                                            if (!tmpClass.equalsIgnoreCase("thermometer")) {
                                                devLog->Write("MQTT: Class mismatch - topic says [" + tmpClass + "], actual is Thermometer:" + tmpName, LOGLEVEL_WARNING);
                                            } else { 
                                                // handleThermometer(comp, data);
                                            }
                                        } break;

                                        case CLASS_CURRENTMETER: {
                                            if (!tmpClass.equalsIgnoreCase("currentmeter")) {
                                                devLog->Write("MQTT: Class mismatch - topic says [" + tmpClass + "], actual is Currentmeter:" + tmpName, LOGLEVEL_WARNING);
                                            } else { 
                                                // handleCurrentmeter(comp, data);
                                            }
                                        } break;

                                        default: {
                                            devLog->Write("MQTT: Unsupported class on " + tmpName, LOGLEVEL_WARNING);
                                        } break;
                                    }
                                }
                            } else {
                                devLog->Write("MQTT data received does not have expected structure", LOGLEVEL_WARNING);
                            }
                        });

                        if (devMQTT->Connect()) {
                            devLog->Write("MQTT: Connected", LOGLEVEL_INFO);
                        } else {
                            devLog->Write("MQTT: Not connected", LOGLEVEL_INFO);
                        }
                    } else {
                        devLog->Write("MQTT: Disabled", LOGLEVEL_INFO);
                    }

                    // Refresh all components, sending initial state to MQTT
                    for (auto m : Settings.Components) { m->Refresh(); }

                    interfacesRegistered = true;
                }
            } break;
            case SoftAP: {
                devLog->Write("Network: " + devNetwork->Hostname() + " MAC " + devNetwork->MAC_Address() + " connected to " + devNetwork->SSID() + " (AP Mode) IP " + devNetwork->IP_Address().toString(), LOGLEVEL_INFO);
            } break;
            default:
            case Offline: {
                devLog->Write("Network: Networking is Offline", LOGLEVEL_INFO);
            } break;
        }

        if (Settings.Update.CheckAtStartup()) devUpdateClient->CheckUpdateNow();
    });

    devNetwork->Connect();

    if (Settings.FirstRun()) {
        devLog->Write("First Run - New configuration file " + String(Defaults.ConfigFileName) + " saved", LOGLEVEL_INFO);
        Settings.Save();
    }

    devSaveState = new Timer(Settings.General.SaveStatePooling() * 1000);
    devSaveState->OnTimeout([&] {
        if (Settings.SaveComponentsStateFlag()) {
            if (Settings.SaveComponentsState()) {
                devLog->Write("Component: States saved successfully (Pooling every " + String(Settings.General.SaveStatePooling())+ " second(s))", LOGLEVEL_INFO);
            } else {
                devLog->Write("Component: Failed to save states (Pooling every " + String(Settings.General.SaveStatePooling())+ " second(s))", LOGLEVEL_ERROR);
            }
        }
    });
    devSaveState->Start();
}

void loop() {
    static bool s_updating = false;

    devClock->Control();    
    devNetwork->Control();

    if (devNetwork->ConnectionMode() == APMode::WifiClient ) {
        devMQTT->Control();

        if (devUpdateClient) {
            devUpdateClient->Control();

            if (g_cmdCheckNow) {
                g_cmdCheckNow = false;

                DeviceIQ_Update::Manifest mf;
                bool hasUpd = false, forced = false;

                if (devUpdateClient->CheckForUpdate(mf, &hasUpd, &forced)) {
                    if (hasUpd) {
                        if (!s_updating) {
                        s_updating = true;
                        devUpdateClient->InstallLatest();
                        }
                    } else {
                        devLog->Write("Update Client: No update available", LOGLEVEL_INFO);
                    }
                } else {
                    devLog->Write("Update Client: Check failed (network, manifest, model not compatible)", LOGLEVEL_ERROR);

                    devLog->Write("URL: " + devUpdateClient->ManifestURL(), LOGLEVEL_INFO);
                    
                    s_updating = false;
                }
            }
        }
    }

    Settings.Components.Control();

    devSaveState->Control();
}