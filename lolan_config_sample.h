/*
 * lolan_config.h
 *
 *  Created on: Aug 16, 2017
 *      Author: gabor
 */

#ifndef LOLAN_CONFIG_H_
#define LOLAN_CONFIG_H_

#define LOLAN_MAX_PACKET_SIZE	128

#define LOLAN_REGMAP_SIZE	20
// the maximum number of registers to be mapped. This will result in LOLAN_REG_MAP_SIZE*(LOLAN_REGMAP_DEPTH+5) byte data reserved

#define LOLAN_REGMAP_DEPTH 3

#define LOLAN_REGMAP_RECURSION 1

//#define PLATFORM_EFM32

#define DEBUG_PRINTF

#if defined DEBUG_PRINTF
#define DLOG(arg) printf arg
#elif defined DEBUG_SWO
#include "SWOUtils.h"
#define DLOG(arg) SWOPrintf arg
#else
#define DLOG(arg)
#endif

#endif /* LOLAN_CONFIG_H_ */