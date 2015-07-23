

bin_PROGRAMS=udp_locator_service udp_locator_client lcd_display_service lcd_display_client

udp_locator_service_SOURCES=udp_locator_service.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
udp_locator_service_LDFLAGS = -pthread -lrt -lm

udp_locator_client_SOURCES=udp_locator_client.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
udp_locator_client_LDFLAGS = -pthread -lrt -lm

lcd_display_client_SOURCES=lcd_display_client.c skn_network_helpers.c skn_rpi_helpers.c skn_common_headers.h skn_rpi_helpers.h skn_network_helpers.h
lcd_display_client_LDFLAGS = -lwiringPi -lwiringPiDev -lpthread
lcd_display_client_LDADD = -L/usr/local/lib -lrt -lm

lcd_display_service_SOURCES=lcd_display_service.c skn_network_helpers.c skn_rpi_helpers.c skn_common_headers.h skn_rpi_helpers.h skn_network_helpers.h
lcd_display_service_LDFLAGS = -lwiringPi -lwiringPiDev -lpthread 
lcd_display_service_LDADD = -L/usr/local/lib -lrt -lm
