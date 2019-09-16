/*
* Copyright (c) "Aoidsoft" Co.
* Author: Alex Dubrovin (adu@aoidsoft.com)
* LICENSE: MIT
*/
#include "stdafx.h"

#include <iostream>
#include <windows.h>
#include <assert.h>
#include <sys/stat.h>

//#include "MsxSerialPort.h"
#include "MsxIpPort.h"
#include "commands.h"

#define SIZE16K 16384
#define SIZE32K 32768
#define MAXBLKSIZE	56
#define MAXPKTSIZE	128
#define MAX_COMM_LEN 37
#define MAX_MESS_LEN MAXBLKSIZE
#define SECTORSIZE	128

using namespace std;

const char runcom[] = "_nete:DefUsr=&H%04x:?Usr(0):_neti";
const char run170[] = "poke-1,170:DefUsr=&H%04x:?Usr(0)";

unsigned char ROM2BIN_startCode[] =
{
/*
	commented because this code section is needed only for
    disk drive switching off, but we run on a diskless MSX

	0xFB,				// ei
	0x76,				// halt
	0x10,0xFD,			// djnz [go to halt]
	0x3E,0xC9,			// ld a,C9
	0x32,0x9F,0xFD,		// ld (FD9F),a
*/
	// this code switches RAM page on address 0x4000,
	// copies 16K from 0x9000 to 0x4000
	// (or to 0x8000 when "ld de,4000" patched to be "ld de,8000")
	// and then execues ROM code
	// (or returns to Basic if "jp hl" is patched to be "nop")

	0xCD,0x38,0x01,		// call 0138
	0xE6,0x30,			// and 30
	0x0F,				// rrca
	0x0F,				// rrca
	0x0F,				// rrca
	0x0F,				// rrca
	0x4F,				// ld c,a
	0x06,0x00,			// ld b,00
	0x21,0xC5,0xFC,		// ld hl,FCC5
	0x09,				// add hl,bc
	0x7E,				// ld a,(hl)
	0xE6,0x30,			// and 30
	0x0F,				// rrca
	0x0F,				// rrca
	0xB1,				// or c
	0xF6,0x80,			// or 80
	0x26,0x40,			// ld h,40
	0xCD,0x24,0x00,		// call 0024
	0xF3,				// di
	0x11,0x00,0x40,		// ld de,4000 (0x40 will be patched to 0x80)
	0x21,0x00,0x90,		// ld hl,9000
	0x01,0x00,0x40,		// ld bc,4000
	0xED,0xB0,			// ldir
	0x2A,0x02,0x40,		// ld hl,(4002)
	0xE9,				// jp hl (can be patched to reach next command)

	0x3E, 0x80,			// ld a,80
	0x26, 0x40,			// ld h,40
	0xCD,0x24,0x00,		// call 0024
	0xC9				// ret
};

class MSXhandle
{
	#define SRC 4
	#define DST 2

	bool teacher;
	int studentNo;
	int verbose;
	string txbuf;
	long unsigned ERRno;

	MsxSerialPort *msx;
//	MsxIpPort msx;

	struct xData
	{
		unsigned char H, F, A;
		unsigned char FCB[37];
		xData()
		{
			init();
		}
		void init()
		{
			memset(this, 0, sizeof(xData));
		}
		void setFileName( const char *name )
		{
			FCB[0] = 8;
			int i = 0;
			// copy filename
			for(; i < 8; i++)
			{
				if( name[i] == '.' )
					break;
				FCB[ i + 1 ] = toupper(name[i]);
			}
			for(; i < 11; FCB[ ++i ] = ' ');
			// and extension
			int n = strlen(name);
			for(i = 0; i < 4; i++)
				FCB[ 12 - i ] = toupper(name[n - i]);
		}
	} TxData, RxData;

public:
	enum States
	{
		OK				= 0,
		Write_ERR		= 1,
		Read_ERR		= 2,
		WaitRx_ERR		= 4,
		SendHeader_ERR	= 8,
		SendBuf_ERR		= 0x10,
		Ping_ERR		= 0x20,
		Send_ERR		= 0x40,
		SendCommand_ERR = 0x80,
		Run_ERR			= 0x100,
		Poke_ERR		= 0x200,
		Stop_ERR		= 0x400,
		Init_ERR		= 0x80000000
	};

	MSXhandle( int port ) : teacher(true), studentNo(127),  verbose(0)
	{
		init(port);
	}

	MSXhandle( int port, int stNo ) : teacher(true), studentNo(stNo), verbose(0)
	{
		init(port);
	}

	MSXhandle( int port, int stNo, int verb ) : teacher(true), studentNo(stNo), verbose(verb)
	{
		init(port);
	}

	MSXhandle( const char *host, int port, int stNo, int verb ) : teacher(true), studentNo(stNo), verbose(verb)
	{
		init(port,host);
	}

