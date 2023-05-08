#include <WiFi.h>
#include <EloquentTinyML.h>
#include "sine_model_quantized.h"

#define N_INPUTS 1
#define N_OUTPUTS 1
#define TENSOR_ARENA_SIZE 2*1024

Eloquent::TinyML::TfLite<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> tf;

//FreeRTOS timers for handling reconnections

extern "C" {
	#include "freertos/FreeRTOS.h"
	#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

//WiFi SSID and password

#define WIFI_SSID "Berto's iPhone"
#define WIFI_PASSWORD "fogfogfog"

//MQTT Broker configuration and port

#define MQTT_HOST IPAddress(172, 20, 10, 5)
#define MQTT_PORT 1883

//Global variables for MQTT and timer for handling the reconnections

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

//Method to connect to WiFi

void connectToWifi() {
  WiFi.disconnect();
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

//Method to connect to Mqtt 

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials("federico", "iotdemo");
  mqttClient.connect();
}

//Callback for a WiFi event - ESP32 Only

void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        isWiFiConnected = true;
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        isWiFiConnected = false;
        break;
    }
}

//Callback for mqtt connection

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

//Callback for mqtt disconnection

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
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

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

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

        uint16_t packetId = mqttClient.publish("iotdemo.esp32dev", 1, true, (char*) resultString.c_str());
        Serial.print("Message sent with packetId: ");
        Serial.println(packetId);
    }

  } else {
    Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
  }

  delay(5000);
}