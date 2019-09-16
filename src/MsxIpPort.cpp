/*
* Copyright (c) "Aoidsoft" Co.
* Author: Alex Dubrovin (adu@aoidsoft.com)
* LICENSE: MIT
*/
#include "stdafx.h"
#include "MsxIpPort.h"

MsxIpPort::MsxIpPort()
{
	connected = false;
}

MsxIpPort::MsxIpPort(int portNum)
{
	connected = false;
	open( portNum, "localhost" );
}

MsxIpPort::MsxIpPort(int portNum, const char *host)
{
	connected = false;
	open( portNum, host );
}

//port open, set connected
bool MsxIpPort::open(int portNum, const char *host)
{
    if( connected )
		return true;

    WORD sockVer = MAKEWORD(2,2);;
	WSAStartup(sockVer, &wsaData);

	LPHOSTENT hostEnt = gethostbyname( host );
	if( !hostEnt )
    {
		puts("MsxSerialPort::open ERROR: Unable to collect gethostbyname");
        WSACleanup();
        exit( 1 );
    }

    //make socket
    clientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( clientSock == SOCKET_ERROR )
    {
        puts("MsxSerialPort::open ERROR: Unable to create socket");
        WSACleanup();
        exit( 1 );
    }
 
    SOCKADDR_IN serverInfo;
    serverInfo.sin_family = PF_INET;
    serverInfo.sin_addr = *((LPIN_ADDR)*hostEnt->h_addr_list);
    serverInfo.sin_port = htons(portNum);
 
    if( connect(clientSock,(LPSOCKADDR)&serverInfo, sizeof(serverInfo)) == SOCKET_ERROR )
    {
        puts("MsxSerialPort::open ERROR: Unable to connect");
        WSACleanup();
        exit( 1 );
    }

    connected = true;
	return true;
}

int MsxIpPort::read(char *buffer, unsigned int size, bool verbose)
{
    int bytesRead;
    unsigned int toRead = 0;
	memset(buffer, 0, size);

    if((bytesRead = recv(clientSock, buffer, size, 0)) == SOCKET_ERROR )
    {
        puts("MsxIpPort::read error");
        WSACleanup();
        return 0;
    }

	if( verbose ){ if( bytesRead > 1 ) printf("read[%d]: ",bytesRead); dump((const unsigned char *)buffer, bytesRead);}
	return bytesRead;
}

bool MsxIpPort::write(const char *buffer, unsigned int size, bool verbose)
{
	if( verbose ){ if( size > 1 ) printf("to write[%d]: ",size); dump((const unsigned char *)buffer, size); }
    if( send(clientSock, buffer, size, 0) == SOCKET_ERROR )
    {
        puts("MsxIpPort::write error");
        WSACleanup();
        return false;
    }
	return true;
}

bool MsxIpPort::isConnected()
{
    return connected;
}

void MsxIpPort::close()
{
	if( connected )
	{
		closesocket(clientSock);
		WSACleanup();
		connected = false;
	}
}

//void MsxIpPort::dump(const unsigned char *buf, int size, const char *msg, bool end)
//{
//	if( msg ) printf( msg, size );
//	for(int i=0; i < size; printf("%02X ", buf[i++]));
//	if( end ) puts("");
//}