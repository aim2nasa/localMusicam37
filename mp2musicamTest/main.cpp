#include <iostream>
#include <assert.h>

#define INBUFF			1152	//144*384/48 max frame size

#define ID_ISO13818		0
#define ID_ISO11172		1
#define STEREO			0
#define JOINT_STEREO	1
#define DUAL_CHANNEL	2
#define MONO			3

#define SBLIMIT			32
#define CRC8_POLYNOMIAL 0x1D

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
	{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},		//for 24KHz
	{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384}	//for 48KHz
};

unsigned char sblimit_tbl[3]={ 27,8,30 };

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

static int frameSize(const MP2_HEADER* pHeader)
{
	return 144*getBitrate(pHeader->id,pHeader->bitrateIdx)/((pHeader->id)?48000:24000);
}

static int bit_window;
static int bits_in_window;
static const unsigned char *frame_pos;

#define show_bits(bit_count) (bit_window >> (24 - (bit_count)))

static int get_bits(int bit_count) {
	int result = show_bits(bit_count);
	bit_window = (bit_window << bit_count) & 0xFFFFFF;
	bits_in_window -= bit_count;
	while (bits_in_window < 16) {
		bit_window |= (*frame_pos++) << (16 - bits_in_window);
		bits_in_window += 8;
	}
	return result;
}

int getBitAllocTable(int nBitrateCh,int nSampleFreq)
{
	if(nSampleFreq==24) {
		return 2;		//Table 15
	}else if(nSampleFreq==48) {
		if(nBitrateCh==56||nBitrateCh==64||nBitrateCh==80||nBitrateCh==96||nBitrateCh==112||
			nBitrateCh==128||nBitrateCh==160||nBitrateCh==192) 
			return 0;	//Table 13
		else if(nBitrateCh==32||nBitrateCh==48) 
			return 1;	//Table14
	}
	return -1;
}

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

	unsigned char frame[INBUFF];
	unsigned char outFrame[INBUFF];
	size_t size = 0;
	MP2_HEADER header;

	bool bLoop = true;
	int nFrameCount =0;
	bool bInitFrame = true;
	while( bLoop && !feof( inpStream ) ) {
		size_t nRead = fread(frame,sizeof(unsigned char),4,inpStream);	//read header only
		if(nRead&&nRead!=4) { bLoop=false; continue; }

		// set up the bitstream reader
		bit_window = (frame[2]<<16)|(frame[3]<<8);
		bits_in_window = 16;
		frame_pos = &frame[4];

		parseMp2Header(&header,frame);
		int nFrameSize = frameSize(&header);

		nRead = fread(frame+4,sizeof(unsigned char),nFrameSize-4,inpStream);	//read rest of the frame
		if(nRead&&nRead!=(nFrameSize-4)) { bLoop=false; continue; }

		get_bits(16);
		if(header.protect == 0) get_bits(16);



		int bit_rate = bitrateTable[header.id][header.bitrateIdx];	// kbit/s
		int sample_freq = 0;
		if(header.id == ID_ISO13818){
			sample_freq = 24;
		}else{
			assert(header.id == ID_ISO11172);
			sample_freq = 48;
		}

		int num_channel=2;
		if(header.mode==MONO) num_channel=1;

		int nTable = getBitAllocTable(bit_rate/num_channel,sample_freq);
		assert(nTable!=-1);
		int sblimit = sblimit_tbl[nTable];

		int bound = sblimit;
		// parse the mode_extension, set up the stereo bound
		if (header.mode == JOINT_STEREO) bound = (header.mode_ext + 1) << 2;

		unsigned int bit_alloc[2][32];
		memset(bit_alloc,0,sizeof(bit_alloc));

		int nch = (header.mode == MONO)?1:2;
		//bit allocation
		for(int sb=0; sb<bound; sb++)
			for (int ch=0; ch<nch; ch++)
				bit_alloc[ch][sb] = get_bits(bitalloc_tbl[nTable][sb]);
		for (int sb=bound; sb<sblimit; sb++)
			bit_alloc[0][sb] = bit_alloc[1][sb] = get_bits(bitalloc_tbl[nTable][sb]);
	
		//scfsi
		unsigned int scfsi[2][32];
		for(int sb=0; sb<sblimit; sb++) {
			for (int ch=0; ch < nch; ch++)
				if (bit_alloc[ch][sb])
					scfsi[ch][sb] = get_bits(2);
			if (header.mode == MONO)
				scfsi[1][sb] = scfsi[0][sb];
		}

		//scale factors
		unsigned int scalefactor[2][32][3];
		for (int sb=0; sb<sblimit; sb++) {
			for (int ch=0; ch<nch; ch++)
				if (bit_alloc[ch][sb]) {
					switch (scfsi[ch][sb]) {
					case 0: scalefactor[ch][sb][0] = get_bits(6);
							scalefactor[ch][sb][1] = get_bits(6);
							scalefactor[ch][sb][2] = get_bits(6);
							break;
					case 1: scalefactor[ch][sb][0] =
							scalefactor[ch][sb][1] = get_bits(6);
							scalefactor[ch][sb][2] = get_bits(6);
							break;
					case 2: scalefactor[ch][sb][0] =
							scalefactor[ch][sb][1] =
							scalefactor[ch][sb][2] = get_bits(6);
						break;
					case 3: scalefactor[ch][sb][0] = get_bits(6);
							scalefactor[ch][sb][1] =
							scalefactor[ch][sb][2] = get_bits(6);
						break;
					}
				}
				if (header.mode == MONO)
					for(int part=0; part<3; part++)
						scalefactor[1][sb][part] = scalefactor[0][sb][part];
		}

		int dabExtension = 4;
		((bit_rate/num_channel) >= 56)? dabExtension=4:dabExtension=2;

		unsigned int scfCrc[4];
		memset(scfCrc,0,sizeof(scfCrc));
		for(int i = dabExtension-1;i>=0;i--) {
			CRC_calcDAB(nch,sblimit,bit_alloc,scfsi,scalefactor,&scfCrc[i],i);
		}

		if(bInitFrame){
			bInitFrame=false;
			memcpy(outFrame,frame,nFrameSize);
		}else{
			if(dabExtension==4) {
				outFrame[nFrameSize-6]=scfCrc[3];
				outFrame[nFrameSize-5]=scfCrc[2];
			}
			outFrame[nFrameSize-4]=scfCrc[1];
			outFrame[nFrameSize-3]=scfCrc[0];

			size_t nWrite = fwrite(outFrame,sizeof(unsigned char),nFrameSize,outStream);
			assert(nWrite==nFrameSize);

			memcpy(outFrame,frame,nFrameSize);
		}

		fprintf(stderr, "[%04i", ++nFrameCount);
		fprintf(stderr, "]\r");
		fflush(stderr);
	}
	_fcloseall();
	cout<<"end of main"<<endl;
	return 0;
};