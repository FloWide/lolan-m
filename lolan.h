/*
 * lolan.h
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "cn-cbor.h"


#ifndef LOLAN_H_
#define LOLAN_H_

#define LOLAN_MAX_PACKET_SIZE	64

#define LOLAN_REGMAP_SIZE	20
// the maximum number of registers to be mapped. This will result in LOLAN_REG_MAP_SIZE*8 byte data reserved

#define LOLAN_REGMAP_TYPE_MASK				0x0F

#define LOLAN_REGMAP_LOCAL_UPDATE_BIT		0x80
#define LOLAN_REGMAP_TRAP_REQUEST_BIT		0x40
#define LOLAN_REGMAP_GET_REQUEST_BIT		0x20
#define LOLAN_REGMAP_REMOTE_UPDATE_BIT		0x10


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
	BEACON_PACKET=0,
	DATA_PACKET=1,
	ACK_PACKET=2,
	MAC_PACKET=3,
	LOLAN_INFORM=4,
	LOLAN_GET=5,
	LOLAN_SET=6,
	LOLAN_CONTROL=7
} lolan_PacketType;


// LoLaN packet
typedef struct {
	lolan_PacketType packetType;
	uint8_t securityEnabled;
	uint8_t framePending;
	uint8_t ackRequired;
	uint8_t bytesToBoundary;
	uint16_t fromId;
	uint16_t toId;
	uint32_t timeStamp;
	uint8_t extTimeStamp;
	uint8_t *payload;
	int16_t payloadSize;
	uint8_t *mac;
} lolan_Packet;

typedef struct {
	uint8_t p[3];
	uint8_t flags;
	void *data;
} lolan_RegMap;

typedef struct {
	uint16_t myAddress;
	lolan_RegMap regMap[LOLAN_REGMAP_SIZE];
	void (*replyDeviceCallbackFunc)(uint8_t *buf,uint8_t size);
	uint8_t networkKey[16];
	uint8_t nodeIV[16];
} lolan_ctx;

int8_t lolan_regVar(lolan_ctx *ctx,uint8_t p0,uint8_t p1,uint8_t p2,lolan_VarType vType, void *ptr);
int8_t lolan_rmVar(lolan_ctx *ctx,uint8_t p0,uint8_t p1,uint8_t p2);

void lolan_sendPacket(lolan_ctx *ctx, lolan_Packet *lp);

int8_t lolan_parsePacket(lolan_ctx *ctx,uint8_t *rxp, uint8_t rxp_len, lolan_Packet *lp);
void lolan_init(lolan_ctx *ctx,uint16_t lolan_address);
void lolan_setReplyDeviceCallback(lolan_ctx *ctx,void (*callback)(uint8_t *buf,uint8_t size));


uint8_t lolan_processGet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply);

#if defined (__cplusplus)
}
#endif

#endif /* LOLAN_H_ */
