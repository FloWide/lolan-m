/**
 * Lolan Server Example
 *
 * Optimized Mobile Technologies (C)
 * gabor.feher@omt.hu
 **/


#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <errno.h>

#include <chrono>
#include <deque>
#include <thread>
#include <mutex>

#include "Slip.hpp"
#include <lolan_config.h>
#include <lolan.h>

#define BAUDRATE B115200

bool quit=false;

lolan_ctx lctx;
const uint8_t nodeName_path[LOLAN_REGMAP_DEPTH] = {1,1,0};
const uint8_t testInt_path[LOLAN_REGMAP_DEPTH] = {1,2,0};
std::mutex dequeMutex;
std::deque<std::vector<uint8_t>> lpQueue;

char * nodeName = "LoLaN test node";
uint16_t testInt = 11;

void sendToTTyBin(int fd,uint8_t *bp, int s) {
    /*int w;
    while (s>0) {
    w = write(fd,bp,s);
    tcflush(fd, TCIOFLUSH);
        s = s-w;
        bp+=w;
    }*/
    for (int i=0;i<s;i++) {
    write(fd,bp,1);
    bp++;
    }
//    tcflush(fd, TCIOFLUSH);
}

uint16_t CRC_calc(uint8_t *val, uint8_t size)
{
    uint16_t crc;
    uint16_t q;
    uint8_t c;
    crc = 0;
    for (int i = 0; i < size; i++)
    {
        c = val[i];
        q = (crc ^ c) & 0x0f;
        crc = (crc >> 4) ^ (q * 0x1081);
        q = (crc ^ (c >> 4)) & 0xf;
        crc = (crc >> 4) ^ (q * 0x1081);
    }
    return (uint8_t) crc << 8 | (uint8_t) (crc >> 8);
}


void llSendPacket(int fd,lolan_Packet *lp)
{
    uint8_t txp[LOLAN_MAX_PACKET_SIZE];
    memset(txp,0,LOLAN_MAX_PACKET_SIZE);

    txp[0] = lp->packetType;
    if (lp->securityEnabled) { txp[0]|=0x08; }
    if (lp->framePending) { txp[0]|=0x10; }
    if (lp->ackRequired) { txp[0]|=0x20;}
    txp[1]=0x74; // 802.15.4 protocol version=3
    txp[2] = lp->packetCounter;
    uint16_t *fromId = ((uint16_t *) &txp[3]);
    uint16_t *toId = ((uint16_t *) &txp[5]);
    *fromId = lp->fromId;
    *toId =  lp->toId;
    memcpy(&(txp[7]),lp->payload,lp->payloadSize);
    uint16_t crc16 = CRC_calc(txp,7+lp->payloadSize);

    txp[7+lp->payloadSize] = (crc16>>8)&0xFF;
    txp[7+lp->payloadSize+1] = crc16&0xFF;

    SlipPacketizer slp;
    slp.encode(txp,7+lp->payloadSize+2);

    std::cout << "\n<=[ ";
    for (int i=0;i<7+lp->payloadSize+2;i++) {
	printf("%02X ",txp[i]);
    }
    std::cout << "]\n";

    sendToTTyBin(fd,(uint8_t *) &(slp.encodeBuffer[0]),slp.encodeBuffer.size());
}


void readTTy(int fd)
{
    SlipPacketizer slp;
    char byte;
    int bytesRead;

    while (1) {
	bytesRead = read(fd, &byte, 1);
	if (bytesRead == 1) {
	    if (slp.feedDecode(byte) == 1) {
		if (slp.decodeBuffer.size() > 0) {
		    if (slp.decodeBuffer.at(0)=='{') { // this is an ASCII packet, now we ignore
		    } else {
			std::cout << "=>[ ";
			for (int i=0;i<slp.decodeBuffer.size();i++) {
			    printf("%02X ",(uint8_t) slp.decodeBuffer.at(i));
			}
			std::cout << "]\n";
			std::lock_guard<std::mutex> lock( dequeMutex );
			lpQueue.push_back(slp.decodeBuffer);
		    }
		}
    		slp.decodeBuffer.clear();
    	    }
	} else {
    	    usleep(10);
	}

        if (quit) {
	    break;
	}
    }
}

int main(int argc, char** argv) {
    int fd=0;
    fd = open("/dev/ptmx", O_RDWR | O_NOCTTY | O_SYNC);
    if (fd == -1) {
        std::cerr << "error opening file" << std::endl;
        return -1;
    }

    grantpt(fd);
    unlockpt(fd);

    char* pts_name = ptsname(fd);
    std::cerr << "ptsname: " << pts_name << std::endl;

    /* serial port parameters */
    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio));
    struct termios oldtio;
    tcgetattr(fd, &oldtio);

    newtio = oldtio;
    newtio.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;         /* 8-bit characters */
    newtio.c_cflag &= ~PARENB;     /* no parity bit */
    newtio.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    newtio.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    newtio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    newtio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    newtio.c_oflag &= ~OPOST;

    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 0;
    tcflush(fd, TCIFLUSH);

    cfsetispeed(&newtio, BAUDRATE);
    cfsetospeed(&newtio, BAUDRATE);
    tcsetattr(fd, TCSANOW, &newtio);
    fcntl(fd, F_SETFL, FNDELAY);
    usleep(10000);

    lolan_init(&lctx,1);
    lolan_regVar(&lctx,nodeName_path,LOLAN_STR,(char *) nodeName); 
    lolan_regVar(&lctx,testInt_path,LOLAN_INT16,(int16_t *) &testInt);

    std::thread readerThread = std::thread( [&]{ readTTy(fd); } );

    bool sleep=false;

    while (quit==false) {
	std::vector<uint8_t> lpbuff;
	{
	    std::lock_guard<std::mutex> lock( dequeMutex );
	    if (lpQueue.size()>0) {
		lpbuff = lpQueue.front();
		lpQueue.pop_front();
		sleep = false;
	    } else {
		sleep = true;
	    }
	}

	if (sleep) {
	    usleep(10);
	} else {
	    uint8_t buff[LOLAN_MAX_PACKET_SIZE];
	    uint8_t *b = buff;
	    for (auto& it : lpbuff) {
		*b = it;
		b++;
    	    }
	    lolan_Packet lp;
	    memset(&lp,0,sizeof(lolan_Packet));
	    lp.payload = (uint8_t *) malloc(LOLAN_MAX_PACKET_SIZE);
            if (lolan_parsePacket(&lctx,buff,lpbuff.size(),&lp)==1) {
		if (lctx.myAddress == lp.toId) {
		    if (lp.packetType == LOLAN_GET) {
			lolan_Packet replyPacket;
			memset(&replyPacket,0,sizeof(lolan_Packet));
			replyPacket.payload = (uint8_t *) malloc(LOLAN_MAX_PACKET_SIZE);
			if (lolan_processGet(&lctx,&lp,&replyPacket)) {
			    llSendPacket(fd,&replyPacket);
			}
			free(replyPacket.payload);
		    } else if (lp.packetType == LOLAN_SET) {
			lolan_Packet replyPacket;
			memset(&replyPacket,0,sizeof(lolan_Packet));
			replyPacket.payload = (uint8_t *) malloc(LOLAN_MAX_PACKET_SIZE);
			if (lolan_processSet(&lctx,&lp,&replyPacket)) {
			    llSendPacket(fd,&replyPacket);
			}
			free(replyPacket.payload);
		    }
		}
	    }
	    free(lp.payload);
	}
    }

    readerThread.join();
    close(fd);
    return 0;
}

