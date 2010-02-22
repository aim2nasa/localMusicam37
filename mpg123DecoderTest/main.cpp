#include <iostream>
#include <assert.h>
#include <mpg123.h>

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

void cleanup(mpg123_handle* m=NULL)
{
	if(m) mpg123_delete(m);
	mpg123_exit();
}

int main(int argc, char *argv[])
{
	if (argc!=3) {
		cout<<"Usage: mpg123DecoderTest <input.mp2> <output.pcm>"<<endl;
		return -1;
	}

	cout<<"Input file: "<<argv[1]<<endl;
	cout<<"Output file: "<<argv[2]<<endl;

	if(mpg123_init()!=MPG123_OK) { cout<<"mpg123_init() failed"; cleanup(); return -1; }

	FILE *inpStream,*outStream;
	errno_t err;
	if( (err  = fopen_s( &inpStream,argv[1],"rb" )) !=0 ) {
		cout<<argv[1]<<" open failure"<<endl;
		cleanup();
		return -1;
	}
	if( (err  = fopen_s( &outStream,argv[2],"wb" )) !=0 ) {
		cout<<argv[2]<<" open failure"<<endl;
		return -1;
	}

	int ret=0;
	mpg123_handle *m = mpg123_new(NULL,&ret);
	if(m==NULL) {
		cout<<"mpg123_new() failed"<<endl;
		cleanup();
		return -1;
	}
	cout<<"Current decoder:"<<mpg123_current_decoder(m)<<endl;

	if(mpg123_open_feed(m)!=MPG123_OK) {
		cout<<"mpg123_open_feed() failed"<<endl;
		cleanup(m);
		return -1;
	}

	unsigned char mp2buffer[INBUFF];
	unsigned char pcmBuffer[OUTBUFF];
	size_t size = 0;
	MP2_HEADER header;

	int nFrameCount =0;
	while( !feof( inpStream ) ) {
		size_t nRead = fread(mp2buffer,sizeof(unsigned char),4,inpStream);	//read header only
		parseMp2Header(&header,mp2buffer);

		int nSrcBufferSize = 144*getBitrate(header.id,header.bitrateIdx)/((header.id)?48000:24000);
		nRead = fread(mp2buffer+4,sizeof(unsigned char),nSrcBufferSize-4,inpStream);	//read rest of the frame
		if(nRead) assert(nRead==(nSrcBufferSize-4));

		size_t nWrite = 0;
		int ret = mpg123_decode(m,mp2buffer,nSrcBufferSize,pcmBuffer,OUTBUFF,&size);                                                      
		if(ret == MPG123_NEW_FORMAT)                                                                           
		{                                                                                                      
			long rate;                                                                                     
			int channels, enc;                                                                             
			mpg123_getformat(m, &rate, &channels, &enc);                                                   
			fprintf(stderr, "New format: %li Hz, %i channels, encoding value %i\n", rate, channels, enc);  
		}         
		nWrite = fwrite(pcmBuffer,sizeof(unsigned char),size,outStream);

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	fclose(inpStream);
	fclose(outStream);

	cleanup(m);
	cout<<"end of main"<<endl;
	return 0;
};