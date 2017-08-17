/*
 * lolan.h
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef LOLAN_H_
#define LOLAN_H_

#define LOLAN_REGMAP_SIZE	20
// the maximum number of registers to be mapped. This will result in LOLAN_REG_MAP_SIZE*8 byte data reserved

#define LOLAN_REGMAP_TYPE_MASK				0x1F

#define LOLAN_REGMAP_UPDATE_BIT				0x80
#define LOLAN_REGMAP_TRAP_REQUEST_BIT		0x40
#define LOLAN_REGMAP_GET_REQUEST_BIT		0x20

#if defined (__cplusplus)
extern "C" {
#endif

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
	uint8_t encrypted;
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

typedef struct {
	uint16_t myAddress;
	lolan_RegMap regMap[LOLAN_REGMAP_SIZE];
	uint16_t (*replyDeviceCallbackFunc)(uint8_t *buf,uint8_t size);
	uint8_t networkKey[16];
	uint8_t nodeIV[16];
} lolan_ctx;

int8_t lolan_parsePacket(lolan_ctx *ctx,uint8_t *rxp, lolan_Packet *lp);
void lolan_init(lolan_ctx *ctx,uint16_t lolan_address);
void lolan_setReplyDeviceCallback(lolan_ctx *ctx,uint16_t (*callback)(uint8_t *buf,uint8_t size));

#if defined (__cplusplus)
}
#endif

#endif /* LOLAN_H_ */
