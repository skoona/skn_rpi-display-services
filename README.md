# Raspberry Pi Display/Locator Service
![Raspberry Pi Display/Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/images/rpi_display.png) 

Package includes two features. One designed to both locate your Pi and provide a list of known services.  The 
other acts as a central LCD display service, allowing other network devices/nodes to send a one-liners 
to the service for display on its LCD.

Each Raspberry Pi, Mac OSX, and Linux node in your network could execute *udp_locator_service* (LocatorService) as
a backgound program using SystemD, a LaunchD PList, or an init script of some sort.  The LocatorService collects the
address of the machine it running on and creates a entry in its ServiceRegistry; which it sends to clients that requests it.

This establishes the network of locators, for which a *udp_locator_client* can poll at any time to find your Pi.

Likewise the *lcd_display_service* executes on nodes that also have an LCD Display of some sort; Serial/USB, MCP23008, or PCF8574 based.  It
is the provider of the *shared LCD Display*, and the target of the *lcd_display_client* (DisplayClient).  DisplayClients query all LocatorServices
for their active ServiceRegistry, and pickout the default *lcd_display_service* entry, or via command line option *--alt-service-name=my_service_name*.

Once a DisplayService is located, the DisplayClient sends a message to the DisplayService to display; from the command line *-m* option.  Or if the
DisplayClient has been started in *--non-stop=60* mode, it will send random status messages automatically to the DisplayService; in this case every 30 seconds.

These apps were not intended to be final form applications.  Instead they are well formed starters for your own projects.

Enjoy! 


## Executables
--------------------------------

|Program|Type|Platform|Port|Description|
|---|---|---|---|---|
|udp_locator_service|Server|any|48028|Maintains a list of known network services accessible via UDP socket.|
|udp_locator_client|Client|any|n/a|Collect services info from Service, which includes that service's ip address.|
|lcd_display_service|Server|RPi|48029|Accepts one-line messages over udp and display them on a LCD panel.|
|lcd_display_client|Client|any|n/a|Sends one-liner composed of various Pi metrics; like cpus, temps, etc.|
|para_display_client|Client|Parallella|n/a|Sends one-liner with Zynq chip's temperature.|
|a2d_display_client|Client|Rpi|n/a|Sends one-liner with measured temp and light sensor values from *AD/DA Shield Module For Raspberry Pi *|


### Configuration Options
--------------------------------

#### Locator Service
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/images/skn_rpi1.png) 

#### upd_locator_service --help

    udp_locator_service -- Provides IPv4 Addres/Port Service info.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_service [-v] [-s] [-m "<delimited-response-message-string>"] [-h|--help]
      udp_locator_service 
      udp_locator_service -s -a mcp_display_service
      udp_locator_service -m 'name=mcp_locator_service, ip=10.100.1.19, port=48028|'

    Options:
      -a, --alt-service-name=my_service_name
                     *lcd_display_service* is default, use this to change name.
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
        ** -m "name=rpi_locator_service, ip=10.100.1.19, port=48028|name=lcd_display_service, ip=10.100.1.19, port=48029|"**

#### udp_locator_client --help
    udp_locator_client -- Collect IPv4 Address/Port Service info from all providers.
              Skoona Development <skoona@gmail.com>
    Usage:
      udp_locator_client [-v] [-m 'any text msg'] [-u] [-a 'my_service_name'] [-h|--help]
      udp_locator_client 
      udp_locator_client -u -a 'my_service_name'

    Options:
      -a, --alt-service-name=my_service_name
                              *lcd_display_service* is default, use this to change name.
      -u, --unique-registry   List *unique* entries from all responses.
      -m, --message    Any text to send; 
          _'**QUIT!**' causes service to terminate._
          _'**ADD **<delimited-response-message-string>'  -- add new registry entry into Service_ 
      -v, --version    Version printout.
      -h, --help       Show this help screen.


