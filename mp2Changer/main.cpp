#include <iostream>
#include <assert.h>
#include <kjmp2.h>
#include <twolame.h>

#define INPUT_FILE			"..//contents//streamList_subCh(7).mp2"
#define OUTPCM_FILE			"..//output//streamList_subCh(7).pcm"
#define OUTMP2_FILE			"..//output//new_streamList_subCh(7).mp2"
#define MAX_SAMPLE_SIZE		(1152)
#define MP2_BUF_SIZE		(16384)
#define PCM_BUF_SIZE		(1152*4)

typedef unsigned char		byte;
typedef unsigned __int64	u64;
typedef unsigned int		u32;
typedef unsigned short		u16;
typedef unsigned char		u8;
typedef unsigned int		Bool;
typedef __int64				s64;
typedef int					s32;
typedef short				s16;
typedef char				s8;
typedef float				Float;
typedef double				Double;
typedef long				Long;

#pragma pack(push, 1)
typedef struct{
	u16 sync:12;
	u16 id:1;
	u16 layer:2;
	u16 protect:1;
	u16 bitrateIdx:4;
	u16 sampling:2;
	u16 padding:1;
	u16 priv:1;						
	u16 mode:2;
	u16 mode_ext:2;
	u16 copyright:1;
	u16 orignal:1;
	u16 emphasis:2;
}MP2_HEADER;
#pragma pack(pop)

using namespace std;

static char errMsg[256];
static int bitrateTable[2][15]={
	{8,16,24,32,40,48,56,64,80,96,112,128,144,160},			//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

twolame_options* init(kjmp2_context_t* mp2);
void close(twolame_options* opt);
void parse_Mp2Header(MP2_HEADER* pHeader,byte* pBuffer);
int bitrate(int nId,int nBitrateIdx);
int encode(kjmp2_context_t* mp2,const unsigned char* frame,
		   twolame_options* encopts,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize);

void main(void)
{
	kjmp2_context_t mp2;

	//Decode
	FILE *inpStream,*pcmStream,*mp2OutStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,INPUT_FILE,"rb" )) !=0 ) {
		cout<<INPUT_FILE<<" open failure"<<endl;
		return;
	}
	if( (err  = fopen_s( &pcmStream,OUTPCM_FILE,"wb" )) !=0 ) {
		cout<<OUTPCM_FILE<<" open failure"<<endl;
		return;
	}
	if( (err  = fopen_s( &mp2OutStream,OUTMP2_FILE,"wb" )) !=0 ) {
		cout<<OUTMP2_FILE<<" open failure"<<endl;
		return;
	}

	twolame_options* encopts = init(&mp2);
	if(encopts==NULL) {
		cout<<errMsg<<endl;
		return;
	}

	int nSrcNumCh = 2;
	int nSrcSampleFreq = 48000;
	int nDstBitrate = 384000;

	twolame_set_rawmode(encopts, 1);
	twolame_set_num_channels(encopts,nSrcNumCh);
	twolame_set_in_samplerate(encopts,nSrcSampleFreq);
	twolame_set_out_samplerate(encopts,nSrcSampleFreq);
	twolame_set_mode(encopts,TWOLAME_STEREO);
	twolame_set_bitrate(encopts,nDstBitrate/1000);
	twolame_set_error_protection(encopts,TRUE);

	if(twolame_init_params(encopts)!=0) {
		cout<<"Error: configuring libtwolame encoder failed"<<endl;
		return;
	}
	twolame_print_config(encopts);

	byte buffer[MAX_SAMPLE_SIZE];
	memset(buffer,0x0,sizeof(buffer));
	//short pcm[PCM_BUF_SIZE];
	byte mp2Buffer[MP2_BUF_SIZE];
	MP2_HEADER header;

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(byte),4,inpStream);	//read header only
		parse_Mp2Header(&header,buffer);
		int nSrcBufferSize = 144*bitrate(header.id,header.bitrateIdx)/nSrcSampleFreq;
		nRead = fread(buffer+4,sizeof(byte),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead) assert(nRead==(nSrcBufferSize-4));

		//unsigned long nRtn = kjmp2_decode_frame(&mp2,buffer,pcm);
		//size_t nWrite = fwrite(pcm,sizeof(byte),PCM_BUF_SIZE,pcmStream);
		//assert(nWrite==PCM_BUF_SIZE);

		//int nSize = twolame_encode_buffer_interleaved(encopts,pcm,MAX_SAMPLE_SIZE,mp2Buffer,MP2_BUF_SIZE);
		//nWrite = fwrite(mp2Buffer,sizeof(byte),nSize,mp2OutStream);
		//assert(nWrite==nSize);

		int nEncRtn = encode(&mp2,buffer,encopts,MAX_SAMPLE_SIZE,mp2Buffer,sizeof(mp2Buffer));
		if(nEncRtn<0) {
			cout<<"encode error:"<<errMsg<<endl;
			break;
		}
		int nWrite = fwrite(mp2Buffer,sizeof(byte),nEncRtn,mp2OutStream);
		assert(nWrite==nEncRtn);

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	close(encopts);

	fclose(mp2OutStream);
	fclose(pcmStream);
	fclose(inpStream);
	cout<<"end of main"<<endl;
};

