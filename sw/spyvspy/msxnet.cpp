#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "sendpacket.h"
#include "commands.h"
#include "serial.h"
#include "diags.h"

#include "spybdos.h"
#include "basicsend.h"
#include "ncopy.h"
#include "spy.h"

#define IOPORT "/dev/ttyS0"

void ping(const char* port, int studentNo) 
{
    SerialPort serialPort(port);
    PacketSender packetSender(&serialPort);

    if (serialPort.Setup() != ERR_NONE) {
       eggog("Error: cannot open %s\n", port);
    }

    verbose("ping: opened serial port %s studentNo=%d\n", port, studentNo);

    for(;; pingPong(packetSender, studentNo), usleep(10000));
}

void sendCommand(const char* port, int studentNo, const char* command) 
{
    SerialPort serialPort(port);
    PacketSender packetSender(&serialPort);

    if (serialPort.Setup() != ERR_NONE) {
       eggog("Error: cannot open %s\n", port);
    }

    pingPong(packetSender, studentNo);

    {   SNDCMDPacket cmd(0, studentNo, command);
        packetSender.SendPacketVal(cmd);
        packetSender.ReceivePacket();
    }
}

void halp() 
{
    info("Usage: msxnet [options] [command] file1 file2 ...\n");
    info("  options:\n");
    info("      --verbose|--morbose be verbose | be morbidly verbose\n");
    info("      --port=path         path to serial port\n");
    info("      --dst=number        target workstation address, 0 = broadcast (default 0)\n");
    info("  commands:\n");
    info("      --ping              ping workstation endlessly\n");
    info("      --cpm               issue _CPM command at workstation\n");
    info("      --ncopy             copy files to CP/M ramdisk\n");
    info("      --send              send BIN, ROM or BASIC files\n");
    info("      --spy <file>        bootstrap and run network MSX-DOS on the target\n");
    info("      --serve             serve already bootstrapped target\n");
}

int main(int argc, char *argv[]) {
    char portBuf[32] = IOPORT;
    char* port = portBuf;
    int studentNo = 127;
    int nfiles = 0;
     
    int workdone = 0;

    while (1) {
        static struct option long_options[] =
         {
           /* These options set a flag. */
           {"verbose", no_argument,       0, 0},
           {"morbose", no_argument,       0, 1},
           /* These options don't set a flag.
              We distinguish them by their indices. */
           {"port",     required_argument,      0, 2},
           {"dst",      required_argument,      0, 3},
           {"cpm",      no_argument,            0, 4},
           {"ping",     no_argument,            0, 5},
           {"ncopy",    no_argument,            0, 6},
           {"send",     no_argument,            0, 7},
           {"spy",      no_argument,            0, 8},
           {"test",     no_argument,            0, 9},
           {"serve",     no_argument,           0, 10},
           {0, 0, 0, 0}
         };

        /* getopt_long stores the option index here. */
        int option_index = 0;

        int c = getopt_long_only(argc, argv, "",
                        long_options, &option_index);
     
        /* Detect the end of the options. */
        if (c == -1) {
            if (!workdone)
                halp();
            break;
        }
     
        switch (c) {
            case 0:
                LogLevel = LOGLEVEL_VERBOSE;
                break;
     
            case 1:
                LogLevel = LOGLEVEL_MORBOSE;
                break;
        
                // serial port file
            case 2:
                port = (char *) malloc(strlen(optarg) + 1);
                strcpy(port, optarg);
                break;
     
                // destination address
            case 3:
                studentNo = atoi (optarg);
                // "127" means "to all"
                if (studentNo == 0) studentNo = 127;
                break;
     
            case 4:
                sendCommand(port, studentNo, "   _CPM");
                workdone = 1;
                break;
     
            case 5:
                ping(port, studentNo);
                // no exit
                break;
     
                // CPM ncopy mode
            case 6:
                if (workdone) {
                    info("Waiting for CPM to boot...");
                    sleep(3);
                    info("\n");
                }
                if ((nfiles = argc - optind) > 0) {
                    ncopy(port, studentNo, nfiles, &argv[optind]);
                    exit(0);
                }
                else {
                    eggog("No files to send");
                }
                break;

                // SendROM/BIN/BASIC
            case 7:
                if ((nfiles = argc - optind) > 0) {
                    BasicSender basicSender(port, studentNo, nfiles, &argv[optind]);
                    exit(0);
                }
                else {
                    eggog("No files to send");
                }
                break;

            case 8:
                {   // spy bootstrap & serve
                    nfiles = argc - optind;
                    Spy spy(port, argv[0], studentNo, nfiles, &argv[optind]);
                    spy.initData() &&
                        spy.Bootstrap();
                }
                break;

            case 10:
                {   // spy serve only
                    Spy spy(port, argv[0], studentNo, nfiles, &argv[optind]);
                    spy.initData() &&
                        spy.Serve();
                }

            case 9:
                {
                    NetBDOS bdos;
                    bdos.test();
                    workdone = 1;
                }
                break;

            default:
                eggog("Option error\n");
        }
    }
}

