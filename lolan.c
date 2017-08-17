/*
 * lolan.c
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */

#include "lolan.h"
#include "cn-cbor.h"

#ifdef PLATFORM_EFM32
	#include "em_aes.h"
#else
	#include "aes.h"
#endif

static uint8_t nodeIV[] = 	 	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	0x00,
					   	  0x82, 0x38, 0xF7, 0xE1, 0xA5, 0x3C, 0x4E, 0xC9}; // this part is random, and public known

static uint8_t networkKey[] =	{ 0xF2, 0x66, 0x37, 0x69, 0x01, 0x3E, 0x43, 0x62,
						  0xBE, 0x16, 0x24, 0xE4, 0xFF, 0xC0, 0x64, 0xC6};

#define LOLAN_REGMAP_SIZE	20		// the maximum number of registers to be mapped. This will result in LOLAN_REG_MAP_SIZE*8 byte data reserved


#define LOLAN_INT8			1
#define LOLAN_INT16			2
#define LOLAN_INT32			3
#define LOLAN_STR			4
#define LOLAN_FLOAT			5

#define LOLAN_REGMAP_TYPE_MASK				0x1F

#define LOLAN_REGMAP_UPDATE_BIT				0x80
#define LOLAN_REGMAP_TRAP_REQUEST_BIT		0x40
#define LOLAN_REGMAP_GET_REQUEST_BIT		0x20

static lolan_RegMap regMap[LOLAN_REGMAP_SIZE];

uint16_t (*lolan_replyDeviceCallbackFunc)(uint8_t *buf,uint8_t size);

static uint16_t myAddress;


int8_t lolan_regVar(uint8_t p0,uint8_t p1,uint8_t p2,lolan_PacketType pType, void *ptr)
{
	// TODO: only one p0/p1/p2 var can be in the map. Check for this also
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (regMap[i].p[0] == 0) {
			regMap[i].p[0] = p0;
			regMap[i].p[1] = p1;
			regMap[i].p[2] = p2;
			regMap[i].flags = pType;
			regMap[i].data = ptr;
			return 1;
		}
	}
	return -1;
}

int8_t lolan_rmVar(uint8_t p0,uint8_t p1,uint8_t p2,lolan_PacketType pType, void *ptr)
{
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if ((regMap[i].p[0] == p0) && (regMap[i].p[1] == p1) && (regMap[i].p[2] == p2)) {
			regMap[i].p[0] = 0;
			regMap[i].p[1] = 0;
			regMap[i].p[2] = 0;
			regMap[i].flags = 0;
		}
	}
	return -1;
}

int8_t lolan_updateVar(void *ptr)
{
	for (int i=0; i<LOLAN_REGMAP_SIZE;i++) {
		if (regMap[i].p[0] != 0) {
			if (regMap[i].data == ptr) {
				regMap[i].flags |= LOLAN_REGMAP_UPDATE_BIT;
			}
			return 1;
		}
	}
	return -1;
}

void lolan_init(uint16_t lolan_address)
{
	lolan_replyDeviceCallbackFunc = NULL;
	myAddress = lolan_address;
}

