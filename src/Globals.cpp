#include "Globals.h"

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