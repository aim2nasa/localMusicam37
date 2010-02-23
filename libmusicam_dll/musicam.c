#include "musicam.h"
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#define MAX_ERROR_MSG	256
#define PCM_BUF_SIZE	(1152*4)

static char errMsg[MAX_ERROR_MSG];

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
	return m;
}

void mc_close(mc* m)
{
	free(m->hdr);
	mpg123_delete(m->mpg);
	mpg123_exit();
	twolame_close(&m->opt);
	free(m);
}

int mc_encode(mc* m,const unsigned char* inMp2frame,int inMp2frameSize,int numSampleCh,unsigned char* outMucframe,int outMucframeSize)
{
	unsigned long nRtn;
	int nSize;
	size_t size = 0;
	short pcm[PCM_BUF_SIZE];

	memset(errMsg,0,sizeof(errMsg));
	nRtn = mpg123_decode(m->mpg,inMp2frame,inMp2frameSize,(unsigned char*)pcm,PCM_BUF_SIZE,&size);

	nSize = twolame_encode_buffer_interleaved(m->opt,pcm,numSampleCh,outMucframe,outMucframeSize);
	if(nSize<0) sprintf_s(errMsg,sizeof(errMsg),"twolame_encode_buffer_interleaved() return %d",nSize);
	return nSize;
}

const char* mc_getLastError()
{
	return errMsg;
}