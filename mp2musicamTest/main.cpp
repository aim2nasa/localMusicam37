#include <iostream>
#include <assert.h>
#include "rBit.h"
#include "frmScfCrc.h"

#define INBUFF			1152	//144*384/48 max frame size

#define SBLIMIT			32
#define CRC8_POLYNOMIAL 0x1D








static const int bitalloc_tbl[3][32] = {
	{4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0},	//48KHz Table13
	{4, 4, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	//48KHz Table14
	{4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0},	//24KHz Table15
};

void update_CRCDAB (unsigned int data, unsigned int length, unsigned int *crc)
{
	unsigned int masking, carry;

	masking = 1 << length;

	while ((masking >>= 1)) {
		carry = *crc & 0x80;
		*crc <<= 1;
		if (!carry ^ !(data & masking))
			*crc ^= CRC8_POLYNOMIAL;
	}
	*crc &= 0xff;
}

void CRC_calcDAB (int nch,int sblimit,
				  unsigned int bit_alloc[2][SBLIMIT],
				  unsigned int scfsi[2][SBLIMIT],
				  unsigned int scalar[2][SBLIMIT][3], unsigned int *crc,int packed)
{
	int i, j, k;
	int nb_scalar;
	int f[5] = { 0, 4, 8, 16, 30 };
	int first, last;

	first = f[packed];
	last = f[packed + 1];
	if (last > sblimit)
		last = sblimit;

	nb_scalar = 0;
	*crc = 0x0;
	for (i = first; i < last; i++)
		for (k = 0; k < nch; k++)
			if (bit_alloc[k][i])	/* above jsbound, bit_alloc[0][i] == ba[1][i] */
				switch (scfsi[k][i]) {
					case 0:
						for (j = 0; j < 3; j++) {
							nb_scalar++;
							update_CRCDAB (scalar[k][i][j] >> 3, 3, crc);
						}
						break;
					case 1:
					case 3:
						nb_scalar += 2;
						update_CRCDAB (scalar[k][i][0] >> 3, 3, crc);
						update_CRCDAB (scalar[k][i][2] >> 3, 3, crc);
						break;
					case 2:
						nb_scalar++;
						update_CRCDAB (scalar[k][i][0] >> 3, 3, crc);
				}
}

void doBitAlloc(rBit* pRb,int bound,int sblimit,int nch,int nTable,unsigned int bit_alloc[2][32])
{
	memset(bit_alloc,0,sizeof(bit_alloc));

	for(int sb=0; sb<bound; sb++)
		for (int ch=0; ch<nch; ch++)
			bit_alloc[ch][sb] = rBit_getBits(pRb,bitalloc_tbl[nTable][sb]);
	for (int sb=bound; sb<sblimit; sb++)
		bit_alloc[0][sb] = bit_alloc[1][sb] = rBit_getBits(pRb,bitalloc_tbl[nTable][sb]);
}

void doScfsi(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32])
{
	memset(scfsi,0,sizeof(scfsi));

	for(int sb=0; sb<sblimit; sb++) {
		for (int ch=0; ch < nch; ch++)
			if (bit_alloc[ch][sb])
				scfsi[ch][sb] = rBit_getBits(pRb,2);
		if (pHeader->mode == MONO)
			scfsi[1][sb] = scfsi[0][sb];
	}
}

void doScaleFactor(rBit* pRb,const MP2_HEADER* pHeader,int sblimit,int nch,unsigned int bit_alloc[2][32],unsigned int scfsi[2][32],unsigned int scalefactor[2][32][3])
{
	memset(scalefactor,0,sizeof(scalefactor));

	for (int sb=0; sb<sblimit; sb++) {
		for (int ch=0; ch<nch; ch++)
			if (bit_alloc[ch][sb]) {
				switch (scfsi[ch][sb]) {
					case 0: scalefactor[ch][sb][0] = rBit_getBits(pRb,6);
							scalefactor[ch][sb][1] = rBit_getBits(pRb,6);
							scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
							break;
					case 1: scalefactor[ch][sb][0] =
							scalefactor[ch][sb][1] = rBit_getBits(pRb,6);
							scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
							break;
					case 2: scalefactor[ch][sb][0] =
							scalefactor[ch][sb][1] =
							scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
							break;
					case 3: scalefactor[ch][sb][0] = rBit_getBits(pRb,6);
							scalefactor[ch][sb][1] =
							scalefactor[ch][sb][2] = rBit_getBits(pRb,6);
							break;
				}
			}
			if(pHeader->mode == MONO)
				for(int part=0; part<3; part++)
					scalefactor[1][sb][part] = scalefactor[0][sb][part];
	}
}

