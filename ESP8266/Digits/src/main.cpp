#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EloquentTinyML.h>
#include "digits_model.h"
#include "time.h"

#define NUMBER_OF_INPUTS 64
#define NUMBER_OF_OUTPUTS 10
#define TENSOR_ARENA_SIZE 8 * 1024

Eloquent::TinyML::TfLite<NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, TENSOR_ARENA_SIZE> tf;

// Elements for evaluation

float x_test[64] = {0., 0., 0.625, 0.875, 0.5, 0.0625, 0., 0.,
                    0., 0.125, 1., 0.875, 0.375, 0.0625, 0., 0.,
                    0., 0., 0.9375, 0.9375, 0.5, 0.9375, 0., 0.,
                    0., 0., 0.3125, 1., 1., 0.625, 0., 0.,
                    0., 0., 0.75, 0.9375, 0.9375, 0.75, 0., 0.,
                    0., 0.25, 1., 0.375, 0.25, 1., 0.375, 0.,
                    0., 0.5, 1., 0.625, 0.5, 1., 0.5, 0.,
                    0., 0.0625, 0.5, 0.75, 0.875, 0.75, 0.0625, 0.};
float y_pred[10] = {0};
int y_test = 8;

// To have conditional branches for testing

#define USE_MQTT 0

// WiFi SSID and password

#define WIFI_SSID "Berto's iPhone"
#define WIFI_PASSWORD "iotexamdemo"

// MQTT Broker configuration and port

#define MQTT_HOST IPAddress(172, 20, 10, 5)
#define MQTT_PORT 1883

// Global variables for MQTT and timer for handling the reconnections

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

// NTP server to request epoch time
const char *ntpServer = "pool.ntp.org";

// Variable to save current epoch time
unsigned long epochTime;

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
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

// Function that gets current epoch time
unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Cannot get time from ntp server.");
    return (0);
  }
  time(&now);
  return now;
}

// Setup method

void setup()
{
  Serial.begin(9600);
  Serial.println();

  // TensorFlow initialization

  tf.begin(digits_model);

  // check if model loaded fine
  if (!tf.initialized())
  {
    Serial.print("Error initializing TensorFlow");
    while (true)
      delay(1000);
  }

  if (USE_MQTT)
  {
    configTime(0, 0, ntpServer);

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
}

// Tensorflow prediction

void tensorFlowLoop()
{

  // Runs a loop of 10 iterations, at every one it sends the result (if enabled)

  for (uint8_t i = 0; i < 10; i++)
  {

    currentIteration += 1; // To keep track of iterations between loops

    uint32_t start = micros(); // Evaluation start time
    uint8_t result = tf.predict(x_test, y_pred);
    uint32_t end = micros() - start; // Evaluation end time

    Serial.print("Test output is: ");
    Serial.println(y_test);
    Serial.print("Predicted probabilities are: ");

    for (int i = 0; i < 10; i++)
    {
      Serial.print(y_pred[i]);
      Serial.print(i == 9 ? '\n' : ',');
    }

    Serial.print("Predicted class is: ");
    Serial.println(tf.probaToClass(y_pred));
    Serial.print("Sanity check: ");
    Serial.println(tf.predictClass(x_test));
    Serial.println("Evaluation time: " + String(end) + " microseconds");

    if (USE_MQTT)
    {
      // Here i get the unix time for having a timestamp of the operation

      epochTime = getTime();

      // DTO to be created
      //{"board": "esp8266", "model": "digits", "result": 1, "iteration": 1, "microseconds": 120, "timestamp": "1683815110824"}

      String resultString = "{'digits':" + String(result) + "}";

      uint16_t packetId = mqttClient.publish("iotdemo.esp8266", 1, true, (char *)resultString.c_str());
      Serial.print("Message sent with packetId: ");
      Serial.println(packetId);
    }
  }
}

// Loop method

void loop()
{
  if (USE_MQTT)
  {

    // If we want to use MQTT, we need the WiFi connection and the connection to the broker before evaluating
    // in order to send the results

    if (WiFi.isConnected() && mqttClient.connected())
    {
      tensorFlowLoop();
    }
    else
    {
      Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
    }
  }
  else
  {

    // If we don't have mqtt enabled, we directly run the evaluations for testing

    tensorFlowLoop();
  }

  delay(5000);
}