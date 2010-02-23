#include "scfCrc.h"
#include <malloc.h>
#include <assert.h>
#include <memory.h>

#define ID_ISO13818			0
#define ID_ISO11172			1
#define MODE_STEREO			0
#define MODE_JSTEREO		1
#define MODE_DUAL			2
#define MODE_SINGLE			3
#define CRC_ENABLE			0
#define CRC_DISABLE			1
#define MAX_CHANNEL			2
#define MAX_SUB_BAND		64
#define SCFSI_BIT_SIZE		2
#define SCF_BIT_SIZE		6
#define CRC8_POLY			0x1D

typedef unsigned char 		__u8;
typedef unsigned short		__u16;
typedef unsigned int		__u32;

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

unsigned char bitalloc_tbl[3][32] = {
	{4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3,
	 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 0, 0, 0, 0, 0},	//48KHz Table13
	{4, 4, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	//48KHz Table14
	{4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,
	 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0},	//24KHz Table15
};

int getBitAllocTable(int nBitrateCh,int nSampleFreq)
{
	if(nSampleFreq==24) {
		return 2;	//Table 15
	}else if(nSampleFreq==48) {
		if(nBitrateCh==56||nBitrateCh==64||nBitrateCh==80||nBitrateCh==96||nBitrateCh==112||
			nBitrateCh==128||nBitrateCh==160||nBitrateCh==192) 
			return 0;	//Table 13
		else if(nBitrateCh==32||nBitrateCh==48) 
			return 1;	//Table14
	}
	return -1;
}

void parseMp2Header(MP2_HEADER* pHeader,const unsigned char* pBuffer)
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

__u8 crc_8(__u8 *source, __u16 num_total_bits)
{
	int i, j;
	int num_bytes, num_remain_bits, loop;
	__u8 crc, ch;

	num_bytes       = num_total_bits/8;	
	num_remain_bits = num_total_bits%8;

	crc = 0;
	for(i = 0 ; i < num_bytes+2 ; i++ ){
		if(i == num_bytes){					// 1Byte 단위가 아닌 나머지가 있을 경우
			loop = num_remain_bits;
			ch   = source[i];			
		}
		else if(i == num_bytes+1){	// CRC8 : 마지막 8bit shift
			loop = 8;
			ch   = 0x00;						
		}
		else{
			loop = 8;
			ch   = source[i];			
		}

		for(j = 0 ; j < loop ; j++){
			if(crc & 0x80){
				crc <<= 1;
				crc = (ch&0x80) ? (crc|0x01) : (crc&0xFE); 
				crc = crc ^ CRC8_POLY;
			} 
			else{
				crc <<= 1;
				crc = (ch&0x80) ? (crc|0x01) : (crc&0xFE); 
			} 
			ch <<= 1;
		}
	} 

	return crc;
}

int next(int index)
{
	if(index>0) { return 0; }else{ return 1; }
}

scfCrc* scfCrc_init()
{
	scfCrc* sc = (scfCrc*)malloc(sizeof(scfCrc));
	memset(sc,0,sizeof(scfCrc));
	return sc;
}

void scfCrc_close(scfCrc* sc)
{
	free(sc);
}

int scfCrc_getCrc(const unsigned char* inFrame,int inFrameSize,unsigned char crc[MAX_SCF_CRC_SIZE])
{
	MP2_HEADER header;
	int ptr = 0;
	unsigned int bit_rate, sample_freq;	
	unsigned char num_subband, num_channel;
	unsigned short frame_size = 0;
	int nTable = 0;
	unsigned char bitalloc[MAX_CHANNEL][MAX_SUB_BAND], bitalloc_bit;
	unsigned char scfsi[MAX_CHANNEL][MAX_SUB_BAND]; 
	__u8  scf[MAX_CHANNEL][MAX_SUB_BAND][3], scf_temp[3];
	unsigned char scf_crc_buf[256];
	unsigned char scf_crc_temp, scf_crc_pt, scf_crc_bits, scf_crc_total_bits;	
	unsigned short bit_op_buf, bit_op_mask, remain_bit_pt;
	unsigned short i, j, k;
	int nCrc = 0;

	if(inFrame==0||inFrameSize==0) return -1;
	if((inFrame[0]!=0xFF)||((inFrame[1]&0xF0)!=0xF0)) return -1;	//syncword check
	if(inFrameSize>MAX_FRAME_SIZE) return -1;
	ptr+=2;

	parseMp2Header(&header,inFrame);
	ptr+=2;
	memset(crc,0,MAX_SCF_CRC_SIZE);

	bit_rate = bitrateTable[header.id][header.bitrateIdx];	// kbit/s
	if(header.id == ID_ISO13818){
		sample_freq = 24;
	}else{
		assert(header.id == ID_ISO11172);
		sample_freq = 48;
	}

	// audio mode
	if(header.mode == MODE_STEREO){
		num_channel = 2;
		num_subband = 32;
	}else if(header.mode == MODE_JSTEREO){
		num_channel = 2;
		num_subband = 64;			
	}else if(header.mode == MODE_DUAL){
		num_channel = 2;		
		num_subband = 32;				
	}else if(header.mode == MODE_SINGLE){
		num_channel = 1;		
		num_subband = 32;	
	}			

	frame_size = 144*bit_rate/sample_freq;
	if(frame_size!=inFrameSize) return -1;

	// CRC read
	if(header.protect==CRC_ENABLE) ptr += 2;

	nTable = getBitAllocTable(bit_rate/num_channel,sample_freq);

	// bit allocation
	remain_bit_pt = 0;		
	bit_op_buf = 0;		
	for(i = 0 ; i < num_subband ; i++){
		for(j = 0 ; j < num_channel ; j++){	
			bitalloc_bit = bitalloc_tbl[nTable][i];

			if(remain_bit_pt < bitalloc_bit){
				bit_op_buf = (bit_op_buf<<8)|((unsigned short)inFrame[ptr]&0xFF);
				ptr++;
				remain_bit_pt += 8;
			}

			bit_op_mask    = 0xFFFF<<(remain_bit_pt-bitalloc_bit);
			bitalloc[j][i] = (unsigned char)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-bitalloc_bit));
			bit_op_buf    &= (~bit_op_mask);
			remain_bit_pt -= bitalloc_bit;
		}
	}

	// scfsi
	for(i = 0 ; i < num_subband ; i++){
		for(j = 0 ; j < num_channel ; j++){		
			if(bitalloc[j][i] != 0){
				if(remain_bit_pt < SCFSI_BIT_SIZE){
					bit_op_buf = (bit_op_buf<<8)|((unsigned short)inFrame[ptr]&0xFF);
					ptr++;
					remain_bit_pt += 8;
				}

				bit_op_mask    = 0xFFFF<<(remain_bit_pt-SCFSI_BIT_SIZE);
				scfsi[j][i]    = (unsigned char)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-SCFSI_BIT_SIZE));
				bit_op_buf    &= (~bit_op_mask);
				remain_bit_pt -= SCFSI_BIT_SIZE;
			}
		}
	}

	// scale factor
	for(i = 0 ; i < num_subband ; i++){
		if(i == 0 || i == 4 || i == 8 || i == 16 || i == 27){
			//printf("Sub-band[%2d]  CRC = %X\n", i, crc_8(scf_crc_buf, scf_crc_total_bits));										
			if(i == 4){
				crc[3] = crc_8(scf_crc_buf, scf_crc_total_bits);
			}
			else if(i == 8){
				crc[2] = crc_8(scf_crc_buf, scf_crc_total_bits);
			}
			else if(i == 16){
				crc[1] = crc_8(scf_crc_buf, scf_crc_total_bits);
			}				
			else if(i == 27){
				crc[0] = crc_8(scf_crc_buf, scf_crc_total_bits);
			}								


			memset(scf_crc_buf, 0x00, 100);
			scf_crc_bits = 0;
			scf_crc_pt = 0;
			scf_crc_total_bits = 0;
		}

		for(j = 0 ; j < num_channel ; j++){		
			if(bitalloc[j][i] != 0){
				if(scfsi[j][i] == 0){
					for(k = 0 ; k < 3 ; k++){
						if(remain_bit_pt < SCF_BIT_SIZE){
							bit_op_buf = (bit_op_buf<<8)|((__u16)inFrame[ptr]&0xFF);
							ptr++;
							remain_bit_pt += 8;
						}

						bit_op_mask    = 0xFFFF<<(remain_bit_pt-SCF_BIT_SIZE);
						scf_temp[k]    = (__u8)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-SCF_BIT_SIZE));
						bit_op_buf    &= (~bit_op_mask);
						remain_bit_pt -= SCF_BIT_SIZE;

						// SCF-CRC
						scf_crc_temp = scf_temp[k]>>3;					
						if(scf_crc_bits+3 >= 8){
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp>>(3-(8-scf_crc_bits)))&0xFF;
							scf_crc_pt++;
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<(8-(3-(8-scf_crc_bits))))&0xFF;
							scf_crc_bits = (scf_crc_bits+3)%8;
						}
						else{
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<((8-scf_crc_bits)-3))&0xFF;
							scf_crc_bits = (scf_crc_bits+3);
						}
						scf_crc_total_bits += 3;					
					}

					scf[j][i][0] = scf_temp[0];
					scf[j][i][1] = scf_temp[1];
					scf[j][i][2] = scf_temp[2];
				}
				else if(scfsi[j][i] == 1){
					for(k = 0 ; k < 2 ; k++){
						if(remain_bit_pt < SCF_BIT_SIZE){
							bit_op_buf = (bit_op_buf<<8)|((__u16)inFrame[ptr]&0xFF);
							ptr++;
							remain_bit_pt += 8;
						}

						bit_op_mask    = 0xFFFF<<(remain_bit_pt-SCF_BIT_SIZE);
						scf_temp[k]    = (__u8)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-SCF_BIT_SIZE));
						bit_op_buf    &= (~bit_op_mask);
						remain_bit_pt -= SCF_BIT_SIZE;

						// SCF-CRC
						scf_crc_temp = scf_temp[k]>>3;					
						if(scf_crc_bits+3 >= 8){
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp>>(3-(8-scf_crc_bits)))&0xFF;
							scf_crc_pt++;
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<(8-(3-(8-scf_crc_bits))))&0xFF;
							scf_crc_bits = (scf_crc_bits+3)%8;
						}
						else{
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<((8-scf_crc_bits)-3))&0xFF;
							scf_crc_bits = (scf_crc_bits+3);
						}
						scf_crc_total_bits += 3;			
					}

					scf[j][i][0] = scf_temp[0];
					scf[j][i][1] = scf_temp[0];
					scf[j][i][2] = scf_temp[1];
				}
				else if(scfsi[j][i] == 2){
					for(k = 0 ; k < 1 ; k++){
						if(remain_bit_pt < SCF_BIT_SIZE){
							bit_op_buf = (bit_op_buf<<8)|((__u16)inFrame[ptr]&0xFF);
							ptr++;
							remain_bit_pt += 8;
						}

						bit_op_mask    = 0xFFFF<<(remain_bit_pt-SCF_BIT_SIZE);
						scf_temp[k]    = (__u8)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-SCF_BIT_SIZE));
						bit_op_buf    &= (~bit_op_mask);
						remain_bit_pt -= SCF_BIT_SIZE;

						// SCF-CRC
						scf_crc_temp = scf_temp[k]>>3;					
						if(scf_crc_bits+3 >= 8){
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp>>(3-(8-scf_crc_bits)))&0xFF;
							scf_crc_pt++;
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<(8-(3-(8-scf_crc_bits))))&0xFF;
							scf_crc_bits = (scf_crc_bits+3)%8;
						}
						else{
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<((8-scf_crc_bits)-3))&0xFF;
							scf_crc_bits = (scf_crc_bits+3);
						}
						scf_crc_total_bits += 3;					
					}

					scf[j][i][0] = scf_temp[0];
					scf[j][i][1] = scf_temp[0];
					scf[j][i][2] = scf_temp[0];				
				}
				else if(scfsi[j][i] == 3){
					for(k = 0 ; k < 2 ; k++){
						if(remain_bit_pt < SCF_BIT_SIZE){
							bit_op_buf = (bit_op_buf<<8)|((__u16)inFrame[ptr]&0xFF);
							ptr++;
							remain_bit_pt += 8;
						}

						bit_op_mask    = 0xFFFF<<(remain_bit_pt-SCF_BIT_SIZE);
						scf_temp[k]    = (__u8)((bit_op_buf&bit_op_mask)>>(remain_bit_pt-SCF_BIT_SIZE));
						bit_op_buf    &= (~bit_op_mask);
						remain_bit_pt -= SCF_BIT_SIZE;

						// SCF-CRC
						scf_crc_temp = scf_temp[k]>>3;					
						if(scf_crc_bits+3 >= 8){
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp>>(3-(8-scf_crc_bits)))&0xFF;
							scf_crc_pt++;
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<(8-(3-(8-scf_crc_bits))))&0xFF;
							scf_crc_bits = (scf_crc_bits+3)%8;
						}
						else{
							scf_crc_buf[scf_crc_pt] |= (scf_crc_temp<<((8-scf_crc_bits)-3))&0xFF;
							scf_crc_bits = (scf_crc_bits+3);
						}
						scf_crc_total_bits += 3;	
					}

					scf[j][i][0] = scf_temp[0];
					scf[j][i][1] = scf_temp[1];
					scf[j][i][2] = scf_temp[1];			
				}
			}
		}	
	}
	if((bit_rate/num_channel)>=56) { nCrc=4; }else{ nCrc=2; }
	return nCrc;
}

int scfCrc_apply(scfCrc* sc,unsigned char* inFrame,int inFrameSize,unsigned char* outFrame,int outFrameSize)
{
	int nCrc=0;
	unsigned char crc[MAX_SCF_CRC_SIZE];

	if(sc==0||inFrame==0||inFrameSize==0||outFrame==0) return -1;
	if(inFrameSize!=outFrameSize) return -1;

	nCrc = scfCrc_getCrc(inFrame,inFrameSize,crc);
	if(nCrc==-1) return -1;

	memcpy(sc->buffer[next(sc->index)],inFrame,outFrameSize);	//다음 프레임을 미리 준비

	if(sc->frameCount==0) {
		nCrc=0;	//맨 처음 프레임인 경우 다음 프레임의 CRC를 담을수 없기때문에
	}else{
		memset(sc->buffer[sc->index]+inFrameSize-nCrc-2,0,nCrc+2);
		memcpy(sc->buffer[sc->index]+inFrameSize-nCrc-2,crc+4-nCrc,nCrc);
		memcpy(outFrame,sc->buffer[sc->index],outFrameSize);
	}
	sc->frameCount++;
	sc->index = next(sc->index);
	return nCrc;
}