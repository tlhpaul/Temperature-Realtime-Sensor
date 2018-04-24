CC = clang
ARGS = -Wall -pthread

all: server read_usb.c

server: server.c read_usb.c
	$(CC) -o server $(ARGS) server.c read_usb.c

clean: 
	rm -rf server

run: all
	./server 8005 1411

mac:  all
	./server 8009 /dev/cu.usbmodem1421