#pragma once

#include "serial.h"
#include "sendpacket.h"

class BasicSender {
private:
	SerialPort m_SerialPort;
	PacketSender m_packetSender;
	int m_studentNo;

	int netSend(int nfiles, char* file[]);

private:
	int sendROMSection(FILE* file, int sectionSize, int patchAddr, uint8_t patchVal);
	void runROM(uint16_t defusr);
	int sendSHEXHeader(uint16_t start, uint16_t end);
	void sendBlocks(int start, int end);

public:
	BasicSender(const char* port, int studentNo, int nfiles, char* file[]) 
		: m_SerialPort(port),
		  m_packetSender(&m_SerialPort) ,
		  m_studentNo(studentNo)
	{
	    if (m_SerialPort.Setup() != ERR_NONE) {
	       info("BasicSender error: cannot open serial port %s\n", port);
	       return;
	    }

		netSend(nfiles, file);
	}

	BasicSender(SerialPort* serial, int studentNo) 
		: m_SerialPort(0),
		  m_packetSender(serial),
		  m_studentNo(studentNo)
	{}

	int sendBASIC(FILE* file);
	int sendPoke(uint16_t addr, uint8_t value);
	int sendBIN(FILE* file, uint16_t* out_start, uint16_t* out_end, uint16_t* out_run);
	int sendROM(FILE* file);
	int SendCommand(const char* cmd);


};

