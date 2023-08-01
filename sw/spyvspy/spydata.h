#pragma once

#include "util.h"
#ifdef __APPLE__
#include <sys/time.h>
#else
#include <time.h>
#endif
#include <sys/stat.h>
#include <string.h>

enum SpyRequestCode {
    F0E_SELECT_DISK         = 0,                    
    F0F_OPEN_FILE,          
    F10_CLOSE_FILE,         
    F11_SEARCH_FIRST,       
    F12_SEARCH_NEXT,        
    F13_DELETE,             
    F14_SEQ_READ,           
    F15_SEQ_WRITE,          
    F16_CREAT,              
    F17_RENAME,             
    F18_GETLOGINVECTOR,     
    F19_GET_CURRENT_DRIVE,  

    F1B_GET_ALLOC_INFO      = 0x0D,     
    F21_RANDOM_READ         = 0x13,     
    F22_RANDOM_WRITE,       
    F23_GET_FILE_SIZE,      
    F24_SET_RANDOM_RECORD,  

    F26_RANDOM_BLOCK_WRITE  = 0x18,
    F27_RANDOM_BLOCK_READ,  
    F28_RANDOM_WRITE_ZERO,  

    F2F_ABS_SECTOR_READ     = 0x21, 
    F30_ABS_SECTOR_WRITE    = 0x22, 
};

enum {
    REQ_BYTE = 1, REQ_WORD, REQ_FCB, REQ_DMA, REQ_DMA2, REQ_END = 0,
};

struct FCB {
/*  0 */    uint8_t     Drive;
/*  1 */    uint8_t     NameExt[11];
/* 12 */    uint8_t     ExtentNumberLo;
/* 14 */    uint8_t     ExtentNumberHi;
            uint8_t     RecordSizeLo;
            uint8_t     RecordSizeHi;
/* 16 */    uint32_t    FileSize;

/* 20 */    uint16_t    Date;
/* 22 */    uint16_t    Time;

/* 24 */    uint8_t     DeviceId;           // 40h + Drive (A=40h)
/* 25 */    uint8_t     DirectoryLocation;
/* 26 */    uint16_t    TopCluster;
/* 28 */    uint16_t    LastClusterAccessed;
/* 30 */    uint16_t    RelativeLocation;

/* 32 */    uint8_t     CurrentRecord;      // Current Record within Extent
/* 33 */    uint32_t    RandomRecord;
/* 36 */    uint32_t    Padding;
/* 40 */    uint8_t     Padding2[3];

    FCB() {
        memset(this, 0, sizeof(FCB));
    }

    uint16_t RecordSize() const {
        return (RecordSizeHi << 8) | RecordSizeLo;
    }

    uint16_t GetExtent() const {
        return (ExtentNumberHi << 8) | ExtentNumberLo;
    }

    void SetExtent(const uint16_t extent) {
        ExtentNumberLo = extent & 0xff;
        ExtentNumberHi = (extent >> 8) & 0xff;
    }

    int SetNameExt(const char* fname) {
        return Util::dosname(fname, (char *)NameExt);
    }

    void GetFileName(char* normalname) {
        extractFileName(&NameExt[0], normalname);
    }

    void GetFileName2(char* normalname) {
        extractFileName((uint8_t *)this + 17, normalname);
    }

private:
    void extractFileName(const uint8_t* namext, char* normalname) {
        int i = 0;
        for (int c; i < 8 && (c = namext[i]) != ' '; normalname[i] = c, i++);
        normalname[i++] = '.';
        for (int s = 8, c; s < 11 && (c = namext[s]) != ' '; normalname[i] = c, i++, s++);
        normalname[i] = 0;
    }
} __attribute__((packed));

struct DIRENT {
            uint8_t AlwaysFF;
/*  0 */    char NameExt[11];
/* 11 */    uint8_t Attrib;
/* 12 */    uint8_t Space[10]; 
/* 22 */    uint16_t Time;
/* 24 */    uint16_t Date;
/* 26 */    uint16_t Cluster;
/* 28 */    uint32_t FileSize;

