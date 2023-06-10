# ESP8266 MQTT Sensor Node

This is an ESP8266-based project that reads data from a HTU21D temperature and humidity sensor and a light-dependent resistor (LDR) for light intensity. The sensor data is then sent to an MQTT broker. The data is published to specified MQTT topics. This project is built with [PlatformIO](https://platformio.org/), an open-source ecosystem for IoT development.

The project is equipped with a configuration interface that allows the user to configure WiFi and MQTT broker details via a serial console. It uses LittleFS for storing the configuration in the flash memory of the ESP8266.

## Functionalities

1. Read data from HTU21D and LDR sensors.
2. Connect to a WiFi network.
3. Connect to an MQTT broker and publish sensor data to configured topics.
4. LED indication for various statuses.
5. Ability to set or reset configurations via serial console.
6. Store and retrieve configuration from ESP8266's flash memory.

## Setup

### Hardware

1. Assemble your ESP8266 with HTU21D sensor and LDR module, and ensure all connections are correct.

### Software

1. Install [PlatformIO](https://platformio.org/install/cli) and set it up as per the instructions.
2. Clone this repository.
3. Open the project in PlatformIO.

### Device Configuration

The configuration can be done via a serial console. Use a serial monitor tool and connect to the ESP8266's serial port with a baud rate of 115200.

- To set the configuration, send a JSON payload in the following format (remove all newlines, terminate with newline):

    ```
    {
        "wifi_ssid": "",
        "wifi_password": "",
        "mqtt_server": "",
        "mqtt_port": 1883,
        "mqtt_user": "",
        "mqtt_pass": "",
        "mqtt_name": "name of the main topic",
        "temp_topic": "",
        "hum_topic": "",
        "light_topic": "",
        "postInterval": 10
    }
    ```

- To reset the configuration, send the string `RESET` + \n.

### LittleFS

The project uses LittleFS filesystem to store the configuration data on the ESP8266's flash memory. To format the filesystem, you need to upload a sketch. Please follow the instructions in the official Arduino ESP8266 filesystem documentation: [Arduino ESP8266 filesystem uploader](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system)

## Further Reading

For more information about PlatformIO, visit the [official PlatformIO documentation](https://docs.platformio.org/en/latest/core/quickstart.html).

## Notes

This project is for educational purposes and is not meant for use in production environments without further enhancements and testing.

## Disclaimer

Use this project at your own risk. The author is not responsible for any damages caused.
