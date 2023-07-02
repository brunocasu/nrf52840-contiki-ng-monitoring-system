import paho.mqtt.client as mqtt
import json
import requests
import time

# MQTT broker settings
broker_address = "127.0.0.1"
broker_port = 1883

app_mqtt_client = mqtt.Client()

def on_connect(app_mqtt_client, userdata, flags, rc):
    print("Connected to MQTT broker with result code: " + str(rc))
    connection_status = 1
    # Subscribe to sensor data topic (receives the data in json format)
    app_mqtt_client.subscribe("sensor/data")
    
def on_disconnect(app_mqtt_client, userdata, rc):
    print("Disconnected from MQTT broker with result code: " + str(rc))
    mqtt_reconnect()

def on_message(app_mqtt_client, userdata, msg):
    print("Received message on topic: " + str(msg.topic) + "\n"+ str(msg.payload.decode()))


def mqtt_reconnect():
    print("Attempt to reconnect to broker... ")
    while not app_mqtt_client.is_connected():
        try:
            # Attempt to reconnect - timeout 3 seconds
            app_mqtt_client.reconnect()
            time.sleep(3)  # Wait for a few seconds before attempting again
        except OSError as e:
            print("Reconnection failed. Error: ", e)
            continue
        

def cloud_app():

    # Assign event callbacks
    app_mqtt_client.on_connect = on_connect
    app_mqtt_client.on_message = on_message
    app_mqtt_client.on_disconnect = on_disconnect

    # Connect to the broker
    app_mqtt_client.connect(broker_address, broker_port, 60)

    # Start the MQTT client loop - NON blocking
    app_mqtt_client.loop_start()
    
    while True:
        time.sleep(1)
    
    

cloud_app()
