import paho.mqtt.client as mqtt
import json
import pandas as pd

esp32dev = []
esp32wemos = []
esp8266 = []

def on_connect(client, userdata, flags, rc):
  print("Connected with result code "+str(rc))
  client.subscribe("iotdemo/esp32dev")
  client.subscribe("iotdemo/esp32wemos")
  client.subscribe("iotdemo/esp8266")

def on_message(client, userdata, msg):
  received = json.loads(msg.payload.decode())
  if (received['board'] == 'esp8266'):
    esp8266.append(received)
    if (int(received["iteration"]) % 50) == 0: #Every 50 iterations we replace the data
      filename = "esp8266" + "_" + received['model'] + ".csv"
      df = pd.DataFrame(esp8266)
      df.to_csv(filename, index=False)
      esp8266.clear()
      print("Stored " + filename)
  elif (received['board'] == 'esp32dev'):
    esp32dev.append(received)
    if (int(received["iteration"]) % 50) == 0: #Every 50 iterations we replace the data
      filename = "esp32dev" + "_" + received['model'] + ".csv"
      df = pd.DataFrame(esp32dev)
      df.to_csv(filename, index=False)
      esp32dev.clear()
      print("Stored " + filename)
  else:
    esp32wemos.append(received)
    if (int(received["iteration"]) % 50) == 0: #Every 50 iterations we replace the data
      filename = "esp32wemos" + "_" + received['model'] + ".csv"
      df = pd.DataFrame(esp32wemos)
      df.to_csv(filename, index=False)
      esp32wemos.clear()
      print("Stored " + filename)
    
client = mqtt.Client()
client.username_pw_set("federico", "iotexamdemo")
client.connect("localhost",1883,60)

client.on_connect = on_connect
client.on_message = on_message

client.loop_forever()