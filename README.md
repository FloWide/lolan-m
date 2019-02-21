#  Low Latency Network Protocol (LoLaN)

LoLaN is a stateless protocol, designed to communicate over a wireless UWB network with start topology (however other mediums are targeted also). The design goals are to minimize packet header overhead and the redundancy between network layers.
LoLaN is based on IEEE 802.15.4 frame format (however uses the reserved frame version 3, that makes it incopatible with the standard) and relays on packet size information from the PHY layer.

As most of CPU-s implementing LoLaN will be little endian, thus LoLaN itself is little endian. Eg, the first byte of a 16 bit number is the least significant byte.

## Packet types

### Packet with encryption
This feature is not implemented yet, the following structure is just a proposal for the implementation.<br/>

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
    <td colspan="16">attributes</td>
    <td colspan="8">packet counter (8 bits)</td>
    <td colspan="8">source address (lower 8 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
    <td colspan="8">source address (upper 8 bits)</td>
    <td colspan="16">destination address (16 bits)</td>
    <td colspan="8">timestamp (0-8 bits)</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="32">timestamp (8-40 bits)</td>
  </tr>
  <tr>
    <td>12</td>
    <td>96</td>
    <td colspan="32">payload ...</td>
  </tr>
  <tr>
    <td>(N-1)*4</td>
    <td>(N-1)*32</td>
    <td colspan="8">... payload</td>
    <td colspan="16">HMAC (0-7) bits</td>
  </tr>
  <tr>
    <td>N*4</td>
    <td>N*32</td>
    <td colspan="32">HMAC (8-40) bits</td>
  </tr>
</table>


### Packet without encryption

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
    <td colspan="16">attributes</td>
    <td colspan="8">packet counter (8 bits)</td>
    <td colspan="8">source address (lower 8 bits)</td>
  </tr>
  <tr>
    <td>4</td>
    <td>32</td>
		<td colspan="8">source address (upper 8 bits)</td>
    <td colspan="16">destination address (16 bits)</td>
    <td colspan="8">payload ...</td>
  </tr>
  <tr>
    <td>8</td>
    <td>64</td>
    <td colspan="32">... payload ...</td>
  </tr>
  <tr>
    <td>N*4</td>
    <td>N*32</td>
    <td colspan="16">... payload</td>
    <td colspan="16">CRC-16</td>
  </tr>
</table>

## Packet header *attributes* part
Note: this is intended as the extended version of IEEE 802.15.4 frame control.

### *bits 0..2*: packet type

LoLaN (TRAP/INFORM/GET/SET) payloads are CBOR type serialized data.

Accessing this data is REST like.
The key-value pairs can be converted to *json* with mapping keys, and path numbers to string based on device types.
The zero (0) key entry is a special value, that indicates different things based on packet type.

#### *packet type = 0*: 802.15.4 BEACON
not used

#### *packet type = 1*: 802.15.4 DATA
OTHER protocol data, not LoLaN

#### *packet type = 2*: 802.15.4 ACK
Payload differs on what was ACK-ed, ACK has to be transmitted in the current timeslot. LoLaN related ACK types: <br/>

**INFROM**: no ACK for INFORM packets<br/>
**GET**: see *lolan-get.c* for reply formats<br/>
**SET**: see *lolan-set.c* for reply formats

*Note*: the ACK request bits are currently ignored, every GET and SET request will be replied.<br/>
**Important**: the packet counter value of the ACK packet must be the same as of the packet ACK-ed.

#### *packet type = 3*: 802.15.4 MAC
not used

#### *packet type = 4*: LoLaN INFORM
These packets are sent to provide information about the variables of the sender device without any requests from the other devices. Mainly sent by slave devices and intended as status updates for the server. INFORM packets do not need to be replied (ACK-ed).

Format of INFORM packets: see *lolan-inform.c* 

#### *packet type = 5*: LoLaN GET
Values of remote variables can be obtained with GET packets sent to the target device. See *lolan-get.c* for GET and reply format.

#### *packet type = 6*: LoLaN SET
Values of remote variables can be changed with SET packets sent to the target device. See *lolan-set.c* for SET and reply format.

#### *packet type = 7*: LoLaN CONTROL
Packet type for miscellaneous communication (e.g. timing packet sent by UWB anchors), non-CBOR type payload.

### *bit 3*: security enabled
Indicates an encrypted packet. *Not implemented yet.*

### *bit 4*: frame pending
The next packet will extend the current one. *Not implemented yet.*

### *bit 5*: ACK request
The recipient shall send an ACK in the same timeslot. *Not used yet.*

### *bits 6..9*: bytes to boundary
If security is enabled, these bits inidicate the number of random filled bytes at the end of the packet to meet the security block boundary (15 bytes max)

### *bit 10*: reserved
Should be set.

### *bit 11*: routed packet
If a packet is forwarded (routing request), this bit should be set.

### *bit 12, 13*: frame version
Set both bits (3 dec.) to indicate LoLaN frame.

### *bit 14*: reserved
Should be set.

### *bit 15*: routing request
If this bit is set, the receiver device should forward this packet if it is addressed to an other device.

## Packet header *packet counter* part
Generally, a device should increment this number for each original (not ACK, not forwarded) packet sent. In special cases this rule may be overridden, e.g. anchors create timing packets (LoLaN CONTROL) from the incoming UWB packets with the same packet counter value as the incoming packet.

## Packet header *addresses* part
Device LoLaN addresses can be from 0 to 65534 (0x0000 to 0xFFFE), but 0 address is not recommended. The destination address in LoLaN packets can also be 0xFFFF (*LOLAN_BROADCAST_ADDRESS*), these packets will be processed by all receiver devices.

## Packet payload
Contains the data itself, its length may be also zero (no data).

## CRC-16
The CRC-16 value is generated from all bytes, including the packet header and the payload.

