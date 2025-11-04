#include "serial.h"
#include <stdio.h>
#include <string.h>

static struct sp_port *currentPort = NULL;

void serialInit(void) {
    // Nothing to do!
}

struct sp_port **serialListPorts(void) {
    struct sp_port **ports = NULL;
    if (sp_list_ports(&ports) != SP_OK) {
        return NULL;
    }
    return ports;
}

int serialOpen(const char *portName) {
    if (currentPort) {
        serialClose();
    }

    if (sp_get_port_by_name(portName, &currentPort) != SP_OK) {
        fprintf(stderr, "Error: could not find port %s\n", portName);
        return -1;
    }

    if (sp_open(currentPort, SP_MODE_READ_WRITE) != SP_OK) {
        fprintf(stderr, "Error: failed to open port %s\n", portName);
        sp_free_port(currentPort);
        currentPort = NULL;
        return -1;
    }

    sp_set_baudrate(currentPort, 9600);
    printf("Connected to %s\n", portName);
    return 0;
}

void serialClose(void) {
    if (currentPort) {
        sp_close(currentPort);
        sp_free_port(currentPort);
        currentPort = NULL;
        printf("Serial port closed.\n");
    }
}

int serialSend(const char *data) {
    if (!currentPort) {
        fprintf(stderr, "Error: no open port.\n");
        return -1;
    }

    int written = sp_nonblocking_write(currentPort, data, strlen(data));
    sp_nonblocking_write(currentPort, "\n", 1);
    printf("Sent: %s (%d bytes)\n", data, written);
    return written;
}

int isSerialOpen(void) {
    return currentPort != NULL;
}

int serialRead(char *buf, int maxlen) {
    if (!currentPort) {
        return -1;
    }

    int n = sp_nonblocking_read(currentPort, buf, maxlen);
    if (n < 0) {
        fprintf(stderr, "Error reading from serial port.\n");
        return -1;
    }

    return n;
}