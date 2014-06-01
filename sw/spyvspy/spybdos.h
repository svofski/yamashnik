#pragma once

#include <inttypes.h>
#include <glob.h>
#include <sys/errno.h>
#include "diags.h"
#include "spydata.h"
#include "spy.h"
#include "dosglob.h"

#define SECTORSIZE 0x200

class NetBDOS
{
private:
    int m_disk;
    glob_t m_globbuf;
    Globor m_globor;

    SpyRequest* m_req;
    SpyResponse* m_res;
    int m_lastRequest;

    void announce(const char* functionName) {
        info("\033[1mNetBDOS %02X\033[0m: %s", GetBDOSFunc(), functionName);
    }

    void newlineIfNewFunc() {
        if (m_lastRequest != GetBDOSFunc()) {
            m_lastRequest = GetBDOSFunc();
            info("\n");
        } else {
            info("\r\033[K");
        }
    }

    void newline() {
        m_lastRequest = GetBDOSFunc();
        info("\n");
    }

    int internalGetFileSize(FILE* f) {
        fseek(f, 0, SEEK_END);
        long pos = ftell(f);
        if (pos > 737820) {
            pos = 737820;
        }
        return pos;
    }

    int internalGetFileSize(const char* filename) {
        FILE* f = fopen(filename, "rb");
        if (f == 0) return -1;
        int pos = internalGetFileSize(f);
        fclose(f);

        return pos;
    }

    void selectDisk() {
        int d = m_req->GetAuxData(0);
        newlineIfNewFunc();
        announce("select disk ");
        if (d >= 0 && d <= 8) {
            info(" %c:", 'A' + m_disk);
        } else {
            info(" <probing drives>");
        }
        verbose("\n");
        m_res->SetAuxData(0, 0x01); // number of available drives
        m_res->respond((uint8_t[]){REQ_BYTE, 0});
    }

    void zeroFCBFields(FCB* fcb) {
        fcb->RandomRecord = 0;
        fcb->Date = 0;
        fcb->Time = 0;
        fcb->DirectoryLocation = 0;
        fcb->TopCluster = 0;
        fcb->LastClusterAccessed = 0;
        fcb->RelativeLocation = 0;
        fcb->CurrentRecord = 0;
        fcb->SetExtent(0);
        fcb->RecordSizeLo = 128;
        fcb->RecordSizeHi = 0;
    }

    void openFile() {
        char filename[13];
        m_req->GetFCB()->GetFileName(filename);

        newlineIfNewFunc();
        announce("open file");
        info(" '%s' ", filename);

        m_res->AssignFCB(m_req->GetFCB());

        FILE* f = fopen(filename, "rb");
        do {
            if (f == 0) {
                m_res->SetAuxData(0, 0xff);
                info("not found");
                break;
            } 

            m_res->GetFCB()->Drive = m_disk + 1;
            m_res->GetFCB()->FileSize = internalGetFileSize(f);
            m_res->GetFCB()->DeviceId = 0x40 + m_disk;
            
            m_res->SetAuxData(0, 0x0);

            info(" size=%d", m_res->GetFCB()->FileSize);
        } while(0);

        fclose(f);

        verbose("\n");

        m_res->respond((uint8_t[]){REQ_FCB, REQ_BYTE, 0});
    }

    void closeFile() {
        char filename[13];
        m_req->GetFCB()->GetFileName(filename);
        newline();
        announce("close file");
        info(" '%s'", filename);
        verbose("\n");
        m_res->SetAuxData(0, 0x00);
        m_res->respond((uint8_t[]){REQ_BYTE, 0});
    }

    void getCurrentDrive() {
        newline();
        announce("get current drive");
        info(" %c:", 'A' + m_disk);
        verbose("\n");
        m_res->SetAuxData(0, m_disk);
        m_res->respond((uint8_t[]){REQ_BYTE, 0});
    }

