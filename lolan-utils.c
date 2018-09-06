/**************************************************************************//**
 * @file lolan-utils.c
 * @brief LoLaN utility functions
 * @author OMTLAB Kft.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lolan_config.h"
#include "lolan.h"
#include "lolan-utils.h"
#include "cbor.h"



typedef enum {        // auxiliary enumeration for addCborItemNestedPath() function
  LVTCNPAUX_INITIAL,    // do the initialization and add the current item
  LVTCNPAUX_NORMAL,     // next items
  LVTCNPAUX_FINALIZE    // the recent was the last item, do the finalization
} lolanVarToCborNestedPath_aux;

/**************************************************************************//**
 * @brief
 *   Determine whether a path is valid or not.
 * @details
 *   A valid path should contain zeros only on the bottom n levels.
 *   E.g.  valid paths:    (2, 5, 1, 1)  (1, 7, 2, 0)  (4, 0, 0)
 *                         (0, 0, 0)
 *         invalid paths:  (0, 2, 2)  (3, 0, 2, 2)
 * @param[in] path
 *   Address of the uint8_t array containing the LoLaN variable path.
 * @return
 *   True if the path is valid, otherwise false.
 ******************************************************************************/
bool isPathValid(const uint8_t *path)
{
  uint8_t i, aux;

  aux = 0;
  for (i = 0; i < LOLAN_REGMAP_DEPTH; i++) {
    if (path[i] == 0) {
      aux = 1;
    } else
      if (aux) return false;   // other values under a zero part
  }
  return true;
} /* isPathValid */

/**************************************************************************//**
 * @brief
 *   Determine the path definition level.
 * @details
 *   The path definition level means how deeply is the LoLaN variable
 *   path defined.
 *   Examples (for LOLAN_REGMAP_DEPTH = 4):
 *     2/2/7/1   -> definition level is 4 (always exact)
 *     1/13/2/0  -> definition level is 3
 *     4/0/0/0   -> definition level is 1
 *     0/0/0/0   -> definition level is 0
 * @param[in] ctx
 *   Pointer to the LoLaN context variable. If the parameter "occurrences"
 *   is set to NULL, this parameter may also be NULL.
 * @param[in] path
 *   Address of the uint8_t array containing the LoLaN variable path.
 * @param[out] occurrences
 *   This value will be number of variable occurrences with the specified
 *   (base) path. If the definition level equals to LOLAN_REGMAP_DEPTH,
 *   the path is always exact, meaning that only one variable may be
 *   available with this path. If no LoLaN variable is available with
 *   the specified (base) path, this output will be 0.
 *   If no need for calculating the occurrences, set the pointer to NULL.
 * @param[in] occ_maxrec
 *   If true, only the variables reachable from the specified base path
 *   within LOLAN_REGMAP_RECURSION maximum recursion depth will be counted
 *   in the number of occurrences.
 * @return
 *   The definition level of the specified path.
 ******************************************************************************/
uint8_t pathDefinitionLevel(lolan_ctx *ctx, const uint8_t *path, uint8_t *occurrences, bool occ_maxrec)
{
  uint8_t i, defLvl;

  /* determine the definition level (no check for invalid paths) */
  for (i = 0; i < LOLAN_REGMAP_DEPTH; i++)
    if (path[i] == 0) break;
  defLvl = i;
  /* count the number of occurrences */
  if (occurrences != NULL) {
    *occurrences = 0;
    for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
      if ( (memcmp(ctx->regMap[i].p, path, defLvl) == 0) &&       // (on defLvL=0 memcmp returns 0)
           (ctx->regMap[i].p[0] != 0)                       ) {   // register map entry is valid
        if (!occ_maxrec)    // no restriction for maximum recursion depth
          (*occurrences)++;
          else {   // only variables within the maxiumum recursion level below the base path are counted
            if (pathDefinitionLevel(NULL, ctx->regMap[i].p, NULL, false) <= defLvl + LOLAN_REGMAP_RECURSION)  // (recursive call)
              (*occurrences)++;
        }
      }
    }
  }

  return defLvl;
} /* pathDefinitionLevel */

/**************************************************************************//**
 * @brief
 *   Count the LoLaN variables where the specified flags are set.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] flags
 *   The variables having these flags (e.g. LOLAN_REGMAP_LOCAL_UPDATE_BIT)
 *   set will be counted. (If this parameter specifies multiple flags,
 *   the variable will be counted if all flags are set.)
 * @param[out] dlbpsame
 *   Pointer to a boolean variable. This output will be set TRUE if
 *   the definition level and the base path is the same for all variables
 *   counted. Otherwise it will be false. If true, defLevel will contain
 *   the common definition level.
 *   Set this parameter to NULL if this function is not needed.
 * @param[out] defLevel
 *   If the dlbpsame output is true, the definition level will be output
 *   to the referred variable. This parameter may be also NULL if dlbpsame
 *   is unassigned, otherwise it must point to a valid uint8_t variable.
 * @param[out] bpath
 *   If the dlbpsame output is true, the base path will be output
 *   to the referred array. This parameter may be also NULL if dlbpsame
 *   is unassigned, otherwise it must point to an uint8_t array with
 *   a minimum length of (LOLAN_REGMAP_DEPTH-1).
 * @return
 *   The number of variables counted.
 ******************************************************************************/
uint8_t lolanVarFlagCount(lolan_ctx *ctx, uint16_t flags, bool *dlbpsame,
                          uint8_t *defLevel, uint8_t *bpath)
{
  uint8_t i, found;
  uint8_t bpsave[LOLAN_REGMAP_DEPTH-1];
  uint8_t defLvl = 1;

  found = 0;
  if (dlbpsame != NULL) *dlbpsame = true;
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if ( ((ctx->regMap[i].flags & flags) == flags)      // variable found with the specified flags
         && (ctx->regMap[i].p[0] != 0) ) {              // (not a free entry)
      found++;
      if ((dlbpsame != NULL) && *dlbpsame) {  // if definition level and base path is the same so far (spare computing if not)
        if (found == 1) {  // first
          defLvl = pathDefinitionLevel(ctx, ctx->regMap[i].p, NULL, false);  // get and store definition level
          memcpy(bpsave, ctx->regMap[i].p, defLvl-1);   // store base path
        } else {  // not first
          if ( !( (pathDefinitionLevel(ctx, ctx->regMap[i].p, NULL, false) == defLvl)  // not the same definition level
                  && (memcmp(ctx->regMap[i].p, bpsave, defLvl-1) == 0) ) )    //  or not the same base path
            *dlbpsame = false;   // clear indicator
        }
      }
    }
  }

  if ((dlbpsame != NULL) && *dlbpsame) {  // definition level and base path is the same
    *defLevel = defLvl;  // output the definition level
    memcpy(bpath, bpsave, defLvl-1);   // output the base path
  }

  return found;
} /* lolanVarFlagCount */

