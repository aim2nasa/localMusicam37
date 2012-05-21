#include "frmScfCrc.h"
#include <malloc.h>
#include <memory.h>
#include <assert.h>

static const int bitrateTable[2][15]={
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},		//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

static unsigned char sblimit_tbl[3]={ 27,8,30 };

static const int bitalloc_tbl[3][32] = {
	{4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0},	//48KHz Table13
	{4, 4, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	//48KHz Table14
	{4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0},	//24KHz Table15
};

void parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer);
int getBitAllocTable(int nBitrateCh,int nSampleFreq);
int getBound(const MP2_HEADER* pHeader,int sblimit);
int getBitrate(int nId,int nBitrateIdx);
int frameSize(const MP2_HEADER* pHeader);
int getDabExt(int bit_rate,int num_channel);
void initVariables(frmScfCrc* p,unsigned char* pFrame);
void doBitAlloc(rBit* pRb,int bound,int sblimit,int nch,int nTable,unsigned int bit_alloc[2][32]);
void doScfsi(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32]);
void doScaleFactor(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32],unsigned int scalefactor[2][32][3]);
void update_CRCDAB (unsigned int data, unsigned int length, unsigned int *crc);
void CRC_calcDAB (int nch,int sblimit,
				  unsigned int bit_alloc[2][SBLIMIT],
				  unsigned int scfsi[2][SBLIMIT],
				  unsigned int scalar[2][SBLIMIT][3], unsigned int *crc,int packed);

frmScfCrc* fsc_instance()
{
	frmScfCrc* p = (frmScfCrc*)malloc(sizeof(frmScfCrc));
	memset(p,0,sizeof(frmScfCrc));
	p->pRb = rBit_instance();
	return p;
}

void fsc_delete(frmScfCrc* p)
{
	rBit_delete(p->pRb);
	free(p);
}

void fsc_init(frmScfCrc* p,unsigned char* pFrame)
{
	p->pFrame = pFrame;
	p->bInitFrame = 1;

	memset(p->bit_alloc,0,sizeof(p->bit_alloc));
	memset(p->scfsi,0,sizeof(p->scfsi));
	memset(p->scalefactor,0,sizeof(p->scalefactor));
	memset(p->scfCrc,0,sizeof(p->scfCrc));

	initVariables(p,pFrame);
}

int fsc_applyScfCrc(frmScfCrc* p,unsigned char* pFrame)
{
	int i;
	memcpy(p->inpFrame,pFrame,p->nFrameSize);

	initVariables(p,pFrame);

	// set up the bitstream reader
	rBit_init(p->pRb,pFrame+2);
	rBit_getBits(p->pRb,16);
	if(p->header.protect == 0) rBit_getBits(p->pRb,16);

	//bit allocation
	doBitAlloc(p->pRb,p->bound,p->sblimit,p->nch,p->nTable,p->bit_alloc);

	//scfsi
	doScfsi(p->pRb,&p->header,p->sblimit,p->nch,p->bit_alloc,p->scfsi);

	//scale factors
	doScaleFactor(p->pRb,&p->header,p->sblimit,p->nch,p->bit_alloc,p->scfsi,p->scalefactor);

	p->dabExtension = getDabExt(p->bit_rate,p->nch);

	memset(p->scfCrc,0,sizeof(p->scfCrc));
	for(i = p->dabExtension-1;i>=0;i--)
		CRC_calcDAB(p->nch,p->sblimit,p->bit_alloc,p->scfsi,p->scalefactor,&p->scfCrc[i],i);

	if(p->bInitFrame){
		p->bInitFrame=0;
		memcpy(p->outFrame,p->inpFrame,p->nFrameSize);
		return 0;
	}else{
		if(p->dabExtension==4) {
			p->outFrame[p->nFrameSize-6]=p->scfCrc[3];
			p->outFrame[p->nFrameSize-5]=p->scfCrc[2];
		}
		p->outFrame[p->nFrameSize-4]=p->scfCrc[1];
		p->outFrame[p->nFrameSize-3]=p->scfCrc[0];

		memcpy(pFrame,p->outFrame,p->nFrameSize);	//scfCrc가 적용된 스트림
		memcpy(p->outFrame,p->inpFrame,p->nFrameSize);
		return p->nFrameSize;
	}
}

