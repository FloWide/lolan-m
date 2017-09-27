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

int is_empty(uint8_t *buf, size_t size);
int8_t getPathFromCbor(uint8_t *p, CborValue *it);
int8_t getPathFromPayload(lolan_Packet *lp, uint8_t *p);
uint16_t CRC_calc(uint8_t *val, uint8_t size);