	void init( int port, const char *host = NULL )
	{
		ERRno = 0;
		msx = host ? new MsxIpPort( port, host ) : new MsxSerialPort( port );
//		if(!msx.open( port, host ))
		if(!msx->isConnected())
		{
			ERRno = Init_ERR;
			exit(1);
		}
		else
			if( verbose ) puts("Success connect to port");

	}

	~MSXhandle()
	{
//		msx.close();
		delete msx;
	}

	void setStudentNo( int N )
	{
		studentNo = N;
	}

	void setTeacher( bool status = true)
	{
		teacher = status;
	}

	bool write(const unsigned char *buf, int size)
	{
		bool verb = verbose && (buf[3] != PONG || verbose > 1);
		if( verb ) printf("Write %d bytes\n", size);
		bool fSuccess = msx->write((char *)buf, size, verb );
		if( verb ) puts("");
		if( !fSuccess )
		{
			printf("WriteFile failed with error %d.\n", GetLastError());
			ERRno = Write_ERR;
			return false;
		}
		return true;
	}

	bool write()
	{
		return write((const unsigned char *)txbuf.c_str(), txbuf.size());
	}

	bool write_wait( const unsigned char *buf, int size, const char *msg = "command" )
	{
		if(!write( buf, size))
		{
			printf("Send %s error\n", msg);
			return false;
		}
		if(!WaitRx())
		{
			printf("Failed: no reply from client to %s", msg);
			return false;
		}
		return true;
	}

	bool write_wait( const char *msg = "command" )
	{
		return write_wait((const unsigned char *)txbuf.c_str(), txbuf.size(),msg);
	}

	void printProgress(int pkt)
	{
		if((pkt+1) % 10 == 0)
			printf (". %d ", pkt+1);
		else
			printf (".");
		fflush(stdout);
	}


	void getRxData( unsigned char *p )
	{
		RxData.H = getByte( &p[11] );
		RxData.F = getByte( &p[13] );
		RxData.A = getByte( &p[15] );
		if( verbose ) printf ("\n *** H: %.2x F: %.2x A: %.2x FCB: ", RxData.H, RxData.F, RxData.A);
		for(int i = 0; i < sizeof(RxData.FCB); i++)
		{
			RxData.FCB[i] = getByte( &p[17 + i * 2] );
			if( verbose ) printf(" %.2x", RxData.FCB[i]);
		}
		if( verbose ) puts("");
	}

	bool checkPacket( unsigned char *buf, int size )
	{
		int src, dst;
		int valid = 0;

		if(size < 5)
		{
			if( verbose ){ if( size > 1 ) printf("Wrong packet read[%d]: ",size); msx->dump(buf, size); puts("");}
			return false;
		}
		// Header
		if((buf[0] == 0xf0 || buf[0] == 0x78 || buf[0] == 0x70) && (buf[1] == 00))	//accepted or not accepted ...?
		{
//if(buf[0] != 0xf0 && buf[3] != PONG) {puts("???: ");msx.dump(buf,size);}
			dst = buf[2];
			src = buf[4];
			if(( teacher ? src != studentNo : dst != studentNo ))
				return false;
			bool verb = verbose && buf[3] != PING;
			if( verb || verbose > 2 ) msx->dump(buf, size, "read[%d]: ", true);
			unsigned int word;
			char *msg = NULL;
			switch( buf[3] )
			{
				case BASE:
					switch( buf[6] )
					{
						case RE_NET_CREATE_FILE:
							msg = "RE_NET_CREATE_FILE";
						case RE_NET_CLOSE_FILE:
							msg = msg ? msg : "RE_NET_CLOSE_FILE";
						case RE_NET_WRITE_FILE:
							msg = msg ? msg : "RE_NET_WRITE_FILE";
							if (verbose) printf ("*** %s from %d to %d\n", msg, src, dst);
							word = getWord(&(buf[7]));
							if (verbose) printf ("*** Payload: %d bytes\n", word);
							getRxData(buf);
							break;
						case SEND_FILE:
							if (verbose) printf ("*** receive SEND_FILE\n");
							Ack();
							break;
						default:
							if (verbose) printf ("*** Unknown BASE packet 0x%.2x ***\n", buf[6]);
							Ack();
							break;
					}
					break;
				case PING:
					msg = msg ? msg : "PING";
					Pong();
				case PONG:
					msg = msg ? msg : "PONG";
				case ACK:
					msg = msg ? msg : "ACK";
					if( verb ) printf ("*** %s from %d to %d\n", (msg?msg:"#Unknown#"), src, dst);
					break;
			}
		}
		return true;
	}