void lolan_setReplyDeviceCallback(uint16_t (*callback)(uint8_t *buf,uint8_t size))
{
	lolan_replyDeviceCallbackFunc = callback;
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
 *   parse incoming packet to a lolan packet structure
 * @param[in] rxp
 *   rx packet bytes
 * @param[in] rxp_len
 *   rx packet len
 * @param[out] vsp
 *   pointer to vsun Packet structure
 * @return
 *   parse result
 *   	2 : success, and processed
 *   	1 : success, not processed
 *     -1 : wrong packet format
 *     -2 : auth error / crc error
 *     -3 : process error
 *****************************************************************************/
int8_t lolan_parsePacket(uint8_t *rxp, lolan_Packet *lp)
{
	if ((rxp[0] & 0x40) != 0) {
		return -1;
	}

	lp->packetType = rxp[0]&0x03;
	lp->packetSize = (rxp[0]>>3)&0x03;
	if ((rxp[0]&0x80) != 0) {lp->encrypted=1;} else {lp->encrypted=0;} //ENC

	lp->fromId = *((uint16_t *) &rxp[2]);
	lp->toId = *((uint16_t *) &rxp[4]);

	if (lp->encrypted) {
		lp->timeStamp = (rxp[9]<<24)|(rxp[8]<<16)|(rxp[7]<<8)|(rxp[6]);
		lp->extTimeStamp = rxp[10];

		uint8_t aes_cntr[16];
		uint8_t hmac[16];
		uint8_t i;
		uint8_t x=0;
		memcpy(lp->mac,&(rxp[27]),5);

		memset(hmac,16,0);
		hmac_md5(&(rxp[0]),27,networkKey,16,hmac);
		for (i=0;i<5;i++) {
			x |= lp->mac[i] - hmac[i];
		}

		if (x!=0) {
//				SWOPrintStr("\n lolan_parsePacket(): HMAC verification error!");
//				SWOPrintStr("\n lolan_parsePacket(): rx_packet: ");SWOPrintHex8(rxp,32);
//				SWOPrintStr("\n lolan_parsePacket(): rmac: ");SWOPrintHex8(vsp->mac,5);
//				SWOPrintStr("\n lolan_parsePacket(): cmac: ");SWOPrintHex8(hmac,5);
			return -2; // hmac verification failed
		}

		AES_CTR_Setup(aes_cntr,nodeIV,lp->fromId,lp->timeStamp,lp->extTimeStamp);

#ifdef PLATFORM_EFM32
		AES_CTR128(lp->payloadBuff,&(rxp[11]),16,networkKey,aes_cntr,&AES_CTRUpdate8Bit);
#else
		aes_ctr_encrypt(networkKey, 16,&(rxp[11]), 16, aes_cntr);
		memcpy(lp->payloadBuff,&(rxp[11]),16);
#endif

	} else {
		memcpy(lp->payloadBuff,&(rxp[6]),16);
		uint16_t crc16 = CRC_calc(rxp,24);
		//SWOPrintStr("\n CRC16: ");SWOPrintHex8(&crc16,2);

		if (crc16 != 0) {
			SWOPrintStr("\n lolan_parsePacket(): CRC error\n ");
			return -2;
		}
	}

	if ((lp->packetType == LOLAN_GET) || (lp->packetType == LOLAN_SET) || (lp->packetType == LOLAN_TRAP) || (lp->packetType == LOLAN_INFORM)) {
		if ((lp->payloadBuff[0] & 0x80)!=0) {
			lp->payloadBuff[0]&=0x7F;
			SWOPrintStr("\n lolan_parsePacket(): ext payloadSize\n");
			lp->payloadSize = *((uint16_t *) &(lp->payloadBuff));
			lp->payload = (uint8_t *) &(lp->payloadBuff[2]);
		} else {
			lp->payloadBuff[0]&=0x7F;
			lp->payloadSize = lp->payloadBuff[0];
			lp->payload = (uint8_t *) &(lp->payloadBuff[1]);
		}
	}

	SWOPrintStr("\n lolan_parsePacket(): success\n");
	if (lp->encrypted==1) 	{SWOPrintStr("encrypted: 1\n");}
	char tmp[30];
	sprintf(tmp,"t:%d s:%d ps:%d from:%d to:%d",lp->packetType,lp->packetSize,lp->payloadSize,lp->fromId,lp->toId);
	SWOPrintStr(tmp);

	if (lp->toId == myAddress) {
		if (lp->packetType == LOLAN_GET) {
			cn_cbor *cb;
			cn_cbor_errback err;
			cb = cn_cbor_decode(lp->payload, lp->payloadSize , &err);
			if (cb != NULL) {
				uint8_t p[3];
				memset(p,0,3);
				if (cb->length > 3) { return -3; } // ERROR: too long GET path
				for (int i=0;i<cb->length;i++) {
					 cn_cbor *val = cn_cbor_index(cb, i);
					 if (val->type == CN_CBOR_UINT) {
						 p[i] = val->v.uint;
					 } else {
						 return -3;  // ERROR: unknow type in GET path
					 }
				}
			} else {
				return -3;
			}

			return 2; // successfull processing
		}
	}

	return 1; // successful parsing
}







