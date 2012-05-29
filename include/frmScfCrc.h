#ifndef __FRMSCFCRC_H__
#define __FRMSCFCRC_H__

#include "mp2Header.h"
#include "rbit.h"
#include <stdio.h>

#define MAXBUF			1152	//144*384/48 max frame size
#define SBLIMIT			32
#define MAXCH			2
#define MAXPART			3
#define MAXCRC			4
#define CRC8_POLYNOMIAL 0x1D

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
		unsigned int bit_alloc[MAXCH][SBLIMIT];
		unsigned int scfsi[MAXCH][32];
		unsigned int scalefactor[MAXCH][SBLIMIT][MAXPART];
		unsigned int scfCrc[MAXCRC];
		unsigned char inpFrame[MAXBUF];
		unsigned char outFrame[MAXBUF];
		rBit* pRb;
	} frmScfCrc;

	DLL_EXPORT frmScfCrc* fsc_instance();
	DLL_EXPORT void fsc_delete(frmScfCrc* p);
	DLL_EXPORT void fsc_init(frmScfCrc* p,unsigned char* pFrame);
	DLL_EXPORT size_t fsc_peepHeader(FILE *stream,unsigned char* pFrame);
	DLL_EXPORT const MP2_HEADER* fsc_header(frmScfCrc* p);
	DLL_EXPORT int fsc_frameSize(const MP2_HEADER* pHeader);
	DLL_EXPORT int fsc_applyScfCrc(frmScfCrc* p,unsigned char* pFrame);
	DLL_EXPORT int fsc_getDabExt(int bit_rate,int num_channel);

#ifdef __cplusplus
}
#endif

#endif//__FRMSCFCRC_H__