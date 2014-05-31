#include <inttypes.h>
#include <fcntl.h>
#include <termios.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>
#include <errno.h>
#include <stdio.h>


#include "serial.h"
#include "diags.h"

SerialPort::SerialPort(const char* device) 
{
	m_fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
}

int SerialPort::Setup()
{
	struct termios oldtio, newtio;

	if (m_fd == -1) return ERR_NOFILE;
	
	// drop O_NONBLOCK, become blocking after having opened
    //fcntl(m_fd, F_SETFL, 0);

    tcgetattr(m_fd, &oldtio);

    bzero (&newtio, sizeof(newtio));
    cfmakeraw(&newtio);

    newtio.c_cflag = BAUDRATE | CS8 | PARENB | CLOCAL | CREAD;// | CSTOPB;
    newtio.c_iflag = 0;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc [VTIME]    = 0;   /* inter-character timer */
    newtio.c_cc [VMIN]     = 1;   /* blocking read until 1 chars received */

  	// the following 2 lines are ESSENTIAL for the code to work on CYGWIN
    cfsetispeed(&newtio, BAUDRATE);
	cfsetospeed(&newtio, BAUDRATE);

    tcflush(m_fd, TCIFLUSH);
    tcsetattr(m_fd, TCSANOW, &newtio);

    return ERR_NONE;
}

int SerialPort::waitRx(const int intents)
{
	int readfdSet, count = 0;
	fd_set fdSet;
	struct timeval timeout;

	while(1)
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = SELECT_WAIT;

		FD_ZERO(&fdSet);
		FD_SET(m_fd, &fdSet);

		readfdSet = select(m_fd + 1,
						&fdSet,
						(fd_set *) 0,
						(fd_set *) 0,
						&timeout);

		if (readfdSet < 0) {
			info("Error in select(): %s\n", strerror(errno));
		}

		//info("$ %08x ", readfdSet);

		if (readfdSet > 0) {
			if (m_RxListener == 0) {
				morbose("waitRx: m_RxListener==0\n");
				break;
			}
			if (m_RxListener->RxHandler()) break;
		}
		if (++count == intents) {
			return 0;
		}
	}
	return 1;
}

size_t SerialPort::read(uint8_t* buf, size_t len) const {
	return ::read(m_fd, buf, len);
}

size_t SerialPort::write(uint8_t* buf, size_t len) const {
	return ::write(m_fd, buf, len);
}
