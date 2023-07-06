Project for IoT

How to run:

On COOJA
- setup 
	- start MQTT broker: mosquitto -v
	- Load cooja simulation and start border router: tools -> serial socker server -> contiki1 -> start
 	- Connect border router: make TARGET=cooja connect-router-cooja
 	- Start Cloud App and Control App (python3)

- for grafana: sudo snap start grafana 
