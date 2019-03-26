/**************************************************************************//**
 * @file lolan-inform.c
 * @brief LoLaN INFORM functions
 * @author OMTLAB Kft.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"
#include "cbor.h"


/*
 * LoLaN INFORM definition
 * ~~~~~~~~~~~~~~~~~~~~
 *
 *   <LEGACY FORMAT>
 *
 *   The legacy format may be used if either only one variable needs
 *   to be reported, or the definition level and the base path of the
 *   variables are all the same.
 *   Example for (2, 7, 4):
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  base path = [2, 7] (array of u. integers)
 *   CBOR item #3:  key = 4 (u. integer)
 *   CBOR item #4:  value (any type)
 *
 *   Example for (1, 2, 0), (1, 3, 0):
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  base path = [1] (array of u. integers)
 *   CBOR item #3:  key = 2 (u. integer)
 *   CBOR item #4:  value (any type)
 *   CBOR item #5:  key = 3 (u. integer)
 *   CBOR item #6:  value (any type)
 *
 *   If no zero key is specified, the root level should be considered
 *   as base path.
 *   Example for (2, 0, 0), (7, 0, 0):
 *
 *   CBOR item #3:  key = 2 (u. integer)
 *   CBOR item #4:  value (any type)
 *   CBOR item #5:  key = 7 (u. integer)
 *   CBOR item #6:  value (any type)
 *
 *   <NEW FORMAT>
 *
 *   If the definition level and the base path is not alike for all
 *   variables to be reported (or LOLAN_FORCE_NEW_STYLE_INFORM = true),
 *   the variables will be reported with nested map items where the
 *   keys contain the variable path elements. The zero key contains
 *   the status code 299.
 *   Example for reporting (1, 5, 0), (2, 1, 3), (2, 1, 4), (2, 2, 0):
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  code = 299 (u. integer)
 *   CBOR item #3:  key = 1 (u. integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key = 5 (u. integer)
 *         CBOR item 4/#2:  value of data (1, 5, 0) (any type)
 *   CBOR item #5:  key = 2 (u. integer)
 *   CBOR item #6:  MAP
 *         CBOR item 6/#1:  key = 1 (u. integer)
 *         CBOR item 6/#2:  MAP
 *                 CBOR item 6/2/#1:  key = 3 (u. integer)
 *                 CBOR item 6/2/#2:  value of data (2, 1, 3) (any type)
 *                 CBOR item 6/2/#3:  key = 4 (u. integer)
 *                 CBOR item 6/2/#4:  value of data (2, 1, 4) (any type)
 *         CBOR item 6/#3:  key = 2 (u. integer)
 *         CBOR item 6/#4:  value of data (2, 2, 0) (any type)
 *
 */


/**************************************************************************//**
 * @brief
 *   Check for modified variables and create a LoLaN INFORM packet if needed.
 * @details
 *   This subroutine searches for variables where the
 *   LOLAN_REGMAP_LOCAL_UPDATE_BIT flag is set, and creates a LoLaN INFORM
 *   packet reporting (one/all of) these variables. If too many
 *   variables are affected (LoLaN packet size limit), a
 *   LOLAN_RETVAL_MEMERROR will be returned. In this case the subroutine
 *   should be called again disabling multiple reporting.
 * @note
 *   In the output packet structure the payload parameter should be
 *   assigned to a buffer with a minimum length of
 *   LOLAN_PACKET_MAX_PAYLOAD_SIZE!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[out] pak
 *   Pointer to the output LoLaN packet structure in which the INFORM
 *   will be generated.
 * @param[in] multi
 *   Set this parameter to TRUE to allow multiple reporting. If FALSE,
 *   only a single variable will be reported in an INFORM packet.
 * @return
 *    LOLAN_RETVAL_YES: A LoLaN INFORM packet is filled in the reply
 *      packet structure. Other variables may be present to INFORM,
 *      call again to ensure that no other variables should be
 *      reported (returning LOLAN_RETVAL_NO).
 *    LOLAN_RETVAL_NO: No variables to be reported.
 *    LOLAN_RETVAL_GENERROR: An error has occurred.
 *    LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *    LOLAN_RETVAL_MEMERROR: CBOR out of memory error (too much data is
 *      requested in one step). Disable multiple reporting and try again!
 ******************************************************************************/
