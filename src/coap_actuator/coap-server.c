/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) CoAP Engine example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "dev/button-hal.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "sys/node-id.h"

/** TEMP MONITORING APP **/
#include "coap-server.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP
#define ALIVE_LED_INTERVAL     (1 * CLOCK_SECOND)

/** FOR COOJA SIM **/
//#define APP_COOJA_TEST

/** TEMP MONITORING APP **/
/*
 * Resources to be activated need to be imported through the extern keyword.
 * The build system automatically compiles the resources in the corresponding sub-directory.
 */
extern coap_resource_t
  res_actuator,
  res_hello;


static struct etimer process_timer;
  
PROCESS(actuator_server, "Actuator COAP Server");
AUTOSTART_PROCESSES(&actuator_server);

PROCESS_THREAD(actuator_server, ev, data)
{
  PROCESS_BEGIN();

  // PROCESS_PAUSE();

  LOG_INFO("Starting Actuator COAP Server\n");

  coap_activate_resource(&res_actuator, "actuator/control");
  coap_activate_resource(&res_hello, "actuator/test/hello");

  LOG_INFO("actuator_status = ACTUATOR_OFF\n");
  actuator_status = ACTUATOR_OFF; // start the system with the actuator OFF
#ifdef APP_COOJA_TEST
  app_section_id = (node_id % 2)+1;
#else
  app_section_id = 1;
#endif

  // Print the mote number
  printf("Actuator Section ID: %d\n", app_section_id);
  etimer_set(&process_timer, ALIVE_LED_INTERVAL); 
   
  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == button_hal_release_event) {
      LOG_DBG("*********\nBUTTON TRIGGER\n");
      if (actuator_status == ACTUATOR_OFF){
        actuator_status = ACTUATOR_ON;
        leds_single_on(LEDS_RED);
        LOG_DBG("COOLING SYSTEM ACTIVATION (STATUS: ON)\n");  
      }
      else if (actuator_status == ACTUATOR_ON){
        actuator_status = ACTUATOR_OFF;
        leds_single_off(LEDS_RED);
        LOG_DBG("COOLING SYSTEM DEACTIVATION (STATUS: OFF)\n");  
      }
      
    }
    else{
        leds_single_toggle(LEDS_GREEN);
        etimer_set(&process_timer, ALIVE_LED_INTERVAL); 
    }
  }
  PROCESS_END();
}
