#include <stdio.h>
#include <inttypes.h>
#include "commands.h"
#include "sendpacket.h"
#include "diags.h"

uint8_t PacketUtil::ReadEscapedByte(unsigned char *p) {
    return (p [1] + (p [0] ? 128 : 0));
}

uint16_t PacketUtil::ReadEscapedWord(unsigned char *p) {
    return ((ReadEscapedByte (p) << 8) + ReadEscapedByte (p + 2));
}

void PacketSender::SendEscapedByte(uint8_t b) const {
    SendByte((b & 128) >> 7);
    SendByte(b & 127);
}

void PacketSender::SendEscapedWord(uint16_t w) const {
    SendEscapedByte(w >> 8);
    SendEscapedByte(w & 255);
}

void PacketSender::SendHeader(const uint8_t *h) const {
    for (int i = 0; i < 5; i++) {
        SendByte(h[i]);
    }
}

void PacketSender::SendEscapedBlockWithChecksum(const uint8_t *buf, int len) {
    uint16_t checksum = 0;
    
    for (int i = 0; i < len; i++) {
        SendEscapedByte(buf[i]);
        checksum += buf[i];
    }
    SendEscapedWord(checksum);
}

/*
 * SendPacket
 */
void PacketSender::SendPacket(int srcAddr, int dstAddr, int cmdType, const uint8_t *buf, uint16_t len, uint16_t addr1, uint16_t addr2, int last) {
    uint8_t Header[] = { 0xf0, 0x00, 0x00, 0x01, 0x00 };
    int l;

    morbose("\nSendPacket: from %d to %d, cmd: 0x%.2x, %d bytes ",
        srcAddr, dstAddr, cmdType, len);

    // Header fields, common for all packets
    Header[2] = dstAddr;
    Header[4] = srcAddr;

    switch (cmdType) {
    case PCMD_PING:
        Header[3] = PCMD_PING;
        SendHeader(Header);
        SendByte(LAST);                             // TERMINATOR
        break;

    case NET_CREATE_FILE:
        SendHeader(Header);                         // HEADER
        SendEscapedByte(NET_CREATE_FILE);           // COMMAND
        SendEscapedWord(len);                       // LENGTH: FCB (37 bytes) + H, F, A (3 bytes)
        SendEscapedBlockWithChecksum(buf, len);     // PAYLOAD + CHECHKSUM
        SendByte(LAST);                             // TERMINATOR
        break;

    case NET_CLOSE_FILE:
        SendHeader(Header);                        // HEADER
        SendEscapedByte(NET_CLOSE_FILE);           // COMMAND
        SendEscapedWord(len);                      // LENGTH: FCB (37 bytes) + H, F, A (3 bytes)
        SendEscapedBlockWithChecksum(buf, len);    // PAYLOAD + CHECHKSUM
        SendByte(LAST);                            // TERMINATOR
        break;

    case NET_MASTER_DATA:
        l = len;
        do {
            pos = 0;

            SendHeader(Header);
            SendEscapedByte((l == len) ? NET_MASTER_DATA : NET_MASTER_DATA2);

            SendEscapedWord((l > 56) ? 56 : l);
            SendEscapedBlockWithChecksum(buf + len - l, (l > 56) ? 56 : l);
            SendByte((l > 56) ? INTERMEDIATE : LAST);

            l -= 56;

            serial->waitRx();
        } while (l > 0);
        break;

    case NET_WRITE_FILE:
        SendHeader(Header);
        SendEscapedByte(NET_WRITE_FILE);
        SendEscapedWord(len);
        SendEscapedBlockWithChecksum(buf, len);
        SendByte(LAST);
        break;

    case PCMD_SNDCMD:
        SendHeader(Header);
        SendEscapedByte(PCMD_SNDCMD);
        SendEscapedWord(len);
        SendEscapedBlockWithChecksum(buf, len);
        SendByte(LAST);
        break;

    case PCMD_SHEXHEADER:
        SendHeader(Header);
        SendEscapedByte(PCMD_SHEXHEADER);
        SendEscapedWord(addr1);
        SendEscapedWord(addr2);
        SendByte(last ? LAST : INTERMEDIATE);
        break;

    case PCMD_SHEXDATA:
        SendHeader(Header);
        SendEscapedByte(PCMD_SHEXDATA);
        SendEscapedWord(len);
        SendEscapedBlockWithChecksum(buf, len);
        SendByte(last ? LAST : INTERMEDIATE);
        break;

    case PCMD_POKE:
        SendHeader(Header);
        SendEscapedByte(PCMD_POKE);
        SendEscapedWord(addr1);
        SendEscapedByte((uint8_t) (addr2 & 0377));
        SendByte(LAST);
        break;

    default:
        eggog(" unsupported packet type %02x\n", cmdType);
    }

    morbose("\n");
}

