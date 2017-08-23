#  Low Latency Network Protocol (LoLaN)

LoLaN is a stateless protocol, designed to communicate over a wireless UWB network with start topology (however other mediums are targeted also). The design goals are to minimize packet header overhead and the redundancy between network layers.
LoLaN is based on 802.15.4 frame format (however uses the reserved frame version 3, that makes it incopatible with the standard) and relays on packet size information from the PHY layer.

As most of CPU-s implementing LoLaN will be little endian, thus LoLaN itself is little endian. Eg, the first byte of a 16 bit number is the least significant byte.

## Packet with Encryption

<table>
  <tr>
    <td>Offset</td>
    <td>Octet</td>
    <td colspan="8">0</td>
    <td colspan="8">1</td>
    <td colspan="8">2</td>
    <td colspan="8">3</td>
  </tr>
  <tr>
    <td>Octet</td>
    <td>Bit</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td colspan="16">Attributes</td>
    <td colspan="8">Packet counter (8 bits)</td>
    <td colspan="8">FromAddress (lower 8 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
    <td colspan="8">FromAddress (upper 8 bits)</td>
    <td colspan="16">ToAddress (16 bits)</td>
    <td colspan="8">TimeStamp (0-8 bits)</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="32">TimeStamp (8-40 bits)</td>
  </tr>
  <tr>
    <td>12</td>
    <td>96</td>
    <td colspan="32">Payload ...</td>
  </tr>
  <tr>
    <td>(N-1)*4</td>
    <td>(N-1)*32</td>
    <td colspan="8">... Payload</td>
    <td colspan="16">HMAC (0-7) bits</td>
  </tr>
  <tr>
    <td>N*4</td>
    <td>N*32</td>
    <td colspan="32">HMAC (8-40) bits</td>
  </tr>
</table>


## Packet without Encryption

<table>
  <tr>
    <td>Offset</td>
    <td>Octet</td>
    <td colspan="8">0</td>
    <td colspan="8">1</td>
    <td colspan="8">2</td>
    <td colspan="8">3</td>
  </tr>
  <tr>
    <td>Octet</td>
    <td>Bit</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
    <td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td colspan="16">Attributes</td>
    <td colspan="8">Packet counter (8 bits)</td>
    <td colspan="8">FromAddress (lower 8 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
		<td colspan="8">FromAddress (upper 8 bits)</td>
    <td colspan="16">ToAddress (16 bits)</td>
    <td colspan="8">Payload</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="32">Payload ...</td>
  </tr>
  <tr>
    <td>N*4</td>
    <td>N*32</td>
    <td colspan="16">... Payload</td>
    <td colspan="16">CRC16</td>
  </tr>
</table>

### Attributes (802.15.4 frame control extended)


#### __bit0-2__:  Packet type

LoLaN (TRAP/INFORM/GET/SET)payloads are CBOR (http://cbor.me/) serialized data.

Accessing this data is REST like.
The key-value pairs can be converted to json with mapping keys, and path numbers to string based on device types.
0 key value is a special value, that indicates different things based on data type.

##### 0: 802.15.4 BEACON

Not used

##### 1: 802.15.4 DATA

OTHER protocol data, not LoLaN

##### 2: 802.15.4 ACK

IMPORTANT: ACK packet number is the same as the packet ACK-ed.

Payload differs on what was ACK-ed, ACK has to be transmitted in the current timeslot.

INFROM: no payload

GET: 0 key indicates result code (integer according to HTTP status codes) other key-value pairs contains data requested

SET: 0 key indicates result code (integer according to HTTP status codes), no payload

##### 3: 802.15.4 MAC

Not used

##### 4: LOLAN INFORM
the payload includes characteristic value updates in CBOR format.
0 key defines the base path as an array for the key-value pairs. If no 0 key is specified, the root node is the base path. More updates can be included in an inform package as a CBOR array.

```language-json
	{ 0: [2,33,4], 1:"updated value"}
```

This results in updating the /2/33/4/1 value with "updated value" at server side. 

The INFROM message can be acknowledged in the current timeslot (ACK request bit is set if sent).

##### 5: LOLAN GET

Payload is the path to be accessed. Path is defined as an array

```language-json
	[2,33,4,1]
```

This means GET /2/33/4/1.

The reply should be on air in the same timeslot (ACK request bit is always set).

##### 6: LOLAN SET

0 key indicates the base path for modifing key-value pairs.
Other keys are the key-value pairs to be modified 

ACK request bit is always set.

##### 7: LOLAN CONTROL

TBD.

Like RETRANSMIT REQUEST packet number is an extended packet start packet number


#### __bit3__:  Securtiy enbaled

If set, and packet type is LOLAN

#### __bit4__:  Frame pending

The next packet will extend thisone.

#### __bit5__:  ACK request

The recipiend shall send a ACK in the timeslot.

#### __bit6-9__:  Bytes to boundary

If security enabled, these bits inidicate the number of random filled bytes at the end of the packet to meet the security block boundary (15 bytes max)

#### __bit10-11__:  Dest addressing mode

Set to 01 for direct packet
Set to 11 for routable packet

#### __bit12-13__:  Frame version

Set to 03 for LoLaN

#### __bit14-15__:  Source addressing mode

Set to 01 for direct sending
Set to 11 for routed packet
