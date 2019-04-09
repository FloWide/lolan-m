/**************************************************************************//**
 * @file lolan-utils.h
 * @brief LoLaN utility functions
 * @author OMTLAB Kft.
 ******************************************************************************/

#ifndef LOLAN_UTILS_H_
#define LOLAN_UTILS_H_


#include <stdint.h>
#include <stdbool.h>

#include "lolan.h"
#include "cbor.h"


typedef enum {
  LVUFC_NOTFOUND = 1,
  LVUFC_READONLY,
  LVUFC_MISMATCH,
  LVUFC_OUTOFRANGE
} lolan_UpdateFromCbor_result;

typedef struct {
  LR_SIZE_T found;
  LR_SIZE_T updated;
  LR_SIZE_T notfound;
  bool toodeep;
  LR_SIZE_T invalid_keys;
} lolan_BunchUpdateOutputStruct;


extern bool isPathValid(const uint8_t *path);
extern uint8_t pathDefinitionLevel(lolan_ctx *ctx, const uint8_t *path, LR_SIZE_T *occurrences, bool occ_maxrec);
extern LR_SIZE_T lolanVarFlagCount(lolan_ctx *ctx, uint16_t flags, bool *dlbpsame, uint8_t *defLevel, uint8_t *bpath);

extern void lolan_regMapSort(lolan_ctx *ctx);

extern int8_t getPathFromCbor(uint8_t *path, CborValue *it);
extern int8_t getZeroKeyEntryFromPayload(const lolan_Packet *lp, uint8_t *path, uint16_t *value, bool *isPath);
extern int8_t lolanGetDataFromCbor(CborValue *it, uint8_t *data, LV_SIZE_T data_max, LV_SIZE_T *data_len, uint8_t *type);
extern int8_t lolanVarUpdateFromCbor(lolan_ctx *ctx, const uint8_t *path, CborValue *it, uint8_t *error);
extern int8_t lolanVarBunchUpdateFromCbor(lolan_ctx *ctx, const lolan_Packet *lp, lolan_BunchUpdateOutputStruct *info);

extern int8_t createCborUintDataSimple(CborEncoder *encoder, uint64_t key, uint64_t value, bool container);
extern int8_t lolanVarDataToCbor(uint8_t *data, LV_SIZE_T data_len, lolan_VarType type, CborEncoder *encoder);
extern int8_t lolanVarToCbor(lolan_ctx *ctx, const uint8_t *path, LR_SIZE_T index, CborEncoder *encoder);
extern int8_t lolanVarBranchToCbor(lolan_ctx *ctx, const uint8_t *path, CborEncoder *encoder);
extern int8_t lolanVarFlagToCbor(lolan_ctx *ctx, uint16_t flags, CborEncoder *encoder, bool auxflagset, bool statusCodeInstead);

extern uint16_t CRC_calc(const uint8_t *data, uint32_t size);


#endif /* LOLAN_UTILS_H_ */
