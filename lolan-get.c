/**************************************************************************//**
 * @file lolan-get.c
 * @brief LoLaN GET functions
 * @author Sunstone-RTLS Ltd.
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"
#include "cbor.h"


/*
 * LoLaN GET definition
 * ~~~~~~~~~~~~~~~~~~~~
 *
 *   <COMMAND>
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  path (array of u. integers)
 *
 *   If the number of path levels is less than LOLAN_REGMAP_DEPTH,
 *   the path is considered as a base path, and all variables will
 *   be queried in a depth of LOLAN_REGMAP_RECURSION.
 *   Zeros on bottom levels do the same effect, e.g.:
 *     [1]     is equivalent with  [1, 0, 0]  (LOLAN_REGMAP_DEPTH=3)
 *     [3, 2]  is equivalent with  [3, 2, 0]  (LOLAN_REGMAP_DEPTH=3)
 *     []      is equivalent with  [0, 0, 0]  (LOLAN_REGMAP_DEPTH=3)
 *
 *   <REPLY>
 *
 *   If an exact path was specified, the reply will be only 1 CBOR
 *   item containing the requested value unless setting the
 *   LOLAN_FORCE_GET_VERBOSE_REPLY switch.
 *   (No root map is created in this case!)
 *
 *   CBOR item #1:  value (any type)
 *
 *   If LOLAN_FORCE_GET_VERBOSE_REPLY = true, the single variable
 *   will be also reported with nested map items where the keys contain
 *   the variable path elements. The zero key contains the HTTP status
 *   code 200 (OK).
 *   Example for (2, 7, 4):
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  code = 200 (u. integer)
 *   CBOR item #3:  key = 2 (u. integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key = 7 (u. integer)
 *         CBOR item 4/#2:  MAP
 *                 CBOR item 4/2/#1:  key = 4 (u. integer)
 *                 CBOR item 4/2/#2:  value (any type)
 *
 *   If only a base path was specified, the variables will be
 *   reported with nested map items where the keys contain the
 *   variable path elements. The zero key contains the HTTP status
 *   code 207 (Multi-Status).
 *   Example for getting with (4, 0, 0) base path, where the entries
 *   are (4, 1, 0), (4, 2, 1), (4, 2, 2), (4, 3, 0), the maximum
 *   recursion depth is set to at least 2:
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  code = 207 (u. integer)
 *   CBOR item #3:  key = 4 (u. integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key = 1 (u. integer)
 *         CBOR item 4/#2:  value of data (4, 1, 0) (any type)
 *         CBOR item 4/#3:  key = 2 (u. integer)
 *         CBOR item 4/#4:  MAP
 *                 CBOR item 4/4/#1:  key = 1 (u. integer)
 *                 CBOR item 4/4/#2:  value of data (4, 2, 1) (any type)
 *                 CBOR item 4/4/#3:  key = 2 (u. integer)
 *                 CBOR item 4/4/#4:  value of data (4, 2, 2) (any type)
 *         CBOR item 4/#5:  key = 3 (u. integer)
 *         CBOR item 4/#6:  value of data (4, 3, 0) (any type)
 *
 *
 *   <ERROR>
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  code (u. integer)
 *
 *   Code is an error code, one of the followings:
 *     404: No variable(s) found with the specified path.
 *     405: Recursive request (for multiple variables) is not allowed.
 *     507: The requested amount of data exceeds the LoLaN maximum
 *          packet size limit.
 *
 */


