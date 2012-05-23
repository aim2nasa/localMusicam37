/******************************************************************************
** RBIT -- Bitwise Read Utility		   										 **
** cf. window size is set as 2 bytes										 **
******************************************************************************/
#ifndef __RBIT_H__
#define __RBIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DLL_EXPORT
#define DLL_EXPORT __declspec(dllexport)
#endif

	typedef struct _rBit_context {
		int bit_window;
		int bits_in_window;
		const unsigned char *frame_pos;
	} rBit;

	DLL_EXPORT rBit* rBit_instance();
	DLL_EXPORT void rBit_delete(rBit* p);
	DLL_EXPORT void rBit_init(rBit* p,unsigned char *pFrame);
	DLL_EXPORT int rBit_getBits(rBit* p,int bit_count);

#ifdef __cplusplus
}
#endif

#endif//__RBIT_H__