int8_t lolan_createInform(lolan_ctx *ctx, lolan_Packet *pak, bool multi) {
  LR_SIZE_T i, count;
  uint8_t defLvl, bpath[LOLAN_REGMAP_DEPTH-1];
  bool dlbpsame;
  int8_t err;

  CborError cerr;
  CborEncoder enc, map_enc, array_enc;

  const uint16_t flags = LOLAN_REGMAP_LOCAL_UPDATE_BIT + LOLAN_REGMAP_INFORM_REQUEST_BIT;

  /* count the number of locally updated variables with INFORM request */
  count = lolanVarFlagCount(ctx, flags, &dlbpsame, &defLvl, bpath);
  if (count == 0) return LOLAN_RETVAL_NO;   // no variable to report

  /* clear the LOLAN_REGMAP_AUX_BIT flags */
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++)
    ctx->regMap[i].flags &= ~(LOLAN_REGMAP_AUX_BIT);

  cbor_encoder_init(&enc, pak->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the pak

  if (!dlbpsame || LOLAN_FORCE_NEW_STYLE_INFORM) {   // new style inform
    cerr = cbor_encoder_create_map(&enc, &map_enc, CborIndefiniteLength);   // create root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }
    if (createCborUintDataSimple(&map_enc, 0, 299, false) != LOLAN_RETVAL_YES) {   // encode status code
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }
    if (multi) {  // if multiple variable reporting is allowed
      err = lolanVarFlagToCbor(ctx, flags, &map_enc, true, false);   // encode the variables
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return err;
      }
    } else {   // if multiple variable reporting is not allowed
      for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {   // search for the first variable to report
        if ( ((ctx->regMap[i].flags & flags) == flags)
             && (ctx->regMap[i].p[0] != 0) ) {              // (not a free entry)
          ctx->regMap[i].flags |= LOLAN_REGMAP_AUX_BIT;  // set auxiliary flag
          break;
        }
      }
      err = lolanVarFlagToCbor(ctx, LOLAN_REGMAP_AUX_BIT, &map_enc, false, false);   // encode the variable
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return err;
      }
    }
    cerr = cbor_encoder_close_container(&enc, &map_enc);   // close root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
    }
  } else {   // old style inform
    if (!multi) count = 1;   // if multiple variable reporting is not allowed
    cerr = cbor_encoder_create_map(&enc, &map_enc, (defLvl > 1) ? count+1 : count);   // create root map
    /* the root map size is the count of the variables to report if the definition level is 1 (no base path
     * definition is required), otherwise +1 due to the base path definition with key=0
     */
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }
    /* create base path definition if needed */
    if (defLvl > 1) {  // base path definition is required
      cerr = cbor_encode_uint(&map_enc, 0);   // encode key=0
      if (cerr != CborNoError) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      cerr = cbor_encoder_create_array(&map_enc, &array_enc, defLvl-1);  // create array for base path
      if (cerr != CborNoError) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      for (i = 0; i < defLvl-1; i++) {   // encode base path
        cerr = cbor_encode_uint(&array_enc, bpath[i]);   // encode path item
        if (cerr != CborNoError) {
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
      }
      cerr = cbor_encoder_close_container(&map_enc, &array_enc);   // close array
      if (cerr != CborNoError) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
    }
    /* encode LoLaN variables */
    for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
      if ( ((ctx->regMap[i].flags & flags) == flags)      // find variables
           && (ctx->regMap[i].p[0] != 0) ) {              // (not a free entry)
        cerr = cbor_encode_uint(&map_enc, ctx->regMap[i].p[defLvl-1]);   // encode key (path item)
        if (cerr != CborNoError) {
          DLOG(("\n CBOR encode error"));
          return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
        }
        err = lolanVarToCbor(ctx, NULL, i, &map_enc);   // encode variable
        if (err != LOLAN_RETVAL_YES) {
          DLOG(("\n CBOR encode error"));
          return err;
        }
        ctx->regMap[i].flags |= LOLAN_REGMAP_AUX_BIT;  // set auxiliary flag (to delete the local update flags finally)
        if (!multi) break;   // if multiple variable reporting is not allowed: break after the first encoded variable
      }
    }
    cerr = cbor_encoder_close_container(&enc, &map_enc);   // close root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
    }
  }

  /* reset LOLAN_REGMAP_LOCAL_UPDATE_BIT flags (on variables marked with LOLAN_REGMAP_AUX_BIT)  */
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++)
    if (ctx->regMap[i].flags & LOLAN_REGMAP_AUX_BIT)   // auxiliary flag
      ctx->regMap[i].flags &= ~(LOLAN_REGMAP_LOCAL_UPDATE_BIT);   // reset local update flag

  /* fill the reply packet structure */
  pak->packetCounter = ctx->packetCounter++;   // the packet counter of the context is copied (and incremented)
  pak->packetType = LOLAN_INFORM;
  pak->fromId = ctx->myAddress;
  pak->toId = LOLAN_BROADCAST_ADDRESS;
  pak->ackRequired = false;
  pak->payloadSize = cbor_encoder_get_buffer_size(&enc, pak->payload);   // get the CBOR data size
  DLOG(("\n Encoded INFORM to %d bytes", pak->payloadSize));

  return LOLAN_RETVAL_YES;
} /* lolan_createInform */


// TODO: lolan_processInform()
