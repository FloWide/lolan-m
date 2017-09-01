# Serial Line Internet Protocol - Modified for UWB-RTLS

SLIP simply encapsulates higher level protocols (like LoLaN) to packets over a serial line (eg. UART, USB). 

| Hex Value        | Abbreviation           | Description             |
| ---------------- |:----------------------:| -----------------------:|
| 0x7D             | END                    | Frame End ( } )         |
| 0xDB             | ESC                    | Frame Escape            |
| 0xDC             | ESC_END                | Transposed Frame End    |
| 0xDD             | ESC_ESC                | Transposed Frame Escape |

SLIP modifies a standard higher level packet by 
- appending a special "END" byte to it, which distinguishes datagram boundaries in the byte stream,
- if the END byte occurs in the data to be sent, the two byte sequence ESC, ESC_END is sent instead,
- if the ESC byte occurs in the data, the two byte sequence ESC, ESC_ESC is sent.

