/*
*	MUSICAM: a MUSICAM library for DAB
*
*	Copyright (C) 2010 Seung-Chul,Kwak
*
*/
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
	/** MPEG2 Audio frame header */
	typedef struct{
		unsigned short sync:12;			/**< syncword */
		unsigned short id:1;			/**< identifier */
		unsigned short layer:2;			/**< layer */
		unsigned short protect:1;		/**< protection bit */
		unsigned short bitrateIdx:4;	/**< bit rate index */
		unsigned short sampling:2;		/**< sampling frequency */
		unsigned short padding:1;		/**< padding bit */
		unsigned short priv:1;			/**< private bit */
		unsigned short mode:2;			/**< mode */
		unsigned short mode_ext:2;		/**< mode extension */
		unsigned short copyright:1;		/**< copy right */
		unsigned short orignal:1;		/**< original/copy */
		unsigned short emphasis:2;		/**< emphasis */
	}MP2_HEADER;
#pragma pack(pop)

	/** MUSICAM library context */
	typedef struct _musicam_context {
		MP2_HEADER*		 hdr;			/**< MPEG2 Audio frame header */
		mpg123_handle*	 mpg;			/**< pointer to mpg123 library */
		twolame_options* opt;			/**< pointer to twolame option */
		scfCrc*			 sc;			/**< pointer to scale factor crc library */
	} mc;

	/** ID of MPEG2 Audio frame header */
	typedef enum {
		ISO_13818_3 = 0,	/**< ISO/IEC 13818-3 */
		ISO_11172_3 = 1		/**< ISO/IEC 11172-3 */
	} HEADER_ID;

	/** Initialize MUSICAM library.
	*
	*  \return					pointer to MUSICAM library
	*/
	MUSICAM_DLL_EXPORT mc* mc_init();

	/** Uninitialize and close MUSICAM library
	*
	*	\param m				pointer to MUSICAM library
	*	\return					void
	*
	*/
	MUSICAM_DLL_EXPORT void mc_close(mc* m);

	/** encode input pcm stream to mp2 stream
	*	
	*	\param m				pointer to musicam library
	*	\param pcmFrame			pointer to pcm stream
	*	\param pcmFrameSize		size of pcm stream
	*	\param numSampleCh		number of samples per channel
	*	\param outMucframe		pointer to encoded output buffer
	*	\return					0 successful otherwise -1
	*/
	MUSICAM_DLL_EXPORT int mc_pcm_mp2_encode(mc* m,const unsigned char* pcmFrame,int pcmFrameSize,int numSampleCh,unsigned char* outMucframe);

	/** transcode from given mp2 stream to another mp2 stream
	*	
	*	\param m				pointer to musicam library
	*	\param inMp2frame		pointer to mp2 stream
	*	\param inMp2frameSize	size of mp2 stream
	*	\param numSampleCh		number of samples per channel
	*	\param outMucframe		pointer to encoded output buffer
	*	\return					0 successful otherwise -1
	*/
	MUSICAM_DLL_EXPORT int mc_mp2_mp2_encode(mc* m,const unsigned char* inMp2frame,int inMp2frameSize,int numSampleCh,unsigned char* outMucframe);

	/** get last error message
	*	
	*	\return					pointer to error message buffer
	*/
	MUSICAM_DLL_EXPORT const char* mc_getLastError();

	/** mp2 header parser
	*	
	*	\param pHeader			pointer to mp2 header
	*	\param pBuffer			pointer to mp2 stream
	*	\return					void
	*/
	MUSICAM_DLL_EXPORT void mc_parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer);

	/** get sampling frequency from given identifier
	*	
	*	\param id				identifier in mp2 header
	*	\return					sampling frequency in Hz, -1 if not successful
	*/
	MUSICAM_DLL_EXPORT int mc_getSamplingFrequency(int id);

	/** get bitrate
	*	
	*	\param nId				identifier in mp2 header
	*	\param nBitrateIdx		bitrate index (1~14)
	*	\return					bitrate in bps, -1 if not successful
	*/
	MUSICAM_DLL_EXPORT int mc_getBitrate(int nId,int nBitrateIdx);

	/** set mp2 encode option
	*	
	*	\param encopts			pointer to twolame encode option
	*	\param id				identifier in mp2 header (ISO_13818_3,ISO_11172_3)
	*	\param nDstBitrate		desired bitrate of encoded birstream (kbps)
	*	\param mpegMode			mode (streo,joint stereo,dual channel,mono)
	*	\param verbose			verbosity control
	*	\return					0 successful otherwise -1
	*/
	MUSICAM_DLL_EXPORT int mc_encodeOption(twolame_options* encopts,HEADER_ID id,int nDstBitrate,TWOLAME_MPEG_mode mpegMode,int verbose);

#ifdef __cplusplus
}
#endif

#endif//__MUSICAM_H__
