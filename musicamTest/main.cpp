#include <iostream>
#include <assert.h>
#include <musicam.h>

#define MAX_SAMPLE_SIZE		(1152)
#define MP2_BUF_SIZE		(16384)

using namespace std;

void main(int argc, char *argv[])
{
	if (argc<6) {
		cout<<"Usage: musicamTest <input.mp2> <output.mp2> <output id> <output bitrate> <output mode>"<<endl;
		return;
	}

	cout<<"Input file: "<<argv[1]<<endl;
	cout<<"Output file: "<<argv[2]<<endl;
	cout<<"Desired Output Bitrate:"<<argv[4]<<"kbps"<<endl;

	mc* m = mc_init();

	//Decode
	FILE *inpStream,*mp2OutStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,argv[1],"rb" )) !=0 ) {
		cout<<argv[1]<<" open failure"<<endl;
		return;
	}
	if( (err  = fopen_s( &mp2OutStream,argv[2],"wb" )) !=0 ) {
		cout<<argv[2]<<" open failure"<<endl;
		return;
	}

	unsigned char buffer[MAX_SAMPLE_SIZE];
	memset(buffer,0x0,sizeof(buffer));
	unsigned char mp2Buffer[MP2_BUF_SIZE];
	MP2_HEADER header;

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(unsigned char),4,inpStream);	//read header only
		mc_parseMp2Header(&header,buffer);

		if(nFrameCount==0) {
			if(mc_encodeOption(m->opt,(HEADER_ID)atoi(argv[3]),atoi(argv[4]),(TWOLAME_MPEG_mode)atoi(argv[5]),1)!=0) {
				cout<<mc_getLastError();
				break;
			}
		}

		int nSrcBufferSize = 144*mc_getBitrate(header.id,header.bitrateIdx)/mc_getSamplingFrequency(header.id);
		nRead = fread(buffer+4,sizeof(unsigned char),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead) assert(nRead==(nSrcBufferSize-4));

		nFrameCount++;
		int nEncRtn = mc_mp2_mp2_encode(m,buffer,nSrcBufferSize,MAX_SAMPLE_SIZE,mp2Buffer);
		if(nEncRtn<0) {
			cout<<"encode error:"<<mc_getLastError()<<endl;
			continue;
		}
		if(nEncRtn>0) {
			size_t nWrite = fwrite(mp2Buffer,sizeof(unsigned char),nEncRtn,mp2OutStream);
			assert(nWrite==nEncRtn);
		}

		fprintf(stderr, "[%04i", nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);

	}
	fclose(mp2OutStream);
	fclose(inpStream);

	mc_close(m);
	cout<<"end of main"<<endl;
};