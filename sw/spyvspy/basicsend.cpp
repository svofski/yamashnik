// Non-CPM file send commands: BASIC, ROM, BIN
// Stuff formerly implemented in send.c

#include <stdio.h>
#include "diags.h"
#include "serial.h"
#include "sendpacket.h"
#include "ncopy.h"
#include "basicsend.h"




/************************************************************************
 * ROM images
 ************************************************************************/
const int SIZE32K = 32*1024;
const int SIZE16K = 16*1024;
const int MAXBLKSIZE = 56;

const uint8_t ROM2BIN_startCode[] =
{
/*
    commented because this code section is needed only for
    disk drive switching off, but we run on a diskless MSX

    0xFB,               // ei
    0x76,               // halt
    0x10,0xFD,          // djnz [go to halt]
    0x3E,0xC9,          // ld a,C9
    0x32,0x9F,0xFD,     // ld (FD9F),a
*/
    // this code switches RAM page on address 0x4000,
    // copies 16K from 0x9000 to 0x4000
    // (or to 0x8000 when "ld de,4000" patched to be "ld de,8000")
    // and then execues ROM code
    // (or returns to Basic if "jp hl" is patched to be "nop")

    0xCD,0x38,0x01,     // call 0138
    0xE6,0x30,          // and 30
    0x0F,               // rrca
    0x0F,               // rrca
    0x0F,               // rrca
    0x0F,               // rrca
    0x4F,               // ld c,a
    0x06,0x00,          // ld b,00
    0x21,0xC5,0xFC,     // ld hl,FCC5
    0x09,               // add hl,bc
    0x7E,               // ld a,(hl)
    0xE6,0x30,          // and 30
    0x0F,               // rrca
    0x0F,               // rrca
    0xB1,               // or c
    0xF6,0x80,          // or 80
    0x26,0x40,          // ld h,40
    0xCD,0x24,0x00,     // call 0024
    0xF3,               // di
    0x11,0x00,0x40,     // ld de,4000 (0x40 will be patched to 0x80)
    0x21,0x00,0x90,     // ld hl,9000
    0x01,0x00,0x40,     // ld bc,4000
    0xED,0xB0,          // ldir
    0x2A,0x02,0x40,     // ld hl,(4002)
    0xE9,               // jp hl (can be patched to reach next command)

    0x3E, 0x80,         // ld a,80
    0x26, 0x40,         // ld h,40
    0xCD,0x24,0x00,     // call 0024
    0xC9                // ret
};

unsigned char binBuf[SIZE32K + sizeof(ROM2BIN_startCode)];  // enough for any bloadable binary

void BasicSender::sendBlocks(int start, int end)
{
	static const char progressChar[] = "...--oo*OO0";
    int lastblock = (end - start) / MAXBLKSIZE;

    info("%d blocks to send\n", lastblock);

    int current = start;
    int binBufOffset = 0;

    for (int i = 0; i < lastblock; i++) {
        verbose("\nSending block %d (", i + 1);

        if ((i+1) % 10 == 0) {
            info("%3d%c", i + 1, ((i+1) % 100 == 0) ? '\n' : ' ');
        } 
        else {
            info("%c\010", progressChar[i % 10]);  //_,-'"
        }

        verbose(")");

        {   
            SHEXDataPacket shexdata(0, m_studentNo, &binBuf[binBufOffset], MAXBLKSIZE, 0);
            m_packetSender.SendPacketVal(shexdata);
            if (!m_packetSender.ReceivePacket()) {
            	eggog("send: ack timeout on SHEX data\n");
            }
        }
        current += MAXBLKSIZE;
        binBufOffset += MAXBLKSIZE;
    }

    // Calculate the rest
    verbose("\nLast block: %d bytes\n", end - current + 1);
    {
        SHEXDataPacket shexdata(0, m_studentNo, &binBuf[binBufOffset], end - current + 1, 1);
        m_packetSender.SendPacketVal(shexdata);
        m_packetSender.ReceivePacket();
    }

    info("\n");
}

int BasicSender::sendSHEXHeader(uint16_t start, uint16_t end) 
{
    if (!pingPong(m_packetSender, m_studentNo)) {
    	info("sendSHEXHeader: no PONG from workstation %d\n", m_studentNo);
    	return 0;
    }

    verbose("Sending SHEX header packet to %d start=%04x end=%04x\n", m_studentNo, start, end);

    SHEXHeaderPacket shex(0, m_studentNo, start, end);
    m_packetSender.SendPacketVal(shex);
    if (!m_packetSender.ReceivePacket()) {
    	info("no ack on SHEX header from %d\n", m_studentNo);
    	return 0;
    }

    morbose("SHEX header acknowledged\n");
    return 1;
}

int BasicSender::SendCommand(const char* cmd)
{
    SNDCMDPacket cmdPacket(0, m_studentNo, cmd);
    m_packetSender.SendPacketVal(cmdPacket);
    if (!m_packetSender.ReceivePacket()) {
        info("Warning: ack timeout after run SNDCMD\n");
        return 0;
    }

    return 1;
}

void BasicSender::runROM(uint16_t defusr)
{
    char command[40];
    sprintf(command, " _nete:DefUsr=&H%.4x:?Usr(0):_neti", defusr);
    
    info("Starting up ROM\n");
    verbose("Run command: '%s'\n", command);

    if (!pingPong(m_packetSender, m_studentNo)) {
    	eggog("runROM: no PONG reply from %d\n", m_studentNo);
    }

    SNDCMDPacket cmd(0, m_studentNo, command);
    m_packetSender.SendPacketVal(cmd);
    if (!m_packetSender.ReceivePacket()) {
    	info("Warning: ack timeout after run SNDCMD\n");
    }
}

