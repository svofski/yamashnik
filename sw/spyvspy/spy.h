#pragma once

#include <inttypes.h>
#include <strings.h>
#include "serial.h"
#include "diags.h"
#include "spydata.h"

#define DEBUGBLOCKSIZE 6
#define EMIT_PACE 4000

class Spy 
{
private:
    SerialPort m_serial;

    const char* m_argv0;

    int m_studentNo;
    int m_nfiles;
    char** m_file;

    char *m_ExePath;

    uint8_t* m_MSXRAM;
    uint8_t* m_BDOS;
    int m_BDOSSize;
    uint8_t* m_COMFile;

    void extractExePath();
    int loadFile(const char* filename, uint8_t** buf, int expectedSize);

public:
    Spy(const char* port, const char* argv0, int studentNo, int nfiles, char* file[]) 
        : m_serial(port),
          m_argv0(argv0),
          m_studentNo(studentNo),
          m_nfiles(nfiles),
          m_file(file),
          m_ExePath(0),     
          m_MSXRAM(0),
          m_BDOS(0),
          m_COMFile(0)
    {}

    ~Spy() 
    {
        if (m_ExePath) delete m_ExePath;
        if (m_MSXRAM) delete m_MSXRAM;
        if (m_BDOS) delete m_BDOS;
        if (m_COMFile) delete m_COMFile;
    }

    int initData();

    int Bootstrap();
    int Serve();
};

enum SpyState { SPY_BOOTSTRAP = 0, SPY_POLL = 1, SPY_GETFUNC = 2, SPY_RXDATA = 3};

class SpyResponse;

class SpyRequest 
{
    FCB     m_FCB;
    uint8_t m_DMA[65536];
    int     m_dmasize;
    int     m_data[8];
    uint8_t m_result;
    uint8_t m_func;

    uint8_t* m_rxpattern;
    int m_rxcursor;

    int m_datacursor;
    int m_bufoffset;

public:
    uint8_t GetFunc() const { return m_func; }
    FCB* GetFCB() { return &m_FCB; }
    const uint8_t* GetDMA() const { return m_DMA; }
    int GetDMASize() const { return m_dmasize; }
    int GetAuxData(int index) const { return m_data[index]; }

    void setFunc(const uint8_t func) {
        m_func = func;
    }

    void expect(const uint8_t RxPattern[]) {
        m_rxpattern = new uint8_t[strlen((const char*)RxPattern) + 1];
        strcpy((char *)m_rxpattern, (const char *)RxPattern);
        m_rxcursor = 0;
        m_datacursor = 0;
        m_bufoffset = 0;
        m_dmasize = 0;
    }

    uint16_t getWordBigEndian(uint8_t *cursor) {
        return cursor[1] | ((uint16_t)cursor[0] << 8);
    }

    int eat(uint8_t* buf, int len) {
        int avail = len - m_bufoffset;

        uint8_t* cursor = buf + m_bufoffset;

        // nothing to expect, we're good
        if (m_rxpattern[m_rxcursor] == 0) return 1;

        // something is expected but nothing is given yet
        if (avail == 0) return 0;

        // check what have we got here
        switch(m_rxpattern[m_rxcursor]) {
        case REQ_BYTE:
            m_data[m_datacursor] = cursor[0];   // get byte
            //morbose("REQ_BYTE: %x\n", m_data[m_datacursor]);
            m_bufoffset += 1;                   // input offset ++
            m_datacursor++;                     // data offset ++
            m_rxcursor++;                       // next expected token
            break;

        case REQ_WORD:
            if (avail >= 2) {   
                // get word: big endian
                m_data[m_datacursor] = getWordBigEndian(cursor);
                m_bufoffset += 2;               // advance by 2
                m_datacursor++;
                m_rxcursor++;                   // next token
            }
            break;

        case REQ_FCB:
            if (avail >= (int) sizeof(FCB) + 2) {
                // first 2 bytes are length of FCB: big endian
                // validate just for fun
                int length = getWordBigEndian(cursor);
                cursor += 2;

                if (length != sizeof(FCB)) {
                    info("error: received FCB length=%d, must be %zd",
                        length, sizeof(FCB));
                }

                // copy FCB data
                memcpy(&m_FCB, cursor, sizeof(FCB));

                m_bufoffset += 2 + sizeof(FCB); // shift buffer offset
                m_rxcursor++;                   // next token

                morbose("Received NetFCB, next state=%d\n", m_rxpattern[m_rxcursor]);
                dump(morbose, (uint8_t*) &m_FCB, sizeof(FCB));
            }
            break;

        case REQ_DMA:
            if (avail >= 2) {
                int dmasize = getWordBigEndian(cursor);
                if (avail >= dmasize + 2) {
                    // got all data, hoorj
                    m_dmasize = dmasize;
                    cursor += 2;
                    memcpy(&m_DMA, cursor, m_dmasize);

                    m_bufoffset += 2 + m_dmasize;
                    m_rxcursor++;
                }
            }
            break;

        case REQ_END:
        default:
            break;
        }

        // return 1 when reached end of expected pattern
        return (m_rxpattern[m_rxcursor] == 0) ? 1 : 0;
    }

