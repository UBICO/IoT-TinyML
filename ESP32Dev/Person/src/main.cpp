#include "OV7670.h"
#include <WiFi.h>
#include "EloquentTinyML.h"
#include "eloquent_tinyml/tensorflow/person_detection.h"

const uint8_t SIOD = 21;
const uint8_t SIOC = 22;

const uint8_t VSYNC = 34;
const uint8_t HREF = 35;

const uint8_t XCLK = 32;
const uint8_t PCLK = 33;

const uint8_t D0 = 27;
const uint8_t D1 = 17;
const uint8_t D2 = 16;
const uint8_t D3 = 15;
const uint8_t D4 = 14;
const uint8_t D5 = 13;
const uint8_t D6 = 12;
const uint8_t D7 = 4;

OV7670 *camera;

const uint8_t imageWidth = 160;
const uint8_t imageHeight = 120;

uint8_t blk_count = 0;

Eloquent::TinyML::TensorFlow::PersonDetection<imageWidth, imageHeight> personDetector;

// FreeRTOS timers

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

// WiFi SSID and password

#define WIFI_SSID "Berto's iPhone"
#define WIFI_PASSWORD "fogfogfog"

// MQTT Broker configuration and port

#define MQTT_HOST IPAddress(172, 20, 10, 5)

#define MQTT_PORT 1883

// // Global variables for MQTT and timer for handling the reconnections

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// // Variable to keep track of the iteration number

int currentIteration = 0;

// // Method to connect to WiFi

void connectToWifi()
{
    WiFi.disconnect();
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// // Method to connect to Mqtt

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    mqttClient.setCredentials("federico", "iotexamdemo");
    mqttClient.connect();
}

// // Callback for a WiFi event

void WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
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

// // Callback for mqtt connection

void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);
}

// // Callback for mqtt disconnection

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.println("Disconnected from MQTT.");

    if (WiFi.isConnected())
    {
        xTimerStart(mqttReconnectTimer, 0);
    }
}

// Setup method

void setup()
{
    Serial.begin(9600);
    Serial.println();

    // TensorFlow initialization

    camera = new OV7670(OV7670::Mode::QQVGA_RGB565, SIOD, SIOC, VSYNC, HREF, XCLK, PCLK, D0, D1, D2, D3, D4, D5, D6, D7);

    personDetector.setDetectionAbsoluteThreshold(100);
    personDetector.begin();

    // abort if an error occurred on the detector
    while (!personDetector.isOk())
    {
        Serial.print("Detector init error: ");
        Serial.println(personDetector.getErrorMessage());
    }

    //mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    //wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

    WiFi.onEvent(WiFiEvent);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    //connectToWifi();
}

// Loop method

void loop()
{
    // if (WiFi.isConnected() && mqttClient.connected())
    // {
            currentIteration += 1; // To keep track of iterations between loops

            blk_count = camera->yres / I2SCamera::blockSlice;
            for (int i = 0; i < blk_count; i++)
            {

                if (i == 0)
                {
                    camera->startBlock = 1;
                    camera->endBlock = I2SCamera::blockSlice;
                }

                if (i == blk_count - 1)
                {
                }

                camera->oneFrame();
                camera->startBlock += I2SCamera::blockSlice;
                camera->endBlock += I2SCamera::blockSlice;
            }

            bool isPersonInFrame = personDetector.detectPerson(camera->frame);

            if (!personDetector.isOk())
            {
                Serial.print("Person detector detection error: ");
                Serial.println(personDetector.getErrorMessage());
                delay(1000);
                return;
            }

            Serial.println(isPersonInFrame ? "Person detected" : "No person detected");
            Serial.print("\t > It took ");
            Serial.print(personDetector.getElapsedTime());
            Serial.println("ms to detect");
            Serial.print("\t > Person score: ");
            Serial.println(personDetector.getPersonScore());
            Serial.print("\t > Not person score: ");
            Serial.println(personDetector.getNotPersonScore());

            Serial.println("Evaluation time: " + String(personDetector.getElapsedTime()) + " milliseconds");

            //{"board": "esp32dev", "model": "person", "result": 1, "iteration": 1, "microseconds": 120}

            String startPar = "{";
            String board = "\"board\":\"esp32dev\",";
            String model = "\"model\":\"person\",";
            String result = "\"result\":" + String(personDetector.getPersonScore()) + ",";
            String iteration = "\"iteration\":" + String(int(currentIteration)) + ",";
            String time = "\"microseconds\":" + String(int(personDetector.getElapsedTime()) * 1000);
            String endPar = "}";

            String resultString = startPar + board + model + result + iteration + time + endPar;

            Serial.print(resultString);

            // uint16_t packetId = mqttClient.publish("iotdemo.esp32dev", 1, true, (char *)resultString.c_str());
            // Serial.print("Message sent with packetId: ");
            // Serial.println(packetId);
            //delay(100);
   // }
    // else
    // {
    //     Serial.println("WiFi or MQTT not connected, waiting before running evaluation.");
    // }

    delay(5000);
}