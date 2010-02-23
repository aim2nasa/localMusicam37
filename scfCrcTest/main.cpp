#include <iostream>
#include <assert.h>
#include <scfCrc.h>

#define INBUFF  16384
#define OUTBUFF 32768 

using namespace std;

#pragma pack(push, 1)
typedef struct{
	unsigned short sync:12;
	unsigned short id:1;
	unsigned short layer:2;
	unsigned short protect:1;
	unsigned short bitrateIdx:4;
	unsigned short sampling:2;
	unsigned short padding:1;
	unsigned short priv:1;
	unsigned short mode:2;
	unsigned short mode_ext:2;
	unsigned short copyright:1;
	unsigned short orignal:1;
	unsigned short emphasis:2;
}MP2_HEADER;
#pragma pack(pop)

static const int bitrateTable[2][15]={
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},			//for 24KHz
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

int main(int argc, char *argv[])
{
	if (argc!=3) {
		cout<<"Usage: scfCrcTest <input.mp2> <output.mp2>"<<endl;
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

	unsigned char inpBuffer[INBUFF];
	unsigned char outBuffer[OUTBUFF];
	size_t size = 0;
	MP2_HEADER header;
	unsigned short sync;

	int ptr = 0;
	int nFrameCount = 0;
	scfCrc* sc = scfCrc_init();
	while( !feof( inpStream ) ) {
		do{
			fread(inpBuffer,sizeof(unsigned char),2,inpStream);
			sync = (((unsigned short)inpBuffer[0]&0xFF)<<8) | ((unsigned short)inpBuffer[1]&0xF0);
		}while(sync != 0xFFF0);
		ptr+=2;

		size_t nRead = fread(inpBuffer+2,sizeof(unsigned char),2,inpStream);
		parseMp2Header(&header,inpBuffer);

		assert(inpBuffer[0]==0xff);
		assert((inpBuffer[1]&0xf0)==0xf0);

		int nSrcBufferSize = 144*getBitrate(header.id,header.bitrateIdx)/((header.id)?48000:24000);
		nRead = fread(inpBuffer+4,sizeof(unsigned char),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead&&(nRead!=(nSrcBufferSize-4))) break;

		int nRtn = scfCrc_apply(sc,inpBuffer,nSrcBufferSize,outBuffer,nSrcBufferSize);
		if(nRtn>0) fwrite(outBuffer,sizeof(unsigned char),nSrcBufferSize,outStream);

		cout<<"frameCount="<<nFrameCount<<endl;
		nFrameCount++;
	}
	scfCrc_close(sc);

	fclose(inpStream);
	fclose(outStream);
	cout<<"end of main"<<endl;

	return 0;
};