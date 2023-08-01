#include <stdlib.h>
#include <stdio.h>
#include <glob.h>
#include <strings.h>
#include <sys/stat.h>

#ifndef __APPLE__
#include <linux/limits.h>
#define GLOB_MAXPATH PATH_MAX
#endif

#include "spy.h"
#include "diags.h"
#include "basicsend.h"
#include "spybdos.h"

const char* RAMIMAGE = "memory.ram";
const char* BDOS = "spy_bdos_e900.ram";
const char* STRAPON = "spy_bootstrap.bin";

void Spy::extractExePath() {
	struct stat pe;
	char path[GLOB_MAXPATH];
	const char *fullPath = 0;

	if (stat(m_argv0, &pe) == 0) {
		// easy: the path is given
		fullPath = m_argv0;
	} else {
		// cannot stat
		
		char arg[GLOB_MAXPATH];

		if (strlen(m_argv0) > 1000) eggog("I have a strange executable name %s\n");

		sprintf(arg, "which \"%s\"", m_argv0);

		FILE* which = popen(arg, "r");
		if (which != 0) {
			fread(path, 1, GLOB_MAXPATH-1, which);
			fclose(which);

			fullPath = path;
		} 
		else {
			eggog("Cannot obtain path to executable (2)\n");
		}
	}

	const char* pathend = strrchr(fullPath, '/');
	int pathlen = pathend - fullPath;
	m_ExePath = new char[pathlen + 1];
	strncpy(m_ExePath, fullPath, pathlen);
	m_ExePath[pathlen] = 0;
}

int Spy::loadFile(const char* filename, uint8_t** buf, int expectedSize) 
{
	char path[strlen(m_ExePath) + strlen(filename) + 2];
	
	if (filename[0] == '/' || filename[0] == '.') {
		sprintf(path, "%s", filename);	
	} else {
		sprintf(path, "%s/%s", m_ExePath, filename);
	}

	if (expectedSize == -1) {
		struct stat pe;		
		if (stat(path, &pe) == 0) {
			expectedSize = (int) pe.st_size;
			if (expectedSize > 0x8000) expectedSize = 0x8000;
		}
	}

	morbose("loadFile: filename=%s expectedSize=%d\n", filename, expectedSize);

	FILE* file = fopen(path, "rb");
	if (file == 0) {
		info("Cannot open file %s\n", path);
		return 0;
	}

	*buf = new uint8_t[expectedSize];

	int size = fread(*buf, 1, expectedSize, file);
	fclose(file);

	morbose("loadFile: bytes read=%d\n", size);

	return size;
}

int Spy::initData()
{
	extractExePath();

	int result = loadFile(RAMIMAGE, &m_MSXRAM, 0200000);
	morbose("result=%d BDOS starts at %02x%02x\n", result, m_MSXRAM[7], m_MSXRAM[6]);

	if (result) {
		m_BDOSSize = loadFile(BDOS, &m_BDOS, -1);
		morbose("result=%d NetBDOS starts with %02x size=%d\n", result, m_BDOS[0], m_BDOSSize);
		result = result && m_BDOSSize;
	}
	return result;
}

