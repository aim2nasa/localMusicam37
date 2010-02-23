#include <iostream>
#include <assert.h>
#include <musicam.h>

#define INPUT_FILE			"..//contents//streamList_subCh(7).mp2"
#define OUTMP2_FILE			"..//output//new_streamList_subCh(7).mp2"
#define MAX_SAMPLE_SIZE		(1152)
#define MP2_BUF_SIZE		(16384)

static const int bitrateTable[2][15]={
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},		//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

void parseMp2Header(MP2_HEADER* pHeader,unsigned char* pBuffer)
{
	pHeader->sync		= (((unsigned short)pBuffer[0]&0xFF)<<4) | (((unsigned short)pBuffer[1]&0xF0)>>4);
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

static int getBitrate(int nId,int nBitrateIdx)
{
	assert(nId==0 || nId==1);
	assert(nBitrateIdx>0 && nBitrateIdx<15);
	return 1000*bitrateTable[nId][nBitrateIdx];	//kbps unit
}

using namespace std;

void main(int argc,int *argv[])
{
	mc* m = mc_init();

	//Decode
	FILE *inpStream,*mp2OutStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,INPUT_FILE,"rb" )) !=0 ) {
		cout<<INPUT_FILE<<" open failure"<<endl;
		return;
	}
	if( (err  = fopen_s( &mp2OutStream,OUTMP2_FILE,"wb" )) !=0 ) {
		cout<<OUTMP2_FILE<<" open failure"<<endl;
		return;
	}

	twolame_options* encopts = m->opt;

	TWOLAME_MPEG_version ver = TWOLAME_MPEG1;
	int nSrcNumCh = 2;
	int nSrcSampleFreq;
	(ver==TWOLAME_MPEG1)?nSrcSampleFreq=48000:nSrcSampleFreq=24000;
	int nDstBitrate = 384;

	twolame_set_version(encopts,TWOLAME_MPEG1);
	twolame_set_rawmode(encopts, 1);
	twolame_set_num_channels(encopts,nSrcNumCh);
	twolame_set_in_samplerate(encopts,nSrcSampleFreq);
	twolame_set_out_samplerate(encopts,nSrcSampleFreq);
	twolame_set_mode(encopts,TWOLAME_STEREO);
	twolame_set_bitrate(encopts,nDstBitrate);
	twolame_set_error_protection(encopts,TRUE);

	if(twolame_init_params(encopts)!=0) {
		cout<<"Error: configuring libtwolame encoder failed"<<endl;
		return;
	}
	twolame_print_config(encopts);

	unsigned char buffer[MAX_SAMPLE_SIZE];
	memset(buffer,0x0,sizeof(buffer));
	unsigned char mp2Buffer[MP2_BUF_SIZE];
	MP2_HEADER header;

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(unsigned char),4,inpStream);	//read header only
		parseMp2Header(&header,buffer);

		int nSrcBufferSize = 144*getBitrate(header.id,header.bitrateIdx)/((header.id)?48000:24000);
		nRead = fread(buffer+4,sizeof(unsigned char),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead) assert(nRead==(nSrcBufferSize-4));

		int nEncRtn = mc_encode(m,buffer,nSrcBufferSize,MAX_SAMPLE_SIZE,mp2Buffer,sizeof(mp2Buffer));
		if(nEncRtn<0) {
			cout<<"encode error:"<<mc_getLastError()<<endl;
			break;
		}
		size_t nWrite = fwrite(mp2Buffer,sizeof(unsigned char),nEncRtn,mp2OutStream);
		assert(nWrite==nEncRtn);

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	fclose(mp2OutStream);
	fclose(inpStream);

	mc_close(m);
	cout<<"end of main"<<endl;
};