/*
* Copyright (c) "Aoidsoft" Co.
* Author: Alex Dubrovin (adu@aoidsoft.com)
* LICENSE: MIT
*/
#include "stdafx.h"
#include "MsxSerialPort.h"

MsxSerialPort::MsxSerialPort() : connected(false)
{}

MsxSerialPort::MsxSerialPort(int portNum) : connected(false)
{
	open( portNum );
}

//MsxSerialPort::MsxSerialPort(int portNum,  const char *host) : connected(false)
//{
//	open( portNum );
//}

//port open, set connected
bool MsxSerialPort::open(int portNum)
{
    if( connected )
		return true;

	char portName[12] = "\\\\.\\COM";
	sprintf(portName,"%s%i",portName,portNum);

    handler = CreateFileA(	portName,
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
    if(handler == INVALID_HANDLE_VALUE)
	{
        if(GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			printf("MsxSerialPort::open ERROR: COM%d is not available\n", portNum);
        }
		else
        {
			puts("MsxSerialPort::open Unknown ERROR");
        }
    }
    else 
	{
        DCB dcbSerialParameters = {0};

        if(!GetCommState(handler, &dcbSerialParameters)) 
		{
			puts("MsxSerialPort create ERROR: failed to get current serial port parameters");
        }
        else 
		{
            dcbSerialParameters.BaudRate =	MSX_BAUDRATE;
            dcbSerialParameters.ByteSize =	MSX_RYTESIZE;
            dcbSerialParameters.StopBits =	MSX_STOPBITS;	//ONESTOPBIT;
            dcbSerialParameters.Parity =	MSX_PARITY;	//NOPARITY;
            //dcbSerialParameters.fDtrControl = DTR_CONTROL_ENABLE;

            if(!SetCommState(handler, &dcbSerialParameters))
            {
                puts("MsxSerialPort create WARNING: could not set serial port parameters");
            }
            else 
			{
                connected = true;
                PurgeComm(handler, PURGE_RXCLEAR | PURGE_TXCLEAR);
                Sleep(MSX_WAIT_TIME);
				return true;
            }
        }
		CloseHandle(handler);
    }
	return false;
}

MsxSerialPort::~MsxSerialPort()
{
    close();
}

int MsxSerialPort::read(char *buffer, unsigned int size, bool verbose)
{
    DWORD bytesRead;
    unsigned int toRead = 0;

    ClearCommError(handler, &errors, &status);

    if(status.cbInQue > 0)
	{
		toRead = status.cbInQue > size ? size : status.cbInQue;
		memset(buffer, 0, size);

		if(ReadFile(handler, buffer, toRead, &bytesRead, NULL) && bytesRead)
		{
			if( verbose ){ if( bytesRead > 1 ) printf("read[%d]: ",bytesRead); dump((const unsigned char *)buffer, bytesRead);}
			return bytesRead;
		}
    }

    return 0;
}

bool MsxSerialPort::write(const char *buffer, unsigned int size, bool verbose)
{
    DWORD bytesSend;

	if( verbose ){ if( size > 1 ) printf("to write[%d]: ",size); dump((const unsigned char *)buffer, size); }
    if(!WriteFile(handler, (void*) buffer, size, &bytesSend, 0))
	{
        ClearCommError(handler, &errors, &status);
		puts("MsxSerialPort::write error");
        return false;
    }
    else
	{
		if(FlushFileBuffers(handler))
		{
			if( verbose ) puts("write ok");
		}
		else
			puts("write flush error");
		return true;
	}
}

bool MsxSerialPort::isConnected()
{
    if(!ClearCommError(handler, &errors, &status))
		connected = false;
        
    return connected;
}

void MsxSerialPort::close()
{
	if( connected )
	{
		CloseHandle(handler);
		connected = false;
	}
}

void MsxSerialPort::dump(const unsigned char *buf, int size, const char *msg, bool end)
{
	if( msg ) printf( msg, size );
	for(int i=0; i < size; printf("%02X ", buf[i++]));
	if( end ) puts("");
}