int Spy::Bootstrap()
{
    if (m_serial.Setup() != ERR_NONE) {
       info("Spy error: cannot open serial port\n");
       return 0;
    }

    info("Sending bootstrap code to workstation %d\n", m_studentNo);

    uint16_t bootStart, bootEnd;

	// Phase 1: send and run bootstrap code
	{
		char path[strlen(m_ExePath) + strlen(STRAPON) + 2];
		sprintf(path, "%s/%s", m_ExePath, STRAPON);

		FILE* file = fopen(path, "rb");
		uint8_t sig;
		if (file == 0 || fread(&sig, 1, 1, file) != 1) {
			info("Cannot read %s\n", path);
			return 0;
		}
		if (sig != 0xfe) {
			info("Not a BIN file %s\n", path);
			return 0;
		}

		BasicSender basicSender(&m_serial, m_studentNo);

		if (!basicSender.SendCommand("    WIDTH 80:COLOR9,1")) {
			info("Could not send command\n");
			return 0;
		}

		sleep(2);

		if (!basicSender.sendBIN(file, &bootStart, &bootEnd, 0)) {
			info("Bootstrap code upload error\n");
			return 0;
		}
	}

	info("Waiting for remote party...");
	sleep(1);
	info("OK\n");

	// Phase 2: upload memories
	SpyTransport transport(&m_serial);
	{
		info("Uploading zero page\n");
		// Zero page
		transport.SendMemory(m_MSXRAM + 0x0000, 0x0000, 256);

		// MSX-DOS BDOS area .. 0xED00
		uint16_t start = m_MSXRAM[6] | (m_MSXRAM[7] << 8);
		int length = bootStart - start;

		// patch the stupid loaded flag in MSXDOS+1A
		m_MSXRAM[(start & 0xff00) + 0x1A] = 0;

		if (length > 0) {
			info("Uploading memory area %04x-%04x\n", start, start + length);
			transport.SendMemory(m_MSXRAM + start, start, length);

			usleep(20000);

			// 0xED00 + sizeof(Bootstrap) .. EndOfWorkArea
			start = bootEnd;
		} 
		length = 0xf380 - start;

		info("Uploading memory area %04x-%04x\n", start, start + length);
		transport.SendMemory(m_MSXRAM + start, start, length);

		usleep(20000);

		// Net BDOS
		info("Uploading NetBDOS area %04x-%04x\n", 0xe900, 0xe900 + m_BDOSSize);
		transport.SendMemory(m_BDOS, 0xe900, m_BDOSSize);
	}

	// Phase 3: upload file to TPA if specified
	if (m_nfiles > 0) {
		int comsize;
		info("Uploading TPA file %s\n", m_file[0]);
		if ((comsize = loadFile(m_file[0], &m_COMFile, -1)) != 0) {
			transport.SendMemory(m_COMFile, 0x100, comsize);
		}
	}

	info("Launching remote workstation\n");

	// Phase 3: Launch remote station
	transport.SendCommand(2);

	sleep(1);

	return Serve();
}

int Spy::Serve() {
    if (m_serial.Setup() != ERR_NONE) {
       info("Spy error: cannot open serial port\n");
       return 0;
    }

	SpyTransport transport(&m_serial);

	info("\nServing workstation %d\n", m_studentNo);

	NetBDOS bdos;
	SpyRequest request;

	for(int i = 0;; i++) {
		if (!transport.Poll(m_studentNo)) {
			info((i & 0) == 0 ? "'\010" : "`\010");
			continue;
		}

		if (!transport.ReceiveRequest(&request)) {
			info("Workstation did not send request, fall back to poll mode.\n");
			continue;
		}

		{
			SpyResponse response;
			bdos.ExecFunc(&request, &response);

			transport.TransmitResponse(&response);
		}
	}

	return 1;
}


// ---------------- TRANSPORT
void SpyTransport::SendByte(uint8_t b, printfunc p) const
{
	if (p) p("%02x ", b);
	m_serial->SendByte(b);
	// this value seems to be critical, even though it isn't immediately noticeable
	// example: zork would not be able to read zork1.dat
	// BIOS routines seem to work without artificial pace though. Probably only very long packets fail.
	usleep(150);
}


// Spy uses big endian for some reason
void SpyTransport::SendWord(uint16_t w, printfunc p) const
{
	SendByte((w >> 8) & 0377, p);
	SendByte(w & 0377, p);
}

void SpyTransport::SendChunk(uint8_t* data, int length, printfunc p) const 
{
	for (int i = 0; i < length; i++) {
		SendByte(data[i], p);
	}
}

void SpyTransport::SendMemory(uint8_t* data, uint16_t addrDst, int length) const
{
	SendByte(1);
	SendWord(addrDst);
	SendWord(length);
	SendChunk(data, length);
}