	bool WaitRx(unsigned char *rbuf = NULL, int  delay = 200)
	{
		if( studentNo == 127 )
		{
	 		Sleep( delay );
			return true;
		}

		unsigned char lbuf[MAXPKTSIZE],
					  *buf = rbuf ? rbuf : lbuf;

		if( verbose > 2 ) printf("WaitRx read: ");
		for(int i=0; i < MAXPKTSIZE;)
		{
			int n = 0, err = 0;
//			for(int j = 5; j-- && !(n = msx.read((char *)&buf[i], sizeof(buf)-i)); Sleep( delay ));
			for(int j = 5; j-- && !(n = msx->read((char *)&buf[i], 1, verbose > 2)); /*Sleep( delay )*/);
			if(!n && (err = GetLastError()))
			{
				printf("WaitRx error %d (%d bytes read)\n",GetLastError(),i);
				msx->dump( buf, i+1, NULL, true );
				ERRno = Read_ERR | WaitRx_ERR;
				return false;
			}
			i += n;
			if((buf[i-1] == LAST) || (buf[i-1] == INTERMEDIATE))
			{
//				if( verbose > 1 ) msx.dump( buf, i, "WaitRx[%d]: ", true );
				checkPacket( buf, i );
				if( verbose > 2 ) puts("\nWaitRx ok");
				return true;
			}
		}
		ERRno = WaitRx_ERR;
		puts("\nWaitRx fail");
		return false;
	}

	void setByte( unsigned char *ptr, unsigned char byte )
	{
		ptr[0] = (byte >> 7) & 0x01;
		ptr[1] = byte & 0x7f;
	}

	void setWord( unsigned char *ptr, unsigned short int len )
	{
		unsigned char hi, lo;

		hi = (len >> 8) & 0xff;
		lo = len & 0xff;

		setByte( ptr, hi );
		setByte( ptr + 2, lo );
	}

	unsigned char getByte( unsigned char *p )
	{
        return (p[1] + (p[0] ? 128 : 0));
	}

	unsigned int getWord( unsigned char *p )
	{
		return ((getByte(p) << 8) + getByte(p + 2));
	}

	bool SendHeader( unsigned short start, unsigned short end )
	{
		unsigned char hdr [] = {0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x52,
					0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00, 0x83};

		setSrcDst( hdr );
		setWord( &hdr[7], start );
		setWord( &hdr[11], end );

	    if( verbose ) msx->dump( hdr, sizeof(hdr), "\nHeader: ", true);

		if(!write_wait( hdr, sizeof(hdr), "SendHeader"))
		{
			ERRno |= SendHeader_ERR;
			return false;
		}
		return true;
	}

	struct checkSum
	{
		unsigned char value[2];
		checkSum()
		{
			ZeroMemory(value, sizeof(value));
		}

		void UpdateChecksum (unsigned char ch)
		{
			//if( verbose ) printf("Old CRC16: %2x%2x, ch:%2x\n", value [0], value [1], ch);
			if ((value[1] + ch) > 0xff)
				value[0]++;
			value[1] += ch & 0xff;
			//if( verbose ) printf("CRC16: %2x%2x\n", value [0], value [1]);
		}

		void GetChecksum(unsigned char *crc16)
		{
			crc16[0] = (value[0] >> 7) & 0x01;
			crc16[1] = value[0] & 0x7f;
			crc16[2] = (value[1] >> 7) & 0x01;
			crc16[3] = value[1] & 0x7f;
	//		if( verbose ) printf("CRC16: %.2x%.2x\n", checksum [0], checksum [1]);
		}
	};

	void setHeader( unsigned char *header, unsigned short int len )
	{
		txbuf.assign((char*)header, len);
	}

	void addByte( unsigned char ch )
	{
		unsigned char c = (ch & 0x80) >> 7;
		txbuf.append((char*)&c, 1 );
		c = ch & 0x7f;
		txbuf.append((char*)&c, 1 );
	}

	void addWord( unsigned int w )
	{
		addByte( w >> 8 );
		addByte( w & 255 );
	}

	void addBufEnd( unsigned char *buf, unsigned short int len, bool intermidiate = false )
	{
		unsigned char tail[5];
		checkSum checksum;
		for(int i = 0; i < len; i++)
		{
			addByte( buf[i] );
			checksum.UpdateChecksum( buf[i] );
		}
		checksum.GetChecksum( tail );
		tail[4] = intermidiate ? INTERMEDIATE : LAST;
		txbuf.append((char*)tail, sizeof(tail));
	}

	bool SetBasSize( unsigned int size )
	{
		size += 0x8000;
		if(Poke( 0xF6C2, size % 0x100 ) && Poke( 0xF6C3, size / 0x100 ))
			return true;
		puts("set size error");
		return false;
	}

	//bool SendBasFile( FILE *infile, int size )
	//{
	//	unsigned char buf[MAXBLKSIZE];
	//	unsigned short len;
	//	//an obscure header of the second packet
	//	//00 52 01 00 00 28 00 00
	//	//   52    80    28    00

	//	for(int pkt=0, cur=0; !feof(infile); pkt++)
	//	{
	//		if(!(len = fread(buf, 1, sizeof(buf), infile)))
	//			break;
	//		cur += len;

