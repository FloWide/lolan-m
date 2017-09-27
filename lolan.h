/*
 * lolan.h
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lolan_config.h"

#ifndef LOLAN_H_
#define LOLAN_H_

#define LOLAN_REGMAP_TYPE_MASK			0x0F

#define LOLAN_REGMAP_LOCAL_UPDATE_BIT		0x80
#define LOLAN_REGMAP_TRAP_REQUEST_BIT		0x40
#define LOLAN_REGMAP_GET_REQUEST_BIT		0x20
#define LOLAN_REGMAP_REMOTE_UPDATE_BIT		0x10

#define LOLAN_PACKET(packet) \
	     lolan_Packet packet; \
	     memset(&packet,0,sizeof(lolan_Packet)); \
		uint8_t (packet ## _lolan_pl_buf)[LOLAN_MAX_PACKET_SIZE]; \
		memset((packet ## _lolan_pl_buf), 0, LOLAN_MAX_PACKET_SIZE); \
		packet.payload = (packet ## _lolan_pl_buf);

#if defined (__cplusplus)
extern "C" {
#endif

typedef enum {
	LOLAN_INT=1,
	LOLAN_STR=2,
	LOLAN_FLOAT=3
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

typedef enum {
	TIMING_PACKET=1,
	RESEND_REQUEST_PACKET=2
} lolan_ControlPacketType;

// LoLaN packet
typedef struct {
	lolan_PacketType packetType;
	uint8_t securityEnabled;
	uint8_t framePending;
	uint8_t ackRequired;
	uint8_t bytesToBoundary;
	uint8_t packetCounter;
	uint8_t routingRequested;
	uint8_t packetRouted;
	uint16_t fromId;
	uint16_t toId;
	uint32_t timeStamp;
	uint8_t extTimeStamp;
	uint8_t *payload;
	int16_t payloadSize;
	uint8_t *mac;
} lolan_Packet;

typedef struct {
	uint8_t p[LOLAN_REGMAP_DEPTH];
	uint8_t flags;
	uint8_t size;
	void *data;
} lolan_RegMap;

typedef struct {
	uint16_t myAddress;
	uint8_t packetCounter;
	lolan_RegMap regMap[LOLAN_REGMAP_SIZE];
	void (*replyDeviceCallbackFunc)(uint8_t *buf,uint8_t size);
	uint8_t networkKey[16];
	uint8_t nodeIV[16];
} lolan_ctx;

int8_t lolan_regVar(lolan_ctx *ctx,const uint8_t *p,lolan_VarType vType, void *ptr, uint8_t size);
int8_t lolan_rmVar(lolan_ctx *ctx,void *ptr);
int8_t lolan_regVarUpdated(lolan_ctx *ctx,void *ptr,int clearFlag);

int8_t lolan_createPacket(lolan_ctx *ctx, lolan_Packet *lp, uint8_t *buf, int *size, int withCRC);

int8_t lolan_parsePacket(lolan_ctx *ctx,uint8_t *rxp, uint8_t rxp_len, lolan_Packet *lp);
void lolan_init(lolan_ctx *ctx,uint16_t lolan_address);

uint8_t lolan_processGet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply);
int8_t lolan_setupGet(lolan_ctx *ctx,lolan_Packet *lp, uint16_t toId, const uint8_t *p);

uint8_t lolan_processSet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply);


#if defined (__cplusplus)
}
#endif

#endif /* LOLAN_H_ */
