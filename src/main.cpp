#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "SparkFunHTU21D.h"
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h> // Include the ArduinoJson library

#define CONFIG_FILE "/config.json"

#define LEDR D8
#define LEDG D6
#define LEDB D7
#define BLINK_INTERVAL 250 // define blink interval in ms

unsigned long lastAttemptTime = 0;
unsigned long lastBlinkTime = 0;
long lastMsg = 0;

struct Config
{
  String wifi_ssid;
  String wifi_password;
  String mqtt_server;
  int mqtt_port;
  String mqtt_user;
  String mqtt_pass;
  String mqtt_name;
  String temp_topic;
  String hum_topic;
  String light_topic;
  int postInterval;

  bool isValid()
  {
    return !wifi_ssid.isEmpty() && !wifi_password.isEmpty() && !mqtt_server.isEmpty() &&
           !mqtt_user.isEmpty() && !mqtt_pass.isEmpty() && !temp_topic.isEmpty() &&
           !hum_topic.isEmpty() && !light_topic.isEmpty() && postInterval > 0 && mqtt_port > 0;
  }
};

Config config;

// Create an instance of the HTU21D sensor
HTU21D myHumidity;

// Setup the MQTT client class by passing in the WiFi client
WiFiClient espClient;
PubSubClient client(espClient);

void ledColor(int r = 0, int g = 0, int b = 0)
{
  analogWrite(LEDR, r);
  analogWrite(LEDG, g);
  analogWrite(LEDB, b);
}

void ledGreen(int brightness = 255)
{
  ledColor(0, brightness, 0);
}

void ledRed(int brightness = 255)
{
  ledColor(brightness, 0, 0);
}

void ledBlue(int brightness = 255)
{
  ledColor(0, 0, brightness);
}

void ledYellow(int brightness = 255)
{
  ledColor(brightness, brightness, 0);
}

void ledBlink(int r = 0, int g = 0, int b = 0)
{
  if (millis() - lastBlinkTime > BLINK_INTERVAL)
  {
    if (digitalRead(LEDR) == 0 && digitalRead(LEDG) == 0 && digitalRead(LEDB) == 0)
    {
      ledColor(r, g, b);
    }
    else
    {
      ledColor(0, 0, 0);
    }
    lastBlinkTime = millis();
  }
}

int readLDR(int numSamples = 10)
{
  int val = 0;
  for (int i = 0; i < numSamples; i++)
  {
    val += analogRead(A0);
    delay(10);
  }
  val /= numSamples;
  return val;
}

void readConfiguration()
{
  if (!LittleFS.begin())
  {
    Serial.println("An error occurred while mounting LittleFS");
    return;
  }

  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile)
  {
    Serial.println("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, buf.get());

  if (error)
  {
    Serial.println("Failed to parse config file");
    return;
  }

  config.wifi_ssid = doc["wifi_ssid"] | "";
  config.wifi_password = doc["wifi_password"] | "";
  config.mqtt_server = doc["mqtt_server"] | "";
  config.mqtt_port = doc["mqtt_port"] | 0;
  config.mqtt_user = doc["mqtt_user"] | "";
  config.mqtt_pass = doc["mqtt_pass"] | "";
  config.mqtt_name = doc["mqtt_name"] | "";
  config.temp_topic = doc["temp_topic"] | "";
  config.hum_topic = doc["hum_topic"] | "";
  config.light_topic = doc["light_topic"] | "";

  config.postInterval = doc["postInterval"] | 60000; // Default 1 minute
  configFile.close();

  // Check if any field is a "null" string.
  if (!config.isValid())
  {
    Serial.println("Invalid configuration. Removing config file...");
    LittleFS.remove(CONFIG_FILE);
  }
}

void writeConfiguration()
{
  if (!config.isValid())
  {
    Serial.println("Invalid configuration. Refusing to write");
    return;
  }

  DynamicJsonDocument doc(1024);

  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_password"] = config.wifi_password;
  doc["mqtt_server"] = config.mqtt_server;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_user"] = config.mqtt_user;
  doc["mqtt_pass"] = config.mqtt_pass;
  doc["mqtt_name"] = config.mqtt_name;
  doc["temp_topic"] = config.temp_topic;
  doc["hum_topic"] = config.hum_topic;
  doc["light_topic"] = config.light_topic;
  doc["postInterval"] = config.postInterval;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
    return;
  }

  Serial.println("Writing valid Config file.....");
  serializeJsonPretty(doc, Serial);
  serializeJson(doc, configFile);
  configFile.close();
}

void setup_wifi()
{
  // If the SSID or password is not set, return immediately
  if (config.wifi_ssid.isEmpty() || config.wifi_password.isEmpty())
  {
    Serial.println("WiFi SSID or password not set. Skipping WiFi connection setup.");
    return;
  }

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(config.wifi_ssid.c_str());

  WiFi.begin(config.wifi_ssid.c_str(), config.wifi_password.c_str());

  while (WiFi.status() != WL_CONNECTED)
  {
    ledBlink(255, 0, 0); // flash RED if not able to connect to wifi
    delay(500);          // delay to avoid WDT reset
  }

  ledBlue(); // solid BLUE if connected wifi
}

