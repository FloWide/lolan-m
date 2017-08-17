/*
 * lolan_config.h
 *
 *  Created on: Aug 16, 2017
 *      Author: gabor
 */

#ifndef LOLAN_CONFIG_H_
#define LOLAN_CONFIG_H_

#define PLATFORM_EFM32

//#define DEBUG_PRINTF

#if defined DEBUG_PRINTF
#define DLOG(arg) printf arg
#elif defined DEBUG_SWO
#define DLOG(arg) SWOPrintf arg
#else
#define DLOG(arg)
#endif

#endif /* LOLAN_CONFIG_H_ */