    DIRENT() {
        memset(this, 0, sizeof(DIRENT));
        AlwaysFF = 0x01; // current drive no, 1 = A
    }

    void InitFromFCB(FCB* fcb) {
        memcpy(NameExt, fcb->NameExt, sizeof(NameExt));
        FileSize = fcb->FileSize;
        ((FCB *)this)->SetExtent(0);
        ((FCB *)this)->RecordSizeLo = 1;
        ((FCB *)this)->RecordSizeHi = 0;
        ((FCB *)this)->FileSize = FileSize / 128;
    }

    void SetDateTime(const char* file) {
        struct stat st;
        if (stat(file, &st) == 0) {
            // obtain values from the shortbus structs
            struct tm sanetime;
            localtime_r(&st.st_mtime, &sanetime);
            int year = sanetime.tm_year + 1900;
            int month = sanetime.tm_mon;
            int day = sanetime.tm_mday;
            int hour = sanetime.tm_hour;
            int minute = sanetime.tm_min;
            int sec = sanetime.tm_sec;

            // adjust for msx-dos, fail if values are impossible
            if (year < 1980) return;
            if (year > 2079) return;
            year = year - 1980;
            sec /= 2;
            month++;

            // pack the bits
            Date = (year << 9) | (month << 5) | day;
            Time = (hour << 11) | (minute << 5) | sec; 
            if (Time == 0) Time = 1;
        }

    }
} __attribute__((packed));;

struct DPB {                            // 0xF195
    uint8_t     Drive;                  // 00   DRIVE           DPB drive A:+00 Drive bij the dpb (0=A, 1=B, etc)
    uint8_t     Id;                     // F9   ID                          +01 Media ID byte (0F8h/0F9h)
    uint16_t    SectorSize;             // 0200 SECSIZ                      +02 Sector size (200h = 512 byte)
    uint8_t     DirEntries;             // 0F   DIRMSK (SECSIZ/32)-1        +04 (Directory registraties in 1 sector)-1
    uint8_t     DirShift;               // 04   DIRSHFT                     +05 Number of 1-bits in DIRMSK
    uint8_t     ClusterMask;            // 01   CLUSMSK                     +06 (Number of sectors per cluster)-1
    uint8_t     ClusterShift;           // 02   CLUSSHFT                    +07 (Number of 1-bits in CLUSMSK)+1
    uint16_t    FirstFAT;               // 0001 FIRFAT                      +08 first sector of the FAT
    uint8_t     FatCount;               // 02   FATCNT                      +0A number of FATs
    uint8_t     MaxEntries;             // 70   MAXENT                      +0B the max. number of directory registraties
    uint16_t    FirstRecord;            // 000E FIRREC                      +0C first sector of the DATA gebied
    uint16_t    MaxCluster;             // 02ca MAXCLUS (L)                 +0E the maximum number cluster
    uint8_t     FatSize;                // 03   FATSIZ                      +10 number sectors per FAT
    uint16_t    FirstDir;               // 0007 FIRDIR (L)                  +11 first sector of the DIRectory
    uint16_t    FatAdr;                 // E595 FATADR (L)                  +13 address of the FAT storage in the RAM

    DPB() {
        Drive = 0;
        Id = 0xf9;
        SectorSize = 0x200;
        DirEntries = 0x0f;
        DirShift = 0x04;
        ClusterMask = 0x01;
        ClusterShift = 0x02;
        FirstFAT = 0x0001;
        FatCount = 0x02;
        MaxEntries = 0x70;
        FirstRecord = 0x000e;
        MaxCluster = 0x02ca;
        FatSize = 0x03;
        FirstDir = 0x0007;
        FatAdr = 0xe595;
    };
} __attribute__((packed));;
