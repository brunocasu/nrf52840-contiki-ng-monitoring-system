import paho.mqtt.client as mqtt
import json
import requests
import time
import mysql.connector

# MQTT broker settings
broker_address = "127.0.0.1"
broker_port = 1883
app_mqtt_client = mqtt.Client()

#MySQL settings
app_mysql_cnx = mysql.connector.connect(
        host='localhost',
        user='cloudapp',
        password='4wXlt7&EY1g^',
        database='sensor_db'
    )
mysql_cursor = app_mysql_cnx.cursor()
# the table create in the sensor_db is "sensor_data"
sensor_data_insert_query = '''
    INSERT INTO sensor_data (sensorid, section, datatype, data) VALUES (%s, %s, %s, %s)
'''
mysql_seq_n = 0


## functions
def on_connect(app_mqtt_client, userdata, flags, rc):
    print("Connected to MQTT broker with result code: " + str(rc))
    connection_status = 1
    # Subscribe to sensor data topic (receives the data in json format)
    app_mqtt_client.subscribe("sensor/data")
   
   
def on_disconnect(app_mqtt_client, userdata, rc):
    print("Disconnected from MQTT broker with result code: " + str(rc))
    mqtt_reconnect()


def on_message(app_mqtt_client, userdata, msg):
    print("\n*************\nReceived message on topic: " + str(msg.topic))#  + "\n"+ str(msg.payload.decode()))
    msg_string = msg.payload.decode()
    msg_json = json.loads(msg_string)
    sensor_id = msg_json["SensorID"]
    section = msg_json["SectionID"]
    data_type = msg_json["DataType"]
    sensor_data = msg_json["Data"]
    msg_number = msg_json["MsgNumber"]
    print_msg_info(sensor_id, section, data_type, sensor_data, msg_number)
    # write the data in the mysql table
    write_sensor_data(sensor_id, section, data_type, sensor_data)
    
    

def mqtt_reconnect():
    print("Attempt to reconnect to broker... ")
    while not app_mqtt_client.is_connected():
        try:
            # Attempt to reconnect - timeout 3 seconds before attempting again
            app_mqtt_client.reconnect()
            time.sleep(3)
        except OSError as e:
            print("Reconnection failed. Error: ", e)
            continue
        
        
def print_msg_info(sensor_id, section, data_type, sensor_data, msg_number):
    print("Payload received: ")
    print("SensorID: ", sensor_id )
    print("SectionID: ", section )
    print("MessageSeqNum: ", msg_number )
    print("DataType: ", data_type )
    print("Data: ", sensor_data )
    

def write_sensor_data(sensor_id, section, data_type, sensor_data):
    db_write_values = (sensor_id, section, data_type, sensor_data)
    mysql_cursor.execute(sensor_data_insert_query, db_write_values)
    # Commit the changes to the database
    app_mysql_cnx.commit()
    print("WRITING Data in sensor_data MySQL table")
    

def cloud_app():

    # Assign event callbacks
    app_mqtt_client.on_connect = on_connect
    app_mqtt_client.on_message = on_message
    app_mqtt_client.on_disconnect = on_disconnect
    conn = 0
    # Connect to the broker
    while conn == 0:
        try:
            app_mqtt_client.connect(broker_address, broker_port, 60)
            conn = 1
        except:
            print("Failed to connect to broker...")
        time.sleep(2)

    # Start the MQTT client loop - NON blocking
    app_mqtt_client.loop_start()
    
    # Wait for messages from the MQTT broker
    while True:
        time.sleep(1)
    
    

cloud_app()
