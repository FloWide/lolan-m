/**************************************************************************//**
 * @file lolan.h
 * @brief LoLaN core functions
 * @author OMTLAB Kft.
 ******************************************************************************/

#ifndef LOLAN_H_
#define LOLAN_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "lolan_config.h"


/* LoLaN variable flags and masks */
#define LOLAN_REGMAP_AUX_BIT                       0x8000
#define LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT  0x0400
#define LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT    0x0200
#define LOLAN_REGMAP_REMOTE_READONLY_BIT           0x0100
#define LOLAN_REGMAP_LOCAL_UPDATE_BIT		           0x0080
#define LOLAN_REGMAP_TRAP_REQUEST_BIT		           0x0040
#define LOLAN_REGMAP_INFORM_REQUEST_BIT		         0x0020
#define LOLAN_REGMAP_REMOTE_UPDATE_BIT		         0x0010
#define LOLAN_REGMAP_TYPE_MASK                     0x000F

#define LOLAN_BROADCAST_ADDRESS  0xFFFF

#define LOLAN_PACKET(packet) \
	        lolan_Packet packet; \
	        memset(&packet, 0, sizeof(lolan_Packet)); \
	        uint8_t (packet ## _lolan_pl_buf)[LOLAN_MAX_PACKET_SIZE]; \
	        memset((packet ## _lolan_pl_buf), 0, LOLAN_MAX_PACKET_SIZE); \
	        packet.payload = (packet ## _lolan_pl_buf);


#if defined (__cplusplus)
extern "C" {
#endif

typedef enum {                 // return values for LoLaN functions
  LOLAN_RETVAL_YES = 1,             // yes/o.k.
  LOLAN_RETVAL_NO = 0,              // negative
  LOLAN_RETVAL_GENERROR = -1,       // general error
  LOLAN_RETVAL_CBORERROR = -2,      // CBOR error
  LOLAN_RETVAL_MEMERROR = -3,       // CBOR out of memory error
} lolan_ReturnType;

typedef enum {
  LOLAN_INT = 1,
  LOLAN_UINT = 2,
  LOLAN_FLOAT = 3,
  LOLAN_STR = 4
} lolan_VarType;

typedef enum {
  BEACON_PACKET = 0,
  DATA_PACKET = 1,
  ACK_PACKET = 2,
  MAC_PACKET = 3,
  LOLAN_INFORM = 4,
  LOLAN_GET = 5,
  LOLAN_SET = 6,
  LOLAN_CONTROL = 7
} lolan_PacketType;

typedef enum {
  TIMING_PACKET = 1,
  RESEND_REQUEST_PACKET = 2,
  CLKSYNC_PACKET = 3
} lolan_ControlPacketType;

// LoLaN packet
typedef struct {
  lolan_PacketType packetType;
  bool securityEnabled;
  bool framePending;
  bool ackRequired;
  uint8_t packetCounter;
  bool routingRequested;
  bool packetRouted;
  uint16_t fromId;
  uint16_t toId;
  uint8_t *payload;
  uint16_t payloadSize;
} lolan_Packet;

typedef struct {
  uint8_t p[LOLAN_REGMAP_DEPTH];   // LoLaN variable path
  uint16_t flags;                   // flags (e.g. variable type)
  uint8_t size;                    // size in bytes
  void *data;                      // variable data
} lolan_RegMap;

typedef struct {
  uint16_t myAddress;
  uint8_t packetCounter;    // counter for automatically generated packets (INFORM, reply to SET & GET)
  lolan_RegMap regMap[LOLAN_REGMAP_SIZE];
//  void (*replyDeviceCallbackFunc)(uint8_t *buf, uint8_t size);    // (future plans)
//  uint8_t networkKey[16];
//  uint8_t nodeIV[16];
} lolan_ctx;


extern void lolan_init(lolan_ctx *ctx, uint16_t lolan_address);

extern int8_t lolan_regVar(lolan_ctx *ctx, const uint8_t *path, lolan_VarType vType, void *ptr,
                           uint8_t size, bool readOnly);
extern int8_t lolan_regVarUpdated(lolan_ctx *ctx, const void *ptr, bool clearFlag);
extern int8_t lolan_rmVar(lolan_ctx *ctx, const void *ptr);
extern int8_t lolan_setFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags);
extern int8_t lolan_clearFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags);

extern int8_t lolan_createPacket(lolan_ctx *ctx, const lolan_Packet *lp, uint8_t *buf,
                                 uint32_t maxSize, uint32_t *outputSize, bool withCRC);
extern int8_t lolan_parsePacket(lolan_ctx *ctx, const uint8_t *pak, uint32_t pak_len, lolan_Packet *lp);

extern int8_t lolan_processGet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply);
extern int8_t lolan_processSet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply);
extern int8_t lolan_createInform(lolan_ctx *ctx, lolan_Packet *reply, bool multi);

#if defined (__cplusplus)
}
#endif

#endif /* LOLAN_H_ */