    int NeedsData() const { return m_rxpattern[0] != 0; }
};


class SpyTransport : SerialListener
{
private:
    SerialPort* m_serial;
    SpyState m_state;
    uint8_t m_func;
    SpyRequest* m_rq;

    uint8_t m_rxbuf[65536];
    int m_rxpos;


public:
    SpyTransport(SerialPort* serial) 
        : m_serial(serial),
          m_state(SPY_BOOTSTRAP)
        {
            m_serial->SetRxListener(this);
        }
    
    void SendByte(uint8_t b, printfunc p = 0) const;
    void SendWord(uint16_t w, printfunc p = 0) const;
    void SendChunk(uint8_t* data, int length, printfunc p = 0) const;

    int RxHandler();

    void SendMemory(uint8_t* data, uint16_t addrDst, int length) const;

    void SendCommand(uint8_t cmd) const { SendByte(cmd); }

    int Poll(uint8_t studentNo);
    int ReceiveRequest(SpyRequest* request);
    int TransmitResponse(SpyResponse* response);  
};

class SpyResponse 
{
private:
    FCB*        m_FCB;
    uint8_t*    m_DMA;
    int         m_dmasize;
    int         m_data[8];
    uint8_t     m_result;

    uint8_t* m_txpattern;

public:
    SpyResponse() : m_FCB(0), m_DMA(0) {}

    ~SpyResponse() {
        if (m_FCB) delete m_FCB;
        if (m_DMA) delete[] m_DMA;
        if (m_txpattern) delete[] m_txpattern;
    }

    void respond(const uint8_t TxPattern[]) {
        m_txpattern = new uint8_t[strlen((const char*)TxPattern) + 1];
        strcpy((char *)m_txpattern, (const char *)TxPattern);
    }

    void AssignFCB(const FCB* fcb) {
        m_FCB = new FCB();
        memcpy(m_FCB, fcb, sizeof(FCB));
    }

    FCB* GetFCB() const { return m_FCB; }

    void AllocDMA(int length) {
        m_dmasize = length;
        m_DMA = new uint8_t[m_dmasize];
    }

    uint8_t* GetDMA() const { return m_DMA; }

    void SetDMASize(int n) {
        m_dmasize = n;
    }

    void AssignDMA(const uint8_t *data, int length) {
        if (m_DMA) delete[] m_DMA;
        m_dmasize = length;
        m_DMA = new uint8_t[m_dmasize];
        memcpy(m_DMA, data, m_dmasize);
    }

    void SetAuxData(uint8_t idx, int value)  {
        m_data[idx] = value;
    }

    // The larger the pace, the better the speed 

    void emit(SpyTransport* transport) {
        morbose("SpyResponse::emit: ");
        for(int done = 0, txcursor = 0, datacursor = 0; !done;) {
            switch(m_txpattern[txcursor]) {
            case REQ_END:
                morbose("<end>\n");
                done = 1;
                break;
            case REQ_BYTE:
                transport->SendByte(m_data[datacursor], morbose);
                datacursor++;
                txcursor++;
                usleep(EMIT_PACE);
                break;
            case REQ_WORD:
                transport->SendWord(m_data[datacursor], morbose);
                datacursor++;
                txcursor++;
                usleep(EMIT_PACE);
                break;
            case REQ_FCB:
                transport->SendWord(sizeof(FCB) - 8, morbose);
                usleep(EMIT_PACE);
                transport->SendChunk((uint8_t*)m_FCB, sizeof(FCB) - 8, morbose);
                txcursor++;
                usleep(EMIT_PACE);
                break;
            case REQ_DMA:
                transport->SendWord(m_dmasize, morbose);
                usleep(EMIT_PACE);
                transport->SendChunk(m_DMA, m_dmasize, morbose);
                usleep(EMIT_PACE);
                txcursor++;
                break;
            }
        }
    }
};

