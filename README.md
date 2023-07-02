Project for IoT

How to run:

On COOJA
- setup 
	- start MQTT broker: mosquitto -v
	- start Cloud App and Control App (python3)	
	- add border router (BR) cooja mote (border-router.c)
	- on cooja: tools -> serial socker server -> contiki1 -> start
	- add other noder (MQTT sensor and COAP actuator)
 	- start BR: make TARGET=cooja connect-router-cooja
	- test: ping6 fd00::201:1:1:1

- 
