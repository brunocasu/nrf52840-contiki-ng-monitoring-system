# mqtt-client

MQTT Client - Application Sensor Node
The client sends periodically Temperature readings to the MQTT broker

## building the application 
Clean previous builds: 
```
$ make distclean
```

Build native:
```
make TARGET=native && sudo ./mqtt-client.native
```

Building in the dongle:
```
make TARGET=nrf52840 hello-world.upload
make TARGET=nrf52840 BOARD=dongle ./mqtt-client.dfu-upload PORT=/dev/ttyACM0
```

## device info
Look for the node's global IPv6, e.g.:
```
[INFO: Native    ] Added global IPv6 address fd00::302:304:506:708
```

And ping it (over the tun interface):
```
$ ping6 fd00::302:304:506:708
```
