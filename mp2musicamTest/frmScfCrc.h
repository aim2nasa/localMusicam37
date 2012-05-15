#ifndef __FRMSCFCRC_H__
#define __FRMSCFCRC_H__

#include "mp2Header.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif

	typedef struct _frmScfSrc_context {
		unsigned char* pFrame;
		int bInitFrame;
		int dabExtension;
		MP2_HEADER header;
		int nFrameSize;
		int nch;
		int bit_rate;
		int sample_freq;
		int nTable;
		int sblimit;
		int bound;
		unsigned int bit_alloc[2][32];
		unsigned int scfsi[2][32];
		unsigned int scalefactor[2][32][3];
		unsigned int scfCrc[4];
	} frmScfCrc;

	DLL_EXPORT frmScfCrc* fsc_instance();
	DLL_EXPORT void fsc_delete(frmScfCrc* p);
	DLL_EXPORT void fsc_init(frmScfCrc* p,unsigned char* pFrame);
	DLL_EXPORT const MP2_HEADER* fsc_header(frmScfCrc* p);
	DLL_EXPORT int fsc_frameSize(frmScfCrc* p);

#ifdef __cplusplus
}
#endif

#endif//__FRMSCFCRC_H__