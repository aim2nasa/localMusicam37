#include <iostream>
#include <stdio.h>
#include <assert.h>
#include "frmScfCrc.h"

using namespace std;

int getFrameSize(FILE *stream)
{
	unsigned char frame[4];
	fsc_peepHeader(stream,frame);

	frmScfCrc* pFrmScfCrc = fsc_instance();
	fsc_init(pFrmScfCrc,frame);
	int nSize = fsc_frameSize(fsc_header(pFrmScfCrc));
	fsc_delete(pFrmScfCrc);
	return nSize;
}

int main(int argc, char *argv[])
{
	if(argc<3) {
		cout<<"usage: scfCrcComp [filename1] [filename2]"<<endl;
		exit(-1);
	}

	FILE *stream[2];
	errno_t err;
	int nFrameSize[2]={0};
	for(int i=0;i<2;i++){
		if( (err  = fopen_s( &stream[i],argv[i+1],"rb" )) !=0 ) {
			cout<<"file open error "<<argv[i+1]<<endl;
			exit(-1);
		}
		nFrameSize[i] = getFrameSize(stream[i]);
	}

	if(nFrameSize[0]!=nFrameSize[1]) {
		cout<<"Error,Frame size must be equal : file1("<<nFrameSize[0]<<"),file2("<<nFrameSize[1]<<")"<<endl;
		exit(-1);
	}

	unsigned char** pBuffers = new unsigned char*[2];
	for(int i=0;i<2;i++) pBuffers[i] = new unsigned char[nFrameSize[i]];

	size_t nRead[2];
	bool bloop = true;
	int iFrame = 0;
	while(bloop){
		for(int i=0;i<2;i++) { nRead[i] = fread( pBuffers[i],1,nFrameSize[i],stream[i]); }

		if(nRead[0]!=nRead[1]){
			bloop=false;
			cout<<"("<<iFrame<<") Read size differ : file1("<<nRead[0]<<"),file2("<<nRead[1]<<")"<<endl;
			break;
		}

		if(nRead[0]!=nFrameSize[0]) {
			bloop=false; break;
		}
		assert(nFrameSize[0]==nFrameSize[1]);

		frmScfCrc* pFrmScfCrc = fsc_instance();
		fsc_init(pFrmScfCrc,pBuffers[0]);
		assert(nFrameSize[0]==fsc_frameSize(fsc_header(pFrmScfCrc)));

		int nFs = nFrameSize[0];
		int nDabExt = fsc_getDabExt(pFrmScfCrc->bit_rate,pFrmScfCrc->nch);

		//Frame body
		if(memcmp(pBuffers[0],pBuffers[1],nFs-2-nDabExt)!=0){
			printf("\n(%d) Body\n",iFrame);
		}

		//ScfCRC
		if(memcmp(pBuffers[0]+nFs-2-nDabExt,pBuffers[1]+nFs-2-nDabExt,nDabExt)!=0){
			if(nDabExt==4){
				printf("\n(%d) ScfCRC [%02x %02x %02x %02x] [%02x %02x %02x %02x]\n",
					iFrame,
					*(pBuffers[0]+nFs-6),*(pBuffers[0]+nFs-5),*(pBuffers[0]+nFs-4),*(pBuffers[0]+nFs-3),
					*(pBuffers[1]+nFs-6),*(pBuffers[1]+nFs-5),*(pBuffers[1]+nFs-4),*(pBuffers[1]+nFs-3) );
			}else{
				assert(nDabExt==2);
				printf("\n(%d) ScfCRC [%02x %02x] [%02x %02x]\n",
					iFrame,
					*(pBuffers[0]+nFs-4),*(pBuffers[0]+nFs-3),
					*(pBuffers[1]+nFs-4),*(pBuffers[1]+nFs-3) );
			}
		}

		//F-PAD
		if(memcmp(pBuffers[0]+nFs-2,pBuffers[1]+nFs-2,2)!=0){
			printf("\n(%d) F-PAD [%02x %02x] [%02x %02x]\n",
				iFrame,
				*(pBuffers[0]+nFs-2),
				*(pBuffers[0]+nFs-1),
				*(pBuffers[1]+nFs-2),
				*(pBuffers[1]+nFs-1)
				);
		}

		fsc_delete(pFrmScfCrc);
		iFrame++;
		cout<<".";
	}
	cout<<endl;

	for(int i=0;i<2;i++) delete [] pBuffers[i];

	_fcloseall();
	cout<<"end of main"<<endl;
	return 0;
};