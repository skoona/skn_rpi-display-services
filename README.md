# Raspberry Pi Display/Locator Service
![Raspberry Pi Display/Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/rpi_display.png) 

Package includes two features. One designed to both locate your Pi and provide a list of known services.  The 
other acts as a central LCD display service, allowing other network devices/nodes to send a one-liner 
to the service for display on a 4x20 LCD.


## Executables
--------------------------------

|Program|Type|Platform|Port|Description|
|---|---|---|---|---|
|udp_locator_service|Server|any|48028|Maintains a list of known network services accessable via UDP socket.|
|udp_locator_client|Client|any|n/a|Collect services info from Service, which includes that service's ip address.|
|lcd_display_service|Server|RPi|48029|Accepts one-line messages over udp and display them on a LCD panel.|
|lcd_display_client|Client|any|wip|Sends the one-liner composed of various Pi metrics; like cpus, temps, etc.|



### Configuration Options
--------------------------------

#### Locator Service
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/skn_rpi1.png) 

#### upd_locator_service --help

    udp_locator_service -- Provides IPv4 Addres/Port Service info.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_service [-v] [-s] [-m "<delimited-response-message-string>"] [-h|--help]

    Options:
      -s, --include-display-service  Includes DisplaySerivce in default Registry response.
                    *Presumes both are located on same machine; otherwise enter with '-m'*
      -m, --message  ServiceRegistry<delimited-response-message-string> to send.
      -v, --version  Version printout.
      -h, --help     Show this help screen.
      
      **ServiceRegistry Format:  _<delimited-response-message-string>_**
        delimited-response-message-string : required order -> name,ip,port,line-delimiter...
          format: "name=<service-name>,ip=<service-ipaddress>ddd.ddd.ddd.ddd,port=<service-portnumber>ddddd <line-delimiter>"
            name=<service-name> text_string_with_no_spaces
            ip=<service-ipaddress> ddd.ddd.ddd.ddd
            port=<service-portnumber> ddddd
          REQUIRED   <line-delimiter> is one of these '|', '%', ';'
 
        **example: -m "name=lcd_display_service,ip=192.168.1.15,port=48028|"**
        ** -m "mame=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, ip=10.100.1.19, port=48029|"**

#### udp_locator_client --help
    udp_locator_client -- Collect IPv4 Address/Port Service info from all providers.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_client [-v] [-m 'any text msg'] [-u] [-a 'my_service_name'] [-h|--help]

    Options:
      -a, --alt-service-name=my_service_name
                              *lcd_display_service* is default, use this to change name.
      -u, --unique-registry   List *unique* entries from all responses.
      -m, --message    Any text to send; 'stop' causes service to terminate.
      -v, --version    Version printout.
      -h, --help       Show this help screen.


#### Display Service.
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/skn_rpi2.png)

> **Supports** on the ['YwRobot Arduino **LCM1602** IIC V1 LCD Backpack'](http://arduino-info.wikispaces.com/LCD-Blue-I2C)
> which uses the I2C controller *PCF8574*

        
> Planning to also add support for ['Adafruit **IC2/SPI** LCD Backpack'](https://www.adafruit.com/products/292)
> which is based on the I2C controller *MCP23008*

    
> Planning to also add support for ['Adafruit **USB/Serial** LCD Backpack'](https://www.adafruit.com/products/782)
> which uses */dev/ttyACM0*


#### lcd_display_service --help

    lcd_display_service -- LCD 4x20 Display Provider.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_service [-v] [-m 'Welcome Message'] [-r 4|2] [-c 20|16]  [-h|--help]

    Options:
      -r, --rows     Number of rows in physical display.
      -c, --cols     Number of columns in physical display.
      -m, --message  Welcome Message for line 1.
      -v, --version  Version printout.
      -h, --help     Show this help screen.

#### lcd_display_client --help

    lcd_display_client -- Send messages to display service.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_client [-v] [-m 'text msg for display'] [-u] [-n] [-a 'my_service_name'] [-h|--help]

    Options:
      -a, --alt-service-name=my_service_name
                              *lcd_display_service* is default, use this to change name.
      -m, --message           Message to send to display, default: *$ uname -a output*
      -n, --non-stop=1|300    Continue to send updates every DD seconds until ctrl-break.
      -u, --unique-registry   List unique entries from all responses.
      -v, --version           Version printout.
      -h, --help              Show this help screen.


## Build:  Autotools project
--------------------------------
Requires [WiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/download-and-install/) and assume the Pi is using [Raspbian](https://www.raspberrypi.org/downloads/).

    $ ./configure
    $ make
    $ sudo make install

*Only lcd_display_service requires __WiringPi__*, there is or will be a *client_only* git branch to support serparate compiles of only clients. 


## Notes:
--------------------------------

- The git branch **clients_only** can be compiled any Linux/Mac platform.
- Planning to write *systemd* unit scripts as part of package, add the following to rc.local for now.
  * `/<path>/udp_locator_service >> /tmp/udp_locator_service.log 2>&1 &`

