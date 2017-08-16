/*
 * hmac.h
 *
 *  Created on: Feb 2, 2017
 *      Author: gabor
 */

#ifndef HMAC_H_
#define HMAC_H_

#include <string.h>
#include <stdlib.h>

/*
unsigned char*  text;                pointer to data stream
int             text_len;            length of data stream
unsigned char*  key;                 pointer to authentication key
int             key_len;             length of authentication key
unsigned char*  digest;              caller digest to be filled in
*/

void hmac_md5(const uint8_t *text, uint32_t text_len,
	      const uint8_t *key, uint8_t key_len,
	      uint8_t *digest);

#endif /* HMAC_H_ */