    void searchFirst() {
        char filename[13];
        m_req->GetFCB()->GetFileName(filename);

        newline();
        announce("search first: ");
        info(" '%s'", filename);
        verbose("\n");

        const char* first = m_globor.SearchFirst((const char*) m_req->GetFCB()->NameExt);

        DIRENT dirent;
        FCB fcb;
        fcb.SetNameExt(first);
        fcb.FileSize = internalGetFileSize(first);
        dirent.InitFromFCB(&fcb);
        dirent.SetDateTime(first);

        m_res->AssignDMA((uint8_t *)&dirent, sizeof(DIRENT));
        m_res->SetAuxData(0, first ? 0 : 0xff);
        m_res->respond((uint8_t[]){REQ_DMA, REQ_BYTE, 0});
    }

    void searchNext() {
        int result = 0xff;

        newlineIfNewFunc();
        announce("search next: ");

        const char* next = m_globor.SearchNext();

        info("'%s'", next ? next : "<end>");
        verbose("\n");

        DIRENT dirent;
        FCB fcb;
        fcb.SetNameExt(next);
        fcb.FileSize = internalGetFileSize(next);
        dirent.InitFromFCB(&fcb);
        dirent.SetDateTime(next);
        result = next ? 0 : 0xff;

        m_res->AssignDMA((uint8_t *)&dirent, sizeof(DIRENT));
        m_res->SetAuxData(0, result);

        m_res->respond((uint8_t[]){REQ_DMA, REQ_BYTE, 0});
    }

    void randomBlockRead() {
        char filename[13];

        newlineIfNewFunc();
        announce("random block read");

        m_req->GetFCB()->GetFileName(filename);

        int nrecords = m_req->GetAuxData(0 + DEBUGBLOCKSIZE);
        int recordsize = m_req->GetFCB()->RecordSize();
        if (recordsize != 1) {
            info("ERROR: records of sizes other than 1 not supported\n");
        }
        uint32_t recordno = m_req->GetFCB()->RandomRecord;

        info(" '%s' NRecs=%d RecordSize=%d Cur=%d", filename, nrecords, recordsize, recordno*recordsize);
        morbose("*** DEBUG DATA ***\n DMA= %04x SP=%04x USER SP=%04x [%04x %04x %04x]\n",
            m_req->GetAuxData(0),
            m_req->GetAuxData(1),
            m_req->GetAuxData(2),
            m_req->GetAuxData(3),
            m_req->GetAuxData(4),
            m_req->GetAuxData(5)
            );

        m_res->AssignFCB(m_req->GetFCB());

        FILE* f = fopen(filename, "rb");
        do {
            if ((recordno*recordsize >= m_req->GetFCB()->FileSize) ||
                (fseek(f, recordno*recordsize, SEEK_SET)) != 0) {
                m_res->SetDMASize(0);
                m_res->SetAuxData(0,0);
                m_res->SetAuxData(1,1);
                info("<eof>");
                break;
            }
            m_res->AllocDMA(nrecords*recordsize);
            int recordsread = fread(m_res->GetDMA(), recordsize, nrecords, f);
            m_res->SetDMASize(recordsread * recordsize);
            m_res->SetAuxData(0, recordsread); // records read
            m_res->SetAuxData(1, 0);           // no error
            m_res->GetFCB()->RandomRecord += recordsread;
        } while(0);
        verbose("\n");

        fclose(f);
        m_res->respond((uint8_t[]){REQ_WORD, REQ_DMA, REQ_FCB, REQ_BYTE, 0});
    }

