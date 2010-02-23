#ifndef __MUSICAM_H__
#define __MUSICAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <mpg123.h>
#include <twolame.h>

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
	} mc;

	MUSICAM_DLL_EXPORT mc* mc_init();
	MUSICAM_DLL_EXPORT void mc_close(mc* m);
	MUSICAM_DLL_EXPORT int mc_encode(mc* m,const unsigned char* inMp2frame,int inMp2frameSize,int numSampleCh,unsigned char* outMucframe,int outMucframeSize);
	MUSICAM_DLL_EXPORT const char* mc_getLastError();

	//DLL_EXPORT twolame_options* mp2c_getOptions(mp2Changer* chg);
	//DLL_EXPORT kjmp2_context_t* mp2c_getkjmp2(mp2Changer* chg);
	//DLL_EXPORT void mp2c_parseMp2Header(mp2Changer* chg,unsigned char* pBuffer);
	//DLL_EXPORT int mp2c_bitrate(mp2Changer* chg);
	//DLL_EXPORT int mp2c_encode(mp2Changer* chg,const unsigned char* frame,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize);
	//DLL_EXPORT const char* mp2c_getLastError();
	//DLL_EXPORT int mp2c_getBitrate(int nId,int nBitrateIdx);

#ifdef __cplusplus
}
#endif

#endif//__MUSICAM_H__
