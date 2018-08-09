/**************************************************************************//**
 * @file lolan.c
 * @brief LoLaN core functions
 * @author OMTLAB Kft.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"



/**************************************************************************//**
 * @brief
 *   Initialize the specified LoLaN context variable.
 * @param[in] ctx
 *   Pointer to a LoLaN context variable.
 * @param[in] lolan_address
 *   The 16-bit LoLaN address for this context.
 *****************************************************************************/
void lolan_init(lolan_ctx *ctx, uint16_t lolan_address)
{
  memset(ctx, 0, sizeof(lolan_ctx));
  ctx->myAddress = lolan_address;
  ctx->packetCounter = 1;
} /* lolan_init */

/**************************************************************************//**
 * @brief
 *   Register a new LoLaN variable.
 * @details
 *   Create a new entry in the LoLaN register map for a new variable.
 *   The actual data stays resident on the specified address (no copying),
 *   the LoLaN system handles it by pointer and size only.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] path
 *   The desired path of the new variable (uint8_t array with the length of
 *   LOLAN_REGMAP_DEPTH). The path should contain zeros only on the bottom
 *   n levels, the top level part must be <>0.
 *   E.g.  valid paths:    (2, 5, 1, 1)  (1, 7, 2, 0)  (4, 0, 0)
 *         invalid paths:  (0, 2, 2)  (3, 0, 2, 2)
 * @param[in] vType
 *   The LoLaN variable type.
 * @param[in] ptr
 *   Pointer to the address that the new LoLaN variable will be mapped to.
 *   Only one LoLaN variable can be mapped to a certain address.
 * @param[in] size
 *   Size of the variable in bytes.
 * @param[in] readOnly
 *   If true, the variable will be remotely read-only (can not be modified
 *   with a LoLaN SET command).
 * @return
 *   LOLAN_RETVAL_YES if the action was successful,
 *   otherwise LOLAN_RETVAL_GENERROR.
 *****************************************************************************/
int8_t lolan_regVar(lolan_ctx *ctx, const uint8_t *path, lolan_VarType vType, void *ptr,
                    uint8_t size, bool readOnly)
{
  uint8_t i, defLvl, occ;

  /* check variable size */
  if (size == 0) return LOLAN_RETVAL_GENERROR;  // zero size is not acceptable
  switch (vType) {
    case LOLAN_INT:
    case LOLAN_UINT:
      if ((size != 1) && (size != 2) && (size != 4) && (size != 8))   // unsupported integer number size
        return LOLAN_RETVAL_GENERROR;
      break;
    case LOLAN_FLOAT:
      if ((size != 4) && (size != 8))   // unsupported floating point number size
        return LOLAN_RETVAL_GENERROR;
      break;
  }
  /* check the specified path for formal error */
  if (!isPathValid(path) || path[0] == 0) return LOLAN_RETVAL_GENERROR;
  /* check for duplicates */
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (memcmp(ctx->regMap[i].p, path, LOLAN_REGMAP_DEPTH) == 0)   // the specified path already exists
      return LOLAN_RETVAL_GENERROR;
    if (ctx->regMap[i].data == ptr)   // the specified address is already mapped to an other LoLaN variable
      return LOLAN_RETVAL_GENERROR;
  }
  /* check for other invalid cases */
  defLvl = pathDefinitionLevel(ctx, path, &occ, false);   // get the definition level and occurrences for it
  if (occ > 0) return LOLAN_RETVAL_GENERROR;   // e.g. add (1,2,2) then add (1,2,0)
  if (defLvl > 1) {
    for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
      if (memcmp(ctx->regMap[i].p, path, defLvl-1) == 0)
        if (ctx->regMap[i].p[defLvl-1] == 0)
          return LOLAN_RETVAL_GENERROR;    // e.g. add (1,2,0) then add (1,2,2)
    }
  }
  /* search for a free register map entry for the new variable */
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (ctx->regMap[i].p[0] == 0) {   // free entry found
      memcpy(ctx->regMap[i].p, path, LOLAN_REGMAP_DEPTH);
      ctx->regMap[i].flags = vType + (readOnly ? LOLAN_REGMAP_REMOTE_READONLY_BIT : 0);
      ctx->regMap[i].data = ptr;
      ctx->regMap[i].size = size;
      lolan_regMapSort(ctx);   // sort the register map by path
      return LOLAN_RETVAL_YES;
    }
  }
  /* the register map is full */
  return LOLAN_RETVAL_GENERROR;
} /* lolan_regVar */

/**************************************************************************//**
 * @brief
 *   Check whether a LoLaN variable was remotely updated.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] ptr
 *   Pointer of the variable data (the LoLaN variable will be identified
 *   by this information).
 * @param[in] clearFlag
 *   If true, the remote update flag will be cleared.
 * @return
 *   LOLAN_RETVAL_YES: the LoLaN variable has been remotely
 *     updated.
 *   LOLAN_RETVAL_NO: the LoLaN variable has not been remotely updated
 *     since the last checking.
 *   LOLAN_RETVAL_GENERROR: no LoLaN variable is mapped to the specified
 *     memory address.
 *****************************************************************************/
