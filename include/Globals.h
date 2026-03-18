#pragma once

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

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/icmp.h>
#include <lwip/ip4.h>
#include <lwip/netdb.h>
#include <lwip/inet_chksum.h>

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

extern FileSystem *devFileSystem;
extern Clock *devClock;
extern Log *devLog;
extern MQTT *devMQTT;
extern Network *devNetwork;
extern AsyncWebServer *devWebServer;
extern AsyncTelnetServer *devTelnetServer;
extern UpdateClient *devUpdateClient;

extern Timer *devSaveState;

extern settings_t Settings;
extern orchestrator Orchestrator;