/**************************************************************************//**
 * @brief
 *   Get the status code of a variable after lolanVarUpdateFromCbor().
 * @note
 *   FOR INTERNAL USE ONLY.
 ******************************************************************************/
uint16_t getLolanSetStatusCodeForVariable(lolan_ctx *ctx, uint8_t index)
{
  if (ctx->regMap[index].flags & LOLAN_REGMAP_AUX_BIT) {
    if (ctx->regMap[index].flags & LOLAN_REGMAP_REMOTE_UPDATE_BIT)
      return 200;  // update o.k.
    if (ctx->regMap[index].flags & LOLAN_REGMAP_REMOTE_READONLY_BIT)
      return 405;  // no update, the variable is read-only
    if (ctx->regMap[index].flags & LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT)
      return 472;  // no update, variable type mismatch
    if (ctx->regMap[index].flags & LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT)
      return 473;  // no update, integer overrange or string too long
  }
  return 0;  // (the execution should not get here)
} /* getLolanSetStatusCodeForVariable */

/**************************************************************************//**
 * @brief
 *   Helper function for lolan_regMapSort().
 ******************************************************************************/
void lolan_regMapSwap(lolan_RegMap *entry1, lolan_RegMap *entry2)
{
  lolan_RegMap tmp = *entry1;
  *entry1 = *entry2;
  *entry2 = tmp;
} /* lolan_regMapSwap */

/**************************************************************************//**
 * @brief
 *   Sort the LoLaN register map by path in ascending order. The free
 *   entries will be at the end of the array.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 ******************************************************************************/
void lolan_regMapSort(lolan_ctx *ctx)
{
  uint8_t i, j;
  bool changed;

  j = LOLAN_REGMAP_SIZE;
  do {
    changed = false;
    for (i = 1; i < j; i++) {
      if ((ctx->regMap[i-1].p[0] == 0) && (ctx->regMap[i].p[0] != 0)) {   // the zero entries should be sent to the end
        lolan_regMapSwap(&(ctx->regMap[i-1]), &(ctx->regMap[i]));   // swap entries
        changed = true;
      } else if (ctx->regMap[i].p[0] == 0) {
        // do nothing
      } else if (memcmp(ctx->regMap[i-1].p, ctx->regMap[i].p, LOLAN_REGMAP_DEPTH) > 0) {   // the [i] item is less than the [i-1]
        lolan_regMapSwap(&(ctx->regMap[i-1]), &(ctx->regMap[i]));   // swap entries
        changed = true;
      }
    }
    j--;
  } while (changed);
} /* lolan_regMapSort */

/**************************************************************************//**
 * @brief
 *   This procedure decodes a LoLaN path from a CBOR integer array.
 * @param[out] path
 *   Pointer to the uint8_t array that receives the decoded path.
 *   The size of this buffer must be at least LOLAN_REGMAP_DEPTH.
 * @param[in] it
 *   Pointer to CborValue (iterator).
 * @return
 *   LOLAN_RETVAL_YES:        The path was decoded successfully.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 ******************************************************************************/
int8_t getPathFromCbor(uint8_t *path, CborValue *it)
{
  uint8_t cnt;
  CborValue ait;
  CborError err;
  int val;

  memset(path, 0, LOLAN_REGMAP_DEPTH);  // reset the path elements

  if (!cbor_value_is_container(it)) {   // check whether a container is the next item
    DLOG(("\n LoLaN CBOR packet error: LoLaN variable path container not found"));
    return LOLAN_RETVAL_GENERROR;
  }
  err = cbor_value_enter_container(it, &ait);   // enter the container (may be map or array)
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;

  cnt = 0;
  while (!cbor_value_at_end(&ait)) {   // extract path elements
    if (cnt < LOLAN_REGMAP_DEPTH) {
      if (cbor_value_get_type(&ait) != CborIntegerType) {   // if not integer
        DLOG(("\n LoLaN CBOR packet error: LoLaN variable path must be a container of integers"));
        return LOLAN_RETVAL_GENERROR;
      }
      cbor_value_get_int(&ait, &val);   // decode integer
      if ((val > 255) || (val < 0)) {  // path must consist of 8-bit integer elements
        DLOG(("\n LoLaN CBOR packet error: LoLaN variable path element must be 0..255"));
        return LOLAN_RETVAL_GENERROR;
      }
      path[cnt] = val;  // save path element
      cnt++;
    } else {  // the path is too long
      DLOG(("\n LoLaN CBOR packet error: LoLaN variable path length exceeds local RegMap depth"));
      return LOLAN_RETVAL_GENERROR;
    }
    err = cbor_value_advance_fixed(&ait);
    if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }

  err = cbor_value_leave_container(it, &ait);   // leave the container
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;

  return LOLAN_RETVAL_YES;
} /* getPathFromCbor */

/**************************************************************************//**
 * @brief
 *   Extract the zero key CBOR entry from a LoLaN packet payload, which
 *   has to be an unsigned integer value or a LoLaN path.
 * @details
 *   This subroutine searches for the key=0 entry in a CBOR type LoLaN
 *   packet, and extracts either an unsigned integer value or a LoLaN
 *   path from it. The unsigned integer should be representable on 16 bits.
 * @param[in] lp
 *   Pointer to the LoLaN packet structure from which the path should be
 *   extracted.
 * @param[out] path
 *   Pointer to the uint8_t array that receives the decoded path.
 *   The size of this buffer must be at least LOLAN_REGMAP_DEPTH.
 *   Set this parameter to NULL to return with error if the zero key
 *   entry contains a path.
 * @param[out] value
 *   Pointer to a uin16_t numbert that receives the decoded integer.
 *   Set this parameter to NULL to return with error if the zero key
 *   entry contains an integer.
 * @param[out] isPath
 *   The memory address of the boolean variable that will be TRUE if
 *   the zero key entry contains a path, and FALSE if it is an
 *   integer. (Value is meaningless when returning with error.)
 *   May be set to NULL if either path or value is set to NULL.
 * @return
 *   LOLAN_RETVAL_YES:        The path or integer was extracted successfully.
 *   LOLAN_RETVAL_NO:         No zero key entry found in CBOR data.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 ******************************************************************************/