twolame_options* init(kjmp2_context_t* mp2)
{
	kjmp2_init(mp2);

	twolame_options* encopts = twolame_init();
	if(encopts==NULL) {
		sprintf_s(errMsg,sizeof(errMsg),"initializing libtwolame encoder failed");
	}else{
		memset(errMsg,0,sizeof(errMsg));
	}
	return encopts;
}

void close(twolame_options* opt)
{
	twolame_close(&opt);
}

void parse_Mp2Header(MP2_HEADER* pHeader,byte* pBuffer)
{
	pHeader->sync		= (((u16)pBuffer[0]&0xFF)<<4) | (((u16)pBuffer[1]&0xF0)>>4);
	pHeader->id			= (pBuffer[1]&0x08)>>3;
	pHeader->layer		= (pBuffer[1]&0x06)>>1;
	pHeader->protect	= (pBuffer[1]&0x01);
	pHeader->bitrateIdx	= (pBuffer[2]&0xF0)>>4;
	pHeader->sampling	= (pBuffer[2]&0x0C)>>2;
	pHeader->padding	= (pBuffer[2]&0x02)>>1;
	pHeader->priv		= (pBuffer[2]&0x01);
	pHeader->mode		= (pBuffer[3]&0xC0)>>6;
	pHeader->mode_ext	= (pBuffer[3]&0x30)>>4;
	pHeader->copyright	= (pBuffer[3]&0x08)>>3;
	pHeader->orignal	= (pBuffer[3]&0x04)>>2;
	pHeader->emphasis	= (pBuffer[3]&0x03);
}

int bitrate(int nId,int nBitrateIdx)
{
	assert(nId==0 || nId==1);
	assert(nBitrateIdx>0 && nBitrateIdx<15);
	return 1000*bitrateTable[nId][nBitrateIdx];	//kbps unit
}

int encode(kjmp2_context_t* mp2,const unsigned char* frame,
		   twolame_options* encopts,int numSampleCh,unsigned char* mp2Buffer,int mp2BufferSize)
{
	memset(errMsg,0,sizeof(errMsg));
	short pcm[PCM_BUF_SIZE];

	unsigned long nRtn = kjmp2_decode_frame(mp2,frame,pcm);
	if(nRtn==0) {
		sprintf_s(errMsg,sizeof(errMsg),"kjmp2_decode_frame() return 0");
		return nRtn;
	}

	int nSize = twolame_encode_buffer_interleaved(encopts,pcm,numSampleCh,mp2Buffer,mp2BufferSize);
	if(nSize<0) sprintf_s(errMsg,sizeof(errMsg),"twolame_encode_buffer_interleaved() return %d",nSize);
	return nSize;
}