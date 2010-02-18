#ifndef __MP2CHANGER_H__
#define __MP2CHANGER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <kjmp2.h>
#include <twolame.h>

#define DLL_EXPORT __declspec(dllexport)

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

typedef struct _mp2Changer_context {
	MP2_HEADER*		 hdr;
	kjmp2_context_t* mp2;
	twolame_options* opt;
} mp2Changer;

DLL_EXPORT mp2Changer* mp2c_init();
DLL_EXPORT twolame_options* mp2c_getOptions(mp2Changer* chg);
DLL_EXPORT kjmp2_context_t* mp2c_getkjmp2(mp2Changer* chg);
DLL_EXPORT void mp2c_parseMp2Header(mp2Changer* chg,unsigned char* pBuffer);
DLL_EXPORT int mp2c_bitrate(mp2Changer* chg);
DLL_EXPORT int mp2c_encode(mp2Changer* chg,const unsigned char* frame,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize);
DLL_EXPORT void mp2c_close(mp2Changer* chg);
DLL_EXPORT const char* mp2c_getLastError();

#ifdef __cplusplus
}
#endif

#endif//__MP2CHANGER_H__