int BasicSender::sendROMSection(FILE* file, int sectionSize, int patchAddr, uint8_t patchVal) 
{
    uint16_t start = 0x9000;
    uint16_t end = start + sectionSize + sizeof(ROM2BIN_startCode) - 1;
    uint16_t run = start + sectionSize;

    fread(&binBuf, sectionSize, 1, file);
    memcpy(&binBuf[sectionSize], ROM2BIN_startCode, sizeof(ROM2BIN_startCode));

    if (patchAddr >= 0) {
    	binBuf[patchAddr] = patchVal; 	// replace last Z80 command "jp hl" with "nop"
    }

    binBuf[0] = 0; 						// destroy "AB" signature so it won't reboot

    info("Send ROM section: Start: %x, End: %x, Run: %x ", start, end, run);

    if (sendSHEXHeader(start, end)) {
	    sendBlocks(start, end);
	    usleep(10000);
	    runROM(run);
	    usleep(500000);	
	} else {
		info("unable to initiate transfer\n");
		return 0;
	}

	return 1;
}

int BasicSender::sendROM(FILE* file)
{
    long romSize;
    int result = 0;

    if (fseek(file, 0, SEEK_END) == 0) {
        romSize = ftell(file);
    } 
    else {
        info("Error: sendROM: cannot get file size\n");
        return 0;
    }

    info("ROM file, %ld bytes\n", romSize);

    if (romSize > SIZE32K) {
        info("Sending ROMs bigger than 32K is not supported\n");
        return 0;
    }
    if (romSize != 8*1024 && romSize != SIZE16K && romSize != SIZE32K) {
        info("Sending ROMs with non-standard size (not 8, 16 or 32K) is not supported\n");
        return 0;
    }

  	rewind(file);
    if (romSize < SIZE32K) {
    	result = sendROMSection(file, romSize, -1, 0);
    }
    else {
        // replace last Z80 command "jp hl" with "nop"
        result = sendROMSection(file, SIZE16K, SIZE16K + sizeof(ROM2BIN_startCode) - 9, 0);
        
        if (result) {
	        // patch: replace Z80 command "ld de,4000" with "ld de,8000"
	        result = sendROMSection(file, SIZE16K, SIZE16K + sizeof(ROM2BIN_startCode) - 21, 0x80);
	    }
    }

    info("Sending ROM done.\n");

    return result;
}

int BasicSender::sendBIN(FILE* file, uint16_t* out_start, uint16_t* out_end, uint16_t* out_run)
{
	uint16_t start;
	uint16_t end;
	uint16_t run;
	int result = 0;

	fread (&start, 2, 1, file);
	fread (&end, 2, 1,   file);
	fread (&run, 2, 1,   file);

	fread (&binBuf, end-start+1, 1, file);

    info("Start: %x, End: %x, Run: %x\n", start, end, run);

    if (sendSHEXHeader(start, end)) {
	    sendBlocks(start, end);
	    usleep(10000);
	    runROM(run);
	    usleep(500000);	
	    result = 1;

        if (out_start) *out_start = start;
        if (out_end) *out_end = end;
        if (out_run) *out_run = run;
	} else {
		info("unable to initiate SHEX transfer\n");
		result = 0;
	}

    return result;	
}

int BasicSender::sendPoke(uint16_t addr, uint8_t value)
{
    PokePacket pokor(0, m_studentNo, addr, value);
	m_packetSender.SendPacketVal(pokor);
	if (!m_packetSender.ReceivePacket()) {
		info("Timeout in poke(%04x,%02x) to workstation %d\n", addr, value, m_studentNo);
		return 0;
	}
	return 1;
}

// -----------------------------------------------------
//
//                      BASIC
//
// -----------------------------------------------------
int BasicSender::sendBASIC(FILE* file)
{
	long basSize;
	int result = 0;

    if (fseek(file, 0, SEEK_END) == 0) {
        basSize = ftell(file);
    } 
    else {
        info("Error: sendBASIC: cannot get file size\n");
        return 0;
    }

    fseek(file, 1, SEEK_SET);
	fread(&binBuf, basSize-1, 1, file);

	uint16_t start = 0x8001;
	uint16_t end = start + basSize - 1;
    info("Tokenized BASIC file, %ld bytes; Start: %x, End: %x\n", basSize, start, end);

    if (sendSHEXHeader(start, end)) {
	    sendBlocks(start, end);
	    usleep(10000);
	    result = 1;
	} else {
		info("sendROMSection: unable to initiate transfer\n");
		result = 0;
	}

	result = result && sendPoke(0xF6C2, end % 0x100);
	result = result && sendPoke(0xF6C3, end / 0x100);

	if (result) info("Sending BASIC file done\n");

	return result;
}

int BasicSender::netSend(int nfiles, char* file[])
{
    for (int fileIdx = 0; fileIdx < nfiles; fileIdx++) {
        uint8_t magic;

        FILE* infile = fopen(file[fileIdx], "rb");
        if (infile == 0) {
            eggog("Error: sendROM: cannot open file %s\n", file[fileIdx]);
        }

        if (fread(&magic, 1, 1, infile) != 1) {
            eggog("Error: file %s is empty\n", file[fileIdx]);
        }

        switch (magic) {
            case 0xfe:
                // Binary file
            	info("Sending BIN %s\n", file[fileIdx]);
                sendBIN(infile, 0, 0, 0);
                break;
            case 0xff:
                // tokenized BASIC
            	info("Sending BASIC %s\n", file[fileIdx]);
                sendBASIC(infile);
                break;
            case 0x41:
                // ROM image
            	info("Sending ROM %s\n", file[fileIdx]);
                sendROM(infile);
                break;
            default:
                eggog("Error: %s has unsupported file type %02x - cannot send\n", file[fileIdx], magic);
        }
    }

    return 1;
}

