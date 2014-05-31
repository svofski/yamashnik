#include <stdio.h>
#include <sys/stat.h>
#include "serial.h"
#include "sendpacket.h"

#define SECTORSIZE  128

int pingPong(PacketSender& ps, int adr) 
{
    PingPacket ping(0, adr);

    verbose("PING(%d)...", adr);
    do {
        ps.SendPacketVal(ping);
    } while (!ps.ReceivePacket());
    info("PONG from %d\n", adr);
    return 1;
}

void sendFile(const char* fileName, PacketSender& packetSender, int studentNo)
{
    static const char progressChar[] = "_,-'\".*oO0";
    uint8_t Sector[SECTORSIZE];
    FILE* infile;
    struct stat stat_p;
    int sectNo = 0;

    // check the size of file
    stat(fileName, &stat_p);

    infile = fopen(fileName, "rb");
    if (infile == 0) {
        eggog("Error: cannot open %s\n", fileName);
    }

    info("Sending file %s to workstation %d\n", fileName, studentNo);

    // Send ping
    if (!pingPong(packetSender, studentNo)) eggog("No reply from workstation %d\n", studentNo);

    // Create file on the net disk
    {
        NetCreateFilePacket createFile(0, studentNo, fileName);
        packetSender.SendPacketVal(createFile);
        if (!packetSender.ReceivePacket()) {
            eggog("No ack of create file from %d\n", studentNo);
        }
    }
    verbose("\nGot re: NET_CREATE_FILE.\n");

    // Reading the file sector-by-sector and writing them onto the net disk

    info("\nNumber of sectors to send: %zd\n", (size_t) ((stat_p.st_size / sizeof (Sector)) + (stat_p.st_size % sizeof (Sector))));

    while (!feof(infile))
    {
        if (fread(Sector, sizeof(Sector), 1, infile) == 0) break;

        {
            NetMasterDataPacket data(0, studentNo, Sector, SECTORSIZE);
            packetSender.SendPacketVal(data);

            NetWriteFilePacket writeFile(0, studentNo, packetSender.GetNetFCB());
            packetSender.SendPacketVal(writeFile);
            if (!packetSender.ReceivePacket()) {
                eggog("No ack of file write from %d\n", studentNo);
            }
        }
        verbose("\nSent sector No.%d", sectNo);

        if ((sectNo+1) % 10 == 0) {
            info("%d%c", sectNo+1, ((sectNo+1) % 100 == 0) ? '\n' : ' ');
        }
        else {
            info("%c\010", progressChar[sectNo % 10]);
        }

        sectNo++;
    }

    // close the file on the net disk
    {
        NetCloseFilePacket closeFile(0, studentNo, packetSender.GetNetFCB());
        packetSender.SendPacketVal(closeFile);
        if (!packetSender.ReceivePacket()) {
            eggog("No ack of file close from %d\n", studentNo);
        }
    }
    info("\nSendFile Done.\n");

}

void ncopy(const char* port, int studentNo, int nfiles, char* file[]) 
{
    SerialPort serialPort(port);
    PacketSender packetSender(&serialPort);

    if (serialPort.Setup() != ERR_NONE) {
       eggog("Error: cannot open %s\n", port);
    }

    for (int i = 0; i < nfiles; i++) {
        sendFile(file[i], packetSender, studentNo);
    }

    info("ncopy done\n");
}
