import json
#import requests
import time
import mysql.connector
from coapthon.client.helperclient import HelperClient
from coapthon.messages.request import Request
from coapthon.utils import parse_uri
from coapthon.messages.message import Message
from coapthon import defines as d

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
data_check_interval = 5 # time in seconds between each data check from the DB
section1_id = 1
section2_id = 2
actuator1_status = 0 # start deactivated (1 for activated status, 0 for deactivated)
actuator2_status = 0
TEMP_THRESHOLD = 24
# edit server address based on actuators assigned IPv6
coap_server1_address = 'coap://[fd00::302:304:506:708]:5683/actuator/control'  
coap_server2_address = 'coap://[fd00::302:304:506:708]:5683/actuator/control'


## functions
def print_msg_info(sensor_id, section, data_type, sensor_data):
    print("Payload received: ")
    print("SensorID: ", sensor_id )
    print("SectionID: ", section )
    print("DataType: ", data_type )
    print("Data: ", sensor_data )
    

def read_sensor_data():
    global actuator1_status, actuator2_status
    MAX_RETRIES = 1
    retry_counter = 0
    cmd_sent_ok = 0
    print("READING Data in sensor_data MySQL table")
    mysql_cursor.execute(sensor_data_read_query)
    result = mysql_cursor.fetchall()
    section1_total = 0
    section1_cnt = 0
    section2_total = 0
    section2_cnt = 0
    # Process the result
    for row in result:
        sensor_id = row[0]
        section = row[1]
        data_type = row[2]
        sensor_data =  row[3]
        #print("SensorID:", sensor_id)
        #print("SectionID:", section)
        #print("DataType:", data_type)
        #print("Data:", sensor_data)
        if section == str(section1_id):
            section1_total = section1_total + int(sensor_data) ## Add all the temperature readings from Section 1
            section1_cnt = section1_cnt + 1
        if section == str(section2_id):
            section2_total = section2_total + int(sensor_data) ## Add all the temperature readings from Section 2
            section2_cnt = section2_cnt + 1
    ## check Section 1 average temperature
    if  section1_cnt  > 0:
        status = actuator1_status
        print("SECTION 1 Average Temp: "+str(section1_total/section1_cnt)+"°C Status:"+str(status))
        if (section1_total/section1_cnt) > TEMP_THRESHOLD: ## Threshold crossed
            print("TS Crossed!")
            if status == 0: ## Actuator is registered as OFF (Suspended)
                while (retry_counter <= MAX_RETRIES) and (cmd_sent_ok == 0):
                    if (post_event(section1_id, 1) == 1): # send POST message, Activate Cooling System!
                        actuator1_status = 1
                        cmd_sent_ok = 1
                    else:
                        time.sleep(1)
                        retry_counter = retry_counter + 1
        else: ## Temperature is OK
            if status == 1: ## Actuator is registered as ON (Activated)
                while (retry_counter <= MAX_RETRIES) and (cmd_sent_ok == 0):
                    if (post_event(section1_id, 0) == 1): # send POST message, Activate Cooling System!
                        actuator1_status = 0
                        cmd_sent_ok = 1
                    else:
                        time.sleep(1)
                        retry_counter = retry_counter + 1
    ## check Section 2 average temperature
    if  section2_cnt  > 0:
        print("SECTION 2 Average Temp: "+str(section2_total/section2_cnt)+"°C")
        status = actuator2_status
        if (section2_total/section2_cnt) > TEMP_THRESHOLD: ## Threshold crossed
            print("TS Crossed!")
            if status == 0: ## Actuator is registered as OFF (Suspended)
                while (retry_counter <= MAX_RETRIES) and (cmd_sent_ok == 0):
                    if (post_event(section2_id, 1) == 1): # send POST message, Activate Cooling System!
                        actuator2_status = 1
                        cmd_sent_ok = 1
                    else:
                        time.sleep(1)
                        retry_counter = retry_counter + 1
        else: ## Temperature is OK
            if status == 1: ## Actuator is registered as ON (Activated)
                while (retry_counter <= MAX_RETRIES) and (cmd_sent_ok == 0):
                    if (post_event(section2_id, 0) == 1): # send POST message, Activate Cooling System!
                        actuator2_status = 0
                        cmd_sent_ok = 1
                    else:
                        time.sleep(1)
                        retry_counter = retry_counter + 1

    
def post_event(section, action=0):
    if section == section1_id:
        coap_server_addr = coap_server1_address
    elif section == section2_id:
        coap_server_addr = coap_server2_address
        
    coap_host, coap_port, res_path = parse_uri(coap_server_addr)
    coap_client = HelperClient(server=(coap_host, coap_port))
    
    res_path = res_path+"?activation="+str(action)
    
    if action == 1:
        print("Section("+str(section)+") POST /actuator/control ACTIVATE COOLING SYSTEM")
    elif action == 0:
        print("Section("+str(section)+") POST /actuator/control SUSPEND COOLING SYSTEM")
    
    try:
        response = coap_client.post(res_path, '')
        print('Response Code:', response.code)
        print('Response Payload:', response.payload)
        return 1
    except:
        print("Failed to Send POST msg")
        return 0
        
    
def get_actuator_status(section):
    if section == section1_id:
        coap_server_addr = coap_server1_address
    elif section == section2_id:
        coap_server_addr = coap_server2_address
        
    coap_host, coap_port, res_path = parse_uri(coap_server_addr)
    coap_client = HelperClient(server=(coap_host, coap_port))
    print("GET /actuator/control")
    try:
        response = coap_client.get(res_path)
        print('Response Code:', response.code)
        print('Response Payload:', response.payload)
    except:
        print("Failed to Send GET msg")

def control_app():
    get_actuator_status(1)
    get_actuator_status(2)
    # Wait for messages from the MQTT broker
    while True:
        time.sleep(data_check_interval)
        read_sensor_data()
    
control_app()


