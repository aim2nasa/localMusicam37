#include "musicam.h"
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#define MAX_ERROR_MSG	256
#define INBUFF  16384
#define OUTBUFF 32768 

static char errMsg[MAX_ERROR_MSG];

static const int bitrateTable[2][15]={
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},		//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

mc* mc_init()
{
	int ret=0;
	mc* m=0;
	if(mpg123_init()!=MPG123_OK) return 0;

	m = (mc*)malloc(sizeof(mc));
	memset(m,0,sizeof(mc));
	m->hdr = (MP2_HEADER*)malloc(sizeof(MP2_HEADER));
	memset(m->hdr,0,sizeof(MP2_HEADER));

	m->mpg = mpg123_new(NULL,&ret);
	if(m==NULL) { mpg123_exit(); return 0; }
	if(mpg123_open_feed(m->mpg)!=MPG123_OK) { mpg123_delete(m->mpg); mpg123_exit(); return 0; }

	m->opt = twolame_init();
	m->sc = scfCrc_init();

	memset(m->convPcm,0,sizeof(m->convPcm));
	m->iReadCount = 0;
	m->offset = 0;
	return m;
}

void mc_close(mc* m)
{
	free(m->hdr);
	mpg123_delete(m->mpg);
	mpg123_exit();
	twolame_close(&m->opt);
	scfCrc_close(m->sc);
	free(m);
}

int mc_pcm_mp2_encode(mc* m,const unsigned char* pcmFrame,int pcmFrameSize,int numSampleCh,unsigned char* outMucframe)
{
	unsigned long nRtn;
	int nSize,nCrc;
	size_t size = 0;
	unsigned char tmpBuffer[OUTBUFF];

	memset(errMsg,0,sizeof(errMsg));

	nSize = twolame_encode_buffer_interleaved(m->opt,pcmFrame,numSampleCh,tmpBuffer,OUTBUFF);
	if(nSize<0) {
		sprintf_s(errMsg,sizeof(errMsg),"twolame_encode_buffer_interleaved() return %d",nSize);
		return -1;
	}

	nCrc = scfCrc_apply(m->sc,tmpBuffer,nSize,outMucframe);
	if(nCrc<0) {
		sprintf_s(errMsg,sizeof(errMsg),"scfCrc_apply() return %d",nCrc);
		return -1;
	}
	return nSize;
}

int mc_mp2_mp2_encode(mc* m,const unsigned char* inMp2frame,int inMp2frameSize,int numSampleCh,unsigned char* outMucframe)
{
	int i,nRtn;
	int nSize,nCrc;
	size_t size = 0;
	unsigned char pcm[PCM_BUF_SIZE];
	MP2_HEADER header;
	TWOLAME_MPEG_version ver = twolame_get_version(m->opt);

	memset(errMsg,0,sizeof(errMsg));

	mc_parseMp2Header(&header,(unsigned char*)inMp2frame);
	nRtn = mpg123_decode(m->mpg,inMp2frame,inMp2frameSize,(unsigned char*)pcm,PCM_BUF_SIZE,&size);
	if(nRtn!=0) {
		sprintf_s(errMsg,sizeof(errMsg),"mpg123_decode() return %d",nRtn); return -1; }

	if((TWOLAME_MPEG_version)header.id==TWOLAME_MPEG2 && ver==TWOLAME_MPEG1) {
		nSize = mc_pcm24to48(pcm,m->convPcm,(int)size,(header.mode==3)?1:2);
		if(nSize==-1) return -1;

		nRtn = 0;
		for(i=0;i<2;i++) 
			nRtn+=mc_pcm_mp2_encode(m,m->convPcm+i*nSize/2,nSize/2,numSampleCh,outMucframe+nRtn);

		return nRtn;
	}else if((TWOLAME_MPEG_version)header.id==TWOLAME_MPEG1 && ver==TWOLAME_MPEG2) {
		if(m->iReadCount==0){
			nSize = mc_pcm48to24(pcm,m->convPcm,(int)size,(header.mode==3)?1:2);
			if(nSize==-1) return -1;
			m->offset = nSize;
			m->iReadCount++;
			return 0;
		}else{
			nSize = mc_pcm48to24(pcm,m->convPcm+m->offset,(int)size,(header.mode==3)?1:2);
			if(nSize==-1) return -1;

			nRtn=mc_pcm_mp2_encode(m,m->convPcm,m->offset+nSize,numSampleCh,outMucframe);
			m->offset=0;
			m->iReadCount=0;
			return nRtn;
		}
	}else{
		return mc_pcm_mp2_encode(m,pcm,(int)size,numSampleCh,outMucframe);
	}
}

