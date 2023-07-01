/*
 * Copyright (c) 2020, Carlo Vallati, University of Pisa
  * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "sys/mutex.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "mqtt-client.h"

#include <string.h>
#include <strings.h>

#include "sensor_config.h"
/*---------------------------------------------------------------------------*/
#define LOG_MODULE "mqtt-client"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_DBG
#endif

/** TEMP MONITORING APP **/
#define APP_MQTT_BROKER_ADDR        "fd00::1"
#define DEFAULT_BROKER_PORT         1883
#define DEFAULT_PUBLSH_INTERVAL     (30 * CLOCK_SECOND)
#define SENSOR_DATA_TOPIC           "sensor/data" // used for publishing sensor data
#define SENSOR_CONFIG_TOPIC         "sensor/cfg" // used to subscirbe - only for the demo

#define STATE_MACHINE_PERIODIC      (CLOCK_SECOND >> 1)
#define PUBLISH_DATA_PERIOD         ((20 * CLOCK_SECOND) >> 1)
#define RECONNECT_PERIOD            ((5 * CLOCK_SECOND) >> 1)
#define MAX_RECONNECT_ATTEMPTS      10
#define PUBLISH_DATA_SIZE_ERROR     0

static const char *broker_ip = APP_MQTT_BROKER_ADDR;
static struct etimer pub_data_timer;
static struct etimer reconnect_timer;
int n_reconnect_attempts = 0;
int msg_seq_n = 0;
int mqtt_connect_status;

static int build_publish(void);
static void mqtt_connect_error_handler(const int status);
static bool have_connectivity(void);
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data);
static void pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk, uint16_t chunk_len);
/*---------------------------------------------------------------------------*/
// We assume that the broker does not require authentication
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;

#define STATE_INIT    		  0
#define STATE_NET_OK    	  1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_SUBSCRIBED      4
#define STATE_DISCONNECTED    5

/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topics.
 * Make sure they are large enough to hold the entire respective string
 */
#define BUFFER_SIZE 64

static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
static int value = 0;
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct mqtt_connection conn;
mqtt_status_t status;
char broker_address[CONFIG_IP_ADDR_STR_LEN];

PROCESS(mqtt_connection_process, "MQTT Connection Handling");
PROCESS(mqtt_pub_sensor_data_process, "MQTT Publish");
AUTOSTART_PROCESSES(&mqtt_connection_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_connection_process, ev, data)
{

  PROCESS_BEGIN();
  
  // Start the child process
  process_start(&mqtt_pub_sensor_data_process, NULL);
  
  printf("TEMP MONITORING APP - SENSOR NODE (MQTT Client)\n");
  printf("PROCESS THREAD: mqtt_connection_process Begin\n");

  // Initialize the ClientID as MAC address
  snprintf(client_id, BUFFER_SIZE, "%02x%02x%02x%02x%02x%02x",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  // Broker registration					 
  mqtt_register(&conn, &mqtt_connection_process, client_id, mqtt_event, MAX_TCP_SEGMENT_SIZE);
				  
  state=STATE_INIT;
				    
  // Initialize periodic timer to check the status 
  etimer_set(&reconnect_timer, RECONNECT_PERIOD);

  /* Main loop */
  while(1) {
    PT_YIELD(mqtt_connection_process)  
    //PROCESS_YIELD();
    
    if((ev == PROCESS_EVENT_TIMER && data == &reconnect_timer) ||
        ev == PROCESS_EVENT_POLL || 
        state==STATE_INIT){
        
        switch (state){
            case STATE_INIT:
                // Test network connectivity
                if(have_connectivity()==true){
                    printf("PROCESS THREAD: have_connectivity OK \n");
                    // Connect to MQTT server
                    state = STATE_CONNECTING;
                    printf("PROCESS THREAD: Start connection with Broker\n");
                    memcpy(broker_address, broker_ip, strlen(broker_ip));
                    mqtt_connect(&conn, broker_address, DEFAULT_BROKER_PORT,
                            DEFAULT_PUBLSH_INTERVAL / CLOCK_SECOND,
                            MQTT_CLEAN_SESSION_ON);
                    etimer_set(&reconnect_timer, RECONNECT_PERIOD); // used as a timeout
                    // STATE_CONNECING waits for MQTT EVEN to connect
                }
                else{
                    printf("PROCESS THREAD: have_connectivity FAILED \n");
                    // STATE_INIT is maitained
                    etimer_set(&reconnect_timer, RECONNECT_PERIOD);
                }
            case STATE_CONNECTING:
                n_reconnect_attempts++;
                printf("PROCESS THREAD: (%d)Connecting to broker... \n", n_reconnect_attempts);
                
                // STATE_CONNECING is maitained
                if (n_reconnect_attempts < MAX_RECONNECT_ATTEMPTS){
                    mqtt_connect(&conn, broker_address, DEFAULT_BROKER_PORT,
                            DEFAULT_PUBLSH_INTERVAL / CLOCK_SECOND,
                            MQTT_CLEAN_SESSION_ON);
                    etimer_set(&reconnect_timer, RECONNECT_PERIOD); // set timer for reconnection attempt
                }
                else {
                    n_reconnect_attempts = 0;
                    printf("PROCESS THREAD: Failed to connect to broker... \n");
                }
            
            case STATE_CONNECTED:
                n_reconnect_attempts = 0;
                etimer_stop(&reconnect_timer);
                // Subscribe to a topic
                printf("PROCESS THREAD: Testing subscribe to %s\n", SENSOR_CONFIG_TOPIC);
                strcpy(sub_topic,SENSOR_CONFIG_TOPIC);
                mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);
                // if (status == MQTT_SUBSCRIBE_SUCCESS){
                //     printf("PROCESS THREAD: Subscribe Success %s\n", SENSOR_CONFIG_TOPIC);
                // }
                // else{
                //     printf("PROCESS THREAD: Subscribe Failed %s\n", SENSOR_CONFIG_TOPIC);
                // }
                // STATE_CONNECTED is maintained until MQTT event detects a disconnection

            case STATE_DISCONNECTED:
                // Connect to MQTT server
                state = STATE_CONNECTING;
                printf("PROCESS THREAD: RE-Start connection with Broker\n");
                memcpy(broker_address, broker_ip, strlen(broker_ip));
                mqtt_connect(&conn, broker_address, DEFAULT_BROKER_PORT,
                        DEFAULT_PUBLSH_INTERVAL / CLOCK_SECOND,
                        MQTT_CLEAN_SESSION_ON);
                etimer_set(&reconnect_timer, RECONNECT_PERIOD);
                // STATE_CONNECING waits for MQTT EVEN to connect
                
            default:
                printf("PROCESS THREAD: Unknown state\n");
                PROCESS_EXIT();
        }
    }

  }

  PROCESS_END();
}