    void randomRead() {
        char filename[13];
        m_req->GetFCB()->GetFileName(filename);

        int recordsize = 128;

        uint32_t recordno = m_req->GetFCB()->RandomRecord;
        recordno = recordno & 0x00ffffff;

        m_res->AssignFCB(m_req->GetFCB());

        newlineIfNewFunc();
        announce("random read");
        info(" '%s' Rec=%d Ofs=%d", filename, recordno, recordno*recordsize);

        morbose("*** DEBUG DATA ***\n DMA= %04x SP=%04x USER SP=%04x [%04x %04x %04x]\n",
            m_req->GetAuxData(0),
            m_req->GetAuxData(1),
            m_req->GetAuxData(2),
            m_req->GetAuxData(3),
            m_req->GetAuxData(4),
            m_req->GetAuxData(5)
            );

        m_res->AllocDMA(recordsize);
        m_res->SetDMASize(recordsize);
        memset(m_res->GetDMA(), 0, recordsize);
        m_res->SetAuxData(0,1);

        off_t fileoffset = recordno*recordsize;

        FILE* f = fopen(filename, "rb");
        do {
            if ((fileoffset >= m_req->GetFCB()->FileSize) ||
                (fseek(f, fileoffset, SEEK_SET) != 0) ||
                (ftell(f) != fileoffset)) {
                info("<eof>");
                break;
            }

            int bytesread = fread(m_res->GetDMA(), 1, recordsize, f);

            // Set result
            m_res->SetAuxData(0, bytesread > 0 ? 0 : 1); 

            // Update Current Record and Extent for sequential read
            uint16_t extentsize = 128*128; // 128 records of 128 bytes each == 1 extent

            m_res->GetFCB()->CurrentRecord = fileoffset % extentsize; 
            m_res->GetFCB()->RecordSizeLo = 0;
            m_res->GetFCB()->RecordSizeHi = recordsize;
            
            m_res->GetFCB()->SetExtent(fileoffset / extentsize);
        } while(0);
        fclose(f);

        verbose("\n");

        m_res->respond((uint8_t[]){REQ_DMA, REQ_FCB, REQ_BYTE, 0});
    }

    void sequentialRead() {
        newlineIfNewFunc();
        announce("sequential read");
        char filename[13];
        int recordsize = 128;
        m_req->GetFCB()->GetFileName(filename);
        
        uint32_t recordno = m_req->GetFCB()->CurrentRecord;
        uint32_t extent = m_req->GetFCB()->GetExtent();

                           /* extent base */           /* record */
        off_t fileoffset = 128 * recordsize * extent + recordno * recordsize;

        m_res->AssignFCB(m_req->GetFCB());

        info(" '%s' Rec=%d Ofs=%zd ", filename, recordno, fileoffset);

        // Always return a full record, padded with zeroes if EOF
        m_res->AllocDMA(recordsize);
        m_res->SetDMASize(recordsize);
        memset(m_res->GetDMA(), 0, recordsize);
        m_res->SetAuxData(0,1);

        FILE* f = fopen(filename, "rb");
        do {
            if ((fileoffset >= m_req->GetFCB()->FileSize) ||
                (fseek(f, fileoffset, SEEK_SET) != 0) ||
                (ftell(f) != fileoffset)) {
                info("<eof>");
                break;
            }

            int bytesread = fread(m_res->GetDMA(), 1, recordsize, f);
            // set result
            m_res->SetAuxData(0, bytesread > 0 ? 0 : 1); 
        } while(0);
        fclose(f);

        m_res->GetFCB()->RecordSizeLo = 0;
        m_res->GetFCB()->RecordSizeHi = recordsize;

        m_res->GetFCB()->CurrentRecord = m_res->GetFCB()->CurrentRecord + 1;
        if (m_res->GetFCB()->CurrentRecord == 0x80) {
            m_res->GetFCB()->CurrentRecord = 0;
            m_res->GetFCB()->SetExtent(m_res->GetFCB()->GetExtent() + 1);
        }
        verbose(" advance: rec=%d ext=%d", m_res->GetFCB()->CurrentRecord, m_res->GetFCB()->GetExtent());
        verbose("\n");

        m_res->respond((uint8_t[]){REQ_DMA, REQ_FCB, REQ_BYTE, 0});
    }

    void getLoginVector() {
        newline();
        announce("get login vector");
        verbose("\n");
        m_res->SetAuxData(0, 0x0001); // LSB corresponds to drive A
        m_res->respond((uint8_t[]){REQ_WORD, 0});
    }

    void getFileSize() {
        newline();
        announce("get file size");
        verbose("\n");

        char filename[13];
        m_req->GetFCB()->GetFileName(filename);
        int size = internalGetFileSize(filename);
        m_res->AssignFCB(m_req->GetFCB());
        m_res->GetFCB()->FileSize = size;

        m_res->SetAuxData(0, size == -1 ? 0xff : 0);
        m_res->respond((uint8_t[]) {REQ_FCB, REQ_BYTE, 0});
    }

