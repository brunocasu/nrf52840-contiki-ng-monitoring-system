#ifndef COAP_SERVER_H_
#define COAP_SERVER_H_

// #define ACTUATOR_SECTION_ID "A"

#define ACTUATOR_OFF    0
#define ACTUATOR_ON     1
#define ACTUATOR_FAULT  2

extern int app_section_id;
extern int actuator_status; // o for suspended, 1 for activated

#endif /* COAP_SERVER_H_ */
