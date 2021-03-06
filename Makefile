#***************************************************************
#  Program:      Makefile
#  Version:      20210213
#  Author:       Sifan S. Kahale
#  Description:  INDI PowerStar
#***************************************************************
#CFLAGS = -O2 -Wall -lrt -std=c++11
CFLAGS = -O2 -lrt -std=c++11
CC = g++ 

all: hid control powerstar

hid:
	cc -Wall -g -fpic -c -Ihidapi `pkg-config libusb-1.0 --cflags` hid.c -o hid.o
	
control:
	$(CC) $(CFLAGS)  -g -fpic -c -Ihidapi `pkg-config libusb-1.0 --cflags` PScontrol.cpp -o PScontrol.o

powerstar:
	$(CC) $(CFLAGS) -I/usr/include -I/usr/include/libindi -c indi_PowerStar.cpp
	
	$(CC) $(CFLAGS) -rdynamic hid.o PScontrol.o indi_PowerStar.o `pkg-config libusb-1.0 --libs` -lpthread -o indi_powerstar -lindidriver -lindiAlignmentDriver -lrt

clean:
	@rm -rf *.o indi_PowerStar

install:
	\cp -f indi_powerstar /usr/bin/
	\cp -f indi_powerstar.xml /usr/share/indi/
	service indiwebmanager stop
	service indiwebmanager start

