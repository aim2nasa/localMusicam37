#include <iostream>
#include <assert.h>
#include <kjmp2.h>
#include <twolame.h>
#include <mp2Changer.h>

#define INPUT_FILE			"..//contents//streamList_subCh(7).mp2"
#define OUTMP2_FILE			"..//output//new_streamList_subCh(7).mp2"
#define MAX_SAMPLE_SIZE		(1152)
#define MP2_BUF_SIZE		(16384)

using namespace std;

void main(void)
{
	mp2Changer* mp2c = mp2c_init();

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

	twolame_options* encopts = mp2c->opt;

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

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(unsigned char),4,inpStream);	//read header only
		mp2c_parseMp2Header(mp2c,buffer);

		int nSrcBufferSize = 144*mp2c_bitrate(mp2c)/twolame_get_in_samplerate(encopts);
		nRead = fread(buffer+4,sizeof(unsigned char),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead) assert(nRead==(nSrcBufferSize-4));

		int nEncRtn = mp2c_encode(mp2c,buffer,MAX_SAMPLE_SIZE,mp2Buffer,sizeof(mp2Buffer));
		if(nEncRtn<0) {
			cout<<"encode error:"<<mp2c_getLastError()<<endl;
			break;
		}
		size_t nWrite = fwrite(mp2Buffer,sizeof(unsigned char),nEncRtn,mp2OutStream);
		assert(nWrite==nEncRtn);

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	mp2c_close(mp2c);

	fclose(mp2OutStream);
	fclose(inpStream);
	cout<<"end of main"<<endl;
};