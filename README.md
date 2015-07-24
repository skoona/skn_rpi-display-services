# Raspberry Pi Display/Locator Service
![Raspberry Pi Display/Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/rpi_display.png) 

Package includes two features. One designed to both locate your Pi and provide a list of known services.  The 
other acts as a central LCD display service, allowing other network devices/nodes to send a one-liner 
to the service for display on a 4x20 LCD.

## Executables

|Program|Type|Platform|Included|Description|
|---|---|---|---|---|
|udp_locator_service|Server|any|yes|Maintains a list of known network services accessable via UDP socket.|
|udp_locator_client|Client|any|yes|Collect services info from Service, which includes that service's ip address.|
|lcd_display_service|Server|any|yes|Accepts one-line messages over udp and display them on a 4x20 lcd panel.|
|lcd_display_client|Client|any|wip|Sends the one-liner composed of various Pi metrics; like cpus, temps, etc.|


### Configuration Options
--------------------------------

#### Locator Service
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/skn_rpi1.png) 

#### upd_locator_service --help

    udp_locator_service -- Provides IPv4 Addres/Port Service info.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_service [-v] [-s] [-m "<delimited-response-message-string>"][-d 1|88] [-h|--help]
    Options:
      -s, --include-display-service  Includes DisplaySerivce in default Registry response.
      -d 1|88 --debug=1  Debug mode=1, Debug.
      -m, --message  ServiceRegistry to send.
      -v, --version  Version printout.
      -h, --help   Show this help screen.
      
      ServiceRegistry Format: <delimited-response-message-string>
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
      udp_locator_client [-v] [-m 'any text msg'][-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1, Debug.
      -m, --message  Request message to send  (any string - content is ignored)
      -v, --version  Version printout.
      -h, --help   Show this help screen.


#### Display Service
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/skn_rpi2.png)

    Based on the 'YwRobot Arduino LCM1602 IIC V1'
    * - which uses the I2C controller PCF8574
    Can be reconfigured to support 'Adafruit IC2/SPI LCD Backpack'
    * - which is based on the I2C controller MCP23008

#### lcd_display_service --help

    lcd_display_service -- LCD 4x20 Display Provider.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_service [-v] [-m 'Welcome Message'] [-r 4|2] [-c 20|16] [-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1.
      -r, --rows   Number of rows in physical display.
      -c, --cols   Number of columns in physical display.
      -m, --message  Welcome Message for line 1.
      -v, --version  Version printout.
      -h, --help   Show this help screen.


#### lcd_display_client --help
Work in progress...

    lcd_display_client -- Send messages to display service.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_client [-v] [-m 'text msg for display'][-d 1|88] [-h|--help]
    Options:
      -d 1|88 --debug=1  Debug mode=1.
      -m, --message  Message to send to display, default: 'uname -a output'
      -v, --version  Version printout.
      -h, --help   Show this help screen.


## Build:  Autotools project
Requires [WiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/download-and-install/) and assume the Pi is using [Raspbian](https://www.raspberrypi.org/downloads/).

    $ ./configure
    $ make
    $ sudo make install


## Notes:

- The git branch **clients_only** can be compiled any Linux/Mac platform.
- Planning to write *systemd* unit scripts as part of package, add the following to rc.local for now.
  * `/<path>/udp_locator_service >> /tmp/udp_locator_service.log 2>&1 &`

