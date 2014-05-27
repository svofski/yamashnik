#include <inttypes.h>
#include <fcntl.h>
#include <termios.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>

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

    /* Make the file descriptor asynchronous (the manual page says only
       O_APPEND and O_NONBLOCK, will work with F_SETFL...) */
    fcntl(m_fd, F_SETFL, FASYNC);

    tcgetattr(m_fd, &oldtio);

    bzero (&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | PARENB | CLOCAL | CREAD;
    newtio.c_iflag = 0;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc [VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc [VMIN]     = 1;   /* blocking read until 1 chars received */

  	// the following 2 lines are ESSENTIAL for the code to work on CYGWIN
    cfsetispeed(&newtio, BAUDRATE);
	cfsetospeed(&newtio, BAUDRATE);

    tcflush(m_fd, TCIFLUSH);
    tcsetattr(m_fd, TCSANOW, &newtio);

    return ERR_NONE;
}

int SerialPort::waitRx()
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
			eggog("Error in select()\n");
		}

		if (readfdSet > 0) {
			if (m_RxListener == 0) {
				morbose("waitRx: m_RxListener==0\n");
				break;
			}
			if (m_RxListener->RxHandler()) break;
		}

		count++;
		usleep(10000);

		if (count == 10) {
			//morbose("\nwaitRx: failed after %d select()s\n", count);
			return 0;
		}
	}
	//morbose("\nwaitRx: got reply after %d select()s\n", count);
	return 1;
}

size_t SerialPort::read(uint8_t* buf, size_t len) const {
	return ::read(m_fd, buf, len);
}

size_t SerialPort::write(uint8_t* buf, size_t len) const {
	return ::write(m_fd, buf, len);
}
