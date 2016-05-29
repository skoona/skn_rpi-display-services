/**
 * a2d_display_client.c
 * - Display Room Temperature Client
 **Supports** for ['AD / DA Shield Module For Raspberry Pi'](http://www.amazon.com/Shield-Module-For-Raspberry-Arduino/dp/B00WGW48A8)
 - which is based on the I2C controller *PCF8591T* to access:
 - - on-board temperature sensor; *NTC, MF58103J3950, B value 3950K, 1 K ohm 5% Cantherm*
 - - on-board light sensors; *GL5537-1 CdS Photoresistor*
 - example: **_a2d_display_client -i 73_**
*/

#include "skn_network_helpers.h"
#include <wiringPi.h>
#include <pcf8591.h>
#include <math.h>

/* I2C Constants */
#define A2D_BASE  120
#define LED       120
#define A2D_THERM A2D_BASE + 2
#define A2D_PHOTO A2D_BASE + 3


/* Steinhart Coefficents */

/* resistance at 25 degrees C */
#define THERMISTORNOMINAL 10000
/* temp. for nominal resistance (almost always 25 C) */
#define TEMPERATURENOMINAL 25
/* The beta coefficient of the thermistor (usually 3000-4000) */
#define BCOEFFICIENT 4207.0 // 3977.0 // 4791.8420 // 3977  // 3950
/* the value of the 'other' resistor */
#define SERIESRESISTOR 10240
/* A2D Resolution or bits */
#define A2D_PERCISION 255.0
/* Number of reads to average */
#define MAXREADS 5


/* steinhart-hart coefficents for 10K Ohm resistor, -55 C to 150 C */
#define sA 1.129148E-03 // 1.129241E-03   //  1.129148E-03
#define sB 2.34125E-04  // 2.341077E-04   //  2.34125E-04
#define sC 8.76741E-08  // 8.775468E-08   //  8.76741E-08
#define calibrationOffset -0.5

/**
 * Convert rawADC into Ohms
*/
double adcToOhms( int rawADC ) {
  double resistance = 0.0;

  resistance = (double)(( A2D_PERCISION / rawADC) - 1.0);
  resistance = (double)( SERIESRESISTOR / resistance );

  return resistance;
}

/**
 * Inputs ADC Value from Thermistor and outputs Temperature in Celsius
 *
 * Utilizes the Steinhart-Hart Thermistor Equation:
 *
 *    If  r is the thermistor resistance we can use the Steinhart-Hart
 *       equation to calculate the temperature t in degrees kelvin: return Celsius
 *       t = 1 / (sA + sB * ln(r) + sC * ln(r) * ln(r) * ln(r))
 *
 *    Temperature in Kelvin = 1 / (A + B * ln(r) + C * ln(r) * ln(r) * ln(r))
 *    where A = 0.001129148, B = 0.000234125 and C = 8.76741E-08
*/
double steinhartAdcToCelsius(int rawADC) {
  double kelvin = 0.0, celsius = 0.0;

  kelvin = log( adcToOhms(rawADC) );
  kelvin = 1 / (sA + (sB * kelvin) + sC * kelvin * kelvin * kelvin );
  celsius = kelvin - 273.15;  // Convert Kelvin to Celsius

  return celsius + calibrationOffset; // Return the Temperature in C, with correction offset
}

/**
 * Inputs ADC Value from Thermistor and outputs Temperature in Celsius
 *
 * Utilizes the Beta Factor Equation:
 *  1/T = 1/To + 1/B * ln(R/Ro)
 * I'm concerned about R1, in resistance, which is really 1K not 10K
 * https://learn.adafruit.com/thermistor/using-a-thermistor
*/
double betaAdcToCelsius(int rawADC) {
  double kelvin = 0.0, celsius = 0.0;

  kelvin = adcToOhms(rawADC) / THERMISTORNOMINAL;  // (R/Ro)
  kelvin = log(kelvin);                            // ln(R/Ro)
  kelvin /= BCOEFFICIENT;                          // 1/B * ln(R/Ro)
  kelvin += 1.0 / (TEMPERATURENOMINAL + 273.15);   // + (1/To)
  kelvin = 1.0 / kelvin;                           // Invert

  celsius = kelvin - 273.15;                       // convert to C from Kelvins

  return celsius + calibrationOffset; // Return the Temperature in C, with correction offset
}

/**
 * Averages sensor value from five reads
*/
double readAverageSensorValue(int sensor) {
    int rvalue[MAXREADS+3], index;

    /*
     * Temperature Sensor */
    for (index = 0, rvalue[MAXREADS] = 0; index < MAXREADS; index++) {
        rvalue[index] = analogRead(sensor) ;
        rvalue[MAXREADS] += rvalue[index];
        delay(10);
    }

    return rvalue[MAXREADS] / MAXREADS * 1.0;
}

/**
 * Free Air Temps
 */
int sknGetModuleTemp(char *buffer) {
    double fTemp = 0.0, cTemp = 0.0, value = 0.0;

    value = readAverageSensorValue(A2D_THERM);

    /*
     * Steinhart Method */
    cTemp = steinhartAdcToCelsius(value);
    fTemp = (cTemp * 1.8) + 32.0;          // Convert to USA

    /*
     * Write to output buffer */
    snprintf( buffer, (SZ_INFO_BUFF - 1),"AIR: %3.1fC %3.1fF", cTemp, fTemp);

  return EXIT_SUCCESS;
}

/**
 * Photo Resistor  Brightness Indicator
 */
