#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EloquentTinyML.h>
#include "wine_model.h"

#define NUMBER_OF_INPUTS 13
#define NUMBER_OF_OUTPUTS 3
#define TENSOR_ARENA_SIZE 16 * 1024

Eloquent::TinyML::TfLite<NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, TENSOR_ARENA_SIZE> tf;

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

// Callback for mqtt subscription

void onMqttSubscribe(uint16_t packetId, uint8_t qos)
{
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

// Callback for mqtt unsubscription

void onMqttUnsubscribe(uint16_t packetId)
{
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

// Callback for mqtt published message

void onMqttPublish(uint16_t packetId)
{
  //Serial.println("Publish acknowledged.");
  // Serial.print("  packetId: ");
  // Serial.println(packetId);
}

// Elements for evaluation

float X_test[20][13] = {
    {1.340e+01, 4.600e+00, 2.860e+00, 2.500e+01, 1.120e+02, 1.980e+00,
     9.600e-01, 2.700e-01, 1.110e+00, 8.500e+00, 6.700e-01, 1.920e+00, 6.300e+02},
    {1.285e+01, 3.270e+00, 2.580e+00, 2.200e+01, 1.060e+02, 1.650e+00,
     6.000e-01, 6.000e-01, 9.600e-01, 5.580e+00, 8.700e-01, 2.110e+00, 5.700e+02},
    {1.334e+01, 9.400e-01, 2.360e+00, 1.700e+01, 1.100e+02, 2.530e+00,
     1.300e+00, 5.500e-01, 4.200e-01, 3.170e+00, 1.020e+00, 1.930e+00, 7.500e+02},
    {1.423e+01, 1.710e+00, 2.430e+00, 1.560e+01, 1.270e+02, 2.800e+00,
     3.060e+00, 2.800e-01, 2.290e+00, 5.640e+00, 1.040e+00, 3.920e+00, 1.065e+03},
    {1.483e+01, 1.640e+00, 2.170e+00, 1.400e+01, 9.700e+01, 2.800e+00,
     2.980e+00, 2.900e-01, 1.980e+00, 5.200e+00, 1.080e+00, 2.850e+00, 1.045e+03},
    {1.245e+01, 3.030e+00, 2.640e+00, 2.700e+01, 9.700e+01, 1.900e+00,
     5.800e-01, 6.300e-01, 1.140e+00, 7.500e+00, 6.700e-01, 1.730e+00, 8.800e+02},
    {1.430e+01, 1.920e+00, 2.720e+00, 2.000e+01, 1.200e+02, 2.800e+00,
     3.140e+00, 3.300e-01, 1.970e+00, 6.200e+00, 1.070e+00, 2.650e+00, 1.280e+03},
    {1.390e+01, 1.680e+00, 2.120e+00, 1.600e+01, 1.010e+02, 3.100e+00,
     3.390e+00, 2.100e-01, 2.140e+00, 6.100e+00, 9.100e-01, 3.330e+00, 9.850e+02},
    {1.165e+01, 1.670e+00, 2.620e+00, 2.600e+01, 8.800e+01, 1.920e+00,
     1.610e+00, 4.000e-01, 1.340e+00, 2.600e+00, 1.360e+00, 3.210e+00, 5.620e+02}};

uint8_t y_test[20] = {2, 2, 1, 0, 0, 2, 0, 0, 1, 1, 0, 2, 1, 1, 1, 1, 1, 0, 0, 0};

// Setup method

void setup()
{
  Serial.begin(9600);
  Serial.println();

  // TensorFlow initialization

  tf.begin(wine_model);

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
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
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

      Serial.println("Inside loop");

      currentIteration += 1; // To keep track of iterations between loops

      uint32_t start = micros(); // Evaluation start time
      uint8_t result = tf.predictClass(X_test[i]);
      uint32_t end = micros() - start; // Evaluation end time

      Serial.print("Sample #");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print("predicted ");
      Serial.print(result);
      Serial.print(" vs ");
      Serial.print(y_test[i]);
      Serial.println(" actual");
      Serial.println("Evaluation time: " + String(end) + " microseconds");

      // DTO to be created
      //{"board": "esp8266", "model": "wine", "result": 1, "iteration": 1, "microseconds": 120}

      String resultString = "{'wine':" + String(result) + "}";

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