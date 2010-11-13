#include <iostream>
#include <assert.h>
#include <musicam.h>

using namespace std;

void main(void)
{
	mc* m = mc_init();

	//Decode
	FILE *inpStream,*mp2OutStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,"..\\contents\\48KHz.pcm","rb" )) !=0 ) {
		assert(false);
	}
	if( (err  = fopen_s( &mp2OutStream,"..\\output\\48KHz_to_24KHz.mp2","wb" )) !=0 ) {
		assert(false);
	}

	unsigned char buffer[1152*4*2];
	unsigned char convBuffer[1152*4];
	memset(buffer,0x0,sizeof(buffer));
	unsigned char mp2Buffer[16384];

	int nFrameCount = 0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(buffer,sizeof(unsigned char),1152*4*2,inpStream);	//read header only

		if(nFrameCount==0) {
			if(mc_encodeOption(m->opt,24000,ISO_13818_3,128,TWOLAME_STEREO,1)!=0) {
				cout<<mc_getLastError();
				break;
			}
		}

		int nConv = mc_pcm48to24(buffer,convBuffer,(int)nRead,2);
		cout<<"mc_pcm48to24() return = "<<nConv<<endl;

		nFrameCount++;

		int nEncRtn = mc_pcm_mp2_encode(m,convBuffer,nConv,1152,mp2Buffer);
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