	//		if(!SendBuf( buf, len, (pkt ? SEND_BAS_NEXT : SEND_BAS_HEAD), cur < size ))
	//			return false;

	//		if( verbose )
	//			printf("\nSent pkt No.%d", pkt);
	//		else
	//			printProgress( pkt );
	//	}
	//	return SetBasSize( size );	//Basic set wrong length=size+1
	//}

	bool SendBuf( unsigned char *buf, unsigned short int len, unsigned char cmd = SEND_HEX, bool intermidiate = false )
	{
		unsigned char hdr[] = {0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x42,
								0x00, 0x00, 0x00, 0x00};
		hdr[6] = cmd;
		setSrcDst( hdr );
		setWord( &hdr[7], len );

	    if (verbose)
	    {
			msx->dump( hdr, sizeof(hdr), "\nHeader: ");
			printf ("\nData: ");
		}

		setHeader(hdr, sizeof(hdr));
		addBufEnd( buf, len, intermidiate );

		if(!write_wait("SendBuf"))
		{
			ERRno |= SendBuf_ERR;
			return false;
		}
		return true;
	}

	bool Stop()
	{
		unsigned char stopPacket[] = { 0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x65, 0x83 };
		stopPacket[2] = studentNo;
		if(!write_wait( stopPacket, sizeof(stopPacket), "Stop"))
		{
			puts("Stop write error\n");
			return false;
		}
		return true;
	}

	bool Receive( char *name )
	{
		unsigned char buf[MAXPKTSIZE];
		unsigned char packet[] = { 0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x62, 0x01, 0x7F, 0x01, 0x7F, 0x83 };
		packet[2] = studentNo;

		if( verbose ) printf("Sent RECV to %d\n", studentNo);
		if(!write( packet, sizeof(packet)))
		{
			puts("RECV write error\n");
			return false;
		}
		bool waitRx = false;
		FILE *outfile = fopen( name, "wb" );
		fputc( 0xFF, outfile);
		unsigned int size;
		int pkt = 0;
		do
		{
			if(!(waitRx = WaitRx( buf )))
				break;
			size = getWord(&buf[7]);
			for(unsigned int i=0; i < size; i++)
			{
				int c = getByte(&buf[ 11 + i*2 ]);
				fputc( c, outfile);
			}
			if( verbose == 0 ) printProgress( pkt++ );
		}
		while( buf[15+size*2] == INTERMEDIATE );
		fclose( outfile );

		return waitRx;
	}

	bool Ping( int dstAddr = 0, int srcAddr = 0 )
	{
		unsigned char pingPacket[] = { 0xf0, 0x00, 0x00, 0x05, 0x00, 0x83 };
		pingPacket[2] = dstAddr ? dstAddr : studentNo;
		pingPacket[4] = srcAddr;
		bool waitRes;

		if( verbose ) printf("Sent ping to %d\n", studentNo);
		for(int i=0; i < 3; i++)
		{
			if(!write( pingPacket, sizeof(pingPacket)))
			{
				printf("Ping write error, iteration %i\n",i);
				continue;
			}
			if( verbose ) printf("Sent ping No.%d\n", i+1);
			waitRes = WaitRx();
			if(waitRes) break;
		}
		if(!waitRes)
		{
			printf("\nFailed: no reply to PING\n");
			ERRno |= Ping_ERR;
			return false;
		}
		return true;
	}

	bool Pong( int dstAddr = 0, int srcAddr = 0 )
	{//								0x78
		unsigned char packet[] = { 0xF0, 0x00, 0x00, 0x15, 0x00, 0x83 };
		srcAddr = srcAddr ? srcAddr : studentNo;
		packet[2] = !teacher ? dstAddr : srcAddr;
		packet[4] = !teacher ? srcAddr : dstAddr;

		if( verbose > 1 ) printf("Repl pong to %d\n", dstAddr);
		if(!write( packet, sizeof(packet)))
		{
			puts("Pong write error\n");
			return false;
		}
		return true;
	}

	bool Ack( int dstAddr = 0, int srcAddr = 0 )
	{//								0x78
		unsigned char packet[] = { 0xF0, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83 };
		srcAddr = srcAddr ? srcAddr : studentNo;
		packet[2] = !teacher ? dstAddr : srcAddr;
		packet[4] = !teacher ? srcAddr : dstAddr;

		if( verbose ) printf("Repl ack to %d\n", dstAddr);
		if(!write( packet, sizeof(packet)))
		{
			puts("Ack write error\n");
			return false;
		}
		return true;
	}

