#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EloquentTinyML.h>
#include "sine_model.h"

#define N_INPUTS 1
#define N_OUTPUTS 1
#define TENSOR_ARENA_SIZE 2 * 1024

Eloquent::TinyML::TfLite<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> tf;

// WiFi SSID and password

#define WIFI_SSID "Berto's iPhone"
#define WIFI_PASSWORD "fogfogfog"

// MQTT Broker configuration and port

#define MQTT_HOST IPAddress(172, 20, 10, 5)
#define MQTT_PORT 1883

// Global variables for MQTT and timer for handling the reconnections

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

// Variable to keep track of the iteration number

int currentIteration = 0;

// Method to connect to WiFi

void connectToWifi()
{
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Method to connect to Mqtt

void connectToMqtt()
{
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials("federico", "iotexamdemo");
  mqttClient.connect();
}

// Callback for WiFi connection

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

// Callback for WiFi disconnection

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

// Callback for mqtt connection

void onMqttConnect(bool sessionPresent)
{
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

// Callback for mqtt disconnection

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

// Setup method

void setup()
{
  Serial.begin(9600);
  Serial.println();

  // TensorFlow initialization

  tf.begin(model_data);

  // check if model loaded fine
  if (!tf.initialized())
  {
    Serial.print("Error initializing TensorFlow");
    while (true)
      delay(1000);
  }

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

// Loop method

void loop()
{
  if (WiFi.isConnected() && mqttClient.connected())
  {
    // Runs a loop of 10 iterations, at every one it sends the result (if enabled)

    for (uint8_t i = 0; i < 10; i++)
    {

      currentIteration += 1; // To keep track of iterations between loops

      // pick x from 0 to PI
      float x = 3.14 * i / 10;
      float y = sin(x);
      float input[1] = {x};

      uint32_t start = micros(); // Evaluation start time
      float predicted = tf.predict(input);
      uint32_t end = micros() - start; // Evaluation end time

      Serial.print("sin(");
      Serial.print(x);
      Serial.print(") = ");
      Serial.print(y);
      Serial.print("\t predicted: ");
      Serial.println(predicted);
      Serial.println("Evaluation time: " + String(end) + " microseconds");

      // DTO to be created
      //{"board": "esp8266", "model": "sin", "result": 3.14, "iteration": 1, "microseconds": 120}

      String startPar = "{";
      String board = "\"board\":\"esp8266\",";
      String model = "\"model\":\"sin\",";
      String result = "\"result\":" + String(x) + ",";
      String iteration = "\"iteration\":" + String(uint16_t(i)) + ",";
      String time = "\"microseconds\":" + String(uint16_t(end));
      String endPar = "}";

      String resultString = startPar + board + model + result + iteration + time + endPar;

      uint16_t packetId = mqttClient.publish("iotdemo.esp8266", 1, true, (char *)resultString.c_str());
      Serial.print("Message sent with packetId: ");
      Serial.println(packetId);
      delay(100);
    }
  }
  else
  {
    Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
  }

  delay(5000);
}