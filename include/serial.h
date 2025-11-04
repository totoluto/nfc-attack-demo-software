#ifndef SERIAL_H
#define SERIAL_H

#include <libserialport.h>

void serialInit(void);
struct sp_port **serialListPorts(void);
int serialOpen(const char *portName);
void serialClose(void);
int serialSend(const char *data);
int isSerialOpen(void);
int serialRead(char *buf, int maxlen);

#endif