    void deleteFile() {
        newline();
        announce("delete file");

        char filename[13];
        m_req->GetFCB()->GetFileName(filename);
        const char* errmsg = "OK";

        if (unlink(filename) != 0) {
            errmsg = strerror(errno);
            m_res->SetAuxData(0, 0xff);
        } else {
            m_res->SetAuxData(0, 0);
        }

        info(": %s: %s", filename, errmsg);
        verbose("\n");

        m_res->respond((uint8_t[]) {REQ_BYTE, 0});
    }

    void renameFile() {
        newline();
        announce("rename file");

        char filenameFrom[13];
        char filenameTo[13];
        m_req->GetFCB()->GetFileName(filenameFrom);
        m_req->GetFCB()->GetFileName2(filenameTo);

        info(" from=%s to=%s ", filenameFrom, filenameTo);
        m_res->SetAuxData(0, 0xff);
        if (rename(filenameFrom, filenameTo) == 0) {
            m_res->SetAuxData(0, 0);
            info("OK");
        } else {
            info(": %s", strerror(errno));
        }
        verbose("\n");

        m_res->respond((uint8_t[]) {REQ_BYTE, 0});
    }

    void createFile() {
        newline();
        announce("create file");

        char filename[13];
        m_req->GetFCB()->GetFileName(filename);

        uint16_t extent = m_req->GetFCB()->GetExtent();

        m_res->AssignFCB(m_req->GetFCB());
        m_res->GetFCB()->SetExtent(extent);

        info(" %s: extent=%u: ", filename, extent);

        m_res->SetAuxData(0, 0xff);
        if (extent == 0) {
            // create / overwrite
            FILE* f = fopen(filename, "w");
            if (f != 0) {
                m_res->SetAuxData(0, 0);
                info("OK");
                fclose(f);
            } else {
                info(": %s", strerror(errno));
            }
        } else {
            info("error: non-zero extent create not supported");
        }
        verbose("\n");

        m_res->respond((uint8_t[]) {REQ_FCB, REQ_BYTE, 0});
    }


    // sequential: Rx: FCB<>, DMA<>  Tx: FCB<>, byte<result> (advance record)
    // random:     Rx: FCB<>, DMA<>  Tx: byte<result>
    void randomWrite(int sequential) {
        newlineIfNewFunc();
        announce(sequential ? "sequential write" : "random write");

        char filename[13];
        int recordsize = 128;
        m_req->GetFCB()->GetFileName(filename);
        
        off_t fileoffset;
        uint32_t recordno;
        if (sequential) {
            recordno = m_req->GetFCB()->CurrentRecord;
            uint32_t extent = m_req->GetFCB()->GetExtent();
                           /* extent base */           /* record */
             fileoffset = 128 * recordsize * extent + recordno * recordsize;
         } else {
            recordno = m_req->GetFCB()->RandomRecord;
            recordno = recordno & 0x00ffffff;
            fileoffset = recordno * recordsize;
         }

        info(" '%s' Rec=%d Ofs=%zd Size=%d", filename, recordno, fileoffset, m_req->GetDMASize());

        m_res->AssignFCB(m_req->GetFCB());
        m_res->SetAuxData(0,1); // result: 01 = error, 0 if ok

        FILE* f = fopen(filename, "r+");
        do {
            if (f == 0) {
                info(strerror(errno));
                break;
            }
            if (fseek(f, fileoffset, SEEK_SET) != 0) {
                info(" error: could not seek to %u ", fileoffset);
                break;
            }
            size_t written = fwrite(m_req->GetDMA(), 1, recordsize, f);
            m_res->SetAuxData(0, (written == (size_t)(m_req->GetDMASize())) ? 0 : 1);
        } while(0);
        fclose(f);

        m_res->GetFCB()->RecordSizeLo = 0;
        m_res->GetFCB()->RecordSizeHi = recordsize;

        if (fileoffset + recordsize > m_res->GetFCB()->FileSize)
            m_res->GetFCB()->FileSize = fileoffset + recordsize;

        m_res->GetFCB()->CurrentRecord = m_res->GetFCB()->CurrentRecord + 1;
        if (m_res->GetFCB()->CurrentRecord == 0x80) {
            m_res->GetFCB()->CurrentRecord = 0;
            m_res->GetFCB()->SetExtent(m_res->GetFCB()->GetExtent() + 1);
        }
        verbose(" advance: rec=%d ext=%d", m_res->GetFCB()->CurrentRecord, m_res->GetFCB()->GetExtent());
        verbose("\n");

        if (sequential) {
            m_res->respond((uint8_t[]) {REQ_FCB, REQ_BYTE, 0});
        } else {
            m_res->respond((uint8_t[]) {REQ_BYTE, 0});
        }
    }

