#include <WiFi.h>
#include <EloquentTinyML.h>
#include "sine_model.h"
#include "esp_timer.h"

#define N_INPUTS 1
#define N_OUTPUTS 1
#define TENSOR_ARENA_SIZE 2*1024

Eloquent::TinyML::TfLite<N_INPUTS, N_OUTPUTS, TENSOR_ARENA_SIZE> tf;

//FreeRTOS timers 

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

//Variable to keep track of the iteration number

int currentIteration = 0;

//Method to connect to WiFi

void connectToWifi() {
  WiFi.disconnect();
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

//Method to connect to Mqtt 

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.setCredentials("federico", "iotexamdemo");
  mqttClient.connect();
}

//Callback for a WiFi event

void WiFiEvent(WiFiEvent_t event) {
    //Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
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
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
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
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  //TensorFlow initialization

  tf.begin(model_data);
    
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
  //Waits 5 seconds, then restarts

  for (float i = 0; i < 10; i++) {

        currentIteration += 1; //To keep track of iterations between loops

        // pick x from 0 to PI
        float x = 3.14 * i / 10;
        float y = sin(x);
        float input[1] = { x };

        int start = esp_timer_get_time(); //Evaluation start time
        float predicted = tf.predict(input);
        int end = esp_timer_get_time() - start; //Evaluation end time
        
        Serial.print("sin(");
        Serial.print(x);
        Serial.print(") = ");
        Serial.print(y);
        Serial.print("\t predicted: ");
        Serial.println(predicted);

        //{"board": "esp32wemos", "model": "sin", "result": 3.14, "iteration": 1, "microseconds": 120}

        String startPar = "{";
        String board = "\"board\":\"esp32wemos\",";
        String model = "\"model\":\"sin\",";
        String result = "\"result\":" + String(x) + ",";
        String iteration = "\"iteration\":" + String(uint16_t(i)) + ",";
        String time = "\"microseconds\":" + String(uint16_t(end));
        String endPar = "}";
       
        String resultString = startPar + board + model + result + iteration + time + endPar;
      
        uint16_t packetId = mqttClient.publish("iotdemo.esp32wemos", 1, true, (char*) resultString.c_str());
        Serial.print("Message sent with packetId: ");
        Serial.println(packetId);
        delay(100);
    }

  } else {
    Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
  }

  delay(5000);
}