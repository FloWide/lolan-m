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
 *   Build cbor from regMap recursively
 * @param[in] ctx
 *   context for lolan packet processing
 * @param[in] cb_map
 *   map to put all values in the desired path level
 * @param[in] p
 *   the array of the path requested
 * @param[in] plevel
 *   current level for finding siblings
 * @param[out] err
 *   cbor errors go here
 * @param[in] recursion
 *   Recursively call itself only if requested (can be memory hungry)
 ******************************************************************************/
/*void add_regMap(lolan_ctx *ctx,cn_cbor *cb_map,uint8_t *p,uint8_t plevel,cn_cbor_errback *err,uint8_t recursion)
{
	cn_cbor *cb;
	int i=0;

	for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] == 0) {
			continue; // free slot of regmap
		}

		if (memcmp (p,ctx->regMap[i].p,plevel)==0) {
			if (is_empty(&(ctx->regMap[i].p[plevel+1]),LOLAN_REGMAP_DEPTH-plevel-1)) {
				if ((ctx->regMap[i].flags & 0x7) == LOLAN_STR) { // important, if full match, we consider there is only one instance in regMap
					cb = cn_cbor_string_create(ctx->regMap[i].data,err);
					cn_cbor_mapput_int(cb_map, ctx->regMap[i].p[plevel], cb, err);
				} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT8) {
					int8_t *val = ((int8_t *) ctx->regMap[i].data);
					cb = cn_cbor_int_create(*val, err);
					cn_cbor_mapput_int(cb_map, ctx->regMap[i].p[plevel], cb, err);
				} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT16) {
					int16_t *val = ((int16_t *) ctx->regMap[i].data);
					cb = cn_cbor_int_create(*val, err);
					cn_cbor_mapput_int(cb_map, ctx->regMap[i].p[plevel], cb, err);
				} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT32) {
					int32_t *val = ((int32_t *) ctx->regMap[i].data);
					cb = cn_cbor_int_create(*val, err);
					cn_cbor_mapput_int(cb_map, ctx->regMap[i].p[plevel], cb, err);
				}
			} else {
				if (plevel < (LOLAN_REGMAP_DEPTH-1)) {
					if (cn_cbor_mapget_int(cb_map, p[plevel])==NULL) { // we have not added this level yet
						cn_cbor *cb = cn_cbor_map_create(err);
						if (recursion) {
							add_regMap(ctx,cb,ctx->regMap[i].p,plevel+1,err,recursion);
						}
						cn_cbor_mapput_int(cb_map, ctx->regMap[i].p[plevel], cb, err);
					}
				}
			}
		}
	}
}
*/

/**************************************************************************//**
 * @brief
 *   process GET request
 * @param[in] ctx
 *   context for lolan packet processsing
 * @param[out] lp
 *   pointer to lolan Packet structure
 * @param[out] reply
 *   generated reply packet
 * @return
 *   parse result
 *   	1 : request processed, reply packet generated
 *     -1 : stack unable to handle request
 *     -2 : GET packet parse error
 *****************************************************************************/

int8_t lolan_processGet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply)
{
    int i=0;
    CborError err;

    uint8_t p[LOLAN_REGMAP_DEPTH];
    if (getPathFromPayload(lp, p) != 1) {
	DLOG(("\n path not found in packet"));
    }

    DLOG(("\n GET: "));
    for (i=0;i<3;i++) {
	if (p[i]==0) { if (i==0) { DLOG(("/")); } break; }
	DLOG(("/%d",p[i]));
    }

    int found = 0;

    CborEncoder enc;
    cbor_encoder_init(&enc,reply->payload,LOLAN_MAX_PACKET_SIZE,0);

    if (p[0]==0) { // we want the root node
	CborEncoder map_enc;
	err = cbor_encoder_create_map(&enc,&map_enc,CborIndefiniteLength);
	if (err) {
	    DLOG(("\n cbor encode error"));
	    return -1;
	}
//		add_regMap(ctx,cb,p,0,&err,LOLAN_REGMAP_RECURSION);
    } else {
	uint8_t p_nulls=0;
	uint8_t *pp = (uint8_t *) &(p[LOLAN_REGMAP_DEPTH-1]);
	while (*pp==0) {
	    p_nulls++;
	    pp--;
	}
	for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
	    if (ctx->regMap[i].p[0] == 0) {
		continue; // free slot of regmap
	    }
	    if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
		if ((ctx->regMap[i].flags & 0x7) == LOLAN_STR) { // important, if full match, we consider there is only one instance in regMap
		    err = cbor_encode_text_string(&enc, (const char *) ctx->regMap[i].data, strlen((const char *) ctx->regMap[i].data));
		    if (err) {
			DLOG(("\n cbor encode error"));
			    return -1;
		    }
		    found = 1;
		} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT) {
		    if (ctx->regMap[i].size==1) {
			int8_t *val = ((int8_t *) ctx->regMap[i].data);
			err = cbor_encode_int (&enc, *val);
			if (err) {
			    DLOG(("\n cbor encode error"));
			    return -1;
			}
			found=1;
		    } else if (ctx->regMap[i].size == 2) {
			int16_t *val = ((int16_t *) ctx->regMap[i].data);
			err = cbor_encode_int (&enc, *val);
			if (err) {
			    DLOG(("\n cbor encode error"));
			    return -1;
			}
			found=1;
		    } else if (ctx->regMap[i].size == 4) {
			int32_t *val = ((int32_t *) ctx->regMap[i].data);
			err = cbor_encode_int (&enc, *val);
			if (err) {
			    DLOG(("\n cbor encode error"));
			    return -1;
			}
			found=1;
		    }
		}
		break;
	    } else if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH-p_nulls)==0) {
		CborEncoder map_enc;
		err = cbor_encoder_create_map(&enc,&map_enc,CborIndefiniteLength);
		if (err) {
		    DLOG(("\n cbor encode error"));
		    return -1;
		}
//		add_regMap(ctx,cb,p,LOLAN_REGMAP_DEPTH-p_nulls,&err,LOLAN_REGMAP_RECURSION);
		break;
	    }
	}
    }

    if (found == 0) {
	// resource not found
	CborEncoder map_enc;
	err = cbor_encoder_create_map(&enc,&map_enc,1);
	if (err) {
	    DLOG(("\n cbor encode error"));
	    return -1;
	}
	cbor_encode_int(&map_enc, 0);
	cbor_encode_int(&map_enc, 404);
	cbor_encoder_close_container(&enc, &map_enc);
    }

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
 *   process GET request
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
int8_t lolan_setupGet(lolan_ctx *ctx,lolan_Packet *lp, uint16_t toId, const uint8_t *p)
{
    lp->packetCounter = ctx->packetCounter++;
    lp->packetType = LOLAN_GET;
    lp->fromId = ctx->myAddress;
    lp->toId = toId;
//	lp->payloadSize = cn_cbor_encoder_write(lp->payload, 0, LOLAN_MAX_PACKET_SIZE, cb);
//	DLOG(("\n Encoded reply to %d bytes",lp->payloadSize));
    return 1;
}
