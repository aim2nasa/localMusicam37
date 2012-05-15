#include "frmScfCrc.h"
#include <malloc.h>
#include <memory.h>
#include <assert.h>

static const int bitrateTable[2][15]={
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},		//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

static unsigned char sblimit_tbl[3]={ 27,8,30 };

void parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer);
int getBitAllocTable(int nBitrateCh,int nSampleFreq);
int getBound(const MP2_HEADER* pHeader,int sblimit);
int getBitrate(int nId,int nBitrateIdx);
int frameSize(const MP2_HEADER* pHeader);

frmScfCrc* fsc_instance()
{
	frmScfCrc* p = (frmScfCrc*)malloc(sizeof(frmScfCrc));
	memset(p,0,sizeof(frmScfCrc));
	return p;
}

void fsc_delete(frmScfCrc* p)
{
	free(p);
}

void fsc_init(frmScfCrc* p,unsigned char* pFrame)
{
	p->pFrame = pFrame;
	p->bInitFrame = 1;

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

	memset(p->bit_alloc,0,sizeof(p->bit_alloc));
	memset(p->scfsi,0,sizeof(p->scfsi));
	memset(p->scalefactor,0,sizeof(p->scalefactor));
	memset(p->scfCrc,0,sizeof(p->scfCrc));
}

//int fsc_applyScfCrc(frmScfCrc* p,unsigned char* pFrame)
//{
//	return 0;
//}

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