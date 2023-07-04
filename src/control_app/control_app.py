import json
import requests
import time
import mysql.connector
from coapthon.client.helperclient import HelperClient
from coapthon.utils import parse_uri

#MySQL settings
app_mysql_cnx = mysql.connector.connect(
        host='localhost',
        user='cloudapp',
        password='4wXlt7&EY1g^',
        database='sensor_db'
    )
mysql_cursor = app_mysql_cnx.cursor()
# the table create in the sensor_db is "sensor_data"
sensor_data_read_query = '''
    SELECT sensorid, section, datatype, data
    FROM sensor_data
    ORDER BY timestamp DESC
    LIMIT 10
'''

coap_server_address = 'coap://[fd00::302:304:506:708]:5683/test/hello'  # Replace with the server's IPv6 address and port

# Create a HelperClient object
coap_host, coap_port, res_path = parse_uri(coap_server_address)
coap_client = HelperClient(server=(coap_host, coap_port))


## functions
def print_msg_info(sensor_id, section, data_type, sensor_data):
    print("Payload received: ")
    print("SensorID: ", sensor_id )
    print("SectionID: ", section )
    print("DataType: ", data_type )
    print("Data: ", sensor_data )
    

def read_sensor_data():
    print("READING Data in sensor_data MySQL table")
    mysql_cursor.execute(sensor_data_read_query)
    result = mysql_cursor.fetchall()
    # Process the result
    for row in result:
        sensor_id = row[0]
        section = row[1]
        data_type = row[2]
        sensor_data =  row[3]
        print("SensorID:", sensor_id)
        print("SectionID:", section)
        print("DataType:", data_type)
        print("Data:", sensor_data)

    
def publish_event():
    print("Publish Data in COAP server")
    response = coap_client.get(res_path)
    print('Response Code:', response.code)
    print('Response Payload:', response.payload)

def control_app():
    read_sensor_data()
    publish_event()
    # Wait for messages from the MQTT broker
    while True:
        time.sleep(1)
    
    

control_app()
