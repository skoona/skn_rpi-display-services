# Makefile.am  Remove (WiringPi) or lcd_display_service, a2d_display_client for Clients_Only(Linux/OSX) build

if OSXBUILD
bin_PROGRAMS=udp_locator_service udp_locator_client lcd_display_client
else 
bin_PROGRAMS=lcd_display_service udp_locator_service udp_locator_client lcd_display_client para_display_client a2d_display_client
endif


udp_locator_service_SOURCES=udp_locator_service.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
udp_locator_service_LDFLAGS = -lpthread -lm
udp_locator_service_LDADD = -L/usr/local/lib 

udp_locator_client_SOURCES=udp_locator_client.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
udp_locator_client_LDFLAGS = -lpthread -lm
udp_locator_client_LDADD = -L/usr/local/lib 

lcd_display_client_SOURCES=lcd_display_client.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
lcd_display_client_LDFLAGS = -lpthread -lm
lcd_display_client_LDADD = -L/usr/local/lib 

lcd_display_service_SOURCES=lcd_display_service.c skn_network_helpers.c skn_rpi_helpers.c skn_signal_manager.c skn_common_headers.h skn_rpi_helpers.h skn_network_helpers.h
lcd_display_service_LDFLAGS = -lpthread -lrt -lm -lwiringPi -lwiringPiDev
lcd_display_service_LDADD = -L/usr/local/lib

para_display_client_SOURCES=para_display_client.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
para_display_client_LDFLAGS = -lpthread -lm 
para_display_client_LDADD = -L/usr/local/lib 

a2d_display_client_SOURCES=a2d_display_client.c skn_network_helpers.c skn_common_headers.h skn_network_helpers.h
a2d_display_client_LDFLAGS = -lpthread -lm -lrt -lwiringPi
a2d_display_client_LDADD = -L/usr/local/lib 
