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

int8_t lolan_setIntRegFromCbor(lolan_ctx *ctx,const uint8_t *p, int val)
{
    int8_t found=0;
    int i;
    for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
	if (ctx->regMap[i].p[0] == 0) {
	    continue; // free slot of regmap
	}
	if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
	    if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_INT) {
		if (ctx->regMap[i].size == 1) {
		    int8_t *sv;
		    sv = ctx->regMap[i].data;
		    *sv = val;
		    ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;
		    found = 1;
		} else if (ctx->regMap[i].size == 2) {
		    int8_t *sv;
		    sv = ctx->regMap[i].data;
		    *sv = val;
		    ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;
		    found = 1;
		} else if (ctx->regMap[i].size == 4) {
		    int8_t *sv;
		    sv = ctx->regMap[i].data;
		    *sv = val;
		    ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;
		    found = 1;
		}
	    }
	    break;
	}
    }
    return found;
}

int8_t lolan_setStrRegFromCbor(lolan_ctx *ctx,const uint8_t *p, const char *str)
{
    int8_t found=0;
    int i;
    for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
	if (ctx->regMap[i].p[0] == 0) {
	    continue; // free slot of regmap
	}
	if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
	    if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_STR) {
		char *sv;
		sv = ctx->regMap[i].data;
		strncpy(sv,str,ctx->regMap[i].size);
		ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;
		found = 1;
	    }
	    break;
	}
    }
    return found;
}

/**************************************************************************//**
 * @brief
 *   process SET request
 * @param[in] ctx
 *   context for lolan packet processsing
 * @param[out] lp
 *   pointer to lolan Packet structure
 * @param[out] reply
 *   generated reply packet
 * @return
 *   parse result
 *   	1 : request processed, reply packet generated
 *     -1 : stack unable to handle request (possibly low on memory)
 *     -2 : SET packet parse error
 *****************************************************************************/

