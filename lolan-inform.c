/*
 * lolan-get.c
 *
 *  Created on: Aug 19, 2017
 *      Author: gabor
 */


#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"

#include <stdint.h>

/**************************************************************************//**
 * @brief
 *   process INFORM request
 * @param[in] ctx
 *   pointer to lolan Packet structure
 * @param[out] reply
 *   generated reply packet
 * @return
 *   parse result
 *   	1 : need to inform, reply packet generated
 *      0 : there is nothing to inform
 *     -1 : CBOR encode error
 *****************************************************************************/
int8_t lolan_processInform(lolan_ctx *ctx, lolan_Packet *reply)
{
    int i;
    CborError err;

    uint8_t p[LOLAN_REGMAP_DEPTH+1];
    memset(p,0,LOLAN_REGMAP_DEPTH+1);
    int8_t found=0;

    for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
	if (ctx->regMap[i].p[0] == 0) {
	    continue; // free slot of regmap
	}
        if ((ctx->regMap[i].flags & (LOLAN_REGMAP_INFORM_REQUEST_BIT|LOLAN_REGMAP_TRAP_REQUEST_BIT)) != 0) {
    	    if ((ctx->regMap[i].flags & (LOLAN_REGMAP_LOCAL_UPDATE_BIT)) != 0) {
		memcpy(p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH);
		ctx->regMap[i].flags &= ~(LOLAN_REGMAP_LOCAL_UPDATE_BIT);
		found = 1;
		break;
	    }
        }
    }

    if (found==0) {
	return 0;
    }

    CborEncoder enc;
    cbor_encoder_init(&enc,reply->payload,LOLAN_MAX_PACKET_SIZE,0);

    CborEncoder map_enc;
    err = cbor_encoder_create_map(&enc,&map_enc,2);
    if (err) {
	DLOG(("\n cbor encode error"));
	return -1;
    }

    err = cbor_encode_int (&map_enc,0);
    if (err) {
	DLOG(("\n cbor encode error"));
	return -1;
    }

    CborEncoder array_enc;
    err = cbor_encoder_create_array(&map_enc,&array_enc,LOLAN_REGMAP_DEPTH);
    if (err) {
	DLOG(("\n cbor encode error"));
	return -1;
    }

    int k;
    for (k=0;k<LOLAN_REGMAP_DEPTH;k++) {
	if (p[k]==0) {
	    break;
	}
    }
    k--;
    int key = p[k];
    p[k]=0;

    int j;
    for (j=0;j<LOLAN_REGMAP_DEPTH;j++) {
	err = cbor_encode_int (&array_enc,p[j]);
	if (err) {
	    DLOG(("\n cbor encode error"));
	    return -1;
	}
    }
    cbor_encoder_close_container(&map_enc, &array_enc);

    err = cbor_encode_int (&map_enc,key);
    if (err) {
	DLOG(("\n cbor encode error"));
	return -1;
    }

    if ((ctx->regMap[i].flags & 0x7) == LOLAN_STR) { // important, if full match, we consider there is only one instance in regMap
	err = cbor_encode_text_string(&map_enc, (const char *) ctx->regMap[i].data, strlen((const char *) ctx->regMap[i].data));
	if (err) {
	    DLOG(("\n cbor encode error"));
	    return -1;
	}
    } else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT) {
	if (ctx->regMap[i].size==1) {
	    int8_t *val = ((int8_t *) ctx->regMap[i].data);
	    err = cbor_encode_int (&map_enc, *val);
	    if (err) {
		DLOG(("\n cbor encode error"));
		return -1;
	    }
	} else if (ctx->regMap[i].size == 2) {
	    int16_t *val = ((int16_t *) ctx->regMap[i].data);
	    err = cbor_encode_int (&map_enc, *val);
	    if (err) {
		DLOG(("\n cbor encode error"));
		return -1;
	    }
	} else if (ctx->regMap[i].size == 4) {
	    int32_t *val = ((int32_t *) ctx->regMap[i].data);
	    err = cbor_encode_int (&map_enc, *val);
	    if (err) {
		DLOG(("\n cbor encode error"));
		return -1;
	    }
	}
    }
    cbor_encoder_close_container(&enc, &map_enc);

    reply->packetCounter = ctx->packetCounter++;
    reply->packetType = LOLAN_INFORM;
    reply->fromId = ctx->myAddress;
    reply->toId = 0xFFFF;
    reply->payloadSize = cbor_encoder_get_buffer_size(&enc,reply->payload);
    DLOG(("\n Encoded inform to %d bytes",reply->payloadSize));
    return 1;
}

