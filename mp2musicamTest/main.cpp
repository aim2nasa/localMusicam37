#include <iostream>
#include <assert.h>
#include "frmScfCrc.h"

size_t peepHeader(FILE *stream,unsigned char* pFrame)
{
	size_t nRead = fread(pFrame,sizeof(unsigned char),4,stream);	//read header only
	assert(nRead==4);
	fseek(stream,-4,SEEK_CUR);	//reset location
	return nRead;
}

using namespace std;

int main(int argc, char *argv[])
{
	if (argc!=3) {
		cout<<"Usage: mp2musicamTest <input.mp2> <output.mp2>"<<endl;
		return -1;
	}

	cout<<"Input file: "<<argv[1]<<endl;
	cout<<"Output file: "<<argv[2]<<endl;

	FILE *inpStream,*outStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,argv[1],"rb" )) !=0 ) {
		cout<<argv[1]<<" open failure"<<endl;
		return -1;
	}
	if( (err  = fopen_s( &outStream,argv[2],"wb" )) !=0 ) {
		cout<<argv[2]<<" open failure"<<endl;
		return -1;
	}

	frmScfCrc* pFrmScfCrc = fsc_instance();

	unsigned char frame[MAXBUF];
	peepHeader(inpStream,frame);
	fsc_init(pFrmScfCrc,frame);

	bool bLoop = true;
	int nFrameCount =0;
	while( bLoop && !feof( inpStream ) ) {
		size_t nRead = fread(frame,sizeof(unsigned char),pFrmScfCrc->nFrameSize,inpStream);
		if(nRead!=pFrmScfCrc->nFrameSize) { bLoop=false; continue; }

		int nRtn = fsc_applyScfCrc(pFrmScfCrc,frame);

		if(nRtn>0){
			assert(nRtn==pFrmScfCrc->nFrameSize);
			size_t nWrite = fwrite(frame,sizeof(unsigned char),pFrmScfCrc->nFrameSize,outStream);
			assert(nWrite==pFrmScfCrc->nFrameSize);
		}

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	fsc_delete(pFrmScfCrc);

	_fcloseall();
	cout<<"end of main"<<endl;
	return 0;
};