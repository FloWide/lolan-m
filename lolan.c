/*
 * lolan.c
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */


#include "lolan_config.h"
#include "lolan.h"


#include <stdint.h>

#ifdef PLATFORM_EFM32
#include "em_aes.h"
#else
#include "aes.h"
#include "aes_wrap.h"
#endif

#include "hmac.h"


uint16_t CRC_calc(uint8_t *start, uint8_t size);
void AES_CTRUpdate8Bit(uint8_t *ctr);

static const uint8_t nodeIV[] = 	 	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					   	  0x82, 0x38, 0xF7, 0xE1, 0xA5, 0x3C, 0x4E, 0xC9}; // this part is random, and public known

static const uint8_t networkKey[] =	{ 0xF2, 0x66, 0x37, 0x69, 0x01, 0x3E, 0x43, 0x62,
						  0xBE, 0x16, 0x24, 0xE4, 0xFF, 0xC0, 0x64, 0xC6};

int8_t lolan_regVar(lolan_ctx *ctx,const uint8_t *p,lolan_VarType vType, void *ptr)
{
	// TODO: only one p path can be in the map. Check for this also
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
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
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
			memset(ctx->regMap[i].p,0,LOLAN_REGMAP_DEPTH);
			ctx->regMap[i].flags = 0;
		}
	}
	return -1;
}

int8_t lolan_updateVar(lolan_ctx *ctx,void *ptr)
{
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] != 0) {
			if (ctx->regMap[i].data == ptr) {
				ctx->regMap[i].flags |= LOLAN_REGMAP_LOCAL_UPDATE_BIT;
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
	memcpy(ctx->networkKey,networkKey,16);
	memcpy(ctx->nodeIV,nodeIV,16);
}

void lolan_setReplyDeviceCallback(lolan_ctx *ctx,void (*callback)(uint8_t *buf,uint8_t size))
{
	ctx->replyDeviceCallbackFunc = callback;
}

uint16_t CRC_calc(uint8_t *start, uint8_t size)
{
  uint16_t crc = 0x0;
  uint8_t  *data;
  uint8_t *end;
  end = start+size;

  for (data = start; data < end; data++)
  {
    crc  = (crc >> 8) | (crc << 8);
    crc ^= *data;
    crc ^= (crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0xff) << 5;
  }
  return crc;
}

void AES_CTRUpdate8Bit(uint8_t *ctr)
{
	(*ctr)++;
}

void AES_CTR_Setup(uint8_t *ctr,uint8_t *iv, uint16_t nodeId, uint32_t systime, uint8_t ext_systime)
{
	memcpy(ctr,iv,16);
	ctr[0]=0;
	ctr[1]=ext_systime;
	ctr[2]=(systime>>24)&0xFF;
	ctr[3]=(systime>>16)&0xFF;
	ctr[4]=(systime>>8)&0xFF;
	ctr[5]=(systime)&0xFF;
	ctr[6]=(nodeId>>8)&0xFF;
	ctr[7]=(nodeId)&0xFF;
}

/**************************************************************************//**
 * @brief
 *   send packet with the packet transmitting callback
 * @param[in] ctx
 *   context for lolan packet processing
 * @param[in] lp
 *   pointer to lolan Packet structure
 *****************************************************************************/
void lolan_sendPacket(lolan_ctx *ctx, lolan_Packet *lp)
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

	DLOG(("\n Sending packet with size=%d \n",7+lp->payloadSize+2));
	for (int i=0;i<(7+lp->payloadSize+2);i+=4) {
		DLOG((" %02x %02x %02x %02x",txp[i],txp[i+1],txp[i+2],txp[i+3]));
	}

	if (ctx->replyDeviceCallbackFunc != NULL) {
		ctx->replyDeviceCallbackFunc(txp,7+lp->payloadSize+2);
	}
}

/**************************************************************************//**
 * @brief
 *   parse incoming packet to a lolan packet structure
 *   IMPORTANT: lp->payload will point to memory allocated for the packet. If it is not null at start, free is called on it.
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
 *   	2 : success (packet valid), and processed (we are the consignee and command served, ack packet sent)
 *   	1 : success, not processed (packet valid, but addressed to another node)
 *   	0 : not LoLaN packet
 *     -1 : parse error (wrong LoLaN packet format)
 *     -2 : auth error / crc error
 *     -3 : process error
 *****************************************************************************/