int8_t getZeroKeyEntryFromPayload(const lolan_Packet *lp, uint8_t *path, uint16_t *value, bool *isPath)
{
  CborParser parser;
  CborValue it, rit;
  CborError err;
  int key;
  uint64_t u64;

  /* initialize and enter the root container (map) */
  err = cbor_parser_init(lp->payload, lp->payloadSize, 0, &parser, &it);  // initialize CBOR parser
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (cbor_value_get_type(&it) != CborMapType) {   // the root entry must be a CBOR map
    DLOG(("\n LoLaN CBOR packet error: root map not found"));
    return LOLAN_RETVAL_GENERROR;
  }
  err = cbor_value_enter_container(&it, &rit);   // enter root map
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;

  /* find key = 0 */
  while (!cbor_value_at_end(&rit)) {    // until entries are available in the root map
    if (cbor_value_get_type(&rit) != CborIntegerType) {  // check key of a key-data pair (must be integer)
      DLOG(("\n LoLaN CBOR packet error: key has to be integer"));
      return LOLAN_RETVAL_GENERROR;
    }
    cbor_value_get_int(&rit, &key);   // get key
    err = cbor_value_advance_fixed(&rit);   // advance iterator to data
    if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
    if (cbor_value_at_end(&rit)) {   // unexpected end of root map (no data for key)
      DLOG(("\n LoLaN CBOR packet error: key must be followed by data"));
      return LOLAN_RETVAL_GENERROR;
    }
    if (key == 0) {   // zero key found
      if (cbor_value_is_container(&rit)) {   // the next item is a container
        if (path == NULL) return LOLAN_RETVAL_GENERROR;   // if no path is acceptable -> error
        if (isPath) *isPath = true;   // it may be a path
        return getPathFromCbor(path, &rit);
      } else {
        if (cbor_value_is_unsigned_integer(&rit)) {   // the next item is an unsigned integer
          if (value == NULL) return LOLAN_RETVAL_GENERROR;   // if no integer is acceptable -> error
          if (isPath) *isPath = false;   // it is an integer
          cbor_value_get_uint64(&rit, &u64);   // get value
          *value = (u64 > UINT16_MAX) ? UINT16_MAX : u64;  // the number should be representable on 16-bit
          return LOLAN_RETVAL_YES;
        } else {
          DLOG(("\n LoLaN CBOR packet error: zero key entry contains neither path nor uint"));
          return LOLAN_RETVAL_GENERROR;
        }
      }
    }
    /* an other key found, advance to the next key */
    err = cbor_value_advance(&rit);
    if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }

  return LOLAN_RETVAL_NO;  // at this point no zero key entry was found
} /* getZeroKeyEntryFromPayload */

/**************************************************************************//**
 * @brief
 *   Get the next LoLaN variable data from CBOR.
 * @details
 *   This procedure parses the next data from a CBOR stream, and returns it
 *   with its length and type. Only the types that may represent a LoLaN
 *   variable are allowed. The CBOR iterator will be advanced in all cases.
 * @param[in] it
 *   Pointer to CborValue (iterator).
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
 *   LOLAN_RETVAL_GENERROR: An error has occurred.
 *   LOLAN_RETVAL_CBORERROR: A CBOR-related error has occurred.
 *****************************************************************************/
int8_t lolanGetDataFromCbor(CborValue *it, uint8_t *data, uint8_t data_max,
           uint8_t *data_len, uint8_t *type)
{
  CborType ctype;
  CborError cerr;

  /* error checking */
  if (data_max != 0 && data_max < 8) return LOLAN_RETVAL_GENERROR;  // (see description of data_max)

  /* get data */
  ctype = cbor_value_get_type(it);   // get CBOR entry type
  switch (ctype) {
    case CborIntegerType:   // integer
      if (cbor_value_is_unsigned_integer(it)) {   // non-negative integer
        uint64_t val;
        cbor_value_get_uint64(it, &val);  // decode value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        /* determine minimum representation width and store value */
        if (val > UINT32_MAX) {   // 64-bit
          *data_len = 8;
          *((uint64_t*) data) = val;
        } else if (val > UINT16_MAX) {  // 32-bit
          *data_len = 4;
          *((uint32_t*) data) = val;
        } else if (val > UINT8_MAX) {  // 16-bit
          *data_len = 2;
          *((uint16_t*) data) = val;
        } else {   // 8-bit
          *data_len = 1;
          *data = val;
        }
        *type = LOLAN_UINT;
      } else {   // negative integer
        int64_t val;
        cbor_value_get_int64(it, &val);   // decode value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        /* determine minimum representation width and store value */
        if (val > INT32_MAX || val < INT32_MIN) {   // 64-bit
          *data_len = 8;
          *((int64_t*) data) = val;
        } else if (val > INT16_MAX || val < INT16_MIN) {  // 32-bit
          *data_len = 4;
          *((int32_t*) data) = val;
        } else if (val > INT8_MAX || val < INT8_MIN) {  // 16-bit
          *data_len = 2;
          *((int16_t*) data) = val;
        } else {   // 8-bit
          *data_len = 1;
          *((int8_t*) data) = val;
        }
        *type = LOLAN_INT;
      }
      break;
    case CborByteStringType:   // byte string
      {
        size_t len;
        len = data_max == 0 ? 255 : data_max;    // 255 = "unlimited size"
        cbor_value_copy_byte_string(it, data, &len, it);   // get string (the CBOR iterator is also advanced)
        *data_len = len;
        *type = LOLAN_STR;
      }
      break;
    case CborTextStringType:   // text string
      {
        size_t len;
        len = data_max == 0 ? 255 : data_max;    // 255 = "unlimited size"
        cbor_value_copy_text_string(it, data, &len, it);   // get string (the CBOR iterator is also advanced)
        *data_len = len;
        *type = LOLAN_STR;
      }
      break;
    case CborFloatType:   // 32-bit floating-point
      {
        float val;
        cbor_value_get_float(it, &val);   // get value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        *((float*) data) = val;   // decode and store value
        *data_len = 4;
        *type = LOLAN_FLOAT;
      }
      break;
    case CborDoubleType:
      {
        double val;
        cbor_value_get_double(it, &val);   // get value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        *((double*) data) = val;   // decode and store value
        *data_len = 8;
        *type = LOLAN_FLOAT;
      }
      break;
    case CborInvalidType:   // invalid CBOR entry
      return LOLAN_RETVAL_CBORERROR;
      break;
    default:   // type not allowed in LoLaN
      return LOLAN_RETVAL_GENERROR;
      break;
  }

  return LOLAN_RETVAL_YES;
} /* lolanGetDataFromCbor */