	bool Send(unsigned char *binBuf, unsigned short start, unsigned short end)
	{
		if( verbose ) puts("Send header");
		if(!SendHeader( start, end ))
		{
			ERRno |= Send_ERR;
			return false;
		}

		unsigned short int len = (end - start +1);
		printf ("\n%d blocks to send: ", len / MAXBLKSIZE + (len % MAXBLKSIZE ? 1 : 0));
		for(int i=0, l=len; l > 0; l -= MAXBLKSIZE )
		{
			if (verbose)
				printf("\nSent block No.%d", ++i);
			else
				printProgress( i++ );

			if(!SendBuf( &binBuf[len-l], (l > MAXBLKSIZE ? MAXBLKSIZE : l), SEND_HEX, l > MAXBLKSIZE ))
			{
				ERRno |= Send_ERR;
				return false;
			}
		}
		return true;
	}

	bool RunBasic( unsigned short row = 0xFFFF )
	{
		unsigned char hdr[] =	{0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x5e, 0x01, 0x7f, 0x01, 0x7f, 0x83};
		//							0	  1		2	  3		4	  5		6	  7		8	  9		10
		setSrcDst( hdr );
		setWord( &hdr[7], row );

		if( verbose ) msx->dump( hdr, sizeof(hdr), "\nHeader: ", true);
		return write_wait( hdr, sizeof(hdr), "RUN command");
	}

	bool Run( unsigned short run, const char *runstr = runcom )
	{
		char string[MAX_COMM_LEN+1];	//размер строки не должен быть превышен
		sprintf(string, runstr, run);				//размер строки не изменится
		printf("Run command: '%s'\n", string);
		return SendCommand( string );
	}

	bool SendCommand( const char *cmd, unsigned char cmdNo = 0x48, int maxLen = MAX_COMM_LEN, const char *msg = "command" )
	{
		unsigned char hdr[] =	{0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00};
		//							0	  1		2	  3		4	  5		6	  7		8	  9		10
		setSrcDst( hdr );
		hdr[6] = cmdNo;
		int len = strlen(cmd);
		if( len > maxLen )
		{
			printf("Unsuported command|message length %d > %d bytes", len, maxLen);
			return false;
		}
		setWord( &hdr[7], len );
		if( verbose ) msx->dump( hdr, sizeof(hdr), "\nHeader: ", true );
		setHeader(hdr, sizeof(hdr));
		addBufEnd((unsigned char *)cmd, len );
		return write_wait( msg );
	}

	bool Message( const char *cmd )
	{
		return SendCommand( cmd, 0x46, MAX_MESS_LEN, "message" );
	}

	void setSrcDst( unsigned char *hdr )
	{
		hdr[SRC] = 0;
		hdr[DST] = studentNo;
	}

	bool Poke( unsigned int addr, unsigned char byte )
	{
		unsigned char poke[] =	{0xf0, 0x00, 0x00, 0x01, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83};
		//							0     1     2     3     4     5     6     7     8     9    10    11    12    13
		setSrcDst( poke );
		setWord( &poke[7], addr );
		setByte( &poke[11], byte );
		if( verbose ) {printf ("\nPoke &H%04x, %.2x : ", addr, byte); msx->dump( poke, sizeof(poke), NULL, true);}
		return write_wait(poke, sizeof(poke), "<poke>");
	}

	bool SendPacket( int cmdType, unsigned char *buf, int len, int dstAddr = 0, int srcAddr = 0 )
	{
		unsigned char Header[] = { 0xf0, 0x00, 0x00, 0x01, 0x00 };
		if( verbose ) printf("SendPacket: from %d to %d, cmd: 0x%.2x, %d bytes\n", srcAddr, dstAddr, cmdType, len);
		// Header fields, common for all packets
		Header[2] = dstAddr ? dstAddr : studentNo;
		Header[4] = srcAddr;

		const char *msg = NULL;
		switch( cmdType )
		{
		case PING:
			//Header[3] = PING;
			return Ping( dstAddr, srcAddr );
		case NET_CREATE_FILE:
			msg = msg ? msg : "NET_CREATE_FILE";
		case NET_CLOSE_FILE:
			msg = msg ? msg : "NET_CLOSE_FILE";
		case NET_WRITE_FILE:
			msg = msg ? msg : "NET_WRITE_FILE";
			if( verbose ) printf("Send %s:\n", msg);
			setHeader(Header, sizeof(Header));
			addByte( cmdType );		// COMMAND
			addWord( len );			// LENGTH: FCB (37 bytes) + H, F, A (3 bytes)
			addBufEnd( buf, len );	// PAYLOAD + CHECHKSUM + TERMINATOR
			return write_wait( msg );
		case NET_MASTER_DATA:
			if( verbose ) puts("Send NET_MASTER_DATA:\n");
			for( int l = len; l > 0; l -= MAXBLKSIZE )
			{
				setHeader(Header, sizeof(Header));
				addByte((l == len) ? NET_MASTER_DATA : NET_MASTER_DATA2);

				addWord((l > MAXBLKSIZE) ? MAXBLKSIZE : l);
				addBufEnd(&buf[len - l], (l > MAXBLKSIZE ? MAXBLKSIZE : l), l > MAXBLKSIZE);
				if(!write_wait( "NET_MASTER_DATA" ))
					return false;
			}
			break;
		default:
			if( verbose ) puts("Unable to send UNKNOWN data");
		}
		return true;
	}

