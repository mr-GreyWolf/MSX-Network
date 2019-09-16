/*
* Copyright (c) "Aoidsoft" Co.
* Author: Alex Dubrovin (adu@aoidsoft.com)
* LICENSE: MIT
*/
#ifndef MSXIPPORT_H
#define MSXIPPORT_H

#include <windows.h>
#include <stdio.h>
//#include <winsock2.h>
#include "MsxSerialport.h"
#pragma comment(lib, "Ws2_32.lib")

class MsxIpPort : public MsxSerialPort
{
private:
    WSADATA wsaData;
	SOCKET clientSock;
    //bool connected;

public:
    MsxIpPort();
    MsxIpPort(int portNum);
    MsxIpPort(int portNum, const char *host = "localhost");

    virtual bool open(int portNum, const char *host);
	virtual int read(char *buffer, unsigned int size, bool verbose = false);
    virtual bool write(const char *buffer, unsigned int size, bool verbose = false);
    virtual bool isConnected();
    virtual void close();
	//virtual void dump(const unsigned char *buf, int size, const char *msg = NULL, bool end = false);
};

#endif // MSXIPPORT_H