int8_t lolan_processSet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply)
{
    int i=0;

    uint8_t p[LOLAN_REGMAP_DEPTH];
    if (getPathFromPayload(lp, p) != 1) {
	DLOG(("\n path not found in packet"));
    }

    DLOG(("\n SET: "));
    for (i=0;i<3;i++) {
        if (p[i]==0) { if (i==0) { DLOG(("/")); } break; }
        DLOG(("/%d",p[i]));
    }

    if (i>(LOLAN_REGMAP_DEPTH-1)) {
	DLOG(("\n path too deep"));
	return -2;
    }

    CborError err;
    uint8_t numberOfKeys=0;
    uint8_t error=0;
    int key=0;

    CborParser parser;
    CborValue it;
    err = cbor_parser_init(lp->payload, lp->payloadSize, 0, &parser, &it);

    if (err) {
	DLOG(("\n cbor parse error"));
	return -2;
    }

    if (cbor_value_get_type(&it) != CborMapType) {
	DLOG(("\n cbor parse error: map not found"));
	return -2;
    }
    size_t mapLen;
    err = cbor_value_get_map_length(&it,&mapLen);
    DLOG(("parsed map length=%d",(int) mapLen));
    if (err) {
	DLOG(("\n cbor parse error"));
	return -2;
    }

    CborEncoder enc;
    cbor_encoder_init(&enc,reply->payload,LOLAN_MAX_PACKET_SIZE,0);
    CborEncoder map_enc;

    if (mapLen<3) {mapLen=1;}

    err = cbor_encoder_create_map(&enc,&map_enc,mapLen);
    if (err) {
	DLOG(("\n cbor parse error"));
	return -2;
    }

    CborValue rit;
    err = cbor_value_enter_container(&it, &rit);
    if (err) {
	DLOG(("\n cbor parse error"));
	return -2;
    }

    while (!cbor_value_at_end(&rit)) {
        int key = -1;
        if (cbor_value_get_type(&rit) != CborIntegerType) {
    	    DLOG(("\n cbor parse error: key has to be integer"));
        }
        cbor_value_get_int(&rit,&key);

        err = cbor_value_advance_fixed(&rit);
        if (err) {
    	    DLOG(("\n cbor parse error"));
    	    return -2;
        }

	if (cbor_value_at_end(&rit)) {
    	    DLOG(("\n cbor parse error"));
    	    return -2;
	}

	if (key==0) { // ignore key 0 that is the path
    	    if (getPathFromCbor(p, &rit)!=1) {
    		DLOG(("\n path decoding error"));
		return -2;
    	    }
	    continue;
	} else {
	    DLOG(("\n key:%d ",key));
	    if (cbor_value_get_type(&rit) == CborByteStringType) {
		size_t len=LOLAN_MAX_PACKET_SIZE;
		uint8_t buf[LOLAN_MAX_PACKET_SIZE];
		err = cbor_value_copy_byte_string(&rit, buf, &len, &rit);
		if (err) {
		    DLOG(("\n cbor parse string error"));
		    return -2;
		}
		DLOG((" value:%s ",buf));
		p[i]=key;
		cbor_encode_int(&map_enc, key);
		if (lolan_setStrRegFromCbor(ctx,p,buf)) {
		    cbor_encode_int(&map_enc, 200);
		} else {
		    cbor_encode_int(&map_enc, 404);
		}
		numberOfKeys++;
		continue;
    	    } else if (cbor_value_get_type(&rit) == CborTextStringType) {
		size_t len=LOLAN_MAX_PACKET_SIZE;
		char buf[LOLAN_MAX_PACKET_SIZE];
		err = cbor_value_copy_text_string(&rit, buf, &len, &rit);
		if (err) {
		    DLOG(("\n cbor parse string error"));
		    return -2;
		}
		DLOG((" value:%s ",buf));
		p[i]=key;
		cbor_encode_int(&map_enc, key);
		if (lolan_setStrRegFromCbor(ctx,p,buf)) {
		    cbor_encode_int(&map_enc, 200);
		} else {
		    cbor_encode_int(&map_enc, 404);
		}
		numberOfKeys++;
		continue;
    	    } else if (cbor_value_get_type(&rit) == CborIntegerType) {
		int val;
		cbor_value_get_int(&rit,&val);
		DLOG((" value: %d",val));
		p[i]=key;
		cbor_encode_int(&map_enc, key);
		if (lolan_setIntRegFromCbor(ctx,p,val)) {
		    cbor_encode_int(&map_enc, 200);
		} else {
		    cbor_encode_int(&map_enc, 404);
		}
		numberOfKeys++;
	    } else {
		cbor_encode_int(&map_enc, key);
		cbor_encode_int(&map_enc, 400);
		DLOG((" value: unsupported type"));
		numberOfKeys++;
		// unsupported type
	    }
	}

        err = cbor_value_advance_fixed(&rit);
        if (err) {
    	    DLOG(("\n cbor parse error"));
    	    return -2;
        }
    }


    cbor_encode_int(&map_enc, 0);
    if (numberOfKeys > 1) {
	cbor_encode_int(&map_enc, 207);
    } else {
	// resource not found
	if (numberOfKeys==1) {
	    if (error) {
		cbor_encode_int(&map_enc, 500);
	    } else {
		cbor_encode_int(&map_enc, 200);
	    }
	} else {
	    cbor_encode_int(&map_enc, 404);
	}
    }
    cbor_encoder_close_container(&enc, &map_enc);

    reply->packetCounter = lp->packetCounter;
    reply->packetType = ACK_PACKET;
    reply->fromId = lp->toId;
    reply->toId = lp->fromId;
    reply->payloadSize = cbor_encoder_get_buffer_size(&enc,reply->payload);
    DLOG(("\n Encoded reply to %d bytes",reply->payloadSize));
    return 1;
}


/**************************************************************************//**
 * @brief
 *   process SET request
 * @param[in] ctx
 *   context for lolan packet processsing
 * @param[out] lp
 *   pointer to lolan Packet structure
 * @param[in] toId
 *   target node
 * @param[in] p
 *   target path
 * @return
 *   parse result
 *   	1 : request processed, packet generated
 *     -1 : stack unable to handle request (possibly low on memory)
 *****************************************************************************/
int8_t lolan_setupSet(lolan_ctx *ctx,lolan_Packet *lp, uint16_t toId, const uint8_t *p)
{
	lp->packetCounter = ctx->packetCounter++;
	lp->packetType = LOLAN_GET;
	lp->fromId = ctx->myAddress;
	lp->toId = toId;
//	lp->payloadSize = cn_cbor_encoder_write(lp->payload, 0, LOLAN_MAX_PACKET_SIZE, cb);
//	DLOG(("\n Encoded reply to %d bytes",lp->payloadSize));
	return 1;
}
