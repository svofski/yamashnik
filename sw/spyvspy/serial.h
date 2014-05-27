#pragma once

#include <inttypes.h>
#include <unistd.h>

#include "diags.h"

#define BAUDRATE 		38400
#define SELECT_WAIT		100000

enum _SIO_ERR {ERR_NONE, ERR_NOFILE, ERR_WTF};

class SerialPort;


class SerialListener {
public:
	virtual int RxHandler() = 0;
};

class SerialPort {
private:
	int m_fd;
	SerialListener* m_RxListener;

public:
	SerialPort(const char* device);
	~SerialPort() { 
		if (m_fd != -1) close(m_fd); 
		m_fd = -1; 
	}

	int Setup();

	void SendByte(unsigned char b) const { write(&b, 1); }

	size_t read(uint8_t* buf, size_t len) const;
	size_t write(uint8_t* buf, size_t len) const;

	void SetRxListener(SerialListener* listener) { 
		m_RxListener = listener;
	};

	int waitRx();
};