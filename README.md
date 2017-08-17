#  Low Latency Network Protocol (LoLaN)

LoLaN is a stateless protocol, designed to communicate over a wireless UWB network with start topology (however other mediums are targeted also). The design goals are to minimize packet header overhead and the redundancy between network layers.
LoLaN is able to send 16 byte payload within only 24 byte packet without encryption, and in 32 byte packet with encryption and stateless authentication. However also extended packets up to 1326 bytes (suitable for IPv6) are supported with handling fragmentation to 118 bytes (goes over 802.15.4 networks). 

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
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td>1</td>
    <td>0</td>
    <td colspan="6">Attributes</td>
    <td colspan="8">Packet counter (8 bits)</td>
    <td colspan="16">FromAddress (16 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
    <td colspan="16">ToAddress (16 bits)</td>
    <td colspan="16">TimeStamp (0-23 bits)</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="24">TimeStamp (24-40 bits)</td>
    <td colspan="8">Payload</td>
  </tr>
  <tr>
    <td>12</td>
    <td>96</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>16</td>
    <td>128</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>20</td>
    <td>160</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>24</td>
    <td>192</td>
    <td colspan="24">Payload</td>
    <td colspan="16">HMAC (0-7) bits</td>
  </tr>
  <tr>
    <td>28</td>
    <td>224</td>
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
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
    <td>7</td><td>6</td><td>5</td><td>4</td><td>3</td><td>2</td><td>1</td><td>0</td>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td>0</td>
    <td>0</td>
    <td colspan="6">Attributes</td>
    <td colspan="8">Packet counter (8 bits)</td>
    <td colspan="16">FromAddress (16 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
    <td colspan="16">ToAddress (16 bits)</td>
    <td colspan="16">Payload</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>12</td>
    <td>96</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>16</td>
    <td>128</td>
    <td colspan="32">Payload</td>
  </tr>
  <tr>
    <td>20</td>
    <td>160</td>
    <td colspan="16">Payload</td>
    <td colspan="16">CRC16</td>
  </tr>
</table>

### Attributes
#### __bit3-5__: extended packet indicator

0: small LoLaN packet (with encryption: 32 bytes (16 byte payload), without encryption: 24 bytes (16 bytes payload))

1: single extended packet = 1 (with encryption: 118 bytes (102 byte payload), without encryption: 110 bytes (102 bytes payload))

2: start extended packet with size = 2 (204 bytes payload)

3: start extended packet with size = 3 (306 bytes payload)

4: start extended packet with size = 4 (408 bytes payload)

5: start extended packet with size = 8 (816 bytes payload)

6: start extended packet with size = 13 (1326 bytes payload)

7: extended packet fragment (with encryption: 118 bytes (102 byte payload), without encryption: 110 bytes (102 bytes payload))

#### __bit0-2__: LoLaN data type

LoLaN (TRAP/INFORM/GET/SET)payloads are CBOR (http://cbor.me/) serialized data with the first 1 (or 2 bytes determining the size). 
If bit 7 of first byte is 1, than the size is extended to a 15 bit size with the following byte.

Accessing this data is REST like.
The key-value pairs can be converted to json with mapping keys, and path numbers to string based on device types.
0 key value is a special value, that indicates different things based on data type.

##### 0: TRAP
##### 1: INFORM
the payload includes characteristic value updates in CBOR format.
0 key defines the base path as an array for the key-value pairs. If no 0 key is specified, the root node is the base path. More updates can be included in an inform package as a CBOR array.

```language-json
	{ 0: [2,33,4], 1:"updated value"}
```

This results in updating the /2/33/4/1 value with "updated value" at server side. 

The INFROM message is always acknowledged in the current timeslot.
TRAP is not acknowledged, and not requires to be in a timeslot.

##### 2: GET

Payload is the path to be accessed. Path is defined as an array

```language-json
	[2,33,4,1]
```

This means GET /2/33/4/1.

The reply should be on air in the same timeslot.

##### 3: SET

0 key indicates the base path for modifing key-value pairs.
Other keys are the key-value pairs to be modified 

##### 4: ACK

IMPORTANT: ACK packet number is the same as the packet ACK-ed.

Payload differs on what was ACK-ed, ACK has to be transmitted in the current timeslot.

INFROM: no payload

GET: 0 key indicates result code (integer according to HTTP status codes) other key-value pairs contains data requested

SET: 0 key indicates result code (integer according to HTTP status codes), no payload

##### 5: RETRANSMIT REQUEST

IMPORTANT: RETRANSMIT REQUEST packet number is an extended packet start packet number

the first byte is the number of packets to be retransmitted, and after that the array of packet numbers to be retransmitted

##### 6: CONTROL PACKET

TBD

Timesync and other dinamyc LoLaN configurations will be handeled with this

##### 7: OTHER PROTOCOL DATA

