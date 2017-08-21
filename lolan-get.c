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
 *   helper function for checking a buffer is all zero
 * @param[in] buf
 *   pointer to the buffer
 * @param[in] size
 *   size to check
 ******************************************************************************/

int is_empty(uint8_t *buf, size_t size)
{
    static const char zero[LOLAN_REGMAP_DEPTH] = { 0 };
    return !memcmp(zero, buf, size > LOLAN_REGMAP_DEPTH ? LOLAN_REGMAP_DEPTH : size);
}


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
void add_regMap(lolan_ctx *ctx,cn_cbor *cb_map,uint8_t *p,uint8_t plevel,cn_cbor_errback *err,uint8_t recursion)
{
	cn_cbor *cb;

	for (int i=0;i<LOLAN_REGMAP_SIZE;i++) {
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
 *     -2 : GET packet parse error
 *****************************************************************************/

uint8_t lolan_processGet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply)
{
	cn_cbor *cb;
	cn_cbor_errback err;

	cb = cn_cbor_decode(lp->payload, lp->payloadSize , &err);
	if (cb != NULL) {
		uint8_t p[LOLAN_REGMAP_DEPTH];
		memset(p,0,LOLAN_REGMAP_DEPTH);
		if (cb->length > LOLAN_REGMAP_DEPTH) { return -3; } // ERROR: too long GET path
		for (int i=0;i<cb->length;i++) {
			 cn_cbor *val = cn_cbor_index(cb, i);
			 if (val->type == CN_CBOR_UINT) {
				 p[i] = val->v.uint;
			 } else {
				 return -3;  // ERROR: unknow type in GET path
			 }
		}
		DLOG(("\n GET: "));
		for (int i=0;i<3;i++) {
			if (p[i]==0) { break; }
			DLOG(("/%d",p[i]));
		}
		cn_cbor_free(cb);
		cb=NULL;

		if (p[0]==0) { // we want the root node
			cb = cn_cbor_map_create(&err);
			add_regMap(ctx,cb,p,0,&err,LOLAN_REGMAP_RECURSION);
		} else {
			uint8_t p_nulls=0;
			uint8_t *pp = (uint8_t *) &(p[LOLAN_REGMAP_DEPTH-1]);
			while (*pp==0) {
				p_nulls++;
				pp--;
			}
			for (int i=0;i<LOLAN_REGMAP_SIZE;i++) {
				if (ctx->regMap[i].p[0] == 0) {
					continue; // free slot of regmap
				}
				if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH)==0) {
					if ((ctx->regMap[i].flags & 0x7) == LOLAN_STR) { // important, if full match, we consider there is only one instance in regMap
						cb = cn_cbor_string_create(ctx->regMap[i].data,&err);
					} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT8) {
						int8_t *val = ((int8_t *) ctx->regMap[i].data);
						cb = cn_cbor_int_create(*val, &err);
					} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT16) {
						int16_t *val = ((int16_t *) ctx->regMap[i].data);
						cb = cn_cbor_int_create(*val, &err);
					} else if ((ctx->regMap[i].flags & 0x7) == LOLAN_INT32) {
						int32_t *val = ((int32_t *) ctx->regMap[i].data);
						cb = cn_cbor_int_create(*val, &err);
					}
					break;
				} else if (memcmp (p,ctx->regMap[i].p,LOLAN_REGMAP_DEPTH-p_nulls)==0) {
					cb = cn_cbor_map_create(&err);
					add_regMap(ctx,cb,p,LOLAN_REGMAP_DEPTH-p_nulls,&err,LOLAN_REGMAP_RECURSION);
					break;
				}
			}
		}

		if (err.err != CN_CBOR_NO_ERROR) {
			DLOG(("\n cbor error = %d",err.err));
			return -1;
		}

		if (cb == NULL) {
			// resource not found
			cb = cn_cbor_map_create(&err);
			cn_cbor_mapput_int(cb, 0, cn_cbor_int_create(404, &err), &err);
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
	return -2;
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
	cn_cbor *cb = NULL;
	cn_cbor_errback err;

	cb = cn_cbor_array_create(&err);
	if (cb == NULL) {
		return -1;
	}

	for (int i=0;i<LOLAN_REGMAP_DEPTH;i++) {
		if (p[i] == 0) {
			break;
		}
		cn_cbor_array_append(cb, cn_cbor_int_create(p[i], &err), &err);
	}

	if (err.err != CN_CBOR_NO_ERROR) {
		DLOG(("\n cbor error = %d",err.err));
		return -1;
	}

	lp->packetCounter = ctx->packetCounter++;
	lp->packetType = LOLAN_GET;
	lp->fromId = ctx->myAddress;
	lp->toId = toId;
	lp->payloadSize = cn_cbor_encoder_write(lp->payload, 0, LOLAN_MAX_PACKET_SIZE, cb);
	DLOG(("\n Encoded reply to %d bytes",lp->payloadSize));
	cn_cbor_free(cb);
	return 1;
}

