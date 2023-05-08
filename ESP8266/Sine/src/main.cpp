#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EloquentTinyML.h>
#include "sine_model_quantized.h"

#define N_INPUTS 1
#define N_OUTPUTS 1
#define TENSOR_ARENA_SIZE 2*1024

Eloquent::TinyML::TfLite<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> tf;

//WiFi SSID and password

#define WIFI_SSID "Berto's iPhone"
#define WIFI_PASSWORD "fogfogfog"

//MQTT Broker configuration and port

#define MQTT_HOST IPAddress(172, 20, 10, 5)
#define MQTT_PORT 1883

//Global variables for MQTT and timer for handling the reconnections

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

//Method to connect to WiFi

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

//Method to connect to Mqtt 

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials("federico", "iotdemo");
  mqttClient.connect();
}

//Callback for WiFi connection

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

//Callback for WiFi disconnection

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

//Callback for mqtt connection

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

//Callback for mqtt disconnection

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

//Callback for mqtt subscription

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

//Callback for mqtt unsubscription

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

//Callback for mqtt published message

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

//Setup method

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println();

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  //TensorFlow initialization

  tf.begin(sine_model_quantized_tflite);
    
  // check if model loaded fine
  if (!tf.initialized()) {
      Serial.print("Error initializing TensorFlow");        
      while (true) delay(1000);
  }

}

//Loop method

void loop() {
  if (WiFi.isConnected() && mqttClient.connected()) {

  //Runs 10 iterations and for each one sends the result

  for (float i = 0; i < 10; i++) {
        // pick x from 0 to PI
        float x = 3.14 * i / 10;
        float y = sin(x);
        float input[1] = { x };
        float predicted = tf.predict(input);
        
        Serial.print("sin(");
        Serial.print(x);
        Serial.print(") = ");
        Serial.print(y);
        Serial.print("\t predicted: ");
        Serial.println(predicted);

        //TODO: add timestamp and other useful info

        String resultString = "{'sin':" + String(x) + "}";
      
        uint16_t packetId = mqttClient.publish("iotdemo.esp8266", 1, true, (char*) resultString.c_str());
        Serial.print("Message sent with packetId: ");
        Serial.println(packetId);
    }

  } else {
    Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
  }

  delay(5000);
}