	bool SendFile( FILE *infile, const char *name, int size )
	{
		unsigned char Sector[SECTORSIZE];

		TxData.setFileName( name );
		// Create file on the net disk
		if(!SendPacket( NET_CREATE_FILE, (unsigned char *)&TxData, sizeof (TxData)))
		{
			puts("Create file on the net disk ERROR");
			return false;
		}
		if(verbose) printf("Got re: NET_CREATE_FILE.\n");

		// Reading the file sector-by-sector and writing them onto the net disk
		printf("\nNumber of sectors to send: %d\n", (size / sizeof(Sector)) + ((size % sizeof(Sector))?1:0));
		int len;
		for(int sectNo = 0; !feof(infile); sectNo++)
		{
			if((len = fread(Sector, 1, sizeof(Sector), infile)) == 0 )
				break;

   			SendPacket( NET_MASTER_DATA, Sector, sizeof(Sector) );
			SendPacket( NET_WRITE_FILE, (unsigned char *)&RxData, sizeof(RxData));

			if( verbose )
				printf("\nSent sector No.%d", sectNo);
			else
				printProgress( sectNo );
		}
		// close the file on the net disk
		SendPacket( NET_CLOSE_FILE, (unsigned char *)&RxData, sizeof(RxData));
		printf ("\nDone.\n");
		return true;
	}

};