const MP2_HEADER* fsc_header(frmScfCrc* p)
{
	return &p->header;
}

int fsc_frameSize(frmScfCrc* p)
{
	return frameSize(&p->header);
}

void parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer)
{
	pHeader->sync		= (((unsigned short)pBuffer[0]&0xFF)<<4) | (((unsigned short)pBuffer[1]&0xF0)>>4);
	pHeader->id			= (pBuffer[1]&0x08)>>3;
	pHeader->layer		= (pBuffer[1]&0x06)>>1;
	pHeader->protect	= (pBuffer[1]&0x01);
	pHeader->bitrateIdx	= (pBuffer[2]&0xF0)>>4;
	pHeader->sampling	= (pBuffer[2]&0x0C)>>2;
	pHeader->padding	= (pBuffer[2]&0x02)>>1;
	pHeader->priv		= (pBuffer[2]&0x01);
	pHeader->mode		= (pBuffer[3]&0xC0)>>6;
	pHeader->mode_ext	= (pBuffer[3]&0x30)>>4;
	pHeader->copyright	= (pBuffer[3]&0x08)>>3;
	pHeader->orignal	= (pBuffer[3]&0x04)>>2;
	pHeader->emphasis	= (pBuffer[3]&0x03);
}

int getBitAllocTable(int nBitrateCh,int nSampleFreq)
{
	if(nSampleFreq==24) {
		return 2;		//Table 15
	}else if(nSampleFreq==48) {
		if(nBitrateCh==56||nBitrateCh==64||nBitrateCh==80||nBitrateCh==96||nBitrateCh==112||
			nBitrateCh==128||nBitrateCh==160||nBitrateCh==192) 
			return 0;	//Table 13
		else if(nBitrateCh==32||nBitrateCh==48) 
			return 1;	//Table14
	}
	return -1;
}

int getBound(const MP2_HEADER* pHeader,int sblimit)
{
	int bound = sblimit;
	// parse the mode_extension, set up the stereo bound
	if(pHeader->mode == JOINT_STEREO) bound = (pHeader->mode_ext + 1) << 2;
	return bound;
}

int getBitrate(int nId,int nBitrateIdx)
{
	assert(nId==0 || nId==1);
	assert(nBitrateIdx>0 && nBitrateIdx<15);
	return 1000*bitrateTable[nId][nBitrateIdx];	//kbps unit
}

int frameSize(const MP2_HEADER* pHeader)
{
	return 144*getBitrate(pHeader->id,pHeader->bitrateIdx)/((pHeader->id)?48000:24000);
}

int getDabExt(int bit_rate,int num_channel)
{
	return ((bit_rate/num_channel) >= 56)? 4:2;
}

void doBitAlloc(rBit* pRb,int bound,int sblimit,int nch,int nTable,unsigned int bit_alloc[2][32])
{
	int sb,ch;
	memset(bit_alloc,0,sizeof(bit_alloc));

	for(sb=0; sb<bound; sb++)
		for(ch=0; ch<nch; ch++)
			bit_alloc[ch][sb] = rBit_getBits(pRb,bitalloc_tbl[nTable][sb]);
	for(sb=bound; sb<sblimit; sb++)
		bit_alloc[0][sb] = bit_alloc[1][sb] = rBit_getBits(pRb,bitalloc_tbl[nTable][sb]);
}

void doScfsi(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32])
{
	int sb,ch;
	memset(scfsi,0,sizeof(scfsi));

	for(sb=0; sb<sblimit; sb++) {
		for(ch=0; ch < nch; ch++)
			if(bit_alloc[ch][sb])
				scfsi[ch][sb] = rBit_getBits(pRb,2);
		if(pHeader->mode == MONO)
			scfsi[1][sb] = scfsi[0][sb];
	}
}