const char* mc_getLastError()
{
	return errMsg;
}

void mc_parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer)
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

int mc_getSamplingFrequency(int id)
{
	if(id==1){ return 48000; }else if(id==0){ return 24000; }else{
		sprintf_s(errMsg,sizeof(errMsg),"id must be 0 or 1");
		return -1;
	}
}

int mc_getBitrate(int nId,int nBitrateIdx)
{
	if(nId!=0&&nId!=1) {
		sprintf_s(errMsg,sizeof(errMsg),"id must be 0 or 1");
		return -1;
	}

	if(nBitrateIdx<=0 || nBitrateIdx>=15) {
		sprintf_s(errMsg,sizeof(errMsg),"bitrate index should be greater than 0 and less than 15");
		return -1;
	}
	return 1000*bitrateTable[nId][nBitrateIdx];	//kbps unit
}

int mc_encodeOption(twolame_options* encopts,int inpSampleFreq,HEADER_ID id,int nDstBitrate,TWOLAME_MPEG_mode mpegMode,int verbose)
{
	int nRtn = 0;

	twolame_set_version(encopts,id);
	twolame_set_rawmode(encopts, 1);
	twolame_set_in_samplerate(encopts,inpSampleFreq);
	twolame_set_out_samplerate(encopts,twolame_get_in_samplerate(encopts));
	twolame_set_mode(encopts,mpegMode);
	twolame_set_num_channels(encopts,(mpegMode==TWOLAME_MONO)?1:2);
	twolame_set_bitrate(encopts,nDstBitrate);
	twolame_set_error_protection(encopts,TRUE);

	//No X-PAD assumed
	twolame_set_num_ancillary_bits(encopts,8*(2+(((twolame_get_bitrate(encopts)/twolame_get_num_channels(encopts))>=56)?4:2)));

	if((nRtn=twolame_init_params(encopts))!=0) {
		sprintf_s(errMsg,sizeof(errMsg),"configuring libtwolame encoder failed, return %d",nRtn); return -1;
	}

	if(verbose) twolame_print_config(encopts);
	return 0;
}

int mc_computeBitrate(int nFrameSize,int nSampleFreq)
{
	return nFrameSize*nSampleFreq/144;
}

int mc_pcm48to24(unsigned char *src, unsigned char *dst, int nSrclen, int nChannel)
{
	int i,loopcnt,nBytes=0;
	int offset = sizeof(short)*nChannel;

	if(nSrclen==0||nChannel==0||nSrclen%offset != 0) {
		sprintf_s(errMsg,sizeof(errMsg),"wrong arguments mc_pcm48to24(0x%p,0x%p,%d,%d)",src,dst,nSrclen,nChannel);
		return -1;
	}

	loopcnt = nSrclen / offset / 2;
	for(i=0;i<loopcnt;i++) {
		memcpy(&dst[i*offset], &src[i*offset*2], offset);
		nBytes+=offset;
	}
	return nBytes;
}

int mc_pcm24to48(unsigned char *src, unsigned char *dst, int nSrclen, int nChannel)
{
	int i,j,loopcnt,nBytes=0;
	int offset = sizeof(short)*nChannel;

	if(nSrclen==0||nChannel==0||nSrclen%offset!= 0) {
		sprintf_s(errMsg,sizeof(errMsg),"wrong arguments mc_pcm24to48(0x%p,0x%p,%d,%d)",src,dst,nSrclen,nChannel);
		return -1;
	}

	loopcnt = nSrclen / offset;
	for(i=0;i<loopcnt;i++) {
		for(j=0; j<2; j++) {
			memcpy(&dst[i*offset*2 + offset*j], &src[i*offset], offset);
			nBytes+=offset;
		}
	}
	return nBytes;
}