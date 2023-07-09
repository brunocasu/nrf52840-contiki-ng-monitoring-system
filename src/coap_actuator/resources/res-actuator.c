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
 *      Example resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include <stdio.h>
#include "dev/leds.h"
/** TEMP MONITORING APP **/
#include "../coap-server.h"
#include "sys/log.h"

/** **/
#define REPLY_BUFF_MAX_LEN  64

int app_section_id;
int actuator_status;

static char reply_buff[REPLY_BUFF_MAX_LEN];
static int activation_code = 0; // 0 set the system to state OFF, 1 set the system to state ON

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
RESOURCE(res_actuator,
         "title=\"ActuatorCoolingSystem: ?activation=0..\";rt=\"Text\"",
         res_get_handler,
         res_post_handler,
         NULL,
         NULL);



static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  int length;
  
  if(actuator_status == ACTUATOR_OFF) {
    printf("GET msg: status COOLING SYSTEM STATUS(OFF)\n");
    sprintf(reply_buff, "Actuator SECTION(%d) COOLING SYSTEM STATUS(OFF)", app_section_id);
    length = strlen(reply_buff);
  }
  else if(actuator_status == ACTUATOR_ON) {
    printf("GET msg: status COOLING SYSTEM STATUS(ON)\n");
    sprintf(reply_buff, "Actuator SECTION(%d) COOLING SYSTEM STATUS(ON)", app_section_id);
    length = strlen(reply_buff);
  }
  else if(actuator_status == ACTUATOR_FAULT) {
    printf("GET msg: status COOLING SYSTEM STATUS(FAULT)an");
    sprintf(reply_buff, "Actuator SECTION(%d) COOLING SYSTEM STATUS(FAULT)", app_section_id);
    length = strlen(reply_buff);
  }
  else {
    printf("GET msg: ERROR status UNKNOWN\n");
    sprintf(reply_buff, "UNKNOWN STATE");
    length = 13;  
  }
    
  if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
  }
  memcpy(buffer, reply_buff, length);

  coap_set_header_content_format(response, TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  coap_set_header_etag(response, (uint8_t *)&length, 1);
  coap_set_payload(response, buffer, length);
}

static void
res_post_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const char *msg_activation = NULL;
  int length; 

  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if(coap_get_query_variable(request, "activation", &msg_activation)) {
    // get the activation code sent in the message 
    activation_code = atoi(msg_activation);
    if(activation_code == 0) { // activation code 0 turns off the actuator
      actuator_status = ACTUATOR_OFF;
      leds_single_off(LEDS_RED);
      printf("Actuator CMD Received: actuator_status = ACTUATOR_OFF\n");
      sprintf(reply_buff, "Actuator CMD Received COOLING SYSTEM STATUS(OFF)");
      length = strlen(reply_buff);
    }
    else if(activation_code == 1) { // activation code 1 turns on the actuator
      actuator_status = ACTUATOR_ON;
      leds_single_on(LEDS_RED);
      printf("Actuator CMD Received: actuator_status = ACTUATOR_ON\n");
      sprintf(reply_buff, "Actuator CMD Received COOLING SYSTEM STATUS(ON)");
      length = strlen(reply_buff);
    }
    else {
      printf("Actuator CMD Received: Unknown activation code\n");
      sprintf(reply_buff, "Actuator CMD Error: Unknown activation code");
      length = strlen(reply_buff);
    }
  }
  else{
    printf("Actuator CMD Received: No Activation code sent\n");
    sprintf(reply_buff, "Actuator CMD Received: No Activation code sent");
    length = strlen(reply_buff);
  }
    
    if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
    }
    memcpy(buffer, reply_buff, length);


  coap_set_header_content_format(response, TEXT_PLAIN); /* text/plain is the default, hence this option could be omitted. */
  coap_set_header_etag(response, (uint8_t *)&length, 1);
  coap_set_payload(response, buffer, length);
}
