#ifndef __MUSICAM_H__
#define __MUSICAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <mpg123.h>
#include <twolame.h>
#include <scfCrc.h>

#ifndef MUSICAM_DLL_EXPORT
#define MUSICAM_DLL_EXPORT __declspec(dllexport)
#endif

#pragma pack(push, 1)
	typedef struct{
		unsigned short sync:12;
		unsigned short id:1;
		unsigned short layer:2;
		unsigned short protect:1;
		unsigned short bitrateIdx:4;
		unsigned short sampling:2;
		unsigned short padding:1;
		unsigned short priv:1;
		unsigned short mode:2;
		unsigned short mode_ext:2;
		unsigned short copyright:1;
		unsigned short orignal:1;
		unsigned short emphasis:2;
	}MP2_HEADER;
#pragma pack(pop)

	typedef struct _musicam_context {
		MP2_HEADER*		 hdr;
		mpg123_handle*	 mpg;
		twolame_options* opt;
		scfCrc*			 sc;
	} mc;

	MUSICAM_DLL_EXPORT mc* mc_init();
	MUSICAM_DLL_EXPORT void mc_close(mc* m);
	MUSICAM_DLL_EXPORT int mc_pcm_mp2_encode(mc* m,const unsigned char* pcmFrame,int pcmFrameSize,int numSampleCh,unsigned char* outMucframe);
	MUSICAM_DLL_EXPORT int mc_mp2_mp2_encode(mc* m,const unsigned char* inMp2frame,int inMp2frameSize,int numSampleCh,unsigned char* outMucframe);
	MUSICAM_DLL_EXPORT const char* mc_getLastError();

#ifdef __cplusplus
}
#endif

#endif//__MUSICAM_H__
