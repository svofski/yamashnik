#ifndef __COMMANDS__
#define __COMMANDS__

typedef enum _PACKETCMD {
	PCMD_BASE = 0x01,
	PCMD_PING = 0x05,
	PCMD_ACK  = 0x06,
	PCMD_PONG = 0x15,

	PCMD_SHEXDATA = 0x42, 
	
	PCMD_SNDCMD = 0x48,

	PCMD_SHEXHEADER = 0x52, 

	PCMD_POKE = 0x5c,


	NET_CLOSE_FILE     = 0x20,
 	NET_WRITE_FILE     = 0x25,
 	NET_CREATE_FILE    = 0x26,
 	NET_MASTER_DATA    = 0x2c,
 	NET_MASTER_DATA2   = 0x2d,

 	RE_NET_CLOSE_FILE	= 0x30,
 	RE_NET_WRITE_FILE	= 0x35,
 	RE_NET_CREATE_FILE	= 0x36,
} PACKETCMD;


#define LAST				0x83
#define INTERMEDIATE 		0x97


#endif

