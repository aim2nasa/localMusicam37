#ifndef __SCFCRC_H__
#define __SCFCRC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif

#define MAX_FRAME_SIZE	1152
#define MAX_INDEX		2
#define MAX_SCF_CRC_SIZE	4

	typedef struct _scfCrc_context {
		unsigned char buffer[MAX_INDEX][MAX_FRAME_SIZE];
		unsigned int index;
		unsigned int frameCount;
		unsigned char crc[MAX_SCF_CRC_SIZE];
		unsigned int nCrc;
	} scfCrc;

//DLL_EXPORT mp2Changer* mp2c_init();
//DLL_EXPORT twolame_options* mp2c_getOptions(mp2Changer* chg);
//DLL_EXPORT kjmp2_context_t* mp2c_getkjmp2(mp2Changer* chg);
//DLL_EXPORT void mp2c_parseMp2Header(mp2Changer* chg,unsigned char* pBuffer);
//DLL_EXPORT int mp2c_bitrate(mp2Changer* chg);
//DLL_EXPORT int mp2c_encode(mp2Changer* chg,const unsigned char* frame,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize);
//DLL_EXPORT void mp2c_close(mp2Changer* chg);
//DLL_EXPORT const char* mp2c_getLastError();
//DLL_EXPORT int mp2c_getBitrate(int nId,int nBitrateIdx);

	DLL_EXPORT scfCrc* scfCrc_init();
	DLL_EXPORT void scfCrc_close(scfCrc* sc);
	DLL_EXPORT int scfCrc_getCrc(const unsigned char* inFrame,int inFrameSize,unsigned char crc[MAX_SCF_CRC_SIZE]);
	DLL_EXPORT int scfCrc_apply(scfCrc* sc,unsigned char* inFrame,int inFrameSize,unsigned char* outFrame,int outFrameSize);

#ifdef __cplusplus
}
#endif

#endif//__SCFCRC_H__
