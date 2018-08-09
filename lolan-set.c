/**************************************************************************//**
 * @file lolan-set.c
 * @brief LoLaN SET functions
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
 * LoLaN SET definition
 * ~~~~~~~~~~~~~~~~~~~~
 *
 *   <COMMAND>
 *
 *   a) Old Style
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  base path (array of u. integers)
 *   CBOR item #3:  key (u. integer)
 *   CBOR item #4:  value (any type)
 *   CBOR item #5:  key (u. integer)
 *   CBOR item #6:  value (any type)
 *     ...
 *
 *   With zero key a base path should be specified, the other keys
 *   are the last path elements of exact LoLaN variable paths,
 *   the values are the new values for the variables.
 *   If a zero key entry does not exist, the root level should be
 *   considered as base path.
 *
 *   Examples:
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  [3, 1] (array of u. integers)        NOTE: [3, 1, 0] format is also o.k. (LOLAN_REGMAP_DEPTH=3)
 *   CBOR item #3:  1
 *   CBOR item #4:  -72 (integer)
 *   CBOR item #5:  4
 *   CBOR item #6:  "foo" (string)
 *   Meaning:
 *   Set [3, 1, 1] signed integer to -72, set [3, 1, 4] string to "foo".
 *
 *   CBOR item #1:  2
 *   CBOR item #2:  172 (u. integer)
 *   Meaning:
 *   Set [2, 0, 0] (if LOLAN_REGMAP_DEPTH=3) unsigned integer to 172.
 *
 *   b) New Style
 *
 *   The identification mark of a new style SET command is a compulsory
 *   zero key entry with a single integer value of 1. The desired new
 *   values of variables are listed with nested map items where the keys
 *   contain the variable path elements.
 *   Note: mapping zero path elements is invalid (key=0 is not allowed
 *     in nested map items).
 *
 *   Example:
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  value = 1 (u. integer)
 *   CBOR item #3:  key = 1 (u. integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key = 5 (u. integer)
 *         CBOR item 4/#2:  value = 82 (u. integer)
 *   CBOR item #5:  key = 2 (u. integer)
 *   CBOR item #6:  MAP
 *         CBOR item 6/#1:  key = 1 (u. integer)
 *         CBOR item 6/#2:  MAP
 *                 CBOR item 6/2/#1:  key = 3 (u. integer)
 *                 CBOR item 6/2/#2:  value = "bar" (string)
 *                 CBOR item 6/2/#3:  key = 4 (u. integer)
 *                 CBOR item 6/2/#4:  value = -19278 (integer)
 *         CBOR item 6/#3:  key = 2 (u. integer)
 *         CBOR item 6/#4:  value = 3.14159 (float)
 *   Meaning: set (1, 5, 0) unsigned integer to 82, set (2, 1, 3) string
 *   to "bar", set (2, 1, 4) signed integer to -19278, set (2, 2, 0)
 *   floating point number to 3.14159.
 *
 *   <REPLY>
 *
 *   The reply type (Old/New Style) depends on the command type.
 *
 *   a) Old Style
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  main code (u. integer)
 *   CBOR item #3:  key (u. integer)
 *   CBOR item #4:  code for variable (u. integer)
 *   CBOR item #5:  key (u. integer)
 *   CBOR item #6:  code for variable (u. integer)
 *     ...
 *
 *   The value with zero key indicades the overall result of the action,
 *   the other keys are the same as they were in the command, values
 *   indicate the result of each LoLaN variable update action.
 *   If every single variable update was o.k. and
 *   LOLAN_SET_SHORT_REPLY_IF_OK = true, only the zero key entry will
 *   be in the reply.
 *
 *   b) New Style
 *
 *   Contains the codes for the VALID variables requested to update,
 *   nested by variable path.
 *
 *   CBOR item #1:  key = 0 (u. integer)
 *   CBOR item #2:  main code (u. integer)
 *   CBOR item #3:  key (u. integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key (u. integer)
 *         CBOR item 4/#2:  code for variable (u. integer)
 *   CBOR item #5:  key (u. integer)
 *   CBOR item #6:  MAP
 *         CBOR item 6/#1:  key (u. integer)
 *         CBOR item 6/#2:  MAP
 *                 CBOR item 6/2/#1:  key (u. integer)
 *                 CBOR item 6/2/#2:  code for variable (u. integer)
 *                 CBOR item 6/2/#3:  key (u. integer)
 *                 CBOR item 6/2/#4:  code for variable (u. integer)
 *         CBOR item 6/#3:  key (u. integer)
 *         CBOR item 6/#4:  code for variable (u. integer)
 *            ...
 *
 *   If every single variable update was o.k. and
 *   LOLAN_SET_SHORT_REPLY_IF_OK = true (or all paths were invalid),
 *   only the zero key entry will be in the reply.
 *
 *   (For both New and Old Style replies:)
 *   Status codes for "main code":
 *     200: Everything went o.k. (1 variable or short reply)
 *     204: Everything went o.k., but no variables were updated.
 *            (The request contains no target variable to update.)
 *     207: Everything went o.k. (multiple variables)
 *     470: Some errors occurred, but at least 1 variable was updated
 *       successfully.
 *     471: No variables were updated due to errors.
 *   Status codes for "code for variable":
 *     200: Update o.k.
 *     404: Variable not found. (Old Style only)
 *     405: No update, the variable is read-only.
 *     472: Variable type mismatch.
 *     473: Integer overrange or string too long.
 *
 */


/**************************************************************************//**
 * @brief
 *   Process a LoLaN SET command.
 * @details
 *   This subroutine processes a SET command, automatically updates the
 *   requested variables and creates a reply packet structure filled up
 *   with information about the results of the action.
 * @note
 *   In the reply packet structure the payload parameter should be
 *   assigned to a buffer with a minimum length of
 *   LOLAN_PACKET_MAX_PAYLOAD_SIZE!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] pak
 *   Pointer to the input LoLaN packet structure which contains the SET
 *   command.
 * @param[out] reply
 *   Pointer to the LoLaN packet structure in which the reply will be
 *   generated.
 * @return
 *    LOLAN_RETVAL_YES: Request is processed, reply packet structure
 *      is filled.
 *    LOLAN_RETVAL_GENERROR: An error has occurred (e.g. pak is not a
 *      LoLaN SET packet).
 *    LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolan_processSet(lolan_ctx *ctx, lolan_Packet *pak, lolan_Packet *reply) {
  uint8_t i;
  int8_t err;
  uint8_t path[LOLAN_REGMAP_DEPTH], defLvl;
  uint16_t zerovalue;
  bool oldStyle;
  int key;
  uint8_t exterr, success, overall;
  bool problems;
  uint16_t code;
  lolan_BunchUpdateOutputStruct buStruct;

  CborError cerr;
  CborEncoder enc, map_enc;
  CborParser parser;
  CborValue it, map_it;

  DLOG(("\n LoLaN SET:  "));

  if (pak->packetType != LOLAN_SET) {   // not a LoLaN SET packet
    DLOG(("not a LoLaN SET packet"));
    return LOLAN_RETVAL_GENERROR;
  }

  /* extract (base) path */
  err = getZeroKeyEntryFromPayload(pak, path, &zerovalue, &oldStyle);   // (Old Style if a base path is specified)
  switch (err)  {
    case LOLAN_RETVAL_YES:   // zero key entry got
      /* (nothing to do here) */
      break;
    case LOLAN_RETVAL_NO:   // no zero key entry found
      /* Old Style SET, base path is root */
      memset(path, 0, LOLAN_REGMAP_DEPTH);  // reset the path elements
      oldStyle = true;
      break;
    case LOLAN_RETVAL_CBORERROR:
      DLOG(("CBOR error"));
      return LOLAN_RETVAL_CBORERROR;
      break;
    default:
      DLOG(("other error"));
      return LOLAN_RETVAL_GENERROR;
      break;
  }

  if (oldStyle) {   // Old Style SET

    DLOG(("Old Style "));
    for (i = 0; i < LOLAN_REGMAP_DEPTH; i++) {   // print path to debug log
      DLOG(("/%d", path[i]));
    }
    defLvl = pathDefinitionLevel(ctx, path, NULL, false);   // get path definition level
    if (defLvl >= LOLAN_REGMAP_DEPTH) {   // path should be a base path
      DLOG(("\n LoLaN CBOR packet error: path should be a base path"));
      return LOLAN_RETVAL_GENERROR;
    }

    /* initialize and enter the root map */
    if (!isPathValid(path)) {   // check path formal validity
      DLOG(("\n Formally invalid path in request."));
      return LOLAN_RETVAL_GENERROR;
    }
    cerr = cbor_parser_init(pak->payload, pak->payloadSize, 0, &parser, &it);   // initialize CBOR parser
    if (cerr != CborNoError) {
      DLOG(("\n CBOR parse error"));
      return LOLAN_RETVAL_CBORERROR;
    }
    if (cbor_value_get_type(&it) != CborMapType) {   // the root entry must be a CBOR map
      DLOG(("\n LoLaN CBOR packet error: root map not found"));
      return LOLAN_RETVAL_GENERROR;
    }
    cerr = cbor_value_enter_container(&it, &map_it);   // enter root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR parse error"));
      return LOLAN_RETVAL_CBORERROR;
    }
    /* initialize encoder and create root map */
    cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the reply
    cerr = cbor_encoder_create_map(&enc, &map_enc, CborIndefiniteLength);   // create root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }

    /* process entries */
    problems = false;
    success = 0;
    overall = 0;
    while (!cbor_value_at_end(&map_it)) {   // until the end of root map is reached
      /* extract key */
      if (cbor_value_get_type(&map_it) != CborIntegerType) {  // check key of a key-data pair (must be integer)
        DLOG(("\n LoLaN CBOR packet error: key has to be integer"));
        return LOLAN_RETVAL_GENERROR;
      }
      cbor_value_get_int(&map_it, &key);   // get key
      cerr = cbor_value_advance_fixed(&map_it);   // advance iterator to data
      if (cerr != CborNoError) {
        DLOG(("\n CBOR parse error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      if (cbor_value_at_end(&map_it)) {   // unexpected end of root map (no data for key)
        DLOG(("\n LoLaN CBOR packet error: key must be followed by data"));
        return LOLAN_RETVAL_GENERROR;
      }
      /* process key-data pair */
      if ((key <= 0) || (key > 255)) {   // key can not be a path element, or zero key found
        /* advance to the next key */
        cerr = cbor_value_advance(&map_it);
        if (cerr != CborNoError) {
          DLOG(("\n CBOR parse error"));
          return LOLAN_RETVAL_CBORERROR;
        }
        if (key != 0) problems = true;  // set problem indicator
      } else {  // key is o.k.
        path[defLvl] = key;   // complete path with key
        err = lolanVarUpdateFromCbor(ctx, path, &map_it, &exterr);  // update variable
        switch (err) {   // encode status code according to the result
          case LOLAN_RETVAL_YES:   // successfully updated
            code = 200;
            success++;   // count the number of successfully updated variables
            break;
          case LOLAN_RETVAL_NO:  // no update
            problems = true;   // set problem indicator
            switch (exterr) {
              case LVUFC_NOTFOUND:   // variable not found
                code = 404;
                break;
              case LVUFC_READONLY:   // read-only
                code = 405;
                break;
              case LVUFC_MISMATCH:   // type mismatch
                code = 472;
                break;
              case LVUFC_OUTOFRANGE:   // out of range
                code = 473;
                break;
            }
            break;
          default:  // other errors
            DLOG(("\n Error during lolanVarUpdateFromCbor()."));
            return err;
            break;
        }
        overall++;   // count the overall number of reported stati
        err = createCborUintDataSimple(&map_enc, key, code, false);   // encode status code
        if (err != LOLAN_RETVAL_YES) {
          DLOG(("\n CBOR encode error"));
          return LOLAN_RETVAL_CBORERROR;
        }
      }
    }
    /* finalization */
    if (!problems && LOLAN_SET_SHORT_REPLY_IF_OK) {  // no problems found and short reply needed
      cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // re-initialize CBOR encoder for the reply
      /* at this point the recently encoded CBOR output is invalidated ("erased") */
      err = createCborUintDataSimple(&enc, 0, 200, true);   // encode status code (with container)
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
    } else {   // long reply (preserve recently encoded CBOR output with the status codes for every variables)
      if (!problems) {  // no problems found
        switch (overall) {
          case 0:   // no variable is updated
            code = 204;
            break;
          case 1:   // a single variable is updated
            code = 200;
            break;
          default:   // multiple variables are updated
            code = 207;
            break;
        }
      } else {   // problem(s) found
        if (success == 0)  // only errors found
          code = 471;
          else   // at least 1 variable was updated in spite of problems
          code = 470;
      }
      err = createCborUintDataSimple(&map_enc, 0, code, false);   // encode main status code
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      cerr = cbor_encoder_close_container(&enc, &map_enc);  // close the root map
      if (cerr != CborNoError) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
    }

  } else {   // New Style SET

    DLOG(("New Style"));

    if (zerovalue != 1) {   // check "signature"
      DLOG(("\n Not a valid New Style LoLaN SET packet!"));
      return LOLAN_RETVAL_GENERROR;
    }
    /* clear the LOLAN_REGMAP_AUX_BIT flags */
    for (i = 0; i < LOLAN_REGMAP_SIZE; i++)
      ctx->regMap[i].flags &= ~(LOLAN_REGMAP_AUX_BIT);

    /* update the variables from CBOR with the new values nested by path  */
    err = lolanVarBunchUpdateFromCbor(ctx, pak, &buStruct);   // the AUX flags are set on the affected variables
    if (err != LOLAN_RETVAL_YES) {
      DLOG(("\n lolanVarBunchUpdateFromCbor() error"));
      return err;
    }

    /* initialize encoder and create root map */
    cbor_encoder_init(&enc, reply->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the reply
    cerr = cbor_encoder_create_map(&enc, &map_enc, CborIndefiniteLength);   // create root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }

    /* create zero key entry in function of the update results, create nested status code structure for variables affected */
    problems = (buStruct.invalid_keys > 0) || buStruct.toodeep || (buStruct.notfound > 0)
               || (buStruct.found > buStruct.updated);   // check for problems
    if (!problems && LOLAN_SET_SHORT_REPLY_IF_OK) {  // no problems found and short reply needed
      err = createCborUintDataSimple(&map_enc, 0, 200, false);   // encode status code
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
    } else {   // long reply
      if (!problems) {  // no problems found (-> what is found is also updated)
        switch (buStruct.found) {
          case 0:   // no variable is updated
            code = 204;
            break;
          case 1:   // a single variable is updated
            code = 200;
            break;
          default:   // multiple variables are updated
            code = 207;
            break;
        }
      } else {   // problem(s) found
        if (buStruct.updated == 0)  // only errors found
          code = 471;
          else   // at least 1 variable was updated in spite of problems
          code = 470;
      }
      err = createCborUintDataSimple(&map_enc, 0, code, false);   // encode main status code
      if (err != LOLAN_RETVAL_YES) {
        DLOG(("\n CBOR encode error"));
        return LOLAN_RETVAL_CBORERROR;
      }
      /* (the AUX flag indicates that the variable is affected by the update process) */
      err = lolanVarFlagToCbor(ctx, LOLAN_REGMAP_AUX_BIT, &map_enc, false, true);  // generate status codes in nested structure
      if ((err != LOLAN_RETVAL_YES) && (err != LOLAN_RETVAL_NO)) {
        DLOG(("\n lolanVarFlagToCbor() error"));
        return err;
      }
    }

    cerr = cbor_encoder_close_container(&enc, &map_enc);  // close the root map
    if (cerr != CborNoError) {
      DLOG(("\n CBOR encode error"));
      return LOLAN_RETVAL_CBORERROR;
    }

  }

  reply->packetCounter = pak->packetCounter;
  reply->packetType = ACK_PACKET;
  reply->fromId = ctx->myAddress;
  reply->toId = pak->fromId;
  reply->payloadSize = cbor_encoder_get_buffer_size(&enc, reply->payload);
  DLOG(("\n Encoded reply to %d bytes", reply->payloadSize));

  return LOLAN_RETVAL_YES;
} /* lolan_processSet */


// TODO: lolan_createSet(), lolan_processSetReply()