int sknGetModuleBright(char *buffer) {
    double value = 0.0;
    char *plights;

    /*
     * Photo Sensor Lumens */
    value = readAverageSensorValue(A2D_PHOTO) ;
    value = 256.0 - value;

    if (value < 2.5) {
        plights = "Dark";
    } else if (value < 50.0) {
        plights = "Dim";
    } else if (value < 125.0) {
        plights = "Light";
    } else if (value < 199.0) {
        plights = "Bright";
    } else {
        plights = "Brightest";
    }

    /*
     * Write to output buffer */
    snprintf( buffer, (SZ_INFO_BUFF - 1), "%s", plights);

  return EXIT_SUCCESS;
}


int main(int argc, char *argv[])
{
    char request[SZ_INFO_BUFF];
    char registry[SZ_CHAR_BUFF];
    PServiceRegistry psr = NULL;
    PRegistryEntry pre = NULL;
    PServiceRequest pnsr = NULL;
    int vIndex = 0;

    gd_i_i2c_address = 0;

    memset(registry, 0, sizeof(registry));
    memset(request, 0, sizeof(request));
	strncpy(registry, "DisplayClient: Raspberry Pi where are you?", sizeof(registry) - 1);

    skn_program_name_and_description_set(
    		"a2d_display_client",
			"Send Measured Temperature and Light(lux) to Display Service."
    );

	/* Parse any command line options,
	 * like request string override */
    if (skn_handle_locator_command_line(argc, argv) == EXIT_FAILURE) {
        exit(EXIT_FAILURE);
    }
    if (gd_pch_message != NULL) {
    	    strncpy(request, gd_pch_message, sizeof(request));
	    free(gd_pch_message); // from strdup()
	    gd_pch_message = request;
    } else if (argc == 2) {
    	    strcpy(request, argv[1]);
    }

	skn_logger(SD_DEBUG, "Request  Message [%s]", request);
	skn_logger(SD_DEBUG, "Registry Message [%s]", registry);

	/* Initialize Signal handler */
	skn_signals_init();

    // wiringPiSetup () ;
    wiringPiSetupSys();

    // Add in the pcf8591
    if (gd_i_i2c_address == 0) {
        gd_i_i2c_address = 0x49;
    }
    pcf8591Setup (A2D_BASE, gd_i_i2c_address) ;

    pinMode (LED, OUTPUT) ;   // On-board LED
    analogWrite(LED, 0) ;     // Turn off the LED

	/* Create local socket for sending requests */
	gd_i_socket = skn_udp_host_create_broadcast_socket(0, 4.0);
	if (gd_i_socket == EXIT_FAILURE) {
        skn_signals_cleanup(gi_exit_flag);
    	exit(EXIT_FAILURE);		
	}

    skn_logger(SD_NOTICE, "Application Active...");

	/* Get the ServiceRegistry from Provider
	 * - could return null if error */
	psr = skn_service_registry_new(gd_i_socket, registry);
	if (psr != NULL && skn_service_registry_entry_count(psr) != 0) {
	    char *service_name = "lcd_display_service";

	    if (gd_pch_service_name != NULL) {
	        service_name = gd_pch_service_name;
	    }

		/* find a single entry */
		pre = skn_service_registry_find_entry(psr, service_name);
		if (pre != NULL) {
            skn_logger(" ", "\nLCD DisplayService (%s) is located at IPv4: %s:%d\n", pre->name, pre->ip, pre->port);
		}

        /*
         * Switch to non-broadcast type socket */
        close(gd_i_socket); gd_i_socket = -1;
        gd_i_socket = skn_udp_host_create_regular_socket(0, 8.0);
        if (gd_i_socket == EXIT_FAILURE) {
            if (psr != NULL) skn_service_registry_destroy(psr);
            skn_signals_cleanup(gi_exit_flag);
            exit(EXIT_FAILURE);
        }
    }

	// we have the location
	if (pre != NULL) {
	    if (request[0] == 0) {
	        sknGetModuleTemp(request);
	    }
	    pnsr = skn_udp_service_provider_service_request_new(pre, gd_i_socket, request);
	}
	if (pnsr != NULL) {
        do {
            analogWrite(LED, 255) ; // Flicker the LED

            /*
             * Do Work */
            sknGetModuleTemp(pnsr->request);
            vIndex = skn_udp_service_provider_send(pnsr);
            if ((vIndex == EXIT_FAILURE) && (gd_i_update == 0)) { // ignore if non-stop is set
                break;
            }

            sleep(3);

            /*
             * Do Work */
            sknGetModuleBright(pnsr->request);
            vIndex = skn_udp_service_provider_send(pnsr);
            if ((vIndex == EXIT_FAILURE) && (gd_i_update == 0)) { // ignore if non-stop is set
                break;
            }

            analogWrite(LED, 0) ; // Flicker the LED

            sleep(gd_i_update);
        } while(gd_i_update != 0 && gi_exit_flag == SKN_RUN_MODE_RUN);
        free(pnsr);  // Done

    } else {
        skn_logger(SD_WARNING, "Unable to create Network Request.");
    }

	/* Cleanup and shutdown
	 * - if shutdown was caused by signal handler
	 *   then a termination signal will be sent via signal()
	 *   otherwise, a normal exit occurs
	 */
    analogWrite(LED, 0) ; // LED off
    if (gd_i_socket) close(gd_i_socket);
    if (psr != NULL) skn_service_registry_destroy(psr);
    skn_signals_cleanup(gi_exit_flag);

    exit(EXIT_SUCCESS);
}
