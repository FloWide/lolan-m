/**************************************************************************//**
 * @file lolan.h
 * @brief LoLaN core functions
 * @author Sunstone-RTLS Ltd.
 ******************************************************************************/
#ifndef LOLAN_H_
#define LOLAN_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "lolan_config.h"


#define LOLAN_VERSION      108    // LoLaN version number


/* common defines */
#define LOLAN_PACKET_MAX_PAYLOAD_SIZE  (LOLAN_MAX_PACKET_SIZE - 9)   // maximum size of LoLaN packet payload (do not modify!)
#define LOLAN_BROADCAST_ADDRESS                    0xFFFF   // address for broadcast

/* size defines */
#if LOLAN_REGMAP_SIZE <= UINT8_MAX   // integer type to represent register map size
  #define LR_SIZE_T    uint8_t   // 8-bit
#elif LOLAN_REGMAP_SIZE <= UINT16_MAX
  #define LR_SIZE_T    uint16_t  // 16-bit
#endif

#if LOLAN_VARSIZE_BITS == 8   // integer type to represent LoLaN variable size
  #define LV_SIZE_T    uint8_t   // 8-bit
  #define LV_SIZE_MAX  UINT8_MAX
#elif LOLAN_VARSIZE_BITS == 16
  #define LV_SIZE_T    uint16_t  // 16-bit
  #define LV_SIZE_MAX  UINT16_MAX
#elif (LOLAN_VARSIZE_BITS == 32) && (SIZE_MAX == UINT32_MAX)
  #define LV_SIZE_T    uint32_t  // 32-bit
  #define LV_SIZE_MAX  UINT32_MAX
#endif

#ifndef LP_SIZE_T   // integer type to represent packet & payload size
  #if LOLAN_MAX_PACKET_SIZE <= UINT8_MAX
    #define LP_SIZE_T    uint8_t
  #elif LOLAN_MAX_PACKET_SIZE <= UINT16_MAX
    #define LP_SIZE_T    uint16_t
  #elif LOLAN_MAX_PACKET_SIZE <= UINT32_MAX
    #define LP_SIZE_T    uint32_t
  #endif
#endif

/* LoLaN variable flags and masks */
#define LOLAN_REGMAP_AUX_BIT                        0x8000    // (internal use)
#define LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT   0x0400    // (internal use)
#define LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT     0x0200    // (internal use)
#define LOLAN_REGMAP_REMOTE_READONLY_BIT            0x0100    // (internal use)
#define LOLAN_REGMAP_INFORMSEC_REQUEST_BIT          0x0080    // secondary INFORM request
#define LOLAN_REGMAP_LOCAL_UPDATE_BIT               0x0040    // local update indicator
#define LOLAN_REGMAP_INFORM_REQUEST_BIT             0x0020    // INFORM request
#define LOLAN_REGMAP_REMOTE_UPDATE_BIT              0x0010    // remote update indicator
#define LOLAN_REGMAP_USER_MASK                      0x00F0    // user flags mask
#define LOLAN_REGMAP_TYPE_MASK                      0x000F    // variable type mask


typedef enum {                 // return values for LoLaN functions
  LOLAN_RETVAL_YES = 1,             // yes/o.k.
  LOLAN_RETVAL_NO = 0,              // negative
  LOLAN_RETVAL_GENERROR = -1,       // general error
  LOLAN_RETVAL_CBORERROR = -2,      // CBOR error
  LOLAN_RETVAL_MEMERROR = -3,       // CBOR out of memory error
} lolan_ReturnType;

typedef enum {
  LOLAN_INT = 1,      // signed integer
  LOLAN_UINT = 2,     // unsigned integer
  LOLAN_FLOAT = 3,    // floating-point number
  LOLAN_STR = 4,      // string (CBOR type for input: byte/text string, for output: text string)
  LOLAN_DATA = 5      // arbitrary data type (CBOR type: byte string)
} lolan_VarType;

typedef enum {
  LOLAN_PAK_BEACON = 0,
  LOLAN_PAK_DATA = 1,
  LOLAN_PAK_ACK = 2,
  LOLAN_PAK_MAC = 3,
  LOLAN_PAK_INFORM = 4,
  LOLAN_PAK_GET = 5,
  LOLAN_PAK_SET = 6,
  LOLAN_PAK_CONTROL = 7
} lolan_PacketType;

typedef enum {
  LOLAN_MPC_NOMULTIPART = 0,
  LOLAN_MPC_MULTIPART_START = 1,
  LOLAN_MPC_MULTIPART_MIDDLE = 2,
  LOLAN_MPC_MULTIPART_END = 3
} lolan_MultiPart;

typedef enum {
  LOLAN_CONTROL_RX_PACKET = 1,
  LOLAN_CONTROL_CLKSYNC_PACKET = 2,
  LOLAN_CONTROL_BEACON_PACKET = 3,
  LOLAN_CONTROL_TWR_PACKET = 4,
  LOLAN_CONTROL_DEBUGMSG_PACKET = 16
} lolan_ControlPacketType;

