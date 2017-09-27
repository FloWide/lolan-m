/*
 * lolan.c
 *
 *  Created on: Aug 11, 2017
 *      Author: gabor
 */


#include "lolan_config.h"
#include "lolan.h"
#include "cbor.h"


#include <stdint.h>
#include <stdio.h>


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
 *   helper function for decoding path from array
 * @param[in] p
 *   pointer to path array
 * @param[in] it
 *   pointer to CborValue
 ******************************************************************************/

int8_t getPathFromCbor(uint8_t *p, CborValue *it)
{
    int cnt=0;
    CborValue ait;
    CborError err;

    err = cbor_value_enter_container(it, &ait);
    if (err) {
	return -1;
    }

    while (!cbor_value_at_end(&ait)) {
        if (cnt < LOLAN_REGMAP_DEPTH) {
    	    if (cbor_value_get_type(&ait) != CborIntegerType) {
		return -1;
    	    }
    	    int val;
    	    cbor_value_get_int(&ait,&val);
    	    p[cnt] = val;
    	    cnt++;
	}
	err = cbor_value_advance_fixed(&ait);
	if (err) {
    	    return -1;
	}
    }

    err = cbor_value_leave_container(it, &ait);
    if (err) {
	return -1;
    }

    return 1;
}


int8_t getPathFromPayload(lolan_Packet *lp, uint8_t *p)
{
    CborParser parser;
    CborValue it;
    CborError err = cbor_parser_init(lp->payload, lp->payloadSize, 0, &parser, &it);

    if (err) {
	DLOG(("\n cbor parse error"));
	return -2;
    }
    
    if (cbor_value_get_type(&it) != CborMapType) {
	DLOG(("\n cbor parse error: map not found"));
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
        
	if (key==0) {
	    if (getPathFromCbor(p, &rit)!=1) {
		return -2;
	    }
	    return 1;
        } else {
	    if (cbor_value_get_type(&rit) == CborByteStringType) {
		size_t len=LOLAN_MAX_PACKET_SIZE;
		uint8_t buf[LOLAN_MAX_PACKET_SIZE];
		err = cbor_value_copy_byte_string(&rit, buf, &len, &rit);
		if (err) {
		    DLOG(("\n cbor parse string error"));
		    return -2;
		}
		continue;
	    } else if (cbor_value_get_type(&rit) == CborTextStringType) {
		size_t len=LOLAN_MAX_PACKET_SIZE;
		char buf[LOLAN_MAX_PACKET_SIZE];
		err = cbor_value_copy_text_string(&rit, buf, &len, &rit);
		if (err) {
		    DLOG(("\n cbor parse string error"));
		    return -2;
		}
    		continue;
	    }
	}

        err = cbor_value_advance_fixed(&rit);
        if (err) {
	    DLOG(("\n cbor parse error"));
	    return -2;
        }
    }
    return 0;
}

uint16_t CRC_calc(uint8_t *val, uint8_t size)
{
    uint16_t crc;
    uint16_t q;
    uint8_t c;
    crc = 0;
    int i;
    for (i = 0; i < size; i++)
    {
        c = val[i];
        q = (crc ^ c) & 0x0f;
        crc = (crc >> 4) ^ (q * 0x1081);
        q = (crc ^ (c >> 4)) & 0xf;
        crc = (crc >> 4) ^ (q * 0x1081);
    }
    return (uint8_t) crc << 8 | (uint8_t) (crc >> 8);
}
