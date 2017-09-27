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

#include "json.hpp"
#include "Slip.hpp"
#include <lolan_config.h>
#include <lolan.h>

#define BAUDRATE B115200

bool quit=false;

void sendToTTyBin(int fd,uint8_t *bp, int s) {
    int w;

/*    while (s>0) {
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
    std::cout << "]";

    sendToTTyBin(fd,(uint8_t *) &(slp.encodeBuffer[0]),slp.encodeBuffer.size());
}

lolan_ctx lctx;
std::mutex dequeMutex;
std::mutex lctxMutex;
std::deque<std::vector<uint8_t>> lpQueue;

void readTTy(int fd)
{
    SlipPacketizer slp;
    uint8_t byte;
    int bytesRead;

    while (quit==false) {
	bytesRead = read(fd, &byte, 1);
	if (bytesRead==1) {
    	    if (slp.feedDecode(byte) == 1) {
    		if (slp.decodeBuffer.size() > 0) {
		    if (slp.decodeBuffer.at(0)=='{') { // this is an ASCII packet, now we ignore
		    } else {
//			std::cout << "\n=>[ ";
//			for (int i=0;i<slp.decodeBuffer.size();i++) {
//	    		    printf("%02X ",(uint8_t) slp.decodeBuffer.at(i));
//			}
//			std::cout << "]";
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
    int fd = 0;

    if (argc < 2) {
	std::cout << "usage: lolan-client [serial port] [lolan address] [GET/SET/INFORM] \"{payload}\"\n";
    }

    fd = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC);
    if (fd == -1) {
        std::cerr << "error opening file" << std::endl;
        return -1;
    } else {
	std::cout << "opened " << std::string(argv[1]) << "\n";
    }

    int toAddress = std::stol(std::string(argv[2]));
    std::string cmd = std::string(argv[3]);
    
    std::string cbor = std::string(argv[4]);
//    std::cout << "cbor=" << cbor << "\n" << std::flush;
    struct termios newtio;
    memset(&newtio, 0, sizeof(newtio));
    struct termios oldtio;
    tcgetattr(fd, &oldtio);

    newtio = oldtio;
    newtio.c_cflag |= (CLOCAL | CREAD);
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    newtio.c_cflag &= ~PARENB;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CRTSCTS;

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

    lolan_init(&lctx,11);

    std::thread readerThread = std::thread( [&]{ readTTy(fd); } );

    lolan_Packet lp;

    memset(&lp,0,sizeof(lolan_Packet));
    lp.fromId = lctx.myAddress;
    lp.toId = toAddress;
    lp.packetCounter=1;
    if (cmd=="GET") {
	lp.packetType = LOLAN_GET;
    } else if (cmd=="SET") {
	lp.packetType = LOLAN_SET;
    } else if (cmd=="INFORM") {
	lp.packetType = LOLAN_INFORM;
    }

    nlohmann::json j = nlohmann::json::parse(cbor);
    std::vector<std::uint8_t> v_cbor = nlohmann::json::to_cbor(j);

    lp.payload = &(v_cbor[0]);
    lp.payloadSize = v_cbor.size();
    llSendPacket(fd,&lp);

    usleep(100000);

    auto start = std::chrono::steady_clock::now();
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
    	    std::cout << "\n=>[ ";
    	    uint8_t buff[130];
    	    uint8_t *b = buff;
    	    for (auto& it : lpbuff) {
		*b = it;
		printf("%02X ",*b);
		b++;
    	    }
    	    std::cout << "]";
	    lolan_Packet rlp;
	    memset(&rlp,0,sizeof(lolan_Packet));
	    rlp.payload=(uint8_t *) malloc(LOLAN_MAX_PACKET_SIZE);
            if (lolan_parsePacket(&lctx,buff,lpbuff.size(),&rlp)==1) {
		if ((rlp.packetType==ACK_PACKET) && (rlp.fromId == lp.toId) && (rlp.toId == lp.fromId) && (rlp.packetCounter==lp.packetCounter)) {
		    std::cout << "\n reply caught" << std::flush;
		    std::vector<uint8_t> v_cbor;
		    v_cbor.resize(rlp.payloadSize);
		    v_cbor.assign(rlp.payload,rlp.payload+rlp.payloadSize);
		    nlohmann::json j_from_cbor = nlohmann::json::from_cbor(v_cbor);
		    std::cout << " cbor="<<j_from_cbor << std::flush;
		    quit=true;
		}
	    }
	    free(rlp.payload);
	}
    }

    std::cout << "\n" << std::flush;

    /* read from stdin and send it to the serial port */
    readerThread.join();
    close(fd);
    return 0;
}

