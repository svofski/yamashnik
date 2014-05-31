#pragma once

#include <inttypes.h>
#include <strings.h>
#include <ctype.h>
#include "serial.h"
#include "commands.h"
#include "diags.h"

#include <stdlib.h>

typedef struct _xdata {
    uint8_t H;
    uint8_t F;
    uint8_t A;
    uint8_t FCB[37];
    _xdata() {}

    _xdata(_xdata& origin) : H(origin.H), F(origin.F), A(origin.A) 
    {
    	memcpy(&FCB, &origin.FCB, sizeof(FCB));
    }

    _xdata(uint8_t _H, uint8_t _F, uint8_t _A, const char* fileName) : H(_H), F(_F), A(_A)
    {
	    FCB[0] = 8;
    	memset(&FCB[1], ' ', 11);

    	int i, fi, flen;
	    // Let's copy the filename
	    for (i = 0, fi = 1, flen = strlen(fileName); i < 8 && i < flen && fileName[i] != '.'; i++, fi++) {
	        FCB[fi] = toupper(fileName[i]);
	    }
	    if (fileName[i] == '.') i++;
	    fi = 1 + 8;

	    for(; fi < 12 && i < flen; i++, fi++) {
	    	FCB[fi] = toupper(fileName[i]);
	    }
    	memset(&FCB[12], 0, sizeof(FCB) - 12);
    }
} NetFCB;

class PacketUtil {
private:
	virtual ~PacketUtil() = 0;
public:
	static uint8_t ReadEscapedByte (uint8_t *p);
	static uint16_t ReadEscapedWord (uint8_t *p);
};

class GenericPacket {
private:
	int m_Cmd;
	int m_Length;
	int m_SrcAddr;
	int m_DstAddr;
	uint8_t* m_OwnData;
	const uint8_t* m_ExtData;
	uint16_t m_Addr1;
	uint16_t m_Addr2;

protected:
	GenericPacket(int srcAddr, int dstAddr, int cmdType, int length) 
		: m_Cmd(cmdType), m_Length(length), 
		  m_SrcAddr(srcAddr), m_DstAddr(dstAddr), 
		  m_OwnData(new uint8_t[length]),
		  m_ExtData(0),
		  m_Addr1(0),
		  m_Addr2(0)
	{}	

	GenericPacket(int srcAddr, int dstAddr, int cmdType, const uint8_t* data, int length) 
		: m_Cmd(cmdType), m_Length(length), 
		  m_SrcAddr(srcAddr), m_DstAddr(dstAddr), 
		  m_OwnData(0),
		  m_ExtData(data),
		  m_Addr1(0),
		  m_Addr2(0)
	{}	

	GenericPacket(int srcAddr, int dstAddr, int cmdType, uint16_t addr1, uint16_t addr2)
		: m_Cmd(cmdType), m_Length(0), 
		  m_SrcAddr(srcAddr), m_DstAddr(dstAddr), 
		  m_OwnData(0),
		  m_ExtData(0),
		  m_Addr1(addr1),
		  m_Addr2(addr2)
	{}

	void AssignData(const uint8_t* data) {
		if (m_OwnData) memcpy(m_OwnData, data, m_Length);
	}

public:
	~GenericPacket() {
		if (m_OwnData) delete[] m_OwnData;
	}

	int GetSrcAddr() const { return m_SrcAddr; }
	int GetDstAddr() const { return m_DstAddr; }
	int GetCmd() const { return m_Cmd; }
	int GetLength() const { return m_Length; }
	uint16_t GetAddr1() const { return m_Addr1; }
	uint16_t GetAddr2() const { return m_Addr2; }
	virtual int GetIsLast() const { return 1; }
	virtual const uint8_t* GetData() const { return m_OwnData ? m_OwnData : m_ExtData; }
};

class PingPacket : public GenericPacket 
{
public:
	PingPacket(int srcAddr, int dstAddr) : GenericPacket(srcAddr, dstAddr, PCMD_PING, 0) {}
};

class SNDCMDPacket : public GenericPacket 
{
public:
	SNDCMDPacket(int srcAddr, int dstAddr, const char* cmd) 
		: GenericPacket(srcAddr, dstAddr, PCMD_SNDCMD, (int) strlen(cmd)) 
	{
		AssignData((uint8_t *) cmd);
	}
};