    void absSectorRead() {
        newlineIfNewFunc();
        announce("absolute sector read");
        info(" [sec=%d, count=%d, %c:] not implemented", 
            m_req->GetAuxData(0),           // sector number 
            m_req->GetAuxData(1),           // no. of sectors to read
            'A' + m_req->GetAuxData(2));    // drive
        verbose("\n");
        m_res->AllocDMA(SECTORSIZE * m_req->GetAuxData(1));
        m_res->SetDMASize(SECTORSIZE * m_req->GetAuxData(1));
        memset(m_res->GetDMA(), 0, SECTORSIZE * m_req->GetAuxData(1));
        m_res->respond((uint8_t[]) {REQ_DMA, 0});
    }

/*
http://www.kameli.net/lt/bdos1var.txt 

F195H-F1A9H     Driver Parameter Block (DPB) A:
F1AAH-F1BEH     DPB B:
*/
    void getAllocInfo() {
        newline();
        announce("get allocation information");
        info(" %c:", 'A' + m_req->GetAuxData(0));

        //uint8_t dpb[32];
        //memset(dpb, 0, 32);
        DPB dpb;
        uint8_t fat[512];

        m_res->AssignDMA((uint8_t *)&dpb, sizeof(DPB));
        m_res->AssignDMA2(&fat[0], 512);

        m_res->SetAuxData(0, 0xF195);       // address of DPB in MSX-DOS area
        m_res->SetAuxData(1, 0xE595);       // address of first FAT sector in MSX-DOS area
        m_res->SetAuxData(2, SECTORSIZE);   // BC, sector size
        m_res->SetAuxData(3, 0x2c9);        // DE, total clusters
        m_res->SetAuxData(4, 0x2c9);        // HL, free clusters
        m_res->SetAuxData(5, 2);            // A, sectors per cluster

        m_res->respond((uint8_t[]) {
            /* pointer to DPB */ REQ_WORD,  /* IX  = F195 */
            /* DPB */            REQ_DMA,
            /* pointer to FAT */ REQ_WORD,  /* IY = E595 */
            /* FAT */            REQ_DMA2,
            /* Sector size */    REQ_WORD,
            /* Total clusters */ REQ_WORD,
            /* Free clusters */  REQ_WORD,
            /* Sctrs per clstr */REQ_BYTE
        });
    }

private:
    int test_fileNameFromPCB() {
        char filename[13];
        FCB fcb;
        memcpy(&fcb.NameExt, "AUTOEXECBAT", 11);
        fcb.GetFileName(filename);
        info("test_fileNameFromPCB 1 '%s'\n", filename);

        if (strcmp("AUTOEXEC.BAT", filename) != 0) return 0;

        memcpy(&fcb.NameExt, "FOO     BAR", 11);
        fcb.GetFileName(filename);
        info("test_fileNameFromPCB 2 '%s'\n", filename);
        if (strcmp("FOO.BAR", filename) != 0) return 0;

        memcpy(&fcb.NameExt, "FUUU    SO ", 11);
        fcb.GetFileName(filename);
        info("test_fileNameFromPCB 3 '%s'\n", filename);
        if (strcmp("FUUU.SO", filename) != 0) return 0;

        return 1;
    }

