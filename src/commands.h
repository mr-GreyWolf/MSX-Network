#ifndef __COMMANDS__
#define __COMMANDS__

#define BASE		0x01
#define PING		0x05
#define ACK			0x06
#define PONG		0x15

#define NET_CLOSE_FILE      0x20
#define NET_WRITE_FILE      0x25
#define NET_CREATE_FILE     0x26
#define NET_MASTER_DATA     0x2c
#define NET_MASTER_DATA2    0x2d

#define RE_NET_CLOSE_FILE	0x30
#define RE_NET_WRITE_FILE	0x35
#define RE_NET_CREATE_FILE	0x36

#define SEND_BAS_HEAD		0x40
#define SEND_BAS_NEXT		0x41
#define SEND_HEX			0x42
#define SEND_FILE			0x4B

#define LAST			0x83
#define INTERMEDIATE 	0x97

#endif

