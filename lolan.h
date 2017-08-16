/*
 * lolan.h
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */

#include "em_aes.h"
#include "em_device.h"

#include "hmac.h"
#include "md5.h"

#include <string.h>
#include <stdlib.h>

#ifndef LOLAN_H_
#define LOLAN_H_

typedef enum {
	LOLAN_INT8=1,
	LOLAN_INT16=2,
	LOLAN_INT32=3,
	LOLAN_STR=4,
	LOLAN_FLOAT=5
} lolan_VarType;

typedef enum {
	LOLAN_TRAP=0,
	LOLAN_INFORM=1,
	LOLAN_GET=2,
	LOLAN_SET=3,
	LOLAN_ACK=4,
	LOLAN_RETRANSMIT_REQ=5,
	LOLAN_CONTROL=6,
	LOLAN_OTHER_PROTOCOL=7
} lolan_PacketType;

typedef enum {
	PKT_SMALL=0,
	PKT_S1_EX=1,
	PKT_S2_EX=2,
	PKT_S3_EX=3,
	PKT_S4_EX=4,
	PKT_S8_EX=5,
	PKT_S13_EX=6,
	PKT_FRAGMENT=7
} lolan_PacketSize;

// LoLaN packet
typedef struct {
	lolan_PacketType packetType;
	lolan_PacketSize packetSize;
	bool encrypted;
	uint16_t fromId;
	uint16_t toId;
	uint32_t timeStamp;
	uint8_t extTimeStamp;
	uint8_t *payload;
	uint16_t payloadSize;
	uint8_t payloadBuff[30];
	uint8_t mac[5];
} lolan_Packet;

typedef struct {
	uint8_t p[3];
	uint8_t flags;
	void *data;
} lolan_RegMap;

uint16_t CRC_calc(uint8_t *start, uint8_t size);
void AES_CTRUpdate8Bit(uint8_t *ctr);

int8_t lolan_parsePacket(uint8_t *rxp, lolan_Packet *lp);
void lolan_init(uint16_t lolan_address);
void lolan_setReplyDeviceCallback(uint16_t (*callback)(uint8_t *buf,uint8_t size));


#endif /* LOLAN_H_ */
