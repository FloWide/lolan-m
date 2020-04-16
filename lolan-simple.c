/**************************************************************************//**
 * @file lolan-simple.c
 * @brief LoLaN simple master functions
 * @author Sunstone-RTLS Ltd.
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"
#include "cbor.h"


/**************************************************************************//**
 * @brief
 *   Search for data in a LoLaN CBOR structure and get it.
 * @note
 *   FOR INTERNAL USE ONLY.
 * @param[in] pak
 *   Pointer to the LoLaN packet structure which contains the packet to be
 *   processed.
 * @param[in] rpath
 *   Address of the uint8_t array containing the path of the
 *   LoLaN variable to obtain. If the packet contains variable data
 *   nested by path, the exact path should be specified. In case of a
 *   legacy format INFORM payload the relative path should be passed
 *   (rpath[0] is used only).
 *   Set this parameter to NULL to get the first data found (excluding
 *   the zero key entry).
 * @param[out] data
 * @param[in] data_max
 * @param[out] data_len
 * @param[out] type
 *   See lolanGetDataFromCbor() for description.
 * @return
 *   LOLAN_RETVAL_YES: Data found.
 *   LOLAN_RETVAL_NO:  No data found.
 *   LOLAN_RETVAL_GENERROR: An error has occurred.
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
static int8_t lolan_seekAndGet(lolan_Packet *pak, const uint8_t *rpath, uint8_t *data,
          LV_SIZE_T data_max, LV_SIZE_T *data_len, uint8_t *type)
{
  uint8_t path[LOLAN_REGMAP_DEPTH];
  uint8_t i, alevel;
  int key;

  CborParser parser;
  CborValue root_it, it[LOLAN_REGMAP_DEPTH];
  CborError cerr;

  /* initialize and enter the root container (map) */
  cerr = cbor_parser_init(pak->payload, pak->payloadSize, 0, &parser, &root_it);  // initialize CBOR parser
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (cbor_value_get_type(&root_it) != CborMapType) return LOLAN_RETVAL_GENERROR;   // the root entry must be a CBOR map
  cerr = cbor_value_enter_container(&root_it, &it[0]);   // enter root map
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;

  /* process the nested CBOR structure */
  alevel = 0;  // address level is 0 at this point
  while (!((alevel == 0) && cbor_value_at_end(&it[alevel]))) {   // until entries are available in the root map level
    /* check for end of container */
    if (cbor_value_at_end(&it[alevel])) {   // end of map (alevel is always >0 at this point)
      cerr = cbor_value_leave_container(&it[alevel-1], &it[alevel]);   // leave container
      alevel--;  // decrement current address level
      if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
      continue;
    }
    /* extract key */
    if (cbor_value_get_type(&it[alevel]) != CborIntegerType) return LOLAN_RETVAL_GENERROR;  // check key of a key-data pair (must be integer)
    cbor_value_get_int(&it[alevel], &key);   // get key
    cerr = cbor_value_advance_fixed(&it[alevel]);   // advance iterator to data
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    if (cbor_value_at_end(&it[alevel])) return LOLAN_RETVAL_GENERROR;   // unexpected end of map (no data for key)
    /* process key-data pair */
    if ((key <= 0) || (key > 255)) {   // zero key found, or key can not be a path element
      /* advance to the next key */
      cerr = cbor_value_advance(&it[alevel]);
      if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    } else {   // other key found
      path[alevel] = key;  // store path element
      if (cbor_value_get_type(&it[alevel]) == CborMapType) {  // the data is a map -> subpath branch
        if (alevel < LOLAN_REGMAP_DEPTH-1) {  // entering a lower path level is o.k.
          cerr = cbor_value_enter_container(&it[alevel], &it[alevel+1]);   // enter map
          if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
          alevel++;  // increment current address level
        } else {  // can not enter a lower path, skip this sub-branch
          cerr = cbor_value_advance(&it[alevel]);   // skip map
          if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        }
      } else {  // the data is not a map (may be valid data)
        for (i = alevel+1; i < LOLAN_REGMAP_DEPTH; i++)   // correct path with zeros if needed
          path[i] = 0;
        if (rpath == NULL) {   // do not care the path, just return the position of data
          return lolanGetDataFromCbor(&it[alevel], data, data_max, data_len, type);    // get data
        } else {   // data with only the specified path is needed
          if (memcmp(path, rpath, LOLAN_REGMAP_DEPTH) == 0) {   // the specified path is found
            return lolanGetDataFromCbor(&it[alevel], data, data_max, data_len, type);    // get data
          }
        }
      }
    }
  }

  return LOLAN_RETVAL_NO;  // no data found if this point is reached
} /* lolan_seekAndGet */

