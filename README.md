# Raspberry Pi Locator Service

Package includes two features. One designed to both locate your Pi and provide a list of known services.  The 
other acts as a central LCD display service, allowing other network devices/nodes to send a one-liner 
to the service for display on a 4x20 LCD.

## Executables

|Program|Type|Platform|Included|Description|
|---|---|---|---|---|
|udp_locator_service|Server|any|yes|Maintains a list of known network services accessable via UDP socket.|
|udp_locator_client|Client|any|yes|Collect services info from Service, which includes that service's ip address.|
|lcd_display_service|Server|any|yes|Accepts one-line messages over udp and display them on a 4x20 lcd panel.|
|lcd_display_client|Client|any|no yet|Sends the one-liner composed of various Pi metrics; like cpus, temps, etc.|


## Build:  Autotools project
Requires [WiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/download-and-install/) and assume the Pi is using [Raspbian](https://www.raspberrypi.org/downloads/).

    $ ./configure
    $ make
    $ sudo make install