void PacketSender::SendPacketVal(GenericPacket& packet) 
{
    SendPacket(packet.GetSrcAddr(), packet.GetDstAddr(), packet.GetCmd(),
        packet.GetData(), packet.GetLength(), 
        packet.GetAddr1(), packet.GetAddr2(),
        packet.GetIsLast());
}

int PacketSender::ReceivePacket() {
    pos = 0;
    return serial->waitRx(50);
}

void PacketSender::CheckPacket() {
    int src, dst;

    morbose("CheckPacket: ");

    // Header
    if ((buf[0] == 0xf0) || (buf [0] == 0x78) // sorry :)
        && (buf[1] == 00))
    {
        dst = buf[2];
        src = buf[4];
        switch (buf[3]) {
            int readEscWord;

            case PCMD_BASE:
                switch (buf [6]) {
                    case RE_NET_CREATE_FILE:
                        morbose("RE_NET_CREATE_FILE from %d to %d ", src, dst);
                        readEscWord = PacketUtil::ReadEscapedWord (&(buf[7]));
                        morbose("Payload: %d bytes ", readEscWord);
                        GetRxData(buf);
                        break;
                    case RE_NET_CLOSE_FILE:
                        morbose("RE_NET_CLOSE_FILE from %d to %d ", src, dst);
                        readEscWord = PacketUtil::ReadEscapedWord (&(buf[7]));
                        morbose("Payload: %d bytes ", readEscWord);
                        GetRxData(buf);
                        break;
                    case RE_NET_WRITE_FILE:
                        morbose("RE_NET_WRITE_FILE from %d to %d ", src, dst);
                        readEscWord = PacketUtil::ReadEscapedWord (&(buf[7]));
                        morbose("Payload: %d bytes ", readEscWord);
                        GetRxData(buf);
                        break;
                    default:
                        info("Unknown BASE packet 0x%.2x. ", buf[6]);
                        break;
                }
                break;
            case PCMD_PING:
                break;
            case PCMD_ACK:
                morbose("ACK from %d to %d ", src, dst);
                break;
            case PCMD_PONG:
                morbose("PONG from %d to %d STATUS=%02x. ", src, dst, buf[5]);
                break;
        }
        morbose("\n");
    }
}

void PacketSender::GetRxData(uint8_t *p) {
    m_RxData.H = PacketUtil::ReadEscapedByte(&p[11]);
    m_RxData.F = PacketUtil::ReadEscapedByte(&p[13]);
    m_RxData.A = PacketUtil::ReadEscapedByte(&p[15]);
    verbose("\n *** H: %.2x F: %.2x A: %.2x FCB: ", m_RxData.H, m_RxData.F, m_RxData.A);
    for (int i = 0; i < 37; i++) {
        m_RxData.FCB[i] = PacketUtil::ReadEscapedByte(&buf[17 + i * 2]);
        verbose(" %.2x", m_RxData.FCB[i]);
    }
}

int PacketSender::RxHandler() {
    int n;

    int result = 0;

    pos += n = serial->read(&(buf[pos]), 1024 - pos);

    if (pos > 256) {
        pos = 0;
        info("\nERROR: RxHandler: buffer overrun\n");
    }
    else {
        if ( (buf[pos - 1] == 0x83) || (buf[pos - 1] == 0x97)) {
            CheckPacket();
            result = 1;

            morbose("RxHandler: got %d bytes, pos=%d, last byte: 0x%.2x: ", n, pos, buf [pos]);
            for (int i = 0; i < pos; i++)
            {
                morbose("%.2x ", buf [i]);
            }
            morbose("\n");
        }
    }

    return result;
}

