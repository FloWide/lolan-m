/*
 * lolan.c
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */


#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"

#include <stdint.h>
#include <stdio.h>


int8_t lolan_regVar(lolan_ctx *ctx,const uint8_t *p,lolan_VarType vType, void *ptr)
{
	int i;
	// TODO: only one p path can be in the map. Check for this also
	for (i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] == 0) {
			memcpy(ctx->regMap[i].p,p,LOLAN_REGMAP_DEPTH);
			ctx->regMap[i].flags = vType;
			ctx->regMap[i].data = ptr;
			return 1;
		}
	}
	return -1;
}

int8_t lolan_rmVar(lolan_ctx *ctx,const uint8_t *p)
{
	int i;
	for (i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
			memset(ctx->regMap[i].p,0,LOLAN_REGMAP_DEPTH);
			ctx->regMap[i].flags = 0;
		}
	}
	return -1;
}

int8_t lolan_setFlags(lolan_ctx *ctx,void *ptr, uint8_t flag)
{
	int i;
	for (i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] != 0) {
			if (ctx->regMap[i].data == ptr) {
				ctx->regMap[i].flags |= flag;
			}
			return 1;
		}
	}
	return -1;
}

int8_t lolan_clearFlags(lolan_ctx *ctx,void *ptr, uint8_t flag)
{
	int i;
	for (i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] != 0) {
			if (ctx->regMap[i].data == ptr) {
				ctx->regMap[i].flags &= ~(flag);
			}
			return 1;
		}
	}
	return -1;
}

void lolan_init(lolan_ctx *ctx,uint16_t lolan_address)
{
	ctx->replyDeviceCallbackFunc = NULL;
	ctx->myAddress = lolan_address;
	ctx->packetCounter = 1;
//	memcpy(ctx->networkKey,networkKey,16);
//	memcpy(ctx->nodeIV,nodeIV,16);
}

/**************************************************************************//**
 * @brief
 *   create the packet with CRC in the provided buffer
 * @param[in] ctx
 *   context for lolan packet processing
 * @param[in] lp
 *   pointer to lolan Packet structure
 * @param[out] buf
 *   buffer to fill
 * @param[out] size
 *   size of the created packet
 * @param[in] withCRC
 *   append software calculated CRC to the packet
 *****************************************************************************/
int8_t lolan_createPacket(lolan_ctx *ctx, lolan_Packet *lp, uint8_t *buf, int *size, int withCRC)
{
	buf[0] = lp->packetType;
	if (lp->securityEnabled) { buf[0]|=0x08; }
	if (lp->framePending) { buf[0]|=0x10; }
	if (lp->ackRequired) { buf[0]|=0x20;}

	buf[1]=0x74; // 802.15.4 protocol version=3
	if (lp->routingRequested) {
	    buf[1] |= 0x80;
	}
	if (lp->packetRouted) {
	    buf[1] |= 0x08;
	}

	buf[2] = lp->packetCounter;

	buf[3] = (lp->fromId)&0xFF;
	buf[4] = (lp->fromId>>8)&0xFF;
	buf[5] = (lp->toId)&0xFF;
	buf[6] = (lp->toId>>8)&0xFF;

	memcpy(&(buf[7]),lp->payload,lp->payloadSize);
	if (withCRC) {
	    uint16_t crc16 = CRC_calc(buf,7+lp->payloadSize);
	    buf[7+lp->payloadSize] = (crc16>>8)&0xFF;
	    buf[7+lp->payloadSize+1] = crc16&0xFF;
	    *size = lp->payloadSize+9;
	} else {
	    *size = lp->payloadSize+7;
	}

	return 1;
}

/**************************************************************************//**
 * @brief
 *   parse incoming packet to a lolan packet structure
 *   IMPORTANT: lp->payload hat to point to a memory space enough for LOLAN_MAX_PACKET_SIZE
 * @param[in] ctx
 *   context for lolan packet processsing
 * @param[in] rxp
 *   rx packet bytes
 * @param[in] rxp_len
 *   rx packet len
 * @param[out] lp
 *   pointer to lolan Packet structure
 * @return
 *   parse result
 *   	1 : success
 *   	0 : not LoLaN packet
 *     -1 : parse error (wrong LoLaN packet format)
 *     -2 : auth error / crc error
 *     -3 : process error
 *****************************************************************************/
int8_t lolan_parsePacket(lolan_ctx *ctx,uint8_t *rxp, uint8_t rxp_len, lolan_Packet *lp)
{
	if (((rxp[1]>>4)&0x3) != 3) { // Checking 802.15.4 FRAME version
		return 0;
	}

	if ((rxp[1]&0x80) != 0) { lp->routingRequested=1; } else { lp->routingRequested=0; }

	if ((rxp[1]&0x08) != 0) { lp->packetRouted=1; } else { lp->packetRouted=0; }

	lp->packetType = rxp[0]&0x07;
	if (!((lp->packetType == ACK_PACKET) || (lp->packetType == LOLAN_INFORM) || (lp->packetType == LOLAN_GET) || (lp->packetType == LOLAN_SET) || (lp->packetType == LOLAN_CONTROL))) {
		return 0;
	}

	if ((rxp[0]&0x08) != 0) {lp->securityEnabled=1;} else {lp->securityEnabled=0;} //ENC
	if ((rxp[0]&0x10) != 0) {lp->framePending=1;} else {lp->framePending=0;} //FIXME: implement extended frames
	if ((rxp[0]&0x20) != 0) {lp->ackRequired=1;} else {lp->ackRequired=0;}

	lp->packetCounter = rxp[2];

	lp->fromId = rxp[3] | (rxp[4]<<8);
	lp->toId =  rxp[5] | (rxp[6]<<8);

	if (lp->securityEnabled) {
//TODO: implement security
	} else {
		uint16_t crc16 = CRC_calc(rxp,rxp_len);

		if (crc16 != 0) {
			DLOG(("\n lolan_parsePacket(): CRC error\n "));
			DLOG(("\n CRC16: %04x",crc16));
			return -2;
		}

		lp->payloadSize = rxp_len-9;
		memcpy(lp->payload,&(rxp[7]),lp->payloadSize);
	}

	DLOG(("\n LoLaN packet t:%d s:%d ps:%d from:%d to:%d enc:%d",lp->packetType,rxp_len,lp->payloadSize,lp->fromId,lp->toId,lp->securityEnabled));

	return 1; // successful parsing
}


