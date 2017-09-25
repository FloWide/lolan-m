/*
 * lolan-get.c
 *
 *  Created on: Aug 19, 2017
 *      Author: gabor
 */


#include "lolan_config.h"
#include "lolan.h"
#include "cn-cbor.h"


#include <stdint.h>

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
 *     -1 : stack unable to handle request (possibly low on memory)
 *     -2 : SET packet parse error
 *****************************************************************************/

uint8_t lolan_processSet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply)
{
	int i=0;

	cn_cbor *cb;
	cn_cbor *cb_path_array;
	cn_cbor_errback err;

	cb = cn_cbor_decode(lp->payload, lp->payloadSize , &err);
	if (cb == NULL) { return -1;}
	cb_path_array = cn_cbor_mapget_int(cb,0);
	if (cb_path_array == NULL) { cn_cbor_free(cb); return -1;}

	uint8_t p[LOLAN_REGMAP_DEPTH];
	memset(p,0,LOLAN_REGMAP_DEPTH);
	if (cb_path_array->length > LOLAN_REGMAP_DEPTH) { cn_cbor_free(cb); return -3; } // ERROR: too long GET path
	for (i=0;i<cb_path_array->length;i++) {
		 cn_cbor *val = cn_cbor_index(cb_path_array, i);
		 if (val->type == CN_CBOR_UINT) {
			 p[i] = val->v.uint;
		 } else {
			cn_cbor_free(cb);
			return -3;  // ERROR: unknow type in SET path
		 }
	}
	DLOG(("\n SET: "));
	for (i=0;i<3;i++) {
		if (p[i]==0) { break; }
		DLOG(("/%d",p[i]));
	}

	cn_cbor *val = cn_cbor_mapget_int(cb,1);
	if (val == NULL) {
		DLOG(("\nSET VALUE not found"));
		return -3;
	}

	int found = 0;

	for (i=0;i<LOLAN_REGMAP_SIZE;i++) {
		if (ctx->regMap[i].p[0] == 0) {
		    continue; // free slot of regmap
		}
		if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
		    DLOG(("\n found variable"));
		    if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_INT8) {
			int8_t *sv;
			sv = ctx->regMap[i].data;
			if (val->type == CN_CBOR_UINT) {
			    *sv = val->v.uint;
			} else if (val->type == CN_CBOR_INT) {
			    *sv = val->v.sint;
			} else if (val->type == CN_CBOR_TRUE) {
			    *sv = 1;
			} else if (val->type == CN_CBOR_FALSE) {
			    *sv = 0;
			}
		    } else if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_INT16) {
			int16_t *sv;
			sv = ctx->regMap[i].data;
			if (val->type == CN_CBOR_UINT) {
			    *sv = val->v.uint;
			} else if (val->type == CN_CBOR_INT) {
			    *sv = val->v.sint;
			} else if (val->type == CN_CBOR_TRUE) {
			    *sv = 1;
			} else if (val->type == CN_CBOR_FALSE) {
			    *sv = 0;
			}
		    }
		    found = 1;
		    break;
		}
	}

	cn_cbor_free(cb);
	cb=NULL;

	cb = cn_cbor_map_create(&err);
	if (found) {
		// resource not found
		cn_cbor_mapput_int(cb, 0, cn_cbor_int_create(200, &err), &err);
	} else {
		cn_cbor_mapput_int(cb, 0, cn_cbor_int_create(404, &err), &err);
	}


	if (err.err != CN_CBOR_NO_ERROR) {
		DLOG(("\n cbor error = %d",err.err));
		return -1;
	}

	reply->packetCounter = lp->packetCounter;
	reply->packetType = ACK_PACKET;
	reply->fromId = lp->toId;
	reply->toId = lp->fromId;
	reply->payloadSize = cn_cbor_encoder_write(reply->payload, 0, LOLAN_MAX_PACKET_SIZE, cb);
	DLOG(("\n Encoded reply to %d bytes",reply->payloadSize));
	cn_cbor_free(cb);
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
	int i=0;

	cn_cbor *cb = NULL;
	cn_cbor *cb_path_array = NULL;
	cn_cbor_errback err;

	cb = cn_cbor_map_create(&err);

	if (cb == NULL) {
		return -1;
	}

	cb_path_array = cn_cbor_array_create(&err);

	if (cb_path_array == NULL) {
		return -1;
	}

	for (i=0;i<LOLAN_REGMAP_DEPTH;i++) {
		if (p[i] == 0) {
			break;
		}
		cn_cbor_array_append(cb_path_array, cn_cbor_int_create(p[i], &err), &err);
	}
	cn_cbor_mapput_int(cb, 0, cb_path_array, &err);

	if (err.err != CN_CBOR_NO_ERROR) {
		DLOG(("\n cbor error = %d",err.err));
		return -1;
	}

	lp->packetCounter = ctx->packetCounter++;
	lp->packetType = LOLAN_GET;
	lp->fromId = ctx->myAddress;
	lp->toId = toId;
	lp->payloadSize = cn_cbor_encoder_write(lp->payload, 0, LOLAN_MAX_PACKET_SIZE, cb);
//	DLOG(("\n Encoded reply to %d bytes",lp->payloadSize));
	cn_cbor_free(cb);
	return 1;
}