/**************************************************************************//**
 * @brief
 *   Process a LoLaN GET command.
 * @details
 *   This subroutine processes a GET command, and automatically creates the
 *   reply packet structure filled up with the requested data. Multiple
 *   values may be got with a single command, but the requested amount of
 *   data should not exceed the packet size limit.
 * @note
 *   In the reply packet structure the payload parameter should be
 *   assigned to a buffer with a minimum length of
 *   LOLAN_PACKET_MAX_PAYLOAD_SIZE!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] pak
 *   Pointer to the input LoLaN packet structure which contains the GET
 *   command.
 * @param[out] reply
 *   Pointer to the LoLaN packet structure in which the reply will be
 *   generated.
 * @return
 *   	LOLAN_RETVAL_YES: Request is processed, reply packet structure
 *   	  is filled.
 *    LOLAN_RETVAL_GENERROR: An error has occurred (e.g. pak is not a
 *      LoLaN GET packet).
 *    LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolan_processGet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply)
{
  LR_SIZE_T i;
  int8_t err;
  uint8_t path[LOLAN_REGMAP_DEPTH];
  LR_SIZE_T occ;
  bool force_vr;

  CborEncoder enc, map_enc;
  CborError cerr;

  DLOG(("\n LoLaN GET:  "));

  if (pak->packetType != LOLAN_PAK_GET) {   // not a LoLaN GET packet
    DLOG(("not a LoLaN GET packet"));
    return LOLAN_RETVAL_GENERROR;
  }

  /* extract (base) path */
  err = lolanGetZeroKeyEntryFromPayload(pak, path, NULL, NULL);
  switch (err)  {
    case LOLAN_RETVAL_YES:   // path is got
      for (i = 0; i < LOLAN_REGMAP_DEPTH; i++) {
        DLOG(("/%d", path[i]));
      }
      break;
    case LOLAN_RETVAL_NO:
      DLOG(("no path found in CBOR data"));
      return LOLAN_RETVAL_GENERROR;
      break;
    case LOLAN_RETVAL_CBORERROR:
      DLOG(("CBOR error"));
      return LOLAN_RETVAL_CBORERROR;
      break;
    default:
      return LOLAN_RETVAL_GENERROR;
      break;
  }

  if (!lolanIsPathValid(path)) {   // check path formal validity
    DLOG(("\n Formally invalid path in request."));
    return LOLAN_RETVAL_GENERROR;
  }
  lolanPathDefinitionLevel(ctx, path, &occ, true);  // obtain the number of variable occurrences

  cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the reply

  switch (occ) {   // the number of variable occurrences with the specified (base) path determine the reply type
    case 0:   // no variable found
      if (createCborUintDataSimple(&enc, 0, 404, true) != LOLAN_RETVAL_YES) {   // encode error code
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      break;
    case 1:   // one variable found
      /* figure out whether a single variable has been requested intentionally */
      force_vr = true;  // (the path of the variable should be reported when the only one occurrence is due to recursion depth restrictions)
      for (i = 0; i < LOLAN_REGMAP_SIZE; i++)   // check whether the path definition is exact
        if (memcmp(ctx->regMap[i].p, path, LOLAN_REGMAP_DEPTH) == 0) {  // a single variable is requested intentionally
          force_vr = false; // a simplified reply is allowed
          break;
        }
      /* encode variable */
      if (LOLAN_FORCE_GET_VERBOSE_REPLY || force_vr) {   // a full reply is needed
        cerr = cbor_encoder_create_map(&enc, &map_enc, CborIndefiniteLength);   // create root map
        if (cerr != CborNoError) {
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
        if (createCborUintDataSimple(&map_enc, 0, 200, false) != LOLAN_RETVAL_YES) {   // encode status code
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
        err = lolanVarBranchToCbor(ctx, path, &map_enc);   // encode the variable with nested path entries
        switch (err) {
          case LOLAN_RETVAL_YES:   // o.k.
            cerr = cbor_encoder_close_container(&enc, &map_enc);   // close the root map
            if (cerr != CborNoError) {
              DLOG(("\n CBOR encode error"));
              return LOLAN_RETVAL_CBORERROR;
            }
            break;
          case LOLAN_RETVAL_MEMERROR:   // the reply would be too big
            cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // re-initialize CBOR encoder
            if (createCborUintDataSimple(&enc, 0, 507, true) != LOLAN_RETVAL_YES) {   // encode error code
              DLOG(("\n CBOR encode error"));
              return LOLAN_RETVAL_CBORERROR;
            }
            break;
          default:   // other error
            DLOG(("\n error"));
            return err;
            break;
        }
      } else {   // a simplified reply is needed
        err = lolanVarToCbor(ctx, path, 0, &enc);   // encode the variable only
        switch (err) {
          case LOLAN_RETVAL_YES:   // o.k.
            /* do nothing */
            break;
          case LOLAN_RETVAL_MEMERROR:   // the reply would be too big
            cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // re-initialize CBOR encoder
            if (createCborUintDataSimple(&enc, 0, 507, true) != LOLAN_RETVAL_YES) {   // encode error code
              DLOG(("\n CBOR encode error"));
              return LOLAN_RETVAL_CBORERROR;
            }
            break;
          default:   // other error
            DLOG(("\n error"));
            return err;
            break;
        }
      }
      break;
    default:  // multiple variables are requested and found
      if (LOLAN_REGMAP_RECURSION == 0) {   // refuse recursive request
        if (createCborUintDataSimple(&enc, 0, 405, true) != LOLAN_RETVAL_YES) {   // encode error code
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
      } else {   // allow recursive request
        cerr = cbor_encoder_create_map(&enc, &map_enc, CborIndefiniteLength);   // create root map
        if (cerr != CborNoError) {
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
        if (createCborUintDataSimple(&map_enc, 0, 207, false) != LOLAN_RETVAL_YES) {   // encode status code
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
        err = lolanVarBranchToCbor(ctx, path, &map_enc);   // encode the variables with nested path entries
        switch (err) {
          case LOLAN_RETVAL_YES:   // o.k.
            cerr = cbor_encoder_close_container(&enc, &map_enc);   // close the root map
            if (cerr != CborNoError) {
              DLOG(("\n CBOR encode error"));
              return LOLAN_RETVAL_CBORERROR;
            }
            break;
          case LOLAN_RETVAL_MEMERROR:   // the reply would be too big
            cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // re-initialize CBOR encoder
            if (createCborUintDataSimple(&enc, 0, 507, true) != LOLAN_RETVAL_YES) {   // encode error code
              DLOG(("\n CBOR encode error"));
              return LOLAN_RETVAL_CBORERROR;
            }
            break;
          default:   // other error
            DLOG(("\n error"));
            return err;
            break;
        }
      }
      break;
  }

  /* fill the reply packet structure */
  reply->packetCounter = pak->packetCounter;   // the packetCounter value for the reply should be the same as the request's
  reply->packetType = LOLAN_PAK_ACK;
  reply->multiPart = LOLAN_MPC_NOMULTIPART;
  if (LOLAN_COPY_ROUTINGREQUEST_ON_ACK) reply->routingRequested = pak->routingRequested;
  reply->fromId = ctx->myAddress;   // from us
  reply->toId = pak->fromId;        // back to the sender of the request
  reply->payloadSize = cbor_encoder_get_buffer_size(&enc, reply->payload);   // get the CBOR data size
  DLOG(("\n Encoded reply to %d bytes", reply->payloadSize));

  return LOLAN_RETVAL_YES;
} /* lolan_processGet */


/**************************************************************************//**
 * @brief
 *   Create a LoLaN GET request.
 * @details
 *   This procedure creates a GET request in the specified LoLaN packet
 *   structure. The addressee of the request should be configured with
 *   the same parameters as the local settings (LOLAN_REGMAP_DEPTH,
 *   LOLAN_MAX_PACKET_SIZE).
 * @note
 *   In the LoLaN packet structure the payload parameter should be
 *   assigned to a buffer with a minimum length of
 *   LOLAN_PACKET_MAX_PAYLOAD_SIZE!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable. If NULL, the packetCounter
 *   and fromId fields will not be modified in pak.
 * @param[out] pak
 *   Pointer to the LoLaN packet structure which will contain the GET
 *   request.
 * @param[in] path
 *   Address of the uint8_t array containing the (sub)path of the
 *   remote LoLaN variable(s) to get.
 * @return
 *   LOLAN_RETVAL_YES: Request is successfully created.
 *   LOLAN_RETVAL_GENERROR: An error has occurred (e.g. formally invalid
 *     path).
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolan_createGet(lolan_ctx *ctx, lolan_Packet *pak, uint8_t *path)
{
  CborEncoder enc, map_enc, array_enc;
  CborError cerr;
  uint8_t i, defLvl;

  /* check path */
  if (!lolanIsPathValid(path)) return LOLAN_RETVAL_GENERROR;    // invalid
  defLvl = lolanPathDefinitionLevel(NULL, path, NULL, false);   // get definition level

  /* encode GET request */
  cbor_encoder_init(&enc, pak->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the request
  cerr = cbor_encoder_create_map(&enc, &map_enc, 1);   // create root map (1 entry)
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  cerr = cbor_encode_uint(&map_enc, 0);   // encode key=0
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  cerr = cbor_encoder_create_array(&map_enc, &array_enc, defLvl);   // create array to encode (sub)path
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  for (i = 0; i < defLvl; i++) {   // encode path elements (only up to definition level to reduce size)
    cerr = cbor_encode_uint(&array_enc, path[i]);   // encode path element
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }
  cerr = cbor_encoder_close_container(&map_enc, &array_enc);   // close array
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  cerr = cbor_encoder_close_container(&enc, &map_enc);   // close root map
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;

  /* fill the LoLaN packet structure */
  pak->packetType = LOLAN_PAK_GET;
  pak->multiPart = LOLAN_MPC_NOMULTIPART;
  pak->payloadSize = cbor_encoder_get_buffer_size(&enc, pak->payload);   // get the CBOR data size
  if (ctx != NULL) {  // if context is specified
    pak->fromId = ctx->myAddress;
    pak->packetCounter = ctx->packetCounter++;   // the packet counter of the context is copied (and incremented)
  }
  DLOG(("\n Encoded GET request to %d bytes", pak->payloadSize));

  return LOLAN_RETVAL_YES;
} /* lolan_createGet */


// TODO: lolan_processGetReply()