int SpyTransport::RxHandler()
{
	int n;
	int result = 0;

	//morbose("RxHandler: m_state=%d\n", m_state);
	switch(m_state) {
	case SPY_BOOTSTRAP:
		n = m_serial->read(m_rxbuf, 1024);
		verbose("RxHandler: ignored %d bytes", n);
		result = 1;
		break;
	case SPY_POLL:
		n = m_serial->read(m_rxbuf, 1);
		result = (n == 1) && (m_rxbuf[0] == 0xff || m_rxbuf[0] == 0x7f);
		break;
	case SPY_GETFUNC:
		n = m_serial->read(m_rxbuf, 1);
		m_func = m_rxbuf[0];
		result = (n == 1);
		break;
	case SPY_RXDATA:
		// read next available portion
		n = m_serial->read(&m_rxbuf[m_rxpos], 1024);
		//morbose("\nSPY_RXDATA: read n=%d:", n);
		//for (int i = 0; i < n; i++) {
		//	morbose("%02x ", m_rxbuf[m_rxpos + i]);
		//}
		//morbose("\n");
		m_rxpos += n;
		// consume data: eat will return 1 when receiver state will have reached end of expected data
		result = m_rq->consume(m_rxbuf, m_rxpos);
		//morbose("m_rq.consume() returned %d\n", result);
		break;

	default:
		eggog("unknown state %d\n", m_state);
		break;
	}

//	morbose("\n   RxHandler: m_rxpos=%d n=%d result=%d\n", m_rxpos, n, result);
//	dump(morbose, m_rxbuf, m_rxpos);
//	morbose("\n");

	return result;
}

int SpyTransport::Poll(uint8_t studentNo) 
{
	m_state = SPY_POLL;

	m_serial->SendByte(studentNo);
	m_serial->SendByte(0xF4);
	return m_serial->waitRx();
}