void sendValues()
{
  ledGreen(50); // flash GREEN when busy sending
  delay(200);
  ledGreen(0);

  float hum = myHumidity.readHumidity();
  float temp = myHumidity.readTemperature();
  int ldr = readLDR();

  Serial.println(String(hum, 2) + "|" + String(temp, 2) + "|" + String(ldr));

  String full_temp_topic = config.mqtt_name + "/" + config.temp_topic;
  String full_hum_topic = config.mqtt_name + "/" + config.hum_topic;
  String full_light_topic = config.mqtt_name + "/" + config.light_topic;

  if (client.publish(full_temp_topic.c_str(), String(temp).c_str(), true) &&
      client.publish(full_hum_topic.c_str(), String(hum).c_str(), true) &&
      client.publish(full_light_topic.c_str(), String(ldr).c_str(), true))
  {
    ledGreen(); // solid GREEN once the first message was correctly sent over MQTT
  }
  else
  {
    ledRed(); // solid RED if error
  }
}

void connectMqtt()
{
  // Check if we're already connected
  if (client.connected())
  {
    return;
  }

  // Check if it has been 5 seconds since the last connection attempt
  if (millis() - lastAttemptTime > 5000)
  {
    lastAttemptTime = millis(); // Update the last attempt time

    ledBlink(0, 0, 255); // flash BLUE if connected to wifi but not able to send MQTT

    Serial.println("Attempting MQTT connection to " + config.mqtt_server + ":" + config.mqtt_port);
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    String clientId = config.mqtt_name + "_sensor_temphum_" + WiFi.macAddress();
    clientId.replace(":", ""); // Remove colons from MAC address

    if (client.connect(clientId.c_str(), config.mqtt_user.c_str(), config.mqtt_pass.c_str()))
    {
      ledGreen(); // solid GREEN once the first message was correctly sent over MQTT
      sendValues();
      Serial.println("connected");
    }
    else
    {
      ledRed(); // solid RED if error
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
    }
  }
}

void processSerialInput(String input)
{
  input.trim(); // This will remove leading/trailing whitespace from input

  Serial.println(input);

  if (input.startsWith("{"))
  {
    DynamicJsonDocument doc(1024);
    auto error = deserializeJson(doc, input);
    if (error)
    {
      Serial.println("Failed to parse configuration");
      return;
    }

    Serial.println("Processing config data..");

    config.wifi_ssid = doc["wifi_ssid"].as<String>();
    config.wifi_password = doc["wifi_password"].as<String>();
    config.mqtt_server = doc["mqtt_server"].as<String>();
    config.mqtt_port = doc["mqtt_port"].as<int>();
    config.mqtt_user = doc["mqtt_user"].as<String>();
    config.mqtt_pass = doc["mqtt_pass"].as<String>();
    config.mqtt_name = doc["mqtt_name"].as<String>();
    config.temp_topic = doc["temp_topic"].as<String>();
    config.hum_topic = doc["hum_topic"].as<String>();
    config.light_topic = doc["light_topic"].as<String>();
    config.postInterval = doc["postInterval"].as<int>() * 1000;

    writeConfiguration();
  }
  else if (input == "RESET")
  {
    if (LittleFS.remove(CONFIG_FILE))
    {
      Serial.println("Configuration file removed. Please send a new configuration.");
    }
    else
    {
      Serial.println("Failed to remove configuration file. Please try again.");
    }
  }
  else
  {
    Serial.println("Invalid command or JSON configuration. Please try again.");
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  myHumidity.begin(Wire);

  if (!LittleFS.begin())
  {
    Serial.println("An error occurred while mounting LittleFS");
    return;
  }

  if (!LittleFS.exists(CONFIG_FILE))
  {
    ledYellow(); // solid YELLOW if not configured
    Serial.println("\n\n\nNo configuration found. Send a JSON object with the following structure to configure:");
    Serial.println("{ \"wifi_ssid\": \"your_ssid\", \"wifi_password\": \"your_password\", \"mqtt_server\": \"your_server\", \"mqtt_user\": \"your_user\", \"mqtt_pass\": \"your_pass\", \"temp_topic\": \"your_temp_topic\", \"hum_topic\": \"your_hum_topic\", \"light_topic\": \"your_light_topic\", \"postInterval\": \"your_postInterval\" }");
  }
  else
  {
    readConfiguration();

    if (!config.isValid())
    {
      LittleFS.remove(CONFIG_FILE);
      Serial.println("\nInvalid configuration, removed the configuration file. Restarting...");
      ESP.restart();
    }
  }
}

void loop()
{
  // Always read serial if available
  if (Serial.available())
  {
    String input = Serial.readStringUntil('\n');
    processSerialInput(input);
  }

  // If no valid config, indicate with YELLOW LED and wait for new configuration
  if (!LittleFS.exists(CONFIG_FILE) || !config.isValid())
  {
    ledBlink(255, 255, 0); // flash YELLOW if not configured
  }
  // If not connected to WiFi, attempt to connect
  else if (WiFi.status() != WL_CONNECTED)
  {
    ledBlink(255, 0, 0); // flash RED if not able to connect to wifi
    setup_wifi();
  }
  // If not connected to MQTT, attempt to reconnect
  else if (!client.connected())
  {
    ledBlink(0, 0, 255); // flash BLUE if connected to wifi but not able to send MQTT
    client.setServer(config.mqtt_server.c_str(), config.mqtt_port);
    connectMqtt();
  }
  // If connected to MQTT, process MQTT messages and send values at the defined interval
  else
  {
    client.loop();

    long now = millis();
    if (now - lastMsg > config.postInterval)
    {
      ledBlink(0, 255, 0); // flash GREEN when busy sending
      lastMsg = now;
      sendValues();
    }
    else
    {
      ledGreen(); // solid GREEN if not sending
    }
  }
}
