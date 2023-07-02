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

/* Edited by Bruno Casu - 2023 */
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
#include "random.h"
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
#define PUBLISH_DATA_PERIOD         ((5 * CLOCK_SECOND) >> 1)
#define RECONNECT_PERIOD            ((5 * CLOCK_SECOND) >> 1)
#define MAX_RECONNECT_ATTEMPTS      10
#define PUBLISH_DATA_SIZE_ERROR     0

#define SIMULATION_TRHESHOLD_COUNT  20

static const char *broker_ip = APP_MQTT_BROKER_ADDR;
static struct etimer process_timer;
static int n_reconnect_attempts = 0;
static int msg_seq_n = 0;
static int sensor_reading = 0;
// int mqtt_connect_status;

// static void mqtt_connect_error_handler(const int status);
static bool have_connectivity(void);
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data);
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
// static char sub_topic[BUFFER_SIZE];
// static int value = 0;
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

PROCESS_NAME(mqtt_client_process);
AUTOSTART_PROCESSES(&mqtt_client_process);

PROCESS(mqtt_client_process, "MQTT Client");

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{

  PROCESS_BEGIN();
  
  printf("TEMP MONITORING APP - SENSOR NODE (MQTT Client)\n");
  printf("PROCESS THREAD: mqtt_client_process Begin\n");

  // Initialize the ClientID as MAC address
  snprintf(client_id, BUFFER_SIZE, "%02x%02x%02x%02x%02x%02x",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  // Broker registration					 
  mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event, MAX_TCP_SEGMENT_SIZE);
				  
  state=STATE_INIT;
  leds_single_on(LEDS_RED);

  /* Main loop */
  while(1) {
      
    if((ev == PROCESS_EVENT_TIMER && data == &process_timer) ||   
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
                            (DEFAULT_PUBLSH_INTERVAL * 3) / CLOCK_SECOND,
                            MQTT_CLEAN_SESSION_ON);
                    etimer_set(&process_timer, RECONNECT_PERIOD); // used as a timeout
                    // STATE_CONNECING waits for MQTT EVEN to connect
                }
                else{
                    printf("PROCESS THREAD: have_connectivity FAILED \n");
                    // STATE_INIT is maitained
                    etimer_set(&process_timer, RECONNECT_PERIOD);
                }
                break;
                
            case STATE_CONNECTING:
                n_reconnect_attempts++;
                // STATE_CONNECING is maitained
                if (n_reconnect_attempts < MAX_RECONNECT_ATTEMPTS){
                    printf("PROCESS THREAD: (%d) Retry broker connection...\n", n_reconnect_attempts);
                    mqtt_connect(&conn, broker_address, DEFAULT_BROKER_PORT,
                            DEFAULT_PUBLSH_INTERVAL / CLOCK_SECOND,
                            MQTT_CLEAN_SESSION_ON);
                    etimer_set(&process_timer, RECONNECT_PERIOD); // set timer for reconnection attempt
                }
                else {
                    n_reconnect_attempts = 0;
                    printf("PROCESS THREAD: Failed to connect to broker... \n");
                    leds_single_on(LEDS_RED);
                }
                break;
            
            case STATE_CONNECTED:
                n_reconnect_attempts = 0;                
                sprintf(pub_topic, "%s", SENSOR_DATA_TOPIC);
                // simluate the temperature data
                if (msg_seq_n > SIMULATION_TRHESHOLD_COUNT && msg_seq_n < SIMULATION_TRHESHOLD_COUNT+10){
                    sensor_reading = 25 + (random_rand() % (10)); // Temperatures between 25~34
                }
                else{
                    sensor_reading = 15 + (random_rand() % (8)); // Temperatures between 15~22
                }
                sprintf(app_buffer, 
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
                    "\"Uptime (sec)\":%lu", sensor_reading, msg_seq_n, clock_seconds());
                // sprintf(app_buffer, "report %d", msg_seq_n);
                msg_seq_n++;
				printf("PROCESS THREAD: Publish message (%d) Sensor reading: %d \n", msg_seq_n, sensor_reading);
                mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer, strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
                etimer_set(&process_timer, PUBLISH_DATA_PERIOD);
                // STATE_CONNECTED is maintained until MQTT event detects a disconnection
                break;
                

            case STATE_DISCONNECTED:
                state = STATE_CONNECTING;
                etimer_set(&process_timer, RECONNECT_PERIOD);
                // STATE_CONNECING waits for MQTT EVEN to connect
                break;
                
            default:
                printf("PROCESS THREAD: Unknown state\n");
                PROCESS_EXIT();
                break;
        }
    }
    PROCESS_YIELD();
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
    case MQTT_EVENT_CONNECTED: {
        printf("MQTT EVENT: Connected to MQTT broker\n");
        leds_single_off(LEDS_RED);
        state = STATE_CONNECTED;
        // process_poll(&mqtt_client_process);
        break;
    }
    case MQTT_EVENT_DISCONNECTED: {
        printf("MQTT EVENT: MQTT broker disconnected. Reason %u\n", *((mqtt_event_t *)data));
        leds_single_on(LEDS_RED);
        state = STATE_DISCONNECTED;
        process_poll(&mqtt_client_process);
        break;
    }
    case MQTT_EVENT_PUBLISH: {
        msg_ptr = data;
        printf("MQTT EVENT: Received publish\n");
        // APP: the sensor mqtt client node will not be subscribed to any topic
        // pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk, msg_ptr->payload_length);
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
