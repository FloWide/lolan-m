/**************************************************************************//**
 * @file lolan_config.h
 * @brief LoLaN settings
 * @author Sunstone-RTLS Ltd.
 ******************************************************************************/
#ifndef LOLAN_CONFIG_H_
#define LOLAN_CONFIG_H_


#define LOLAN_MAX_PACKET_SIZE	   128   // maximum size of a LoLaN packet in bytes

#define LOLAN_REGMAP_SIZE	       20    // the maximum number of registers to be mapped (maximum: 65535)
#define LOLAN_REGMAP_DEPTH       3     // depth of register paths
#define LOLAN_VARSIZE_BITS       8     // variable size storage bits (8, 16, 32  /default: 8/)
#define LOLAN_VARIABLE_TAG_TYPE  int   // type of auxiliary field in the LoLaN register map structure (do not define to disable this feature)
// #define LP_SIZE_T                (unsigned char)   // specify integer type to represent LoLaN packet & payload size (undef to auto-select)

#define LOLAN_REGMAP_RECURSION            2       // the recursion depth for a LoLaN GET command (set 0 to refuse recursive requests)
#define LOLAN_FORCE_GET_VERBOSE_REPLY     false   // force reply for a LoLaN GET command with key-value pair even if one variable is requested
#define LOLAN_FORCE_NEW_STYLE_INFORM      false   // force new style LoLaN INFORM packets
#define LOLAN_SET_SHORT_REPLY_IF_OK       false   // send only the main status code in a reply for SET if all actions were o.k.
#define LOLAN_COPY_ROUTINGREQUEST_ON_ACK  false   // copy the routing request flag from the source packet when replying to a GET or SET
// #define LOLAN_ALLOW_VARLEN_LOLANDATA           // define this to allow variable length encoding of LOLAN_DATA type

//#define DEBUG_PRINTF

#if defined DEBUG_PRINTF
#define DLOG(arg) printf arg
#elif defined DEBUG_SWO
#include "SWOUtils.h"
#define DLOG(arg) SWOPrintf arg
#else
#define DLOG(arg)
#endif


#endif /* LOLAN_CONFIG_H_ */
