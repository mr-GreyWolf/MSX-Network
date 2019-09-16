/*
* Copyright (c) "Aoidsoft" Co.
* Author: Alex Dubrovin (adu@aoidsoft.com)
* LICENSE: MIT
*/
#ifndef MSXSERIALPORT_H
#define MSXSERIALPORT_H

#define MSX_WAIT_TIME	1000
#define MAX_DATA_LENGTH	255
#define MSX_BAUDRATE	CBR_38400
#define MSX_RYTESIZE	8
#define MSX_STOPBITS	2
#define MSX_PARITY		EVENPARITY

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

class MsxSerialPort
{
private:
    HANDLE handler;
    COMSTAT status;
    DWORD errors;

protected:
    bool connected;

public:
    MsxSerialPort();
    MsxSerialPort(int portNum);
//    MsxSerialPort(int portNum,  const char *host = "com");
    ~MsxSerialPort();

    virtual bool open(int portNum);
	virtual int read(char *buffer, unsigned int size, bool verbose = false);
    virtual bool write(const char *buffer, unsigned int size, bool verbose = false);
    virtual bool isConnected();
    virtual void close();
	virtual void dump(const unsigned char *buf, int size, const char *msg = NULL, bool end = false);
};

#endif // SERIALPORT_H