PROCESS_THREAD(mqtt_pub_sensor_data_process, ev, data)
{
    while(1){
        
        //PROCESS_YIELD();
       
        if((ev == PROCESS_EVENT_TIMER && data == &pub_data_timer) // When the publish timer expires
            || ev == PROCESS_EVENT_POLL){ // When connection is established the mqtt even polls this thread
            if (state == STATE_CONNECTED){    
                // Publish sensor data
                sprintf(pub_topic, "%s", SENSOR_DATA_TOPIC);
                
                if (build_publish() != PUBLISH_DATA_SIZE_ERROR){
                    printf("PUBLISH SENSOR DATA: Payload\n %s\n", app_buffer);
                    mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
                }
            }        
        }
        etimer_set(&pub_data_timer, PUBLISH_DATA_PERIOD);
    }

}

static int build_publish(void)
{
  int len;
  int remaining = APP_BUFFER_SIZE;
  int i;
  int sensor_reading = 123;
  char def_rt_str[64];

  msg_seq_n++;

  len = snprintf(app_buffer, remaining,
                 "{"
                 "\"d\":{"
                 "\"SectionID\":\""APP_SECTION_ID"\","
                 "\"SensorID\":\""APP_SENSOR_ID"\","
                 "\"DataType\":\""APP_DATA_TYPE"\","
                 "\"Data\":%d,"
                 "\"Platform\":\""CONTIKI_TARGET_STRING"\","
#ifdef CONTIKI_BOARD_STRING
                 "\"Board\":\""CONTIKI_BOARD_STRING"\","
#endif
                 "\"MsgNumber\":%d,"
                 "\"Uptime (sec)\":%lu",
                 sensor_reading, msg_seq_n, clock_seconds());

  if(len < 0 || len >= remaining) {
    LOG_ERR("PUBLISH ERROR: Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return PUBLISH_DATA_SIZE_ERROR;
  }
  return 1;
}

static void mqtt_connect_error_handler(const int status){
    printf("MQTT CONNECT ERROR: Error number: %d", status);
    return;
}

/*---------------------------------------------------------------------------*/
/** TEMP MONITORING APP - The sensor node subscribes to the "sensor/cfg" topic - ONLY FOR DEMO PURPOSES **/ 
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk, uint16_t chunk_len)
{
  printf("PUB HANDLER: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len, chunk_len);
  return;
}
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
    case MQTT_EVENT_CONNECTED: {
        printf("MQTT EVENT: Connected to MQTT broker\n");
        state = STATE_CONNECTED;
        process_poll(&mqtt_connection_process);
        process_poll(&mqtt_pub_sensor_data_process);
        break;
    }
    case MQTT_EVENT_DISCONNECTED: {
        printf("MQTT EVENT: MQTT broker disconnected. Reason %u\n", *((mqtt_event_t *)data));
        state = STATE_DISCONNECTED;
        process_poll(&mqtt_connection_process);
        break;
    }
    case MQTT_EVENT_PUBLISH: {
        msg_ptr = data;
        printf("MQTT EVENT: Received publish\n");
        pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk, msg_ptr->payload_length);
        break;
    }
    case MQTT_EVENT_SUBACK: {
#if MQTT_311
        mqtt_suback_event_t *suback_event = (mqtt_suback_event_t *)data;
        if(suback_event->success) {
            printf("MQTT EVENT: Application is subscribed to topic successfully\n");
        }
        else {
            printf("MQTT EVENT: Application failed to subscribe to topic (ret code %x)\n", suback_event->return_code);
        }
#else
        printf("MQTT EVENT: Application is subscribed to topic successfully\n");
#endif
        break;
    }
    case MQTT_EVENT_UNSUBACK: {
        printf("MQTT EVENT: Application is unsubscribed to topic successfully\n");
        break;
    }
    case MQTT_EVENT_PUBACK: {
        printf("MQTT EVENT: Publishing complete.\n");
        break;
    }
    default:
        printf("MQTT EVENT: Application got a unhandled MQTT event: %i\n", event);
        break;
  }
}

static bool
have_connectivity(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
     uip_ds6_defrt_choose() == NULL) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
