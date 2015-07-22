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


### Configuration Options
--------------------------------

#### upd_locator_service --help

    udp_locator_service -- Provides IPv4 Addres/Port Service info.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_service [-v] [-m "<delimited-response-message-string>"][-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1, Debug & no demonize=88.
      -m, --message  Response message to send.
      -v, --version  Version printout.
      -h, --help   Show this help screen.
      
      Format: service_registry_message
        delimited-response-message-string : required order -> name,ip,port,line-delimiter...
          format: "name=<service-name>,ip=<service-ipaddress>ddd.ddd.ddd.ddd,port=<service-portnumber>ddddd <line-delimiter>"
            name=<service-name> text_string_with_no_spaces
            ip=<service-ipaddress> ddd.ddd.ddd.ddd
            port=<service-portnumber> ddddd
          REQUIRED   <line-delimiter> is one of these '|', '%', ';'
 
          example: -m "name=lcd_display_service,ip=192.168.1.15,port=48028|"
          udp_locator_service -m "mame=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, pip=10.100.1.19, port=48029|name=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, port=48029|"


#### udp_locator_client --help
    udp_locator_client -- Collect IPv4 Address/Port Service info from all providers.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_client [-v] [-m 'text msg'][-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1, Debug & no demonize=88.
      -m, --message  Request/Response message to send  (any string - content is ignored)
      -v, --version  Version printout.
      -h, --help   Show this help screen.


#### lcd_display_service --help

    lcd_display_service -- LCD 4x20 Display Provider.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_service [-v] [-m 'text msg'][-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1.
      -r, --rows   Number of rows in physical display.
      -c, --cols   Number of columns in physical display.
      -m, --message  Welcome Message for line 1.
      -v, --version  Version printout.
      -h, --help   Show this help screen.


## Build:  Autotools project
Requires [WiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/download-and-install/) and assume the Pi is using [Raspbian](https://www.raspberrypi.org/downloads/).

    $ ./configure
    $ make
    $ sudo make install