int8_t lolan_parsePacket(lolan_ctx *ctx,uint8_t *rxp, uint8_t rxp_len, lolan_Packet *lp)
{
	if (lp->payload != NULL) { // free payload buffer if it points to somewhere
		free(lp->payload);
		lp->payload = NULL;
	}

	if (((rxp[1]>>4)&0x3) != 3) { // Checking 802.15.4 FRAME version
		return 0;
	}

	lp->packetType = rxp[0]&0x07;
	if (!((lp->packetType == ACK_PACKET) || (lp->packetType == LOLAN_INFORM) || (lp->packetType == LOLAN_GET) || (lp->packetType == LOLAN_SET) || (lp->packetType == LOLAN_CONTROL))) {
		return 0;
	}

	if ((rxp[0]&0x08) != 0) {lp->securityEnabled=1;} else {lp->securityEnabled=0;} //ENC
	if ((rxp[0]&0x10) != 0) {lp->framePending=1;} else {lp->framePending=0;} //FIXME: implement extended frames
	if ((rxp[0]&0x20) != 0) {lp->ackRequired=1;} else {lp->ackRequired=0;}

	lp->packetCounter = rxp[2];

	lp->fromId = *((uint16_t *) &rxp[3]);
	lp->toId = *((uint16_t *) &rxp[5]);

	if (lp->securityEnabled) {
		lp->bytesToBoundary = ((rxp[1]&0x3)<<2) | ((rxp[0]>>6)&0x3);
		lp->timeStamp = (rxp[10]<<24)|(rxp[9]<<16)|(rxp[8]<<8)|(rxp[7]);
		lp->extTimeStamp = rxp[11];

		uint8_t aes_cntr[16];
		uint8_t hmac[16];
		uint8_t i;
		uint8_t x=0;
		lp->mac = &(rxp[rxp_len-5]);

		memset(hmac,16,0);
		hmac_md5(&(rxp[0]),rxp_len-5,networkKey,16,hmac);
		for (i=0;i<5;i++) {
			x |= lp->mac[i] - hmac[i];
		}

		if (x!=0) {
			DLOG(("\n lolan_parsePacket(): HMAC verification error!"));
			DLOG(("\n lolan_parsePacket(): rx_packet: "));
			for (i=0;i<rxp_len;i+=4) {
				DLOG((" %02x %02x %02x %02x",rxp[i],rxp[i+1],rxp[i+2],rxp[i+3]));
			}
			DLOG(("\n lolan_parsePacket(): rmac: %02x %02x %02x %02x %02x",lp->mac[0],lp->mac[1],lp->mac[2],lp->mac[3],lp->mac[4]));
			DLOG(("\n lolan_parsePacket(): cmac: %02x %02x %02x %02x %02x",hmac[0],hmac[1],hmac[2],hmac[3],hmac[4]));
			return -2; // hmac verification failed
		}

		AES_CTR_Setup(aes_cntr,nodeIV,lp->fromId,lp->timeStamp,lp->extTimeStamp);
		return -1; // TODO: implement multiblock encryption

#ifdef PLATFORM_EFM32
		AES_CTR128(lp->payload,&(rxp[11]),16,networkKey,aes_cntr,&AES_CTRUpdate8Bit);
#else
		aes_ctr_encrypt(networkKey, 16,&(rxp[11]), 16, aes_cntr);
		memcpy(lp->payload,&(rxp[11]),16);
#endif

	} else {
		uint16_t crc16 = CRC_calc(rxp,rxp_len);

		if (crc16 != 0) {
			DLOG(("\n lolan_parsePacket(): CRC error\n "));
			DLOG(("\n CRC16: %04x",crc16));
			return -2;
		}
		lp->payloadSize = rxp_len-9;
		lp->payload = malloc(rxp_len-9);
		memcpy(lp->payload,&(rxp[7]),lp->payloadSize);
	}


	DLOG(("\n LoLaN packet t:%d s:%d ps:%d from:%d to:%d enc:%d",lp->packetType,rxp_len,lp->payloadSize,lp->fromId,lp->toId,lp->securityEnabled));

	if (lp->toId == ctx->myAddress) {
		if (lp->packetType == LOLAN_GET) {
			lolan_Packet replyPacket;
			memset(&replyPacket,0,sizeof(lolan_Packet));
			replyPacket.payload = malloc(LOLAN_MAX_PACKET_SIZE);
			if (lolan_processGet(ctx,lp,&replyPacket)) {
				lolan_sendPacket(ctx,&replyPacket);
			}
			free(replyPacket.payload);
			return 2; // successfull processing
		}
	}

	return 1; // successful parsing
}