int8_t lolan_regVarUpdated(lolan_ctx *ctx, const void *ptr, bool clearFlag)
{
  uint8_t i;

  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (ctx->regMap[i].p[0] != 0) {   // (skip free entries)
      if (ctx->regMap[i].data == ptr) {    // variable is found by data pointer
        if (ctx->regMap[i].flags & LOLAN_REGMAP_REMOTE_UPDATE_BIT) {   // check remote update flag
          if (clearFlag) {
            ctx->regMap[i].flags &= ~(LOLAN_REGMAP_REMOTE_UPDATE_BIT);
          }
          return LOLAN_RETVAL_YES;
        } else {
          return LOLAN_RETVAL_NO;
        }
      }
    }
  }
  /* no variable mapped to the specified address was found */
  return LOLAN_RETVAL_GENERROR;
} /* lolan_regVarUpdated */

/**************************************************************************//**
 * @brief
 *   Remove a LoLaN variable from the register map.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] ptr
 *   Pointer of the variable data (the LoLaN variable will be identified
 *   by this information).
 * @return
 *   LOLAN_RETVAL_YES: the LoLaN variable has been removed succesfully.
 *   LOLAN_RETVAL_GENERROR: no LoLaN variable is mapped to the specified
 *     memory address.
 *****************************************************************************/
int8_t lolan_rmVar(lolan_ctx *ctx, const void *ptr)
{
  uint8_t i;

  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (ctx->regMap[i].p[0] != 0) {   // (skip free entries)
      if (ctx->regMap[i].data == ptr) {   // variable is found by data pointer
        memset(ctx->regMap[i].p, 0, LOLAN_REGMAP_DEPTH);   // invalidate path
        ctx->regMap[i].flags = 0;    // invalidate flags (variable type)
        return LOLAN_RETVAL_YES;
      }
    }
  }
  /* no variable mapped to the specified address was found */
  return LOLAN_RETVAL_GENERROR;
} /* lolan_rmVar */

/**************************************************************************//**
 * @brief
 *   Set flags of a LoLaN variable.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] ptr
 *   Pointer of the variable data (the LoLaN variable will be identified
 *   by this information).
 * @param[in] flags
 *   The flags to be set.
 * @return
 *   LOLAN_RETVAL_YES: the action was successful.
 *   LOLAN_RETVAL_GENERROR: no LoLaN variable is mapped to the specified
 *     memory address.
 *****************************************************************************/
int8_t lolan_setFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags)
{
  uint8_t i;

  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (ctx->regMap[i].p[0] != 0) {   // (skip free entries)
      if (ctx->regMap[i].data == ptr) {    // variable is found by data pointer
        ctx->regMap[i].flags |= flags;      // set flags
        return LOLAN_RETVAL_YES;
      }
    }
  }
  /* no variable mapped to the specified address was found */
  return LOLAN_RETVAL_GENERROR;
} /* lolan_setFlag */

/**************************************************************************//**
 * @brief
 *   Clear flags of a LoLaN variable.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] ptr
 *   Pointer of the variable data (the LoLaN variable will be identified
 *   by this information).
 * @param[in] flags
 *   The flags to be cleared.
 * @return
 *   LOLAN_RETVAL_YES: the action was successful.
 *   LOLAN_RETVAL_GENERROR: no LoLaN variable is mapped to the specified
 *     memory address.
 *****************************************************************************/
int8_t lolan_clearFlag(lolan_ctx *ctx, const void *ptr, uint16_t flags)
{
  uint8_t i;

  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if (ctx->regMap[i].p[0] != 0) {    // (skip free entries)
      if (ctx->regMap[i].data == ptr) {    // variable is found by data pointer
        ctx->regMap[i].flags &= ~(flags);   // clear flags
        return LOLAN_RETVAL_YES;
      }
    }
  }
  /* no variable mapped to the specified address was found */
  return LOLAN_RETVAL_GENERROR;
} /* lolan_clearFlag */

/**************************************************************************//**
 * @brief
 *   Output the binary representation of a LoLaN packet to the specified
 *   buffer.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] lp
 *   Pointer to the source LoLaN packet structure.
 * @param[out] buf
 *   Memory address of the destination buffer.
 * @param[in] maxSize
 *   Specifies the maximum number of bytes allowed to be written to the
 *   output buffer to avoid buffer overflow. Set this parameter to zero
 *   if this limitation is not required.
 * @param[out] outputSize
 *   Pointer to a number variable which receives the number of bytes
 *   written to the output buffer.
 * @param[in] withCRC
 *   Appends a software calculated checksum (CRC16) to the packet if true.
 * @return
 *   LOLAN_RETVAL_YES:       LoLaN binary packet is created.
 *   LOLAN_RETVAL_GENERROR:  The packet size would exceed maxSize.
 *****************************************************************************/
