import paho.mqtt.client as mqtt
import json
import pandas as pd

esp32dev = {}
esp32wemos = {}
esp8266 = {}

messagesCounter = 0

def on_connect(client, userdata, flags, rc):
  print("Connected with result code "+str(rc))
  client.subscribe("iotdemo/esp32dev")
  client.subscribe("iotdemo/esp32wemos")
  client.subscribe("iotdemo/esp8266")

def on_message(client, userdata, msg):

  global messagesCounter

  if (messagesCounter % 10 == 0):
    print("Processed " + str(messagesCounter) + " elements")

  messagesCounter = messagesCounter + 1

  received = json.loads(msg.payload.decode())
  modelName = received['model']
  if (received['board'] == 'esp8266'):
    if (modelName in esp8266):
      esp8266[modelName].append(received)
      if (len(esp8266[modelName]) == 50):
        filename = "esp8266" + "_" + modelName + ".csv"
        df = pd.DataFrame(esp8266[modelName])
        df.to_csv(filename, index=False)
        esp8266[modelName].clear()
        print("Stored " + filename)
    else:
      esp8266[modelName] = []
      esp8266[modelName].append(received)
  elif (received['board'] == 'esp32dev'):
    if (modelName in esp32dev):
      esp32dev[modelName].append(received)
      if (len(esp32dev[modelName]) == 50):
        filename = "esp32dev" + "_" + modelName + ".csv"
        df = pd.DataFrame(esp32dev[modelName])
        df.to_csv(filename, index=False)
        esp32dev[modelName].clear()
        print("Stored " + filename)
    else:
      esp32dev[modelName] = []
      esp32dev[modelName].append(received)
  else:
    if (modelName in esp32wemos):
      esp32wemos[modelName].append(received)
      if (len(esp32wemos[modelName]) == 50):
        filename = "esp32wemos" + "_" + modelName + ".csv"
        df = pd.DataFrame(esp32wemos[modelName])
        df.to_csv(filename, index=False)
        esp32wemos[modelName].clear()
        print("Stored " + filename)
    else:
      esp32wemos[modelName] = []
      esp32wemos[modelName].append(received)
    
client = mqtt.Client(client_id="mqttReader")
client.username_pw_set("federico", "iotexamdemo")
client.connect("localhost",1883,60)

client.on_connect = on_connect
client.on_message = on_message

client.loop_forever()