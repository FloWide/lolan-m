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

uint8_t lolan_processGet(lolan_ctx *ctx,lolan_Packet *lp,lolan_Packet *reply)
{
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
		DLOG(("\n GET: "));
		for (int i=0;i<3;i++) {
			if (p[i]==0) { break; }
			DLOG(("/%d",p[i]));
		}
		cn_cbor_free(cb);
		//cb=cn_cbor_map_create(&err);
		for (int i=0;i<LOLAN_REGMAP_SIZE;i++) {
			if (memcmp (p,ctx->regMap[i].p,3)==0) {
				if ((ctx->regMap[i].flags & 0x7) == LOLAN_STR) {
					memset(reply,0,sizeof(reply));
					cb = cn_cbor_string_create(ctx->regMap[i].data,&err);
					reply->packetType = ACK_PACKET;
					reply->fromId = lp->toId;
					reply->toId = lp->fromId;
					reply->payload = malloc(LOLAN_MAX_PACKET_SIZE);
					reply->payloadSize = cn_cbor_encoder_write(reply->payload, 0, sizeof(reply->payload), cb);
					cn_cbor_free(cb);
					return 1;
				}
			}
		}
	} else {
		return -1;
	}
}
