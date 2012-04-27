#include <iostream>
#include <assert.h>
#include <musicam.h>

#define MAX_SAMPLE_SIZE		(1152*4)
#define MP2_BUF_SIZE		(16384)

using namespace std;

void main(int argc, char *argv[])
{
	if (argc<6) {
		cout<<"Usage: pcmEncTest <input.pcm> <output.mp2> <output id> <output bitrate> <output mode>"<<endl;
		cout<<"		   - output id"<<endl;
		cout<<"				0 : ISO/IEC 13818-3 for 24KHz sample freq"<<endl;
		cout<<"				1 : ISO/IEC 11172-3 for 48KHz sample freq"<<endl;
		cout<<"		   - output bitrate(kbps)"<<endl;
		cout<<"				48kHz(32,48,56,64,80,96,112,128,160,192,224,256,320,384)"<<endl;
		cout<<"				24KHz(8,16,24,32,40,48,56,64,80,96,112,128,144,160)"<<endl;
		cout<<"		   - output mode"<<endl;
		cout<<"				0(Stereo),1(JStereo),2(DualChannel),3(Mono)"<<endl;
		cout<<" ex> 48000.pcm 48000.mp2 1 128 0"<<endl;
		cout<<" ex> 24000.pcm 24000.mp2 0 128 0"<<endl;
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

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(unsigned char),1152*4,inpStream);

		if(nFrameCount==0) {
			if(mc_encodeOption(m->opt,atoi(argv[3])==0?24000:48000,(HEADER_ID)atoi(argv[3]),atoi(argv[4]),(TWOLAME_MPEG_mode)atoi(argv[5]),1)!=0) {
				cout<<mc_getLastError();
				break;
			}
		}

		nFrameCount++;
		int nEncRtn = mc_pcm_mp2_encode(m,buffer,1152*4,1152,mp2Buffer);
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