/**************************************************************************//**
 * @brief
 *   Create a simple LoLaN SET request.
 * @details
 *   A simple SET request will be created in the specified LoLaN packet
 *   structure to update a remote LoLaN variable (only one variable!).
 *   The addressee of the request should be configured with
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
 *   Pointer to the LoLaN packet structure which will contain the SET
 *   request.
 * @param[in] path
 *   Address of the uint8_t array containing the exact path of the remote
 *   LoLaN variable to update.
 * @param[in] data
 *   The new data for the remote LoLaN variable.
 * @param[in] data_len
 *   Length of the data specified.
 * @param[in] type
 *   The type of the remote LoLaN variable to update.
 * @return
 *   LOLAN_RETVAL_YES: Request is successfully created.
 *   LOLAN_RETVAL_GENERROR: An error has occurred (e.g. the data length
 *     is not allowed for the specified variable type).
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *   LOLAN_RETVAL_MEMERROR: CBOR out of memory error.
 *****************************************************************************/
int8_t lolan_simpleCreateSet(lolan_ctx *ctx, lolan_Packet *pak, const uint8_t *path,
              uint8_t *data, LV_SIZE_T data_len, lolan_VarType type)
{
  CborEncoder enc, map_enc, array_enc;
  CborError cerr;

  uint8_t i, defLvl;
  int8_t err;

  /* check path */
  if (!lolanIsPathValid(path)) return LOLAN_RETVAL_GENERROR;    // invalid
  defLvl = lolanPathDefinitionLevel(NULL, path, NULL, false);   // get definition level
  if (defLvl == 0) return LOLAN_RETVAL_GENERROR;   // {0, 0, ...} can not be an exact variable path

  /* encode SET request (old style) */
  cbor_encoder_init(&enc, pak->payload, LOLAN_PACKET_MAX_PAYLOAD_SIZE, 0);  // initialize CBOR encoder for the request
  cerr = cbor_encoder_create_map(&enc, &map_enc, defLvl == 1 ? 1 : 2);   // create root map (base path is root: 1 entry, otherwise: 2 entries)
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (defLvl > 1) {   // base path needs to be encoded
    cerr = cbor_encode_uint(&map_enc, 0);   // encode key=0
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    cerr = cbor_encoder_create_array(&map_enc, &array_enc, defLvl-1);   // create array for base path
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    for (i = 0; i < defLvl-1; i++) {   // encode path elements (only up to definition level to reduce size)
      cerr = cbor_encode_uint(&array_enc, path[i]);   // encode path element
      if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    }
    cerr = cbor_encoder_close_container(&map_enc, &array_enc);   // close array
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }
  cerr = cbor_encode_uint(&map_enc, path[defLvl-1]);   // encode key (last path element)
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  err = lolanVarDataToCbor(data, data_len, type, &map_enc);   // encode new variable data
  if (err != LOLAN_RETVAL_YES) return err;
  cerr = cbor_encoder_close_container(&enc, &map_enc);   // close root map
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;

  /* fill the LoLaN packet structure */
  pak->packetType = LOLAN_SET;
  pak->payloadSize = cbor_encoder_get_buffer_size(&enc, pak->payload);   // get the CBOR data size
  if (ctx != NULL) {  // if context is specified
    pak->fromId = ctx->myAddress;
    pak->packetCounter = ctx->packetCounter++;   // the packet counter of the context is copied (and incremented)
  }
  DLOG(("\n Encoded GET request to %d bytes", pak->payloadSize));

  return LOLAN_RETVAL_YES;
} /* lolan_simpleCreateSet */

