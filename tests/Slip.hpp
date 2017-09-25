#define SLIP_END	0x7D
#define SLIP_ESC	0xDB
#define SLIP_ESC_END	0xDC
#define SLIP_ESC_ESC	0xDD

class SlipPacketizer
{
    public:
	std::vector<uint8_t>	decodeBuffer;
	std::vector<uint8_t>	encodeBuffer;

	uint8_t prev_in;	// previous input char

    void finishEncode() {
	encodeBuffer.push_back(SLIP_END);
    }
    
    void feedEncode(uint8_t in) {
	if (in == SLIP_END) {
	    encodeBuffer.push_back(SLIP_ESC);
	    encodeBuffer.push_back(SLIP_ESC_END);
	} else if (in == SLIP_ESC) {
	    encodeBuffer.push_back(SLIP_ESC);
	    encodeBuffer.push_back(SLIP_ESC_ESC);
	} else {
	    encodeBuffer.push_back(in);
	}
    }
    
    void encode(uint8_t *in,int size) {
	do {
	    feedEncode(*in);
	    in++;
	    size--;
	} while (size>0);
	finishEncode();
    }

    // return 1 if packet finished
    int feedDecode(uint8_t in) {
	if (decodeBuffer.size() > 0) {
	    if (prev_in==SLIP_ESC) {
		if (in == SLIP_ESC_END) {
		    decodeBuffer.back()=SLIP_END;
		    prev_in=in;
		    return 0;
		} else if (in == SLIP_ESC_ESC) {
		    //decodeBuffer.back()=SLIP_ESC;
		    prev_in=in;
		    return 0;
		} else {
		    prev_in=in;
		    return -1;
		    // this cannot happen TODO: erro handling
		}
	    }
	}
	
	prev_in=in;
	if (in == SLIP_END) {
	    return 1;
	}
	decodeBuffer.push_back(in);
	return 0;
    }

};
