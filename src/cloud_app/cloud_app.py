import paho.mqtt.client as mqtt


# MQTT broker settings
broker_address = "127.0.0.1"
broker_port = 1883

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker with result code: " + str(rc))
    # Subscribe to topics here
    client.subscribe("status")


def on_message(client, userdata, msg):
    print("Received message: " + str(msg.payload.decode()))


def on_disconnect(client, userdata, rc):
    print("Disconnected from MQTT broker with result code: " + str(rc))

# Create an MQTT client instance
client = mqtt.Client()

# Assign event callbacks
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect

# Connect to the broker
client.connect(broker_address, broker_port)

# Start the MQTT client loop
client.loop_forever()