static constexpr uint8_t EXP_F0E_SELECT_DISK[] {REQ_BYTE, 0};
static constexpr uint8_t EXP_F0F_OPEN_FILE[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F10_CLOSE_FILE[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F11_SEARCH_FIRST[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F12_SEARCH_NEXT[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F13_DELETE[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F14_SEQ_READ[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F15_SEQ_WRITE[] {REQ_FCB, REQ_DMA, 0};
static constexpr uint8_t EXP_F16_CREAT[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F17_RENAME[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F18_GETLOGINVECTOR[] {0};
static constexpr uint8_t EXP_F19_GET_CURRENT_DRIVE[] {0};
static constexpr uint8_t EXP_F1B_GET_ALLOC_INFO[] {REQ_BYTE, 0};
static constexpr uint8_t EXP_F21_RANDOM_READ[] {REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_FCB, 0};
static constexpr uint8_t EXP_F22_RANDOM_WRITE[] {REQ_FCB, REQ_DMA, 0};
static constexpr uint8_t EXP_F23_GET_FILE_SIZE[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F24_SET_RANDOM_RECORD[] {REQ_FCB, 0};
static constexpr uint8_t EXP_F26_RANDOM_BLOCK_WRITE[] {REQ_WORD, REQ_DMA, REQ_FCB, 0};
static constexpr uint8_t EXP_F27_RANDOM_BLOCK_READ[] {REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_WORD, REQ_FCB, 0};
static constexpr uint8_t EXP_F28_RANDOM_WRITE_ZERO[] {REQ_DMA, REQ_FCB, 0};
static constexpr uint8_t EXP_F2F_ABS_SECTOR_READ[] {REQ_WORD, REQ_BYTE, REQ_BYTE, 0};
static constexpr uint8_t EXP_F30_ABS_SECTOR_WRITE[] {REQ_WORD, REQ_BYTE, REQ_BYTE, REQ_DMA, 0};
static constexpr uint8_t EXP_NULL[] {0};

int SpyTransport::ReceiveRequest(SpyRequest* request)
{
	m_rq = request;
	m_state = SPY_GETFUNC;
	if (!m_serial->waitRx()) {
		info("Workstation replied but did not send request\n");
		return 0;
	}

	m_rq->setFunc(m_func);
	switch (m_func) {
	case F0E_SELECT_DISK:		// Rx: byte<disk> Tx: nothing
		{m_rq->expect(EXP_F0E_SELECT_DISK);}
		break;
	case F0F_OPEN_FILE:			// Rx: FCB<>, Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F0F_OPEN_FILE);}
		break;
	case F10_CLOSE_FILE:		// Rx: FCB<>, Tx: byte<result>
		{m_rq->expect(EXP_F10_CLOSE_FILE);}
		break;
	case F11_SEARCH_FIRST:		// Rx: FCB<>, Tx: DMA<>, byte<result>
		{m_rq->expect(EXP_F11_SEARCH_FIRST);}
		break;
	case F12_SEARCH_NEXT: 		// Rx: FCB<>, Tx: DMA<>, byte<result>
		{m_rq->expect(EXP_F12_SEARCH_NEXT);}
		break;
	case F13_DELETE:			// Rx: FCB<>, Tx: byte<result>
		{m_rq->expect(EXP_F13_DELETE);}
		break;
	case F14_SEQ_READ:			// Rx: FCB<>, Tx: DMA<>, FCB<>, byte<result>
		{m_rq->expect(EXP_F14_SEQ_READ);}
		break;
	case F15_SEQ_WRITE: 		// Rx: FCB<>, DMA<>  Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F15_SEQ_WRITE);}
		break;
	case F16_CREAT:				// Rx: FCB<>, Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F16_CREAT);}
		break;
	case F17_RENAME:			// Rx: FCB<>, Tx: byte<result>
		{m_rq->expect(EXP_F17_RENAME);}
		break;
	case F18_GETLOGINVECTOR:	// Rx: nothing Tx: word<?>
		{m_rq->expect(EXP_F18_GETLOGINVECTOR);}
		break;
	case F19_GET_CURRENT_DRIVE:	// Rx: 		  		Tx: byte<disk> 
		{m_rq->expect(EXP_F19_GET_CURRENT_DRIVE);}
		break;
	case F1B_GET_ALLOC_INFO: 	// Rx: byte<drive>
								// Tx: DPB<> (0xF3 bytes)
								//     byte<0xDF>, byte<0x94> ?
								// 	   FAT<> - 0x600 bytes of FAT
								// 	   word<sector size> = 512
								//	   word<total clusters> = ?
								//	   word<free clusters> = ?
								//	   byte<result>
		{m_rq->expect(EXP_F1B_GET_ALLOC_INFO);}
		break;

	case F21_RANDOM_READ:		// Rx: FCB<>, 		Tx: DMA<>, FCB<>, byte<result>
		{m_rq->expect(EXP_F21_RANDOM_READ);}
		break;
	case F22_RANDOM_WRITE:		// Rx: FCB<>, DMA<>	Tx: byte<result>
		{m_rq->expect(EXP_F22_RANDOM_WRITE);}
		break;
	case F23_GET_FILE_SIZE:		// Rx: FCB<>, 		Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F23_GET_FILE_SIZE);}
		break;
	case F24_SET_RANDOM_RECORD:	// Rx: FCB<>, 		Tx: FCB<>
		{m_rq->expect(EXP_F24_SET_RANDOM_RECORD);}
		break;
	case F26_RANDOM_BLOCK_WRITE:// Rx: word<size>, DMA<>, FCB<>
								// Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F26_RANDOM_BLOCK_WRITE);}
		break;


	case F27_RANDOM_BLOCK_READ:	// Rx: word<size>, FCB<>
								// Tx: word<bytesread>, DMA<>, FCB<>, byte<result>
		{m_rq->expect(EXP_F27_RANDOM_BLOCK_READ);}
		break;
	case F28_RANDOM_WRITE_ZERO:	// Rx: FCB<>, DMA<>	Tx: FCB<>, byte<result>
		{m_rq->expect(EXP_F28_RANDOM_WRITE_ZERO);}
		break;
	case F2F_ABS_SECTOR_READ:	// Rx: trash byte
								//     word<sector number>
								//     byte<number of sectors to read>
								//	   byte<drive num 0=A>
								// Tx: DMA<>
		{m_rq->expect(EXP_F2F_ABS_SECTOR_READ);}
		break;
	case F30_ABS_SECTOR_WRITE:	// Rx: word<sector number>
								// 	   byte<number of sectors to write>
								//	   byte<drive num 0=A>
								// 	   DMA<>
								// Tx: nothing
		{m_rq->expect(EXP_F30_ABS_SECTOR_WRITE);}
		break;
	default:
		{m_rq->expect(EXP_NULL);}
		break;
	}

	verbose("Received request function %d (BDOS %02xh): ", m_func, m_func + 14); 
	if (m_rq->NeedsData()) {
		verbose("getting request data: ");
		m_state = SPY_RXDATA;
		m_rxpos = 0;

		while (!m_serial->waitRx());

		verbose("ok\n");
	} else {
		verbose("no data expected\n");
	}

	return 1;
}

int SpyTransport::TransmitResponse(SpyResponse* r) {
	r->emit(this);
	return 1;
}