int getDabExt(int bit_rate,int num_channel)
{
	return ((bit_rate/num_channel) >= 56)? 4:2;
}

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

	unsigned char frame[INBUFF];
	unsigned char outFrame[INBUFF];
	size_t size = 0;

	peepHeader(inpStream,frame);
	fsc_init(pFrmScfCrc,frame);

	bool bLoop = true;
	int nFrameCount =0;
	rBit* pRb = rBit_instance();
	while( bLoop && !feof( inpStream ) ) {
		size_t nRead = fread(frame,sizeof(unsigned char),pFrmScfCrc->nFrameSize,inpStream);
		if(nRead!=pFrmScfCrc->nFrameSize) { bLoop=false; continue; }

		// set up the bitstream reader
		rBit_init(pRb,frame+2);
		rBit_getBits(pRb,16);
		if(fsc_header(pFrmScfCrc)->protect == 0) rBit_getBits(pRb,16);

		//bit allocation
		doBitAlloc(pRb,pFrmScfCrc->bound,pFrmScfCrc->sblimit,pFrmScfCrc->nch,pFrmScfCrc->nTable,pFrmScfCrc->bit_alloc);

		//scfsi
		doScfsi(pRb,&pFrmScfCrc->header,pFrmScfCrc->sblimit,pFrmScfCrc->nch,pFrmScfCrc->bit_alloc,pFrmScfCrc->scfsi);

		//scale factors
		doScaleFactor(pRb,&pFrmScfCrc->header,pFrmScfCrc->sblimit,pFrmScfCrc->nch,pFrmScfCrc->bit_alloc,pFrmScfCrc->scfsi,pFrmScfCrc->scalefactor);

		pFrmScfCrc->dabExtension = getDabExt(pFrmScfCrc->bit_rate,pFrmScfCrc->nch);

		memset(pFrmScfCrc->scfCrc,0,sizeof(pFrmScfCrc->scfCrc));
		for(int i = pFrmScfCrc->dabExtension-1;i>=0;i--)
			CRC_calcDAB(pFrmScfCrc->nch,pFrmScfCrc->sblimit,pFrmScfCrc->bit_alloc,pFrmScfCrc->scfsi,pFrmScfCrc->scalefactor,&pFrmScfCrc->scfCrc[i],i);

		if(pFrmScfCrc->bInitFrame){
			pFrmScfCrc->bInitFrame=0;
		}else{
			if(pFrmScfCrc->dabExtension==4) {
				outFrame[pFrmScfCrc->nFrameSize-6]=pFrmScfCrc->scfCrc[3];
				outFrame[pFrmScfCrc->nFrameSize-5]=pFrmScfCrc->scfCrc[2];
			}
			outFrame[pFrmScfCrc->nFrameSize-4]=pFrmScfCrc->scfCrc[1];
			outFrame[pFrmScfCrc->nFrameSize-3]=pFrmScfCrc->scfCrc[0];

			size_t nWrite = fwrite(outFrame,sizeof(unsigned char),pFrmScfCrc->nFrameSize,outStream);
			assert(nWrite==pFrmScfCrc->nFrameSize);
		}
		memcpy(outFrame,frame,pFrmScfCrc->nFrameSize);

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	rBit_delete(pRb);
	fsc_delete(pFrmScfCrc);

	_fcloseall();
	cout<<"end of main"<<endl;
	return 0;
};