class NetFCBPacket : public GenericPacket
{
protected:
	NetFCBPacket(int srcAddr, int dstAddr, PACKETCMD cmd, const char* fileName)
		: GenericPacket(srcAddr, dstAddr, cmd, sizeof(NetFCB))
	{
		NetFCB fcb(0,0,0, fileName);
		AssignData((const uint8_t*) &fcb);
	}
public:
	NetFCBPacket(NetFCBPacket& otro, PACKETCMD cmd)
		: GenericPacket(otro.GetSrcAddr(), otro.GetDstAddr(), cmd, sizeof(NetFCB)) 
	{
		AssignData((uint8_t *) otro.GetData());
	}

	NetFCBPacket(int srcAddr, int dstAddr, PACKETCMD cmd, const NetFCB* FCB)
		: GenericPacket(srcAddr, dstAddr, cmd, sizeof(NetFCB))
	{
		AssignData((uint8_t *) FCB);
	}


public:
	NetFCB* GetFCB() const { return (NetFCB*) GetData(); }
};

class NetCreateFilePacket : public NetFCBPacket
{
public:
	NetCreateFilePacket(int srcAddr, int dstAddr, const char* fileName)
		: NetFCBPacket(srcAddr, dstAddr, NET_CREATE_FILE, fileName)
	{}
};

class NetWriteFilePacket : public NetFCBPacket
{
public:
	NetWriteFilePacket(int srcAddr, int dstAddr, const NetFCB* FCB)
		: NetFCBPacket(srcAddr, dstAddr, NET_WRITE_FILE, FCB)
	{}
};

class NetCloseFilePacket : public NetFCBPacket
{
public:
	NetCloseFilePacket(int srcAddr, int dstAddr, const NetFCB* FCB)
		: NetFCBPacket(srcAddr, dstAddr, NET_CLOSE_FILE, FCB)
	{}
};

class DataPacket : public GenericPacket 
{
protected:
	DataPacket(int srcAddr, int dstAddr, PACKETCMD cmd, const uint8_t* data, int length) 
		: GenericPacket(srcAddr, dstAddr, cmd, data, length)
	{}
};

class NetMasterDataPacket : public DataPacket
{
public:
	NetMasterDataPacket(int srcAddr, int dstAddr, const uint8_t* data, int length)
		: DataPacket(srcAddr, dstAddr, NET_MASTER_DATA, data, length)
	{}
};

class SHEXHeaderPacket : public GenericPacket
{
public:
	SHEXHeaderPacket(int srcAddr, int dstAddr, uint16_t startAddr, uint16_t endAddr)
		: GenericPacket(srcAddr, dstAddr, PCMD_SHEXHEADER, startAddr, endAddr)
	{}
};

// SHEX data packet type 0x42
// LENGTH|2, payload (max 52 bytes), CHECKSUM|2, 0x97/0x83
class SHEXDataPacket : public DataPacket
{
private:
	int m_Last;

public:
	SHEXDataPacket(int srcAddr, int dstAddr, const uint8_t* data, int length, int last)
		: DataPacket(srcAddr, dstAddr, PCMD_SHEXDATA, data, length), m_Last(last)
	{}
protected:
	int GetIsLast() const { return m_Last; }
};

class PokePacket : public GenericPacket
{
public:
	PokePacket(int srcAddr, int dstAddr, uint16_t address, uint8_t value)
		: GenericPacket(srcAddr, dstAddr, PCMD_POKE, address, value) 
	{}
};


class PacketSender : public SerialListener {
private:
	SerialPort* serial;
	unsigned char buf[1024];
	int pos;
	NetFCB m_RxData;

private:
	int RxHandler();
	void GetRxData(uint8_t *p);
	void CheckPacket();
public:
	PacketSender(SerialPort* p) { 
		serial = p; 
	    serial->SetRxListener(this);
	}
	void SendByte(uint8_t b) const { serial->SendByte(b); }
	void SendEscapedByte(uint8_t b) const;
	void SendEscapedWord(uint16_t w) const;
	void SendHeader(const uint8_t* h) const;
	void SendEscapedBlockWithChecksum(const uint8_t* buf, int len);
	void SendPacket(int srcAddr, int dstAddr, int cmdType, const uint8_t* buf, uint16_t len, uint16_t addr1, uint16_t addr2, int last);
	void SendPacketVal(GenericPacket&);
	int ReceivePacket();
	const NetFCB* GetNetFCB() const { return &m_RxData; }
};