    int test_FCBFromFileName() {
        FCB fcb;
        fcb.SetNameExt("foo.bar");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("foo.b");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("foo.");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("foo");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));


        fcb.SetNameExt(".e");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt(".err");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("autoexec.bat");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("/home/babor/autoexec.bat");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("/home/babor/longnameislong.bat");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("/home/babor/okname.longext");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        fcb.SetNameExt("okname.longext");
        dump(info, (uint8_t *) &fcb, sizeof(FCB));

        return 1;
    }

    int test_searchFirstDirent() {
        info("test_searchFirstDirent\n");
        m_req = new SpyRequest();
        m_res = new SpyResponse();
        m_req->GetFCB()->SetNameExt("A.???");
        searchFirst();
        dump(info, (uint8_t *) m_res->GetDMA(), m_res->GetDMASize());

        delete m_req;
        delete m_res;

        return 1;
    }


    int test_searchFirst() {
        m_req = new SpyRequest();
        m_res = new SpyResponse();
        m_req->GetFCB()->SetNameExt("AUTOEXEC.???");
        searchFirst();

        m_req = new SpyRequest();
        m_req->GetFCB()->SetNameExt("NOSUCH.FIL");
        searchFirst();

        delete m_req;
        delete m_res;

        return 1;
    }

    int test_dosglob() {
        char dosname[12];
        const char *in;
        dosname[11] = 0;

        Globor g(".");

        g.SetPattern("???????????");

        in = "a.b";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = "autoexec.com";
        info("test_dosglob: match %s %d\n", in, g.match(in));
            
        in = "turbo.com";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = "noext.";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = ".ext";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = "";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        g.SetPattern("????????COM");

        in = "a.b";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = "autoexec.com";
        info("test_dosglob: match %s %d\n", in, g.match(in));
            
        in = "turbo.com";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = "noext.";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        in = ".ext";
        info("test_dosglob: match %s %d\n", in, g.match(in));

        for (const char *fu = g.SearchFirst("????????????");
             fu != 0; fu = g.SearchNext()) {
            info("DIR: %s\n", fu);
        }


        return 1;
    }

public:
    NetBDOS() : m_disk(0), m_globor("."), m_lastRequest(-1) {
    }

    ~NetBDOS() {
    }

    int testSuite() {
        return test_fileNameFromPCB() 
            && test_FCBFromFileName()
            && test_searchFirstDirent()
            && test_searchFirst()
            && test_dosglob();
    }

    int GetBDOSFunc() const { return m_req->GetFunc() + 14; }

    void ExecFunc(SpyRequest* request, SpyResponse* response) {
        m_req = request;
        m_res = response;

        switch(request->GetFunc()) {
        case F0E_SELECT_DISK:
            selectDisk();
            break;
        case F0F_OPEN_FILE:
            openFile();
            break;
        case F10_CLOSE_FILE:
            closeFile();
            break;
        case F11_SEARCH_FIRST:
            searchFirst();
            break;
        case F12_SEARCH_NEXT:
            searchNext();
            break;
        case F13_DELETE:
            deleteFile();
            break;
        case F14_SEQ_READ:
            sequentialRead();
            break;
        case F15_SEQ_WRITE:
            randomWrite(1);
            break;
        case F16_CREAT:
            createFile();
            break;
        case F17_RENAME:
            renameFile();
            break;
        case F18_GETLOGINVECTOR:
            getLoginVector();
            break;
        case F19_GET_CURRENT_DRIVE:
            getCurrentDrive();
            break;
        case F1B_GET_ALLOC_INFO:
            getAllocInfo();
            break;
        case F21_RANDOM_READ:
            randomRead();
            break;
        case F22_RANDOM_WRITE:
            randomWrite(0);
            break;
        case F23_GET_FILE_SIZE:
            getFileSize();
            break;
        case F24_SET_RANDOM_RECORD:
            break;
        case F26_RANDOM_BLOCK_WRITE:
            break;
        case F27_RANDOM_BLOCK_READ:
            randomBlockRead();
            break;
        case F28_RANDOM_WRITE_ZERO:
            break;
        case F2F_ABS_SECTOR_READ:
            absSectorRead();
            break;
        case F30_ABS_SECTOR_WRITE:
            break;
        default:
            break;
        }
    }
};