int main(int argc, char **argv)
{
	int port = 1, studentNo = -1;
	int verbose = 0;
	bool err = false, ind, sendFile = false, callCpm = false, testRead = false, callStop = false, sendBas = false;
	int callRun = -1;
	char *fileNamePointer[256] = {NULL},
		 *command = NULL,
		 *message = NULL,
		 *recvFile = NULL,
		 *host = NULL;
	int filesNumber = 0;

	for(int i=0; ++i<argc && !err; ind=false)
	{
		if(*argv[i]=='-' || (ind = *argv[i]=='+'))
		switch(argv[i++][1])
		{
		case 'p':										//-p comN
				port = atoi(argv[i]);
				//if(port < 1 || port >99)
				//{
				//	puts("COM-port number value: 1 - 99");
				//	return 1;
				//}
				break;
		case 's':										//-s studentNo
				studentNo = atoi(argv[i]);
				if(studentNo >15)
				{
					puts("Student number value: -1 - 15 (-1 - for all)");
					return 1;
				}
				break;
		case 'S':										//-S - send file to CPM net disk
				i--;
				sendFile = true;
				break;
		case 'c':										//-c xxx === _SNDCMD xxx
				command = argv[i];
				break;
		case 'C':										//-C === _CPM
				i--;
				callCpm = true;
				break;
		case 'v':										//-v [logLevel0-2] - verbose
				if(i < argc && isdigit(*argv[i]))
					verbose = atoi(argv[i]);
				else
				{
					i--;
					verbose = 1;
				}
				break;
		case 'T':										//-T - Test read&reply student-mode
				i--;
				testRead = true;
				break;
		case 'm':										//-m xxx === _MESSAGE xxx
				message = argv[i];
				break;
		case 'i':										//-i host
				host = argv[i];
				break;
		default:
				err = true;
		}
		else
		if( *argv[i]=='_' )								//CALL section _XXXX || _xxxx
		{
			if( _stricmp(argv[i],"_sndcmd") == 0 )		//_SNDCMD xxx || _sndcmd xxx
				command = argv[++i];
			else
			if( _stricmp(argv[i],"_cpm") == 0 )			//_CPM || _cpm
				callCpm = true;
			else
			if( _stricmp(argv[i],"_message") == 0 )		//_MESSAGE xxx || _message xxx
				command = argv[++i];
			else
			if( _stricmp(argv[i],"_stop") == 0 )			//_STOP || _stop
				callStop = true;
			else
			if( _stricmp(argv[i],"_run") == 0 )		//_RUN xxx || _run xxx
				callRun = i+1 < argc && isxdigit(*argv[i+1]) ? atoi(argv[++i]) : 0xFFFF;
			//else
			//if( _stricmp(argv[i],"_send") == 0 )			//_SEND || _send
			//	sendBas = true;
			else
			if( _stricmp(argv[i],"_recv") == 0 )		//_RECV xxx || _recv xxx
				recvFile = i+1 < argc ? argv[++i] : NULL;
		}
		else
		{
			fileNamePointer[filesNumber++] = argv[i];
		}
	}
    if(err || argc <= 1)
    {
            printf("\n\
  MSX-Link v1.1.20190916,  Copyright (c) 2019,   <<Aoidsoft>> Co. (adu@aoidsoft.com)\n\
  Command line utility for the Main-computer functions in a local network KYBT2(MSX2ru):\n\
                                                                                   PC  <--->>>  KYBT2(MSX2ru)\n\
  Usage:  msx-link.exe [-p <Com|Port>Num] [-s StudentNo] [-<key>...] [_<command>...]  [file1] [file2] [...fileN]\n\
        [file1] [file2] [...fileN] - files for binary send (auto supported formats: BAS, BIN, ROM[8|16|32])\n\
    -key(s):\n\
        i <hst>: Ethernet address of the Host-gate to MSX-net,    no default\n\
        p <C>  : Connect to COM|IP-port number <C>,               default value  1 (treat as IP-port whith -i)\n\
        s <S>  : Work with 'Student' number <S>,                  default value -1:\n\
                   -1  - to all\n\
                    0  - to 'Teacher'                             (for <-T>est mode)\n\
                 1-15  - to <S>tudent number\n\
        c <cmd>        : Send Basic-command <cmd> to <S>tudent(s) (37 symbols limit) [like '_SNDCMD  <cmd> ']\n\
        m <msg>        : Send message <msg> to <S>tudent(s)       (56 symbols limit) [like '_MESSAGE <msg> ']\n\
        C              : Send '_cpm' to <S>tudent(s) for switching into CPM OS\n\
        S              : Send file(s) to CPM net-disk             (should be use with|after -C key)\n\
        T              : Test mode - dump&reply                   (RX & TX lines should swapped!)\n\
        v [0-2]        : Verbose mode with selected logging lvl,  default value 0\n\
        h|H|?          : This help\n\
    _command(s):\n\
        _recv   <file> : Recv Basic-program from <S>tudent into the <file>           [like '_RECEIVE <file>']\n\
        _run    [rowN] : Run  Basic-program on <S>tudent(s) with optional start rowN [like '_RUN     <rowN>']\n\
        _stop          : Stop Basic-program on <S>tudent(s)                          [like '_STOP'          ]\n\
        _sndcmd  <cmd> : Send Basic-command <cmd> to <S>tudent(s) (37 symbols limit) [like '_SNDCMD  <cmd> ']\n\
        _message <msg> : Send message <msg> to <S>tudent(s) (56 symbols limit)       [like '_MESSAGE <msg> ']\n\
        _cpm           : Send '_cpm' to <S>tudent(s) for switching into CPM OS\n\
\n\
  Example:\n\
%s -p 0 -m \"Hi all!\"\n"
//        _send   <file> : Send Basic-file to <S>tudent(s) (file must be MSX Basic fmt)[like '_SEND    <file>']\n
,argv[0]);
            return 1;
    }
	char s[2] = {0,0};
	if (studentNo < 0)
	{
		studentNo = 127;
		*s = 's';
	}
	printf("COM-port: %d, student No.%d\n", port, studentNo);

	MSXhandle msxh( host, port, studentNo, verbose );

	if( testRead )
	{
		msxh.setTeacher( false );
		while(msxh.WaitRx())
		{
			Sleep(1);
		}
		puts("Failed");
		return 1;
	}

	if( callStop )
	{
		msxh.Stop();
		Sleep( 1000 );
	}

	if( recvFile )
	{
		msxh.Receive( recvFile );
	}

	if( callCpm )
	{
		msxh.SendCommand( "_cpm" );
		Sleep( 2000 );
	}

	if(msxh.Ping())
		printf("Client%s ready\n",s);
	else
	{
		printf("Client%s not ready\n",s);
		return 1;
	}

	if( message )
		if(msxh.Message( message ))
			puts("Message sent ok");
		else
			puts("Message send error");

	for( int fileIdx = 0; fileIdx < filesNumber; fileIdx++ )
	{
		unsigned short start, end, run;
		unsigned char ch;
		unsigned char binBuf[SIZE32K];
		struct _stat stat_p;
		int result = _stat( fileNamePointer[fileIdx], &stat_p );
	    FILE *infile = fopen( fileNamePointer[fileIdx], "rb" );
		if (infile == NULL)
		{
			printf ("\nError: cannot open file %s\n", fileNamePointer[fileIdx]);
			break;
	    }

		if( sendFile )					// --- Send file to CPM net-disk ---
		{
			if(!msxh.SendFile( infile, fileNamePointer[fileIdx], stat_p.st_size ))
			{
				printf("Send file<%s> error\n",fileNamePointer[fileIdx]);
				return 1;
			}
			printf("Send file<%s> complete\n",fileNamePointer[fileIdx]);
		}

		fread( &ch, 1, 1, infile );	//read the first byte that can contain the file type

		//if( sendBas )					// --- Use special routine for sending Basic-file in the internal format ---
		//{
		//	if(!msxh.SendBasFile( infile, stat_p.st_size - 1 ))
		//	{
		//		printf("Send basic file<%s> error\n",fileNamePointer[fileIdx]);
		//		return 1;
		//	}
		//	printf("Send basic file<%s> complete\n",fileNamePointer[fileIdx]);
		//	break;
		//}

		if( ch == 0xfe )				//                 --- BINARY FILE ---
		{
			fread( &start, 2, 1, infile );
			fread( &end, 2, 1, infile );
			fread( &run, 2, 1, infile );
			unsigned short int binSize = (unsigned short int)(stat_p.st_size - 7);

			fread( &binBuf, binSize, 1, infile );
			printf( "File: %s; Start: %x, End: %x, Run: %x\n", fileNamePointer[fileIdx], start, start + binSize - 1, run ); //end

			if(!(msxh.Send( binBuf, start, start + binSize - 1 )))
			{
				puts("transmit error");
				break;
			}
			printf ("\n Done.\n");
			if(!msxh.Run( run, ( fileIdx+1 == filesNumber ? runcom : run170 )))
			{
				puts("Run error");
				break;
			}
			puts("\nRun.");
			// this wait is convenient when sending MegaROMs divided into many .BINs
			Sleep(1000);	//usleep (1000000);
		}
		else if (ch == 0xff)		//                  --- BASIC ---
		{
			unsigned short int basSize = (unsigned short int)stat_p.st_size;
			printf( "\nBASIC file, %d bytes\n", basSize );
			binBuf[0] = 0;
			fread( &binBuf[1], basSize-1, 1, infile );

			start = 0x8000;
			end = start + basSize - 1;
			printf( "\nFile: %s; Start: %x, End: %x", fileNamePointer[fileIdx], start+1, end );
			if( !msxh.Send( binBuf, start, end ))
			{
				puts("transmit error");
				break;
			}
			//if(!(msxh.Poke( 0xF6C2, end % 0x100 ) && msxh.Poke( 0xF6C3, end / 0x100 )))
			if(!msxh.SetBasSize( basSize-1 ))
			{
				puts("set size error");
				break;
			}
			printf ("\nSending Basic-file done.");
		}
		else if (ch == 0x41)		//					---	ROM images ---
		{
			printf ("\nROM file, %d bytes\n", stat_p.st_size);
			unsigned int romSize = stat_p.st_size;
			start = 0x9000;
			unsigned short int RS = romSize < SIZE32K ? romSize : SIZE16K;
			end = start + RS + sizeof(ROM2BIN_startCode) - 1;
			run = start + RS;
			unsigned int parts = romSize / RS;
			printf("%u x 16K ROM parts\n",parts);
			if( parts > 2 )
			{
				puts("Suported ROM size is 2x16Kb pages");
				return 1;
			}

			rewind (infile);
			for(unsigned int i=0; i++ < parts;)
			{
				fread (&binBuf, RS, 1, infile);
				memcpy(&binBuf[RS], ROM2BIN_startCode, sizeof(ROM2BIN_startCode));
				if( i == 1 )													//First page
					binBuf[0] = 0;												//	destroy "AB" signature so it won't restart ROM on reboot
				if( i < parts )													//Not Last page - return to load
					binBuf[SIZE16K + sizeof(ROM2BIN_startCode) - 9] = 0x00;		//	replace last Z80 command "jp hl" with "nop"
				binBuf[SIZE16K + sizeof(ROM2BIN_startCode) - 21] = 0x40 * i;	//Replace Z80 command "ld de,4000" with "ld de,8000"

				printf("\nSending 16K ROM part %d: Start: %x, End: %x, Run: %x\n", i, start, end, run);
				if( i > 1 && parts > 1 )
				{
					Sleep( 1000 ); // pause after previous 16K part
					if(msxh.Ping())
						printf("Client%s ready\n",s);
					else
					{
						printf("Client%s not ready\n",s);
						return 1;
					}
				}
				if(!msxh.Send( binBuf, start, end ))
				{
					puts("transmit error");
					return 1;
				}
				if(!msxh.Run( run ))
				{
					puts("Run error");
					return 1;
				}
			}
			printf ("\nROM sent done.");
		}
		else if( ch == 0xf0 )		//                      --- file.pkt ---
		{
			binBuf[0] = ch;
			fread( &binBuf[1], stat_p.st_size - 1, 1, infile );
			puts( "Send PKT file:" );
			msxh.write( binBuf,stat_p.st_size );
			puts( "PKT file sent done." );
		}
		else
		{
			printf("\nUnsupported file type - can't send.\n");
		}
		fclose( infile );
	}

	if(command)
	{
		printf("Command: %s\n",command);
		puts(msxh.SendCommand( command )?"Command sent ok":"Command send error");
	}

	if(callRun >= 0)
		puts(msxh.RunBasic((unsigned int)callRun)?"Run sent ok":"Run send error");

	Sleep(1000);

    return 0;
}
