#ifndef __SCFCRC_H__
#define __SCFCRC_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif

#define MAX_FRAME_SIZE		1152
#define MAX_INDEX			2
#define MAX_SCF_CRC_SIZE	4

	typedef struct _scfCrc_context {
		unsigned char buffer[MAX_INDEX][MAX_FRAME_SIZE];
		unsigned int index;
		unsigned int frameCount;
		unsigned char crc[MAX_SCF_CRC_SIZE];
		unsigned int nCrc;
	} scfCrc;

	DLL_EXPORT scfCrc* scfCrc_init();
	DLL_EXPORT void scfCrc_close(scfCrc* sc);
	DLL_EXPORT int scfCrc_getCrc(const unsigned char* inFrame,int inFrameSize,unsigned char crc[MAX_SCF_CRC_SIZE]);
	DLL_EXPORT int scfCrc_apply(scfCrc* sc,unsigned char* inFrame,int inFrameSize,unsigned char* outFrame);
	DLL_EXPORT const char* scfCrc_getLastError();

#ifdef __cplusplus
}
#endif

#endif//__SCFCRC_H__
