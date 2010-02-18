#include "mp2Changer.h"
#include <common.h>
#include <malloc.h>
#include <assert.h>
#include <memory.h>
#include <stdio.h>

#define PCM_BUF_SIZE	(1152*4)

static char errMsg[256];
static const int bitrateTable[2][15]={
	{8,16,24,32,40,48,56,64,80,96,112,128,144,160},			//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

static int getBitrate(int nId,int nBitrateIdx)
{
	assert(nId==0 || nId==1);
	assert(nBitrateIdx>0 && nBitrateIdx<15);
	return 1000*bitrateTable[nId][nBitrateIdx];	//kbps unit
}

mp2Changer* mp2c_init()
{
	mp2Changer* mp2c = (mp2Changer*)malloc(sizeof(mp2Changer));
	memset(mp2c,0,sizeof(mp2Changer));
	mp2c->hdr = (MP2_HEADER*)malloc(sizeof(MP2_HEADER));
	memset(mp2c->hdr,0,sizeof(MP2_HEADER));
	mp2c->mp2 = (kjmp2_context_t*)malloc(sizeof(kjmp2_context_t));
	memset(mp2c->mp2,0,sizeof(kjmp2_context_t));

	kjmp2_init(mp2c->mp2);
	mp2c->opt = twolame_init();
	return mp2c;
}

twolame_options* mp2c_getOptions(mp2Changer* chg)
{
	return chg->opt;
}

kjmp2_context_t* mp2c_getkjmp2(mp2Changer* chg)
{
	return chg->mp2;
}

void mp2c_parseMp2Header(mp2Changer* chg,unsigned char* pBuffer)
{
	MP2_HEADER* pHeader	= chg->hdr;
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

int mp2c_bitrate(mp2Changer* chg)
{
	return getBitrate(chg->hdr->id,chg->hdr->bitrateIdx);
}

int mp2c_encode(mp2Changer* chg,const unsigned char* frame,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize)
{
	unsigned long nRtn;
	int nSize;
	short pcm[PCM_BUF_SIZE];

	memset(errMsg,0,sizeof(errMsg));
	nRtn = kjmp2_decode_frame(chg->mp2,frame,pcm);
	if(nRtn==0) {
		sprintf_s(errMsg,sizeof(errMsg),"kjmp2_decode_frame() return 0");
		return nRtn;
	}

	nSize = twolame_encode_buffer_interleaved(chg->opt,pcm,numSampleCh,mp2Buffer,mp2BufferSize);
	if(nSize<0) sprintf_s(errMsg,sizeof(errMsg),"twolame_encode_buffer_interleaved() return %d",nSize);
	return nSize;
}

void mp2c_close(mp2Changer* chg)
{
	twolame_close(&chg->opt);
	free(chg->hdr); chg->hdr=0;
	free(chg->mp2); chg->mp2=0;
	chg->opt=0;
	free(chg); chg=0;
}

const char* mp2c_getLastError()
{
	return errMsg;
}