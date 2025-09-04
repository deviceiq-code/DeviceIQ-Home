# DeviceIQ Home

DeviceIQ Home is an open-source home automation framework for ESP32 devices.  
It provides a modular structure for managing components (sensors, actuators, relays, etc.), defining automation rules, and handling over-the-air updates.  

> ⚠️ This project is still in active development. Both DeviceIQ Home and the underlying libraries are being updated constantly with new features and improvements.

---

## Features

- JSON-based configuration for devices and components  
- Lightweight scripting engine for defining triggers and actions  
- Support for multiple component types (relays, buttons, PIR, blinds, thermometers, contact sensors, etc.)  
- Modular library-based architecture for easy extension  
- Local OTA (Over-the-Air) firmware updates  
- Designed to run entirely on ESP32  

---

## Architecture

DeviceIQ Home is built on top of several dedicated libraries, all available in the [deviceiq-code GitHub organization](https://github.com/deviceiq-code):  

- **[DeviceIQ Lib Components](https://github.com/deviceiq-code/DeviceIQ-Lib-Components)** – Manages sensors, actuators, relays, buttons, PIRs, blinds, thermometers, and more  
- **[DeviceIQ Lib Network](https://github.com/deviceiq-code/DeviceIQ-Lib-Network)** – Simplifies UDP/TCP communication between devices and orchestrators  
- **[DeviceIQ Lib DateTime](https://github.com/deviceiq-code/DeviceIQ-Lib-DateTime)** – It provides easy manipulation, formatting, and NTP synchronization without holding persistent NTP client objects in memory  
- **[DeviceIQ Lib Log](https://github.com/deviceiq-code/DeviceIQ-Lib-Log)** – Record messages with different severity levels and send them to multiple endpoints such as the serial port, a Syslog server via UDP, and files stored in LittleFS  
- **[DeviceIQ Lib Configuration](https://github.com/deviceiq-code/DeviceIQ-Lib-Configuration)** – Lightweight configuration management library designed for embedded systems based on ESP32/ESP8266
- **[DeviceIQ Lib MQTT](https://github.com/deviceiq-code/DeviceIQ-Lib-Configuration)** – MQTT client library designed for ESP32/ESP8266 projects
- **[DeviceIQ Lib FileSystem](https://github.com/deviceiq-code/DeviceIQ-Lib-FileSystem)** – file system utility library for Arduino/ESP32 projects using LittleFS
- **[DeviceIQ Lib Updater](https://github.com/deviceiq-code/DeviceIQ-Lib-Updater)** – OTA Update client

---

## OTA Update Server

DeviceIQ Home supports OTA updates using a simple web server.  

1. Place firmware binaries inside a `/bin/` folder  
2. Keep an `update.json` file in the root directory, describing the latest version and checksum  
3. Devices check this file to know when a new update is available  

You can see a working example here:  
[https://server.dts-network.com:8081/](https://server.dts-network.com:8081/)  

---

## Getting Started

1. Clone this repository and the required libraries  
2. Install dependencies via PlatformIO  
3. Adjust the configuration JSON to match your components  
4. Flash to your ESP32 device  
5. Access your update server for OTA when needed  

---

## Roadmap

- Add more supported component types  
- Improve automation scripting language  
- Enhance stability and performance  
- Provide ready-to-use examples for common automation setups  

---

## Contributing

Contributions are welcome!  
Feel free to open issues, submit pull requests, or suggest improvements.  

---

## License

MIT License. See [LICENSE](LICENSE) for details.
