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

static const unsigned short CRC_CCITT_TABLE[256] =
{
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t CRC_calc(uint8_t *buf, uint8_t size)
{
    uint16_t tmp;
    uint16_t crc = 0xffff;

    for (int i=0; i < size ; i++)
    {
        tmp = (crc >> 8) ^ buf[i];
        crc = (crc << 8) ^ CRC_CCITT_TABLE[tmp];
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