/**************************************************************************//**
 * @brief
 *   Update a single LoLaN variable from CBOR.
 * @details
 *   This subroutine reads the next CBOR item and tries to update the LoLaN
 *   variable specified by path with the data. The CBOR iterator will
 *   be advanced in all cases.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] path
 *   Address of the uint8_t array containing the path of the LoLaN
 *   variable to update.
 * @param[in] it
 *   Pointer to CborValue (iterator).
 * @param[out] error
 *   Address of the uint8_t variable that receives the variable update
 *   result. Set this parameter to null if this output is not required.
 *   Meaning of values:
 *     LVUFC_NOTFOUND:  No variable found with the specified path.
 *     LVUFC_READONLY:  The variable can not be updated because it
 *       is read-only.
 *       The LOLAN_REGMAP_AUX_BIT flag of the variable is set.
 *     LVUFC_MISMATCH:  The variable can not be updated because the
 *       type of CBOR data is different.
 *       The LOLAN_REGMAP_AUX_BIT and LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT
 *       flags of the variable are set.
 *     LVUFC_OUTOFRANGE:  The new data is out of range (integer) or
 *       too long (string).
 *       The LOLAN_REGMAP_AUX_BIT and LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT
 *       flags of the variable are set.
 * @return
 *   LOLAN_RETVAL_YES:  The variable was updated successfully.
 *     The LOLAN_REGMAP_AUX_BIT and LOLAN_REGMAP_REMOTE_UPDATE_BIT flags of
 *     the variable are set.
 *   LOLAN_RETVAL_NO:  The variable was not updated. See error output
 *     for more information.
 *   LOLAN_RETVAL_GENERROR:  A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 ******************************************************************************/
