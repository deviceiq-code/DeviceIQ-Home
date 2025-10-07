#include <Arduino.h>
#include <LittleFS.h>
#include <Wire.h>
#include <DevIQ_Network.h>
#include <DevIQ_Components.h>
#include <DevIQ_Configuration.h>
#include <DevIQ_FileSystem.h>
#include <DevIQ_MQTT.h>
#include <DevIQ_DateTime.h>
#include <DevIQ_Log.h>
#include <DevIQ_Update.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <AsyncTCP.h>
#include <AsyncUDP.h>

#include <ESPAsyncWebServer.h>

#include "AsyncTelnetServer.h"
#include "ClientManager.h"
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

settings_t Settings;

FileSystem *devFileSystem;
Configuration *devConfiguration;
Clock *devClock;
Log *devLog;
MQTT *devMQTT;
Network *devNetwork;
AsyncWebServer *devWebhook;
AsyncTelnetServer *devTelnetServer;
UpdateClient *devUpdateClient;

AsyncUDP devUDP;

Collection devCollection;

volatile bool g_cmdCheckNow = false;

void setup() {
    Serial.begin(115200);

    // General Settings
    esp_log_level_set("*", ESP_LOG_NONE);

    // FileSystem
    devFileSystem = new FileSystem();

    // Configuration
    devConfiguration = new Configuration(devFileSystem);
    if (!devConfiguration->LoadConfigurationFile(Defaults.ConfigFileName)) {
        //if (!devConfiguration->ResetToDefaultSettings()) while (1);
    }

    // Clock
    devClock = new Clock();
    devClock->EpochUpdate(Defaults.InitialTimeAndDate);

    // Log
    devLog = new Log(devFileSystem, devClock);
    devLog->SerialPort(&Serial);
    devLog->LogFileName(Defaults.Log.LogFileName);
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

    devConfiguration->Set<String>("Network|MAC Address", devNetwork->MAC_Address());

    // Update
    devUpdateClient = new UpdateClient([&]{
        DeviceIQ_Update::UpdateConfig c;
        c.model = Version.ProductName;
        c.currentVersion = Version.Software.Info();
        c.manifestUrl = devConfiguration->Get<String>("Update|Update Manifest", Defaults.Update.ManifestURL);
        c.rootCA_PEM = nullptr;
        c.allowInsecure = devConfiguration->Get<bool>("Update|Allow Insecure", Defaults.Update.AllowInsecure);
        // c.enableLanOta = true;

        c.lanHostname = devNetwork->Hostname();
        c.lanPassword = "";
        c.checkInterval = devConfiguration->Get<uint32_t>("Update|Check Interval", Defaults.Update.CheckUpdateInterval);
        c.httpTimeout = 15;
        c.streamBufSize = size_t(4096);
        c.autoReboot = devConfiguration->Get<bool>("Update|Auto Reboot", Defaults.Update.AutoReboot);

        return c;
    }());

    // Parei aqui - o Set nao ta salvando
    devConfiguration->Set<String>("Update|Update Manifest", Defaults.Update.ManifestURL, SaveUrgency::Critical);

    devUpdateClient->OnError([&](DeviceIQ_Update::Error e, const String& d) {
        if (devConfiguration->Get<bool>("Update|Debug", Defaults.Update.Debug)) devLog->Write("Update Client: Error [" + String((int)e) + "] Detail [" + d + "]", LOGLEVEL_INFO);
    });

    devUpdateClient->OnEvent([&](DeviceIQ_Update::Event e) {
        const String v   = devUpdateClient->LatestVersion();
        const String min = devUpdateClient->LatestMinVersion();
        const String url = devUpdateClient->LatestUrl();
        const bool forced = DeviceIQ_Update::UpdateClient::IsNewer(min, Version.Software.Info());

        switch (e) {
            case Event::NewVersion: {
                devLog->Write("Update Client: New version " + v + " available (MIN " + min + ", FORCED " + (forced ? "Yes" : "No") + ")", forced ? LOGLEVEL_WARNING : LOGLEVEL_INFO);
            } break;
            case Event::Downloading: {
                if (devConfiguration->Get<bool>("Update|Debug", Defaults.Update.Debug)) devLog->Write("Update Client: Downloading " + url, LOGLEVEL_INFO);
            } break;
            case Event::Verifying: {
                if (devConfiguration->Get<bool>("Update|Debug", Defaults.Update.Debug)) devLog->Write("Update Client: Verifying new firmware integrity", LOGLEVEL_INFO);
            } break;
            case Event::Applying: {
                if (devConfiguration->Get<bool>("Update|Debug", Defaults.Update.Debug)) devLog->Write("Update Client: Applying update", LOGLEVEL_INFO);
            } break;
            case Event::Rebooting: {
                if (devConfiguration->Get<bool>("Update|Debug", Defaults.Update.Debug)) devLog->Write("Update Client: Rebooting to apply update", LOGLEVEL_WARNING);
            } break;
            default: break;
        }
    });

    // Components
    for (uint16_t objs = 0; objs < devConfiguration->Elements("Components"); objs++) {
        Classes NewComponent_Class;
        Buses NewComponent_Bus;
        uint8_t NewComponent_BusAddress = devConfiguration->GetAt<uint8_t>("Components", objs, "Address");

        String tmp_Class = devConfiguration->GetAt<String>("Components", objs, "Class");
        String tmp_Bus = devConfiguration->GetAt<String>("Components", objs, "Bus");


        if (AvailableComponentClasses.find(tmp_Class) != AvailableComponentClasses.end()) NewComponent_Class = AvailableComponentClasses.at(tmp_Class);
        if (AvailableComponentBuses.find(tmp_Bus) != AvailableComponentBuses.end()) NewComponent_Bus = AvailableComponentBuses.at(tmp_Bus);

        Generic *NewComponent = nullptr;

        switch (NewComponent_Class) {
            case CLASS_GENERIC: {
                // Reserved
            } break;

            case CLASS_BLINDS: {
                int16_t relayUpIndex = devCollection.IndexOf(devConfiguration->GetAt<String>("Components", objs, "Relay Up"));
                int16_t relayDnIndex = devCollection.IndexOf(devConfiguration->GetAt<String>("Components", objs, "Relay Down"));

                if (relayUpIndex > -1 && relayDnIndex > -1) {
                    NewComponent = new Blinds(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, devCollection.At(relayUpIndex)->as<Relay>(), devCollection.At(relayDnIndex)->as<Relay>());
                    NewComponent->as<Blinds>()->Position(devConfiguration->GetAt<uint8_t>("Components", objs, "Position"), true);
                } else {
                    devLog->Write("Blinds '" + devConfiguration->GetAt<String>("Components", objs, "Name") + "' not created: relay up/down not found", LOGLEVEL_ERROR);
                }

                auto* n = NewComponent->as<Blinds>();
                n->Event["Changed"]([n, objs] {
                    if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Blinds:" + n->Name() + ":Position", String(n->Position()));
                    if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Blinds:" + n->Name() + ":State", String(n->State()));
                });
            } break;

            case CLASS_BUTTON: {
                NewComponent = new Button(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress, (devConfiguration->GetAt<String>("Components", objs, "Report").equalsIgnoreCase("EdgesOnly") ? ButtonReportModes::BUTTONREPORTMODE_EDGESONLY : ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY));

                auto* n = NewComponent->as<Button>();
                if (n->ReportMode() == ButtonReportModes::BUTTONREPORTMODE_CLICKSONLY) {
                    n->Event["Clicked"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "Clicked"); });
                    n->Event["DoubleClicked"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "DoubleClicked"); });
                    n->Event["TripleClicked"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "TripleClicked"); });
                    n->Event["LongClicked"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "LongClicked"); });
                } else if (n->ReportMode() == ButtonReportModes::BUTTONREPORTMODE_EDGESONLY) {
                    n->Event["Pressed"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "Pressed"); });
                    n->Event["Released"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Button:" + n->Name(), "Released"); });
                }
            } break;

            case CLASS_CURRENTMETER: {
                NewComponent = new Currentmeter(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress);

                auto* n = NewComponent->as<Currentmeter>();
                n->Event["Changed"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Currentmeter:" + n->Name() + ":DC", String(n->CurrentDC())); if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Currentmeter:" + n->Name() + ":AC", String(n->CurrentAC())); });
            } break;

            case CLASS_RELAY: {
                NewComponent = new Relay(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress, DeviceIQ_Components::RelayTypes::RELAYTYPE_NORMALLYCLOSED);

                auto* n = NewComponent->as<Relay>();
                n->State(devConfiguration->GetAt<bool>("Components", objs, "State"));

                n->Event["Changed"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Relay:" + n->Name(), n->State() ? "on" : "off"); devConfiguration->SetAt("Components", objs, "State", n->State()); });
            } break;

            case CLASS_PIR: {
                NewComponent = new PIR(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress);

                auto* n = NewComponent->as<PIR>();
                n->DebounceTime(devConfiguration->GetAt<uint32_t>("Components", objs, "Debounce"));

                n->Event["MotionDetected"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/PIR:" + n->Name(), "M"); });
                n->Event["MotionCleared"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/PIR:" + n->Name(), "C"); });
            } break;

            case CLASS_DOORBELL: {
                NewComponent = new Doorbell(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress);

                auto* n = NewComponent->as<Doorbell>();
                n->Timeout(devConfiguration->GetAt<uint32_t>("Components", objs, "Timeout"));

                n->Event["Ring"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Doorbell:" + n->Name(), "1"); });
                n->Event["DoubleRing"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Doorbell:" + n->Name(), "2"); });
                n->Event["LongRing"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Doorbell:" + n->Name(), "L"); });
            } break;

            case CLASS_CONTACTSENSOR: {
                NewComponent = new ContactSensor(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress, devConfiguration->GetAt<bool>("Components", objs, "InvertClose"));

                auto* n = NewComponent->as<ContactSensor>();
                n->Event["Opened"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/ContactSensor:" + n->Name(), "Opened"); });
                n->Event["Closed"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/ContactSensor:" + n->Name(), "Closed"); });
            } break;

            case CLASS_THERMOMETER: {
                auto it = AvailableThermometerTypes.find(devConfiguration->GetAt<String>("Components", objs, "Type"));
                if (it != AvailableThermometerTypes.end()) NewComponent = new Thermometer(devConfiguration->GetAt<String>("Components", objs, "Name"), objs, NewComponent_Bus, NewComponent_BusAddress, it->second);

                auto* n = NewComponent->as<Thermometer>();
                n->Event["TemperatureChanged"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Thermometer:" + n->Name() + ":Temperature", String(n->Temperature())); });
                n->Event["HumidityChanged"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Thermometer:" + n->Name() + ":Humidity", String(n->Humidity())); });
                n->Event["Changed"]([n, objs] { if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Thermometer:" + n->Name() + ":Temperature", String(n->Temperature())); if (devMQTT) devMQTT->Publish(devNetwork->Hostname() + "/Get/Thermometer:" + n->Name() + ":Humidity", String(n->Humidity())); });
            } break;
        }

        NewComponent->Enabled(devConfiguration->GetAt<bool>("Components", objs, "Enable", Defaults.Components.Enable));

        // Components Events
        if (NewComponent) {
            JsonObjectConst EventsInConfig = devConfiguration->GetAt<JsonObjectConst>("Components", objs, "Events");
            
            for (auto& ComponentEvent : NewComponent->Event) {
                const String& eventName = ComponentEvent.first;
                auto& setHandler = ComponentEvent.second;
                
                JsonVariantConst v = EventsInConfig[eventName.c_str()];
                if (v.isNull() || !v.is<const char*>()) continue;

                String actionFull = String(v.as<const char*>());
                actionFull.trim();

                int open  = actionFull.indexOf('(');
                int close = actionFull.lastIndexOf(')');

                String cmd = (open > 0) ? actionFull.substring(0, open) : actionFull;
                String param = (open >= 0 && close > open) ? actionFull.substring(open + 1, close) : "";
                cmd.trim(); param.trim();

                // static macros
                param.replace("%NAME%", NewComponent->Name());

                if (cmd.equalsIgnoreCase("log")) {
                    String msg = param;
                    auto logPtr = devLog;
                    setHandler([msg, logPtr]{
                        logPtr->Write(msg, LOGLEVEL_INFO);
                    });
                } else if (cmd.equalsIgnoreCase("enable")) {
                    String targetName = param;
                    auto* target = devCollection[targetName];

                    if (target) {
                        auto* generic = target->as<Generic>();
                        setHandler([generic] {
                            generic->Enabled(true);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("disable")) {
                    String targetName = param;
                    auto* target = devCollection[targetName];

                    if (target) {
                        auto* generic = target->as<Generic>();
                        setHandler([generic] {
                            generic->Enabled(false);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("invert")) {
                    String targetName = param;
                    auto* target = devCollection[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->Invert();
                        });
                    }
                } else if (cmd.equalsIgnoreCase("seton")) {
                    String targetName = param;
                    auto* target = devCollection[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->State(true);
                        });
                    }
                } else if (cmd.equalsIgnoreCase("setoff")) {
                    String targetName = param;
                    auto* target = devCollection[targetName];

                    if (target && target->Class() == CLASS_RELAY) {
                        auto* relay = target->as<Relay>();
                        setHandler([relay] {
                            relay->State(false);
                        });
                    }
                }
            }
            devCollection.Add(NewComponent);
        }
    }

    devLog->Write("Components: " + String(devCollection.Count()) + " component(s) installed", LOGLEVEL_INFO);

    // Components callbacks
    static bool interfacesRegistered = false;

    devNetwork->OnModeChanged([&] {
        APMode t = devNetwork->ConnectionMode();

        switch (t) {
            case WifiClient: {
                devLog->Write("Network: " + devNetwork->Hostname() + " MAC " + devNetwork->MAC_Address() + " connected to " + devNetwork->SSID() + " IP " + devNetwork->IP_Address().toString(), LOGLEVEL_INFO);

                // NTP
                if (devConfiguration->Get<bool>("General|NTP Update", Defaults.NTP.Update)) {
                    if (devClock->NTPUpdate(devConfiguration->Get<String>("General|NTP Server", Defaults.NTP.Server))) {
                        devLog->Write("Date and Time: Updated from NTP server " + devConfiguration->Get<String>("General|NTP Server", Defaults.NTP.Server), LOGLEVEL_INFO);
                    } else {
                        devLog->Write("Date and Time: Failed to update from NTP server " + devConfiguration->Get<String>("General|NTP Server", Defaults.NTP.Server), LOGLEVEL_ERROR);
                    }
                } else {
                    devLog->Write("Date and Time: Using local settings", LOGLEVEL_INFO);
                }

                // Orchestrator
                ClientManager::getInstance().begin();
                if (devConfiguration->Get<bool>("Orchestrator|Assigned", Defaults.Orchestrator.Assigned) && !devConfiguration->Get<String>("Orchestrator|Server ID").isEmpty()) {
                    devLog->Write("Orchestrator: Device assigned to server ID " + devConfiguration->Get<String>("Orchestrator|Server ID"), LOGLEVEL_WARNING);
                    JsonDocument cmd;
                    cmd["Server ID"] = devConfiguration->Get<String>("Orchestrator|Server ID");
                    
                    ClientManager::getInstance().Refresh(cmd);
                } else {
                    devLog->Write("Orchestrator: Device is not assigned to an Orchestrator server", LOGLEVEL_WARNING);
                }

                // Webhooks/MQTT
                if (!interfacesRegistered) {
                    devWebhook = new AsyncWebServer(devConfiguration->Get<uint16_t>("Webhooks|Port", Defaults.WebHooks.Port));
                    devWebhook->onNotFound([](AsyncWebServerRequest *request) { request->send(404); });

                    // Webhooks
                    if (devConfiguration->Get<bool>("Webhooks|Enable", Defaults.WebHooks.Enable)) {
                        for (auto m : devCollection) {
                            switch (m->Class()) {

                                case CLASS_RELAY: {
                                    devWebhook->on(String("/" + m->Name()).c_str(), HTTP_GET, [=](AsyncWebServerRequest *request) {
                                        JsonDocument reply;
                                        String json;

                                        if (hasValidHeaderToken(request, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token())) == true) {
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
                                    registerEndpoint(devWebhook, m, "PIR", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Motion"] = comp->as<PIR>()->State();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_BUTTON: {
                                    registerEndpoint(devWebhook, m, "Button", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Pressed"] = comp->as<Button>()->IsPressed();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_CURRENTMETER: {
                                    registerEndpoint(devWebhook, m, "Currentmeter", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Current AC"] = comp->as<Currentmeter>()->CurrentAC();
                                        reply["Current DC"] = comp->as<Currentmeter>()->CurrentDC();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_THERMOMETER: {
                                    registerEndpoint(devWebhook, m, "Thermometer", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Humidity"] = comp->as<Thermometer>()->Humidity();
                                        reply["Temperature"] = comp->as<Thermometer>()->Temperature();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_BLINDS: {
                                    registerEndpoint(devWebhook, m, "Blinds", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["Position"] = comp->as<Blinds>()->Position();
                                        reply["State"] = comp->as<Blinds>()->State() == BlindsStates::BLINDSSTATE_DECREASING ? "Decreasing" : (comp->as<Blinds>()->State() == BlindsStates::BLINDSSTATE_INCREASING ? "Increasing" : "Stopped");
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_DOORBELL: {
                                    registerEndpoint(devWebhook, m, "Doorbell", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["State"] = comp->as<Doorbell>()->State();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;

                                case CLASS_CONTACTSENSOR: {
                                    registerEndpoint(devWebhook, m, "ContactSensor", [](JsonDocument& reply, DeviceIQ_Components::Generic* comp) {
                                        reply["State"] = comp->as<ContactSensor>()->State();
                                    }, devConfiguration->Get<String>("Webhooks|Token", Defaults.WebHooks.Token()), devLog);
                                } break;
                            }
                        }

                        devWebhook->begin();
                        devLog->Write("Webhooks: Enabled on port " + String(devConfiguration->Get<uint16_t>("Webhooks|Port", Defaults.WebHooks.Port)), LOGLEVEL_INFO);
                    } else {
                        devLog->Write("Webhooks: Disabled", LOGLEVEL_INFO);
                    }

                    // MQTT
                    if (devConfiguration->Get<bool>("MQTT|Enable", Defaults.MQTT.Enable)) {
                        devMQTT->Broker(devConfiguration->Get("MQTT|Broker", Defaults.MQTT.Broker));
                        devMQTT->Port(devConfiguration->Get<uint16_t>("MQTT|Port", Defaults.MQTT.Port));
                        devMQTT->User(devConfiguration->Get("MQTT|User", Defaults.MQTT.User));
                        devMQTT->Password(devConfiguration->Get("MQTT|Password", Defaults.MQTT.Password));

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

                                auto comp =  devCollection[tmpName];

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
                    for (auto m : devCollection) { m->Refresh(); }

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

        if (devConfiguration->Get<bool>("Update|Check On Boot", Defaults.Update.CheckOnBoot)) devUpdateClient->CheckUpdateNow();


    });
    devNetwork->Connect();
}

void loop() {
    static bool s_updating = false;

    devClock->Control();    
    devNetwork->Control();
    devConfiguration->Control();

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

    devCollection.Control();
}