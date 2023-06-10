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

unsigned long lastBlinkTime = 0;
long lastMsg = 0;

// WiFi and MQTT configuration variables
String wifi_ssid;
String wifi_password;
String mqtt_server;
String mqtt_user;
String mqtt_pass;
String temp_topic;
String hum_topic;
String light_topic;
int postInterval;

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

void setup_wifi()
{
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid.c_str());

  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

  while (WiFi.status() != WL_CONNECTED)
  {
    ledBlink(255, 0, 0); // flash RED if not able to connect to wifi
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

  if (client.publish(temp_topic.c_str(), String(temp).c_str(), true) &&
      client.publish(hum_topic.c_str(), String(hum).c_str(), true) &&
      client.publish(light_topic.c_str(), String(ldr).c_str(), true))
  {
    ledGreen(); // solid GREEN once the first message was correctly sent over MQTT
  }
  else
  {
    ledRed(); // solid RED if error
  }
}

void readConfiguration() {
  if (!LittleFS.begin()) {
    Serial.println("An error occurred while mounting LittleFS");
    return;
  }

  File configFile = LittleFS.open(CONFIG_FILE, "r");
  if (!configFile) {
    Serial.println("Failed to open config file");
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  DynamicJsonDocument doc(1024);
  auto error = deserializeJson(doc, buf.get());

  if (error) {
    Serial.println("Failed to parse config file");
    return;
  }

  wifi_ssid = doc["wifi_ssid"] | "";
  wifi_password = doc["wifi_password"] | "";
  mqtt_server = doc["mqtt_server"] | "";
  mqtt_user = doc["mqtt_user"] | "";
  mqtt_pass = doc["mqtt_pass"] | "";

  temp_topic = doc["temp_topic"] | "";
  hum_topic = doc["hum_topic"] | "";
  light_topic = doc["light_topic"] | "";

  postInterval = doc["postInterval"] | 1 * 60 * 1000; // Default 1 minute

  configFile.close();
}

void writeConfiguration() {
  DynamicJsonDocument doc(1024);

  doc["wifi_ssid"] = wifi_ssid;
  doc["wifi_password"] = wifi_password;
  doc["mqtt_server"] = mqtt_server;
  doc["mqtt_user"] = mqtt_user;
  doc["mqtt_pass"] = mqtt_pass;

  doc["temp_topic"] = temp_topic;
  doc["hum_topic"] = hum_topic;
  doc["light_topic"] = light_topic;

  doc["postInterval"] = postInterval;

  File configFile = LittleFS.open(CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJson(doc, configFile);
  configFile.close();
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    ledBlink(0, 0, 255); // flash BLUE if connected to wifi but not able to send MQTT

    Serial.print("Attempting MQTT connection...");

    String clientId = "sensor_temphum_" + WiFi.macAddress();
    clientId.replace(":", ""); // Remove colons from MAC address

    if (client.connect(clientId.c_str(), mqtt_user.c_str(), mqtt_pass.c_str()))
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
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.begin(SDA, SCL);
  myHumidity.begin(Wire);

  SPIFFS.begin(); // Initialize SPIFFS

  if (!SPIFFS.exists(CONFIG_FILE))
  {
    ledYellow(); // solid YELLOW if not configured
  }

  String input = Serial.readStringUntil('\n');
  if (input == "RESET")
  {
    SPIFFS.remove(CONFIG_FILE);
    ESP.restart();
  }
  else if (input.startsWith("{"))
  {
    DynamicJsonDocument doc(1024);
    auto error = deserializeJson(doc, input);
    if (error)
    {
      Serial.println("Failed to parse configuration");
      return;
    }

    wifi_ssid = doc["wifi_ssid"].as<String>();
    wifi_password = doc["wifi_password"].as<String>();
    mqtt_server = doc["mqtt_server"].as<String>();
    mqtt_user = doc["mqtt_user"].as<String>();
    mqtt_pass = doc["mqtt_pass"].as<String>();
    temp_topic = doc["temp_topic"].as<String>();
    hum_topic = doc["hum_topic"].as<String>();
    light_topic = doc["light_topic"].as<String>();
    postInterval = doc["postInterval"].as<int>();

    writeConfiguration();
  }
  else
  {
    if (!SPIFFS.exists(CONFIG_FILE))
    {
      Serial.println("No configuration found. Send a JSON object with the following structure to configure:");
      Serial.println("{ \"wifi_ssid\": \"your_ssid\", \"wifi_password\": \"your_password\", \"mqtt_server\": \"your_server\", \"mqtt_user\": \"your_user\", \"mqtt_pass\": \"your_pass\", \"temp_topic\": \"your_temp_topic\", \"hum_topic\": \"your_hum_topic\", \"light_topic\": \"your_light_topic\", \"postInterval\": \"your_postInterval\" }");
    }
    else
    {
      readConfiguration();
    }
  }

  setup_wifi();
  client.setServer(mqtt_server.c_str(), 1883);
}

void loop()
{
  if (!SPIFFS.exists(CONFIG_FILE))
  {
    ledBlink(255, 255, 0); // flash YELLOW if not configured
  }
  else if (WiFi.status() != WL_CONNECTED)
  {
    ledBlink(255, 0, 0); // flash RED if not able to connect to wifi
    setup_wifi();
  }
  else if (!client.connected())
  {
    ledBlink(0, 0, 255); // flash BLUE if connected to wifi but not able to send MQTT
    reconnect();
  }
  else
  {
    client.loop();

    long now = millis();
    if (now - lastMsg > postInterval)
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