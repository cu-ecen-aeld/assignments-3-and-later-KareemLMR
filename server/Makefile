CC=$(CROSS_COMPILE)gcc

USE_AESD_CHAR_DEVICE ?= 1

all:    $(OBJ)
	$(CC) -o aesdsocket aesdsocket.c -pthread -DUSE_AESD_CHAR_DEVICE
clean:
	rm -f *.o aesdsocket