#### Display Service.
![Locator Service](https://github.com/skoona/skn_rpi-display-services/raw/master/images/skn_rpi2.png)

> **Supports** on the ['YwRobot Arduino **LCM1602** IIC V1 LCD Backpack'](http://arduino-info.wikispaces.com/LCD-Blue-I2C)
> - which uses the I2C controller *PCF8574*
> - example: **_lcd_display_service -r4 -c20 -t pcf_**
> - **This is the default.** example: **_lcd_display_service_**

        
> **Supports**  ['Adafruit **IC2/SPI** LCD Backpack'](https://www.adafruit.com/products/292)
> - which is based on the I2C controller *MCP23008*
> - example: **_lcd_display_service -r4 -c20 -t mcp_**

        
> **Supports**  ['Adafruit RGB Negative 16x2 LCD+Keypad Kit'](https://www.adafruit.com/products/1110)
> - which is based on the I2C controller * MCP23017*
> - example: **_lcd_display_service -r2 -c16 -t mc7_**
> - Untested: I don't have one to test

    
> **Supports**  for ['Adafruit **USB/Serial** LCD Backpack'](https://www.adafruit.com/products/782)
> - which uses */dev/ttyACM0*
> - example: **_lcd_display_service -r2 -c16 -p /dev/ttyACM0 -t ser_**


> **Supports** for ['AD / DA Shield Module For Raspberry Pi'](http://www.amazon.com/Shield-Module-For-Raspberry-Arduino/dp/B00WGW48A8)
> - which is based on the I2C controller *PCF8591T* to access:
> - - on-board temperature sensor; *NTC, MF58103J3950, B value 3950K, 1 K ohm 5% Cantherm* 
> - - on-board light sensors; *GL5537-1 CdS Photoresistor*
> - example: **_a2d_display_client -i 73_**



#### lcd_display_service --help

    lcd_display_service -- LCD 4x20 Display Provider.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_service [-v] [-m 'Welcome Message'] [-r 4|2] [-c 20|16] [-i 39|32] [-t pcf|mcp|ser|mc7] [-p string] [-h|--help]

    Options:
      -r, --rows=dd  Number of rows in physical display.
      -c, --cols=dd  Number of columns in physical display.
      -p, --serial-port=string Serial port.       | ['/dev/ttyACM0']
      -i, --i2c-address=ddd  I2C decimal address. | [0x27=39, 0x20=32]
      -t, --i2c-chipset=ccc  I2C Chipset.         | [pcf|mcp|ser|mc7]
      -m, --message  Welcome Message for line 1.
      -v, --version  Version printout.
      -h, --help     Show this help screen.

#### [lcd|para|a2d]_display_client --help

    lcd_display_client -- Send messages to display service.
              Skoona Development <skoona@gmail.com>
    Usage:
      lcd_display_client [-v] [-m 'text msg for display'] [-u] [-n] [-a 'my_service_name'] [-h|--help]
      lcd_display_client -u -m 'Please show this on shared display.'
      lcd_display_client -u -m 'Please show this on shared display.' -a 'ser_display_service'
      lcd_display_client -u -n 60 -a 'ser_display_service'
      lcd_display_client -u -n 60 -a 'mcp_display_service'
      para_display_client -n 330
      a2d_display_client -i 73 -n 330

    Options:
      -a, --alt-service-name=my_service_name
                              *lcd_display_service* is default, use this to change name.
      -m, --message           Message to send to display, default: *$ uname -a output*
          _'**QUIT!**' causes service to terminate._
      -n, --non-stop=1|300    Continue to send updates every DD seconds until ctrl-break.
      -u, --unique-registry   List unique entries from all responses.
      -i, --i2c-address=ddd   I2C decimal address. | [0x49=73, 0x20=32]         
      -v, --version           Version printout.
      -h, --help              Show this help screen.


## Build:  Autotools project
--------------------------------
Requires [WiringPi](https://projects.drogon.net/raspberry-pi/wiringpi/download-and-install/) and assume the Pi is using [Raspbian](https://www.raspberrypi.org/downloads/).

    $ autoreconf -isfv            *Should not be required: use only if .configure produces errors*
    $ ./configure
    $ make
    $ sudo make install


*Only lcd_display_service requires __WiringPi__*, builds automatically determine if wiringPi is available and build what is available to build.



## Notes:
--------------------------------

- WiringPi is a required library for display_service and some of the disply clients.  If WiringPI is not present, which is the case with OSX and most Linuxs, then the autoconf will build the udp_locator_service and a limited set of clients.  When WiringPi is available all services and clients are built.
- Planning to write *systemd* unit scripts as part of package, add the following to rc.local for now.
  * '/<path>/udp_locator_service >> /tmp/udp_locator_service.log 2>&1 &'
- SINGLE QUOTES vs double quotes work a lot better for command line options.  

  
## Known Issues:
-------------------------------

> 08/08/2015 Found that concurrent I2C operations for LCD updates and reads from */sys/class/thermal/thermal_zone0/temp* cause the *lcd_display_service* to lockup with a process state of *uninterruptible sleep*.  This is caused by the RPi's internal firmware, with no immediate resolution.  To work around this issue, I have removed calls to getCpuTemp() from the lcd_display_service program.