void doScaleFactor(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32],unsigned int scalefactor[2][32][3])
{
	int sb,ch,part;
	memset(scalefactor,0,sizeof(scalefactor));

	for(sb=0; sb<sblimit; sb++) {
		for(ch=0; ch<nch; ch++)
			if(bit_alloc[ch][sb]) {
				switch (scfsi[ch][sb]) {
					case 0: scalefactor[ch][sb][0] = rBit_getBits(pRb,6);
						scalefactor[ch][sb][1] = rBit_getBits(pRb,6);
						scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
						break;
					case 1: scalefactor[ch][sb][0] =
								scalefactor[ch][sb][1] = rBit_getBits(pRb,6);
						scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
						break;
					case 2: scalefactor[ch][sb][0] =
								scalefactor[ch][sb][1] =
								scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
						break;
					case 3: scalefactor[ch][sb][0] = rBit_getBits(pRb,6);
						scalefactor[ch][sb][1] =
							scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
						break;
				}
			}
			if(pHeader->mode == MONO)
				for(part=0; part<3; part++)
					scalefactor[1][sb][part] = scalefactor[0][sb][part];
	}
}

void update_CRCDAB (unsigned int data, unsigned int length, unsigned int *crc)
{
	unsigned int masking, carry;

	masking = 1 << length;

	while ((masking >>= 1)) {
		carry = *crc & 0x80;
		*crc <<= 1;
		if (!carry ^ !(data & masking))
			*crc ^= CRC8_POLYNOMIAL;
	}
	*crc &= 0xff;
}

void CRC_calcDAB (int nch,int sblimit,
				  unsigned int bit_alloc[2][SBLIMIT],
				  unsigned int scfsi[2][SBLIMIT],
				  unsigned int scalar[2][SBLIMIT][3], unsigned int *crc,int packed)
{
	int i, j, k;
	int nb_scalar;
	int f[5] = { 0, 4, 8, 16, 30 };
	int first, last;

	first = f[packed];
	last = f[packed + 1];
	if (last > sblimit)
		last = sblimit;

	nb_scalar = 0;
	*crc = 0x0;
	for (i = first; i < last; i++)
		for (k = 0; k < nch; k++)
			if (bit_alloc[k][i])	/* above jsbound, bit_alloc[0][i] == ba[1][i] */
				switch (scfsi[k][i]) {
					case 0:
						for (j = 0; j < 3; j++) {
							nb_scalar++;
							update_CRCDAB (scalar[k][i][j] >> 3, 3, crc);
						}
						break;
					case 1:
					case 3:
						nb_scalar += 2;
						update_CRCDAB (scalar[k][i][0] >> 3, 3, crc);
						update_CRCDAB (scalar[k][i][2] >> 3, 3, crc);
						break;
					case 2:
						nb_scalar++;
						update_CRCDAB (scalar[k][i][0] >> 3, 3, crc);
			}
}

void initVariables(frmScfCrc* p,unsigned char* pFrame)
{
	parseMp2Header(&p->header,pFrame);

	p->nFrameSize = frameSize(&p->header);
	if(p->header.mode==MONO) {p->nch=1;}else{p->nch=2;}
	p->bit_rate = bitrateTable[p->header.id][p->header.bitrateIdx];	// kbit/s

	switch(p->header.id){
		case ID_ISO13818:
			p->sample_freq = 24;
			break;
		case ID_ISO11172:
			p->sample_freq = 48;
			break;
		default:
			assert(0);
	}

	p->nTable = getBitAllocTable(p->bit_rate/p->nch,p->sample_freq);
	assert(p->nTable!=-1);
	p->sblimit = sblimit_tbl[p->nTable];
	p->bound = getBound(&p->header,p->sblimit);
}