/**************************************************************************//**
 * @brief
 *   Process the reply for a simple request.
 * @details
 *   This procedure extracts data from a LoLaN ACK_PACKET replied to a
 *   simple request. (Simple request is SETting or GETting only one variable
 *   at once.)
 *   Operation:
 *     1. If a root map does not exist, the packet will be considered as a
 *        short GET reply, the only CBOR item will be obtained.
 *     2. If a zero key entry exists, but others not, the data from
 *        the zero key entry will be output. (Short SET reply.)
 *     3. If an other entry is found beside the zero key entry, the data
 *        from this entry will be output. (Normal GET and SET replies.)
 * @note
 *   If the packet contains complex data that can not be handled
 *   by this function, error may be returned, or fractional data may be
 *   extracted. So trying to process a reply to a complex request is not
 *   encouraged with this subroutine.
 * @param[in] pak
 *   Pointer to the LoLaN packet structure which contains the reply packet
 *   to be processed.
 * @param[out] data
 *   Address of the buffer which will receive the data (status code on SET,
 *   variable value on GET).
 * @param[in] data_max
 *   The maximum allowable data length (to avoid buffer overflow). Set this
 *   parameter to 0 if no limitation is required. Otherwise, it must be >=8
 *   (not to break integer and floating-point data).
 * @param[out] data_len
 *   Pointer to a number that receives the actual data length. If this
 *   output is higher than data_max, it means that the end of the data
 *   was discarded.
 * @param[out] type
 *   The type of the data got. It can be one of the lolan_VarType constants.
 * @param[out] zerokey
 *   The boolean variable on this memory address will be true if the returned
 *   data is from the zero key entry. In this case the data is always a
 *   16-bit unsigned integer value.
 * @return
 *   LOLAN_RETVAL_YES: Data got.
 *   LOLAN_RETVAL_GENERROR: An error has occurred (e.g. pak is not a
 *     LoLaN ACK packet).
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolan_simpleProcessAck(lolan_Packet *pak, uint8_t *data, LV_SIZE_T data_max,
             LV_SIZE_T *data_len, uint8_t *type, bool *zerokey)
{
  CborParser parser;
  CborValue it;
  CborError cerr;

  int8_t err;
  uint16_t zerovalue;

  /* error checking */
  if (pak->packetType != ACK_PACKET) return LOLAN_RETVAL_GENERROR;   // not an ACK packet
  if (data_max != 0 && data_max < 8) return LOLAN_RETVAL_GENERROR;  // (see description of data_max)

  /* determine reply type */
  cerr = cbor_parser_init(pak->payload, pak->payloadSize, 0, &parser, &it);   // initialize CBOR parser
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (cbor_value_get_type(&it) != CborMapType) {   // no root map found
    /* may be a short GET reply */
    *zerokey = false;
    return lolanGetDataFromCbor(&it, data, data_max, data_len, type);   // try to get data
  } else {   // root map found
    /* search for zero key entry */
    err = lolanGetZeroKeyEntryFromPayload(pak, NULL, &zerovalue, NULL);   // (path in zero key entry is not acceptable)
    if (err != LOLAN_RETVAL_YES) return LOLAN_RETVAL_GENERROR;   // zero key entry with integer data must exist
    /* look for other entries */
    err = lolan_seekAndGet(pak, NULL, data, data_max, data_len, type);   // try to detect any data
    switch (err) {
      case LOLAN_RETVAL_YES:   // data found
        *zerokey = false;
        return LOLAN_RETVAL_YES;
        break;
      case LOLAN_RETVAL_NO:   // no data found, data of the zero key entry should be output
        *zerokey = true;
        *((uint16_t*) data) = zerovalue;
        *data_len = 2;
        *type = LOLAN_UINT;
        return LOLAN_RETVAL_YES;
        break;
      default:   // error
        return err;
    }
  }
} /* lolan_simpleProcessAck */

/**************************************************************************//**
 * @brief
 *   Extract data from a LoLaN INFORM packet.
 * @details
 *   This procedure extracts data located on the specified path from a
 *   LoLaN INFORM packet. The source of the INFORM should be configured with
 *   the same parameters as the local settings (LOLAN_REGMAP_DEPTH,
 *   LOLAN_MAX_PACKET_SIZE).
 * @param[in] pak
 *   Pointer to the LoLaN packet structure which contains the INFORM packet
 *   to be processed.
 * @param[in] path
 *   Address of the uint8_t array containing the path of the
 *   LoLaN variable to obtain.
 * @param[out] data
 *   Address of the buffer which will receive the data.
 * @param[in] data_max
 *   The maximum allowable data length (to avoid buffer overflow). Set this
 *   parameter to 0 if no limitation is required. Otherwise, it must be >=8
 *   (not to break integer and floating-point data).
 * @param[out] data_len
 *   Pointer to a number that receives the actual data length. If this
 *   output is higher than data_max, it means that the end of the data
 *   was discarded.
 * @param[out] type
 *   The type of the data got. It can be one of the lolan_VarType constants.
 * @return
 *   LOLAN_RETVAL_YES: Data got.
 *   LOLAN_RETVAL_NO: No data found on the specified path.
 *   LOLAN_RETVAL_GENERROR: An error has occurred (e.g. invalid INFORM packet).
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolan_simpleExtractFromInform(lolan_Packet *pak, const uint8_t *path, uint8_t *data,
                  LV_SIZE_T data_max, LV_SIZE_T *data_len, uint8_t *type)
{
  int8_t err;
  uint8_t xpath[LOLAN_REGMAP_DEPTH];
  uint16_t zerovalue;
  bool isPath;

  /* error checking */
  if (pak->packetType != LOLAN_INFORM) return LOLAN_RETVAL_GENERROR;   // not an INFORM packet
  if (data_max != 0 && data_max < 8) return LOLAN_RETVAL_GENERROR;  // (see description of data_max)

  /* get base path or code from zero key entry */
  err = lolanGetZeroKeyEntryFromPayload(pak, xpath, &zerovalue, &isPath);
  switch (err) {
    case LOLAN_RETVAL_YES:   // zero key entry found
      break;
    case LOLAN_RETVAL_NO:   // no zero key entry
      /* legacy style INFORM, the base path is the root */
      memset(xpath, 0, LOLAN_REGMAP_DEPTH);
      isPath = true;
      break;
    default:   // error
      return err;
      break;
  }

  /* prepare Seek&Get */
  if (isPath) {    // legacy style INFORM
    uint8_t i, defLvl, xdefLvl;
    /* pre-check path matching */
    xdefLvl = lolanPathDefinitionLevel(NULL, xpath, NULL, false);   // get base path definition level
    defLvl = lolanPathDefinitionLevel(NULL, path, NULL, false);   // get input path definition level
    if (xdefLvl + 1 != defLvl) return LOLAN_RETVAL_NO;   // INFORM path level is not compatible with the specified path
    for (i = 0; i < xdefLvl; i++)   // compare the specified path and base path
      if (path[i] != xpath[i]) return LOLAN_RETVAL_NO;   // mismatch, it is not possible to find data with the specified path
    /* create path for search (only sub-path is needed) */
    memset(xpath, 0, LOLAN_REGMAP_DEPTH);   // zero xpath
    xpath[0] = path[xdefLvl];
  } else {   // new style INFORM
    if (zerovalue != 299) return LOLAN_RETVAL_GENERROR;   // the zero key should contain the status code 299
    memcpy(xpath, path, LOLAN_REGMAP_DEPTH);   // the full input path is required for search
  }

  /* looking for data (xpath now contains the path for search) */
  return lolan_seekAndGet(pak, xpath, data, data_max, data_len, type);
} /* lolan_simpleExtractFromInform */