// LoLaN packet
typedef struct {
  lolan_PacketType packetType;
  lolan_MultiPart multiPart;
  bool securityEnabled;
  bool ackRequired;
  uint8_t packetCounter;
  bool routingRequested;
  uint16_t fromId;
  uint16_t toId;
  uint8_t *payload;
  LP_SIZE_T payloadSize;
} lolan_Packet;

typedef struct {
  uint8_t p[LOLAN_REGMAP_DEPTH];    // LoLaN variable path
  uint16_t flags;                   // flags (e.g. variable type)
  LV_SIZE_T size;                   // size in bytes
#ifdef LOLAN_ALLOW_VARLEN_LOLANDATA
  LV_SIZE_T sizeActual;             // actual size in bytes (for LOLAN_DATA)
#endif
  void *data;                       // variable data
#ifdef LOLAN_VARIABLE_TAG_TYPE
  LOLAN_VARIABLE_TAG_TYPE tag;      // tag (to store auxiliary data if needed)
#endif
} lolan_RegMap;

typedef struct {
  uint16_t myAddress;   // our LoLaN address in the context
  uint8_t packetCounter;    // counter for automatically generated packets (INFORM, reply to SET & GET)
  lolan_RegMap regMap[LOLAN_REGMAP_SIZE];
//  void (*replyDeviceCallbackFunc)(uint8_t *buf, uint8_t size);    // (future plans)
//  uint8_t networkKey[16];
//  uint8_t nodeIV[16];
} lolan_ctx;

typedef void (*lpuCallback)(void*);   // callback function type pointer definition for lolan_processUpdated


extern void lolan_init(lolan_ctx *ctx, uint16_t initial_address);
extern void lolan_setAddress(lolan_ctx *ctx, uint16_t new_address);

extern int8_t lolan_regVar(lolan_ctx *ctx, const uint8_t *path, lolan_VarType vType, void *ptr,
                           LV_SIZE_T size, bool readOnly);
extern int8_t lolan_isVarUpdated(lolan_ctx *ctx, const void *ptr, bool clearFlag);
extern int8_t lolan_processUpdated(lolan_ctx *ctx, bool clearFlag, lpuCallback callback);
extern int8_t lolan_rmVar(lolan_ctx *ctx, const void *ptr);
extern int8_t lolan_setFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags);
extern uint16_t lolan_getFlag(lolan_ctx *ctx, const void *ptr);
extern int8_t lolan_clearFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags);
#ifdef LOLAN_VARIABLE_TAG_TYPE
extern LOLAN_VARIABLE_TAG_TYPE* lolan_getTagPtr(lolan_ctx *ctx, const void *ptr);
#endif
extern LR_SIZE_T lolan_getIndex(lolan_ctx *ctx, bool isPath, const void *ptr_or_path, bool *errorOut);

#ifdef LOLAN_ALLOW_VARLEN_LOLANDATA
extern int8_t lolan_setDataActualLength(lolan_ctx *ctx, const void *ptr, LV_SIZE_T len);
extern LV_SIZE_T lolan_getDataActualLength(lolan_ctx *ctx, const void *ptr);
#endif

extern void lolan_resetPacket(lolan_Packet *lp);
extern int8_t lolan_createPacket(const lolan_Packet *lp, uint8_t *buf, size_t maxSize,
                size_t *outputSize, bool withCRC);
extern int8_t lolan_parsePacket(const uint8_t *pak, size_t pak_len, lolan_Packet *lp);

extern int8_t lolan_processGet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply);
extern int8_t lolan_processSet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply);

extern int8_t lolan_createGet(lolan_ctx *ctx, lolan_Packet *pak, uint8_t *path);
extern int8_t lolan_createInform(lolan_ctx *ctx, lolan_Packet *pak, bool multi);
extern int8_t lolan_createInformEx(lolan_ctx *ctx, lolan_Packet *pak, bool multi,
                bool secondary, LP_SIZE_T plSizeOverride, bool payloadOnly);

extern int8_t lolan_simpleCreateSet(lolan_ctx *ctx, lolan_Packet *pak, const uint8_t *path,
                uint8_t *data, LV_SIZE_T data_len, lolan_VarType type);
extern int8_t lolan_simpleProcessAck(lolan_Packet *pak, uint8_t *data, LV_SIZE_T data_max,
                LV_SIZE_T *data_len, uint8_t *type, bool *zerokey);
extern int8_t lolan_simpleExtractFromInform(lolan_Packet *pak, const uint8_t *path, uint8_t *data,
                LV_SIZE_T data_max, LV_SIZE_T *data_len, uint8_t *type);

#endif /* LOLAN_H_ */