int8_t lolan_createPacket(lolan_ctx *ctx, const lolan_Packet *lp, uint8_t *buf,
                          uint32_t maxSize, uint32_t *outputSize, bool withCRC)
{
  uint32_t size;

  /* pre-compute size */
  size = 7 + lp->payloadSize + (withCRC ? 2 : 0);

  /* error check */
  if (maxSize) {
    if (size > maxSize)   // the packet size would be larger than maxSize
      return LOLAN_RETVAL_GENERROR;
  }

  /* construct the packet header */
  buf[0] = lp->packetType;
  if (lp->securityEnabled)    buf[0] |= 0x08;
  if (lp->framePending)       buf[0] |= 0x10;
  if (lp->ackRequired)        buf[0] |= 0x20;
  buf[1] = 0x74;    // IEEE 802.15.4 protocol version = 3
  if (lp->routingRequested)   buf[1] |= 0x80;
  if (lp->packetRouted)       buf[1] |= 0x08;

  buf[2] = lp->packetCounter;

  buf[3] = (lp->fromId)      & 0xFF;
  buf[4] = (lp->fromId >> 8) & 0xFF;
  buf[5] = (lp->toId)        & 0xFF;
  buf[6] = (lp->toId >> 8)   & 0xFF;

  /* copy the packet payload */
  memcpy(&(buf[7]), lp->payload, lp->payloadSize);

  /* compute CRC if needed */
  if (withCRC) {
    uint16_t crc16 = CRC_calc(buf, 7 + lp->payloadSize);   // compute CRC
    buf[7 + lp->payloadSize]     = (crc16 >> 8) & 0xFF;
    buf[7 + lp->payloadSize + 1] =  crc16       & 0xFF;
  }

  *outputSize = size;   // output the size information

  return LOLAN_RETVAL_YES;
} /* lolan_createPacket */

/**************************************************************************//**
 * @brief
 *   Parse a packet (binary representation), and fill a LoLaN packet
 *   structure from it (if LoLaN).
 * @note
 *   IMPORTANT: lp->payload has to point to a buffer with a minimum size
 *   of LOLAN_PACKET_MAX_PAYLOAD_SIZE!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] pak
 *   The starting address of the input data (packet).
 * @param[in] pak_len
 *   Length of the input data (packet) in bytes.
 * @param[out] lp
 *   pointer to lolan Packet structure
 * @return
 *   LOLAN_RETVAL_YES:       LoLaN packet found, everything went o.k.
 *   LOLAN_RETVAL_NO:        Not a LoLaN packet.
 *   LOLAN_RETVAL_GENERROR:  An error has occurred. (e.g. CRC error)
 *****************************************************************************/
int8_t lolan_parsePacket(lolan_ctx *ctx, const uint8_t *pak, uint32_t pak_len, lolan_Packet *lp)
{
  /* error check */
  if (pak_len < 9)  // packet is too short
    return LOLAN_RETVAL_NO;
  if (pak_len > LOLAN_MAX_PACKET_SIZE)  // packet is too long
    return LOLAN_RETVAL_GENERROR;
  if (((pak[1] >> 4) & 0x03) != 3)   // checking 802.15.4 FRAME version
    return LOLAN_RETVAL_NO;

  /* parsing packet */
  lp->routingRequested = (pak[1] & 0x80) ? true : false;
  lp->packetRouted     = (pak[1] & 0x08) ? true : false;
  lp->packetType = pak[0] & 0x07;
  if (!((lp->packetType == ACK_PACKET) || (lp->packetType == LOLAN_INFORM)   // check type
      || (lp->packetType == LOLAN_GET) || (lp->packetType == LOLAN_SET)
      || (lp->packetType == LOLAN_CONTROL))) {
    return LOLAN_RETVAL_NO;   // invalid type (not a LoLaN packet?)
  }
  lp->securityEnabled = (pak[0] & 0x08) ? true : false;
  lp->framePending    = (pak[0] & 0x10) ? true : false;   //TODO: implement extended frames
  lp->ackRequired     = (pak[0] & 0x20) ? true : false;
  lp->packetCounter   = pak[2];
  lp->fromId          = pak[3] | (pak[4] << 8);
  lp->toId            = pak[5] | (pak[6] << 8);
  if (lp->securityEnabled) {
    /* TODO: implement security */
  } else {
    uint16_t crc16 = CRC_calc(pak, pak_len);
    if (crc16 != 0) {
      DLOG(("\n lolan_parsePacket(): CRC error"));
      DLOG(("\n CRC16: %04x", crc16));
      return LOLAN_RETVAL_GENERROR;
    }
    lp->payloadSize = pak_len - 9;
    memcpy(lp->payload, &(pak[7]), lp->payloadSize);
  }

  DLOG(("\n LoLaN packet t:%d s:%d ps:%d from:%d to:%d enc:%d", lp->packetType, pak_len,
        lp->payloadSize, lp->fromId, lp->toId, lp->securityEnabled));

  return LOLAN_RETVAL_YES;   // done
} /* lolan_parsePacket */