int8_t lolanVarUpdateFromCbor(lolan_ctx *ctx, const uint8_t *path, CborValue *it, uint8_t *error)
{
  uint8_t i;
  bool found;

  CborType type;
  CborError cerr;

  /* searching for the variable by path */
  found = false;
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++)
    if ( (memcmp(ctx->regMap[i].p, path, LOLAN_REGMAP_DEPTH) == 0) &&   // path found
         (ctx->regMap[i].p[0] != 0)                       ) {   // register map entry is valid
      found = true;
      break;
    }
  if (!found) {  // variable not found
    cerr = cbor_value_advance(it);   // advance CBOR iterator
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    if (error) *error = LVUFC_NOTFOUND;
    return LOLAN_RETVAL_NO;
  }

  /* flags */
  ctx->regMap[i].flags &= ~(LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT + LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT
           + LOLAN_REGMAP_REMOTE_UPDATE_BIT);   // clear flags
  ctx->regMap[i].flags |= LOLAN_REGMAP_AUX_BIT;  // set auxiliary flag

  /* check for read-only */
  if (ctx->regMap[i].flags & LOLAN_REGMAP_REMOTE_READONLY_BIT) {  // read-only
    cerr = cbor_value_advance(it);   // advance CBOR iterator
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    if (error) *error = LVUFC_READONLY;
    return LOLAN_RETVAL_NO;
  }

  /* check the CBOR item type and update LoLaN variable if possible */
  type = cbor_value_get_type(it);
  switch (type) {
    case CborIntegerType:
      if (cbor_value_is_unsigned_integer(it)) {   // non-negative integer
        uint64_t val;
        cbor_value_get_uint64(it, &val);  // decode value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        switch (ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) {  // type checking
          case LOLAN_INT:   // the CBOR item is a non-negative integer, the LoLaN variable is a signed integer
            switch (ctx->regMap[i].size) {
              case 1:  // 8-bit signed
                if (val > INT8_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int8_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 2:  // 16-bit signed
                if (val > INT16_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int16_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 4:  // 32-bit signed
                if (val > (uint64_t) INT32_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int32_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 8:  // 64-bit signed
                if (val > (uint64_t) INT64_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int64_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              default:   // (unlikely)
                return LOLAN_RETVAL_GENERROR;   // unsupported integer size
                break;
            }
            break;
          case LOLAN_UINT:   // the CBOR item is a non-negative integer, the LoLaN variable is an unsigned integer
            switch (ctx->regMap[i].size) {
              case 1:  // 8-bit unsigned
                if (val > UINT8_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((uint8_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 2:  // 16-bit unsigned
                if (val > UINT16_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((uint16_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 4:  // 32-bit unsigned
                if (val > UINT32_MAX) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((uint32_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 8:  // 64-bit unsigned
                /* (can not be out of range) */
                *((uint64_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              default:   // (unlikely)
                return LOLAN_RETVAL_GENERROR;   // unsupported integer size
                break;
            }
            break;
          default:   // type mismatch
            if (error) *error = LVUFC_MISMATCH;
            ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
            return LOLAN_RETVAL_NO;
            break;
        }
      } else {   // negative integer
        int64_t val;
        cbor_value_get_int64(it, &val);   // decode value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        switch (ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) {  // type checking
          case LOLAN_INT:   // the CBOR item is a negative integer, the LoLaN variable is a signed integer
            switch (ctx->regMap[i].size) {
              case 1:  // 8-bit signed
                if ((val > INT8_MAX) || (val < INT8_MIN)) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int8_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 2:  // 16-bit signed
                if ((val > INT16_MAX) || (val < INT16_MIN)) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int16_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 4:  // 32-bit signed
                if ((val > INT32_MAX) || (val < INT32_MIN)) {   // out of range
                  if (error) *error = LVUFC_OUTOFRANGE;
                  ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
                  return LOLAN_RETVAL_NO;
                }
                *((int32_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              case 8:  // 64-bit signed
                /* (can not be out of range) */
                *((int64_t*) ctx->regMap[i].data) = val;   // update value
                ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
                break;
              default:   // (unlikely)
                return LOLAN_RETVAL_GENERROR;   // unsupported integer size
                break;
            }
            break;
          case LOLAN_UINT:   // the CBOR item is a negative integer, the LoLaN variable is an unsigned integer
            /* out of range */
            if (error) *error = LVUFC_OUTOFRANGE;
            ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
            return LOLAN_RETVAL_NO;
            break;
          default:   // type mismatch
            if (error) *error = LVUFC_MISMATCH;
            ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
            return LOLAN_RETVAL_NO;
            break;
        }
      }
      break;
    case CborByteStringType:
      if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_STR) {   // type checking
        size_t len;
        cerr = cbor_value_calculate_string_length(it, &len);   // calculate CBOR string length
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (ctx->regMap[i].size < len) {  // the string is too long
          cerr = cbor_value_advance(it);   // advance CBOR iterator
          if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
          if (error) *error = LVUFC_OUTOFRANGE;
          ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
          return LOLAN_RETVAL_NO;
        }
        len = ctx->regMap[i].size;
        cbor_value_copy_byte_string(it, ctx->regMap[i].data, &len, it);   // update value (the CBOR iterator is also advanced)
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
      } else {   // type mismatch
        cerr = cbor_value_advance(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (error) *error = LVUFC_MISMATCH;
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
        return LOLAN_RETVAL_NO;
      }
      break;
    case CborTextStringType:
      if ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_STR) {   // type checking
        size_t len;
        cerr = cbor_value_calculate_string_length(it, &len);   // calculate CBOR string length
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (ctx->regMap[i].size < len) {  // the string is too long
          cerr = cbor_value_advance(it);   // advance CBOR iterator
          if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
          if (error) *error = LVUFC_OUTOFRANGE;
          ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT;  // set flag
          return LOLAN_RETVAL_NO;
        }
        len = ctx->regMap[i].size;
        cbor_value_copy_text_string(it, ctx->regMap[i].data, &len, it);   // update value (the CBOR iterator is also advanced)
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
      } else {   // type mismatch
        cerr = cbor_value_advance(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (error) *error = LVUFC_MISMATCH;
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
        return LOLAN_RETVAL_NO;
      }
      break;
    case CborFloatType:
      if ( ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_FLOAT)
                && (ctx->regMap[i].size == 4) )   {   // type checking
        float val;
        cbor_value_get_float(it, &val);   // get value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        *((float*) ctx->regMap[i].data) = val;   // update value
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
      } else {   // type mismatch (single and double precision floating point are treated as different types)
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (error) *error = LVUFC_MISMATCH;
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
        return LOLAN_RETVAL_NO;
      }
      break;
    case CborDoubleType:
      if ( ((ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK) == LOLAN_FLOAT)
                && (ctx->regMap[i].size == 8) )   {   // type checking
        double val;
        cbor_value_get_double(it, &val);   // get value
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        *((double*) ctx->regMap[i].data) = val;   // update value
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_BIT;  // set flag
      } else {   // type mismatch (single and double precision floating point are treated as different types)
        cerr = cbor_value_advance_fixed(it);   // advance CBOR iterator
        if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        if (error) *error = LVUFC_MISMATCH;
        ctx->regMap[i].flags |= LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT;  // set flag
        return LOLAN_RETVAL_NO;
      }
      break;
    case CborInvalidType:   // invalid CBOR entry
      return LOLAN_RETVAL_CBORERROR;
      break;
    default:   // type not allowed in LoLaN
      return LOLAN_RETVAL_GENERROR;
      break;
  }

  return LOLAN_RETVAL_YES;
} /* lolanVarUpdateFromCbor */

/**************************************************************************//**
 * @brief
 *   Update a bunch of LoLaN variables nested by path from CBOR.
 * @details
 *   This subroutine processes the nested path structure in CBOR
 *   which resides in the specified LoLaN packet, and tries to update the
 *   LoLaN variables specified by paths with the corresponding data.
 *   The zero key CBOR entry will be skipped.
 *
 *   Example of variables with nested path structure:
 *
 *   CBOR item #1:  key = 4 (integer)
 *   CBOR item #2:  MAP
 *         CBOR item 2/#1:  key = 1 (integer)
 *         CBOR item 2/#2:  value of data (4, 1, 0) (any type)
 *         CBOR item 2/#3:  key = 2 (integer)
 *         CBOR item 2/#4:  MAP
 *                 CBOR item 2/4/#1:  key = 1 (integer)
 *                 CBOR item 2/4/#2:  value of data (4, 2, 1) (any type)
 *                 CBOR item 2/4/#3:  key = 2 (integer)
 *                 CBOR item 2/4/#4:  value of data (4, 2, 2) (any type)
 *         CBOR item 2/#5:  key = 3 (integer)
 *         CBOR item 2/#6:  value of data (4, 3, 0) (any type)
 *
 * @note
 *   The LOLAN_REGMAP_AUX_BIT will be set for all variables which are affected
 *   by the updating process. If the update of a variable is succeeded, the
 *   LOLAN_REGMAP_REMOTE_UPDATE_BIT will be set, otherwise the other flags
 *   help determine what was the problem:
 *     LOLAN_REGMAP_REMOTE_UPDATE_OUTOFRANGE_BIT
 *     LOLAN_REGMAP_REMOTE_UPDATE_MISMATCH_BIT
 *     LOLAN_REGMAP_REMOTE_READONLY_BIT
 *   See lolanVarUpdateFromCbor() for more details.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] lp
 *   Pointer to a LoLaN packet structure, whose payload contains the nested
 *   path structure in CBOR format.
 * @param[out] info
 *   Address of a structure that will be filled with information about the
 *   successfulness of the action.
 *   ¤ found
 *     The number of variables found to update in the local register map.
 *   ¤ updated
 *     The number of variables actually updated.
 *   ¤ notfound
 *     The number of variables specified in the CBOR data but not found in
 *     the local register map.
 *   ¤ toodeep
 *     The CBOR data specifies variable(s) whose path length exceeds the
 *     register map depth of the current implementation. These entries
 *     were ignored.
 *   (Total success: found = updated, notfound = 0, toodeep = false)
 * @return
 *   LOLAN_RETVAL_YES:  The action was completed (the end of the nested path
 *     structure is reached without serious errors). The successfulness of
 *     the action is presented in the info output.
 *   LOLAN_RETVAL_GENERROR:  A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 ******************************************************************************/
int8_t lolanVarBunchUpdateFromCbor(lolan_ctx *ctx, const lolan_Packet *lp,
               lolan_BunchUpdateOutputStruct *info)
{
  uint8_t path[LOLAN_REGMAP_DEPTH];
  uint8_t i, alevel;
  int key;
  int8_t err;
  uint8_t exterr;

  CborParser parser;
  CborValue root_it, it[LOLAN_REGMAP_DEPTH];
  CborError cerr;

  /* initialize and enter the root container (map) */
  cerr = cbor_parser_init(lp->payload, lp->payloadSize, 0, &parser, &root_it);  // initialize CBOR parser
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (cbor_value_get_type(&root_it) != CborMapType) {   // the root entry must be a CBOR map
    DLOG(("\n LoLaN CBOR packet error: root map not found"));
    return LOLAN_RETVAL_GENERROR;
  }
  cerr = cbor_value_enter_container(&root_it, &it[0]);   // enter root map
  if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;

  /* zero the info output structure */
  memset(info, 0, sizeof(lolan_BunchUpdateOutputStruct));

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
    if (cbor_value_get_type(&it[alevel]) != CborIntegerType) {  // check key of a key-data pair (must be integer)
      DLOG(("\n LoLaN CBOR packet error: key has to be integer"));
      return LOLAN_RETVAL_GENERROR;
    }
    cbor_value_get_int(&it[alevel], &key);   // get key
    cerr = cbor_value_advance_fixed(&it[alevel]);   // advance iterator to data
    if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
    if (cbor_value_at_end(&it[alevel])) {   // unexpected end of map (no data for key)
      DLOG(("\n LoLaN CBOR packet error: key must be followed by data"));
      return LOLAN_RETVAL_GENERROR;
    }
    /* process key-data pair */
    if ((key <= 0) || (key > 255)) {   // zero key found, or key can not be a path element
      if (key != 0) info->invalid_keys++;  // update statistics
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
          info->toodeep = true;  // set indicator
          cerr = cbor_value_advance(&it[alevel]);   // skip map
          if (cerr != CborNoError) return LOLAN_RETVAL_CBORERROR;
        }
      } else {  // the data is not a map (may be valid data)
        for (i = alevel+1; i < LOLAN_REGMAP_DEPTH; i++)   // correct path with zeros if needed
          path[i] = 0;
        err = lolanVarUpdateFromCbor(ctx, path, &it[alevel], &exterr);   // update LoLaN variable
        switch (err) {
          case LOLAN_RETVAL_YES:   // successfully updated
            info->found++;   // update statistics
            info->updated++;
            break;
          case LOLAN_RETVAL_NO:  // no update
            if (exterr == LVUFC_NOTFOUND) {   // variable not found
              info->notfound++;  // update statistics
            } else {
              info->found++;  // update statistics
            }
            break;
          default:  // other errors
            DLOG(("\n Error during lolanVarUpdateFromCbor()."));
            return err;
            break;
        }
      }
    }
  }

  return LOLAN_RETVAL_YES;  // done
} /* lolanVarBunchUpdateFromCbor */

/**************************************************************************//**
 * @brief
 *   Create a simple integer key-value pair with or without a container.
 * @param[in] encoder
 *   Pointer to a CBOR stream.
 * @param[in] key
 *   Key for the key-value pair.
 * @param[in] value
 *   Value for the key-value pair.
 * @param[in] container
 *   If true, the key-value pair will be nested in a container (map).
 * @return
 *   The return value is LOLAN_RETVAL_YES if the action was successful,
 *   otherwise LOLAN_RETVAL_CBORERROR.
 ******************************************************************************/
int8_t createCborUintDataSimple(CborEncoder *encoder, uint64_t key, uint64_t value, bool container)
{
  CborError err;
  CborEncoder map_enc;

  if (container) {
    err = cbor_encoder_create_map(encoder, &map_enc, 1);   // create map for 1 key-data pair
    if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }
  err = cbor_encode_uint(container ? &map_enc : encoder, key);   // encode key
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  err = cbor_encode_uint(container ? &map_enc : encoder, value);   // encode value
  if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  if (container) {
    err = cbor_encoder_close_container(encoder, &map_enc);   // close the map
    if (err != CborNoError) return LOLAN_RETVAL_CBORERROR;
  }
  return LOLAN_RETVAL_YES;
} /* createCborUintDataSimple */

/**************************************************************************//**
 * @brief
 *   Encode LoLaN variable data to CBOR.
 * @param[in] data
 *   The LoLaN variable data to encode.
 * @param[in] data_len
 *   Length of the data specified.
 * @param[in] type
 *   The type of the LoLaN variable data.
 * @param[in] encoder
 *   Pointer to a CBOR stream.
 * @return
 *   LOLAN_RETVAL_YES:        The variable data is encoded successfully.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 *   LOLAN_RETVAL_MEMERROR:   CBOR out of memory error.
 ******************************************************************************/
int8_t lolanVarDataToCbor(uint8_t *data, uint8_t data_len, lolan_VarType type,
           CborEncoder *encoder)
{
  CborError cerr;

  switch (type) {
    case LOLAN_INT:   // signed integer
      switch (data_len) {
        case 1:
          cerr = cbor_encode_int(encoder, *((int8_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 2:
          cerr = cbor_encode_int(encoder, *((int16_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 4:
          cerr = cbor_encode_int(encoder, *((int32_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 8:
          cerr = cbor_encode_int(encoder, *((int64_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        default:   // unsupported integer length
          return LOLAN_RETVAL_GENERROR;
          break;
      }
      break;
    case LOLAN_UINT:   // unsigned integer
      switch (data_len) {
        case 1:
          cerr = cbor_encode_int(encoder, *((uint8_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 2:
          cerr = cbor_encode_int(encoder, *((uint16_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 4:
          cerr = cbor_encode_int(encoder, *((uint32_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 8:
          cerr = cbor_encode_int(encoder, *((uint64_t*) (data)) );
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        default:   // unsupported integer length
          return LOLAN_RETVAL_GENERROR;
          break;
      }
      break;
    case LOLAN_FLOAT:   // floating point
      switch (data_len) {
        case 4:
          cerr = cbor_encode_floating_point(encoder, CborFloatType, data);
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        case 8:
          cerr = cbor_encode_floating_point(encoder, CborDoubleType, data);
          if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          break;
        default:   // unsupported floating point type
          return LOLAN_RETVAL_GENERROR;
          break;
      }
      break;
    case LOLAN_STR:   // string
      cerr = cbor_encode_text_string(encoder, (char*) data, strlen((char*) data));
      /* XXX: byte string would be more suitable (we do not want to handle UTF-8), text string type is for JSON compatibility */
      if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      break;
    default:   // unsupported variable type
      return LOLAN_RETVAL_GENERROR;
      break;
  }

  return LOLAN_RETVAL_YES;  // success
} /* lolanVarDataToCbor */

/**************************************************************************//**
 * @brief
 *   Encode a single LoLaN variable to CBOR.
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] path
 *   Address of the uint8_t array containing the path of the LoLaN
 *   variable to encode. Set to NULL to bypass the searching part,
 *   in this case the index-th register map entry will be encoded.
 * @param[in] index
 *   Selects the LoLaN variable to encode by register map index.
 *   Has no effect unless the path parameter is unassigned.
 * @param[in] encoder
 *   Pointer to a CBOR stream.
 * @return
 *   LOLAN_RETVAL_YES:        The variable is encoded successfully.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 *   LOLAN_RETVAL_MEMERROR:   CBOR out of memory error.
 ******************************************************************************/
int8_t lolanVarToCbor(lolan_ctx *ctx, const uint8_t *path, uint8_t index, CborEncoder *encoder)
{
  uint8_t i;
  bool found;

  if (path) {   // path is assigned
    /* search for a variable with the specified path */
    found = false;
    for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
      if ( (memcmp(ctx->regMap[i].p, path, LOLAN_REGMAP_DEPTH) == 0) &&    // variable found
           (ctx->regMap[i].p[0] != 0)                       )  {   // register map entry is valid
         found = true;
         break;
      }
    }
    if (!found) return LOLAN_RETVAL_GENERROR;   // no variable found
  } else {  // path is unassigned
    i = index;
    if ((i >= LOLAN_REGMAP_SIZE) || (ctx->regMap[i].p[0] == 0))   // error
      return LOLAN_RETVAL_GENERROR;
  }

  /* encode variable */
  return lolanVarDataToCbor(ctx->regMap[i].data, ctx->regMap[i].size,
                     ctx->regMap[i].flags & LOLAN_REGMAP_TYPE_MASK, encoder);
  } /* lolanVarToCbor */

/**************************************************************************//**
 * @brief
 *   Encode a LoLaN variable to CBOR with nested path items.
 * @details
 *   This procedure creates the required nesting and key values for a LoLaN
 *   variable in CBOR in function of the previously added variable.
 *   Calling order (important!):
 *   1. action = ACINPAUX_INITIAL
 *      (all parameters are required)
 *      The first variable should be added this way.
 *   2. action = ACINPAUX_NORMAL
 *      (parameter "encoder" is not required)
 *      The method for adding all other variables.
 *   3. action = ACINPAUX_FINALIZE
 *      (parameter "path" is not required)
 *      This should be called when the last variable was already added.
 *   The path of every variable added to encode should be after the previous
 *   in order to avoid fragmentation of groups.
 * @param[in] path and index
 *   Path is the address of a uint8_t array containing the exact path of a
 *   LoLaN variable to encode. Index is the register map index of this
 *   variable.
 * @note
 *   FOR INTERNAL USE ONLY.
 ******************************************************************************/
int8_t lolanVarToCborNestedPath(lolan_ctx *ctx, uint8_t index, CborEncoder *encoder,
           lolanVarToCborNestedPath_aux action, bool statusCodeInstead)
{
  static CborEncoder nested_enc[LOLAN_REGMAP_DEPTH];
  static uint8_t last_path[LOLAN_REGMAP_DEPTH];
  static uint8_t last_defLvl;
  uint8_t *path;
  CborError cerr;
  int8_t err;
  uint8_t i, j, defLvl;

  path = ctx->regMap[index].p;   // (just for better readibility)
  switch (action) {
    case LVTCNPAUX_INITIAL:
      nested_enc[0] = *encoder;   // save the initial CBOR encoder struct
      defLvl = pathDefinitionLevel(ctx, path, NULL, false);  // get definition level for path
      if (defLvl == 0) return LOLAN_RETVAL_GENERROR;
      cerr = cbor_encode_uint(&nested_enc[0], path[0]);   // encode the first path element as key
      if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      for (i = 1; i < defLvl; i++) {   // create nested CBOR maps
        cerr = cbor_encoder_create_map(&nested_enc[i-1], &nested_enc[i], CborIndefiniteLength);   // create map for path level
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
        cerr = cbor_encode_uint(&nested_enc[i], path[i]);   // encode the path element as key
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      }
      if (!statusCodeInstead) {  // variable data
        err = lolanVarToCbor(ctx, NULL, index, &nested_enc[defLvl-1]);   // encode the LoLaN variable itself
        if (err != LOLAN_RETVAL_YES) return err;
      } else {   // status code
        cerr = cbor_encode_uint(&nested_enc[defLvl-1], getLolanSetStatusCodeForVariable(ctx, index));  // encode status code
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      }
      memcpy(last_path, path, LOLAN_REGMAP_DEPTH);   // store the path as last path
      last_defLvl = defLvl;   // store the definition level as last path's def. level
      break;
    case LVTCNPAUX_NORMAL:
      defLvl = pathDefinitionLevel(ctx, path, NULL, false);  // get definition level for path
      if (defLvl == 0) return LOLAN_RETVAL_GENERROR;
      for (i = 0; i < last_defLvl; i++) {   // compare the path to the previous
        if (path[i] != last_path[i]) {   // mismatch
          /* revert to the last matching level */
          for (j = last_defLvl-1; j > i; j--) {
            cerr = cbor_encoder_close_container(&nested_enc[j-1], &nested_enc[j]);   // close map assigned to the path level
            if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
          }
          break;
        }
      }
      /* (now i contains the first mismatching path level) */
      cerr = cbor_encode_uint(&nested_enc[i], path[i]);   // encode the first different path element as key
      if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      for (j = i+1; j < defLvl; j++) {   // create nested CBOR maps
        cerr = cbor_encoder_create_map(&nested_enc[j-1], &nested_enc[j], CborIndefiniteLength);   // create map for path level
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
        cerr = cbor_encode_uint(&nested_enc[j], path[j]);   // encode the path element as key
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      }
      if (!statusCodeInstead) {  // variable data
        err = lolanVarToCbor(ctx, NULL, index, &nested_enc[defLvl-1]);   // encode the LoLaN variable itself
        if (err != LOLAN_RETVAL_YES) return err;
      } else {   // status code
        cerr = cbor_encode_uint(&nested_enc[defLvl-1], getLolanSetStatusCodeForVariable(ctx, index));  // encode status code
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      }
      memcpy(last_path, path, LOLAN_REGMAP_DEPTH);   // store the path as last path
      last_defLvl = defLvl;   // store the definition level as last path's def. level
      break;
    case LVTCNPAUX_FINALIZE:
      /* close all open maps */
      for (i = last_defLvl-1; i > 0; i--) {
        cerr = cbor_encoder_close_container(&nested_enc[i-1], &nested_enc[i]);   // close map
        if (cerr != CborNoError) return (cerr == CborErrorOutOfMemory) ? LOLAN_RETVAL_MEMERROR : LOLAN_RETVAL_CBORERROR;
      }
      *encoder = nested_enc[0];   // output the updated CBOR encoder struct
      break;
  }

  return LOLAN_RETVAL_YES;
} /* lolanVarToCborNestedPath */

/**************************************************************************//**
 * @brief
 *   Encode a branch of LoLaN variables to CBOR nested by path.
 * @details
 *   Example for encoding with (4, 0, 0) base path, where the entries
 *   are (4, 1, 0), (4, 2, 1), (4, 2, 2), (4, 3, 0), the maximum
 *   recursion depth is set to at least 2:
 *
 *   CBOR item #1:  key = 4 (integer)
 *   CBOR item #2:  MAP
 *         CBOR item 2/#1:  key = 1 (integer)
 *         CBOR item 2/#2:  value of data (4, 1, 0) (any type)
 *         CBOR item 2/#3:  key = 2 (integer)
 *         CBOR item 2/#4:  MAP
 *                 CBOR item 2/4/#1:  key = 1 (integer)
 *                 CBOR item 2/4/#2:  value of data (4, 2, 1) (any type)
 *                 CBOR item 2/4/#3:  key = 2 (integer)
 *                 CBOR item 2/4/#4:  value of data (4, 2, 2) (any type)
 *         CBOR item 2/#5:  key = 3 (integer)
 *         CBOR item 2/#6:  value of data (4, 3, 0) (any type)
 *
 * @note
 *   The path should be a valid (base) path of at least one LoLaN variable
 *   with LOLAN_REGMAP_RECURSION recursion depth.
 *   IMPORTANT: The LoLaN register map must be sorted by path to use
 *   this subroutine!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] path
 *   Address of the uint8_t array containing the (base) path of the LoLaN
 *   variable(s) to encode.  (See note!)
 * @param[in] encoder
 *   Pointer to a CBOR stream.
 * @return
 *   LOLAN_RETVAL_YES:        The variables were encoded successfully.
 *   LOLAN_RETVAL_NO:         No variable was found to encode.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 *   LOLAN_RETVAL_MEMERROR:   CBOR out of memory error (too much data is
 *     requested in one step).
 ******************************************************************************/
int8_t lolanVarBranchToCbor(lolan_ctx *ctx, const uint8_t *path, CborEncoder *encoder)
{
  uint8_t i;
  uint8_t defLvl;
  bool first;
  int8_t err;

  defLvl = pathDefinitionLevel(ctx, path, NULL, false);  // get definition level for path

  first = true;
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if ((memcmp(ctx->regMap[i].p, path, defLvl) == 0)  // variable found for the specified subpath
         && (ctx->regMap[i].p[0] != 0)) {              // (not a free entry)
      if (pathDefinitionLevel(ctx, ctx->regMap[i].p, NULL, false) > defLvl + LOLAN_REGMAP_RECURSION)  // maximum recursion level is exceeded
        continue;
      if (first) {
        err = lolanVarToCborNestedPath(ctx, i, encoder, LVTCNPAUX_INITIAL, false);  // initializing CBOR tree and add the first item
        if (err != LOLAN_RETVAL_YES) return err;
        first = false;
      } else {
        err = lolanVarToCborNestedPath(ctx, i, NULL, LVTCNPAUX_NORMAL, false);   // add the current item to the CBOR tree
        if (err != LOLAN_RETVAL_YES) return err;
      }
    }
  }
  if (!first) {   // at least one variable was found
    err = lolanVarToCborNestedPath(ctx, i, encoder, LVTCNPAUX_FINALIZE, false);   // finalize CBOR tree
    if (err != LOLAN_RETVAL_YES) return err;
    return LOLAN_RETVAL_YES;
  } else {   // no variable was found
    return LOLAN_RETVAL_NO;
  }
} /* lolanVarBranchToCbor */

/**************************************************************************//**
 * @brief
 *   Encode the LoLaN variables with the specified flags to CBOR
 *   nested by path.
 * @details
 *   Example for encoding (1, 5, 0), (2, 1, 3), (2, 1, 4), (2, 2, 0):
 *
 *   CBOR item #1:  key = 1 (integer)
 *   CBOR item #2:  MAP
 *         CBOR item 2/#1:  key = 5 (integer)
 *         CBOR item 2/#2:  value of data (1, 5, 0) (any type)
 *   CBOR item #3:  key = 2 (integer)
 *   CBOR item #4:  MAP
 *         CBOR item 4/#1:  key = 1 (integer)
 *         CBOR item 4/#2:  MAP
 *                 CBOR item 4/2/#1:  key = 3 (integer)
 *                 CBOR item 4/2/#2:  value of data (2, 1, 3) (any type)
 *                 CBOR item 4/2/#3:  key = 4 (integer)
 *                 CBOR item 4/2/#4:  value of data (2, 1, 4) (any type)
 *         CBOR item 4/#3:  key = 2 (integer)
 *         CBOR item 4/#4:  value of data (2, 2, 0) (any type)
 *
 * @note
 *   IMPORTANT: The LoLaN register map must be sorted by path to use
 *   this subroutine!
 * @param[in] ctx
 *   Pointer to the LoLaN context variable.
 * @param[in] flags
 *   The variables having these flags (e.g. LOLAN_REGMAP_LOCAL_UPDATE_BIT)
 *   set will be encoded. (If this parameter specifies multiple flags,
 *   the variable will be encoded if all flags are set.)
 * @param[in] encoder
 *   Pointer to a CBOR stream.
 * @param[in] auxflagset
 *   If true, the LOLAN_REGMAP_AUX_BIT is set on every encoded variable.
 * @param[in] statusCodeInstead
 *   If true, not the variable data but the status code after
 *   lolanVarUpdateFromCbor() will be output.
 * @return
 *   LOLAN_RETVAL_YES:        The variables were encoded successfully.
 *   LOLAN_RETVAL_NO:         No variable was found to encode.
 *   LOLAN_RETVAL_GENERROR:   A general error has occurred.
 *   LOLAN_RETVAL_CBORERROR:  A CBOR error has occurred.
 *   LOLAN_RETVAL_MEMERROR:   CBOR out of memory error (too much data is
 *     requested in one step).
 ******************************************************************************/
int8_t lolanVarFlagToCbor(lolan_ctx *ctx, uint16_t flags, CborEncoder *encoder,
             bool auxflagset, bool statusCodeInstead)
{
  uint8_t i;
  bool first;
  int8_t err;

  first = true;
  for (i = 0; i < LOLAN_REGMAP_SIZE; i++) {
    if ( ((ctx->regMap[i].flags & flags) == flags)      // variable found with the specified flags
         && (ctx->regMap[i].p[0] != 0) ) {              // (not a free entry)
      if (first) {
        // initialize CBOR tree and add the first item
        err = lolanVarToCborNestedPath(ctx, i, encoder, LVTCNPAUX_INITIAL, statusCodeInstead);
        if (err != LOLAN_RETVAL_YES) return err;
        first = false;
      } else {
        // add the current item to the CBOR tree
        err = lolanVarToCborNestedPath(ctx, i, NULL, LVTCNPAUX_NORMAL, statusCodeInstead);
        if (err != LOLAN_RETVAL_YES) return err;
      }
      if (auxflagset)
        ctx->regMap[i].flags |= LOLAN_REGMAP_AUX_BIT;   // set the auxiliary flag
    }
  }
  if (!first) {   // at least one variable was found
    // finalize CBOR tree
    err = lolanVarToCborNestedPath(ctx, i, encoder, LVTCNPAUX_FINALIZE, statusCodeInstead);
    if (err != LOLAN_RETVAL_YES) return err;
    return LOLAN_RETVAL_YES;
  } else {   // no variable was found
    return LOLAN_RETVAL_NO;
  }
} /* lolanVarFlagToCbor */

/**************************************************************************//**
 * @brief
 *   Calculate the CRC16 of the specified data.
 * @param[in] data
 *   Starting address of the data.
 * @param[in] size
 *   Size of the data in bytes.
 * @return
 *   The CRC16 value.
 *****************************************************************************/
uint16_t CRC_calc(const uint8_t *data, uint32_t size)
{
  uint16_t crc, q;
  uint8_t c;
  uint32_t i;

  crc = 0;
  for (i = 0; i < size; i++) {
    c = data[i];
    q = (crc ^ c) & 0x0f;
    crc = (crc >> 4) ^ (q * 0x1081);
    q = (crc ^ (c >> 4)) & 0xf;
    crc = (crc >> 4) ^ (q * 0x1081);
  }
  return ((crc << 8) & 0xFF00) | ((crc >> 8) & 0x00FF);   // swap bytes
} /* CRC_calc */

