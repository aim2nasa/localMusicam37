#include "rBit.h"
#include <malloc.h>
#include <memory.h>

#define show_bits(bit_count) (p->bit_window >> (24 - (bit_count)))

rBit* rBit_instance()
{
	rBit* p = (rBit*)malloc(sizeof(rBit));
	memset(p,0,sizeof(rBit));
	return p;
}

void rBit_delete(rBit* p)
{
	free(p);
}

void rBit_init(rBit* p,unsigned char *pFrame)
{
	// set up the bitstream reader
	p->bit_window = (pFrame[0]<<16)|(pFrame[1]<<8);
	p->bits_in_window = 16;
	p->frame_pos = pFrame+2;
}

int rBit_getBits(rBit* p,int bit_count)
{
	int result = show_bits(bit_count);
	p->bit_window = (p->bit_window << bit_count) & 0xFFFFFF;
	p->bits_in_window -= bit_count;
	while (p->bits_in_window < 16) {
		p->bit_window |= (*p->frame_pos++) << (16 - p->bits_in_window);
		p->bits_in_window += 8;
	}
	return result;
}