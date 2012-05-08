#include <iostream>
#include <assert.h>

#define SBLIMIT			32
#define CRC8_POLYNOMIAL 0x1D

using namespace std;

void CRC_calcDAB (int nch,int sblimit,
				  unsigned int bit_alloc[2][SBLIMIT],
				  unsigned int scfsi[2][SBLIMIT],
				  unsigned int scalar[2][3][SBLIMIT], unsigned int *crc,int packed);
void update_CRCDAB (unsigned int data, unsigned int length, unsigned int *crc);

void main(int argc, char *argv[])
{
	unsigned int bit_alloc[2][SBLIMIT];
	unsigned int scfsi[2][SBLIMIT];
	unsigned int scalar[2][3][SBLIMIT];
	unsigned int dabExt;
	unsigned int crcRead[4];

	memset(bit_alloc,0,sizeof(unsigned int)*2*SBLIMIT);
	memset(scfsi,0,sizeof(unsigned int)*2*SBLIMIT);
	memset(scalar,0,sizeof(unsigned int)*2*3*SBLIMIT);
	memset(crcRead,0,sizeof(unsigned int)*4);

	FILE* fp=fopen("frame1.bin","rb");

	size_t nRead = 0;
	nRead = fread(bit_alloc,sizeof(unsigned int),2*32,fp);
	assert(nRead==2*32);

	nRead = fread(scfsi,sizeof(unsigned int),2*32,fp);
	assert(nRead==2*32);

	nRead = fread(scalar,sizeof(unsigned int),2*3*32,fp);
	assert(nRead==2*3*32);

	nRead = fread(&dabExt,sizeof(unsigned int),1,fp);
	assert(nRead==1);

	for(int i = dabExt - 1; i >= 0; i--) {
		nRead = fread(&crcRead[i],1,1,fp);
		assert(nRead==1);
	}
	fclose(fp);

	unsigned int scfCrc[4];
	for(int i = dabExt-1;i>=0;i--) {
		CRC_calcDAB(1,30,bit_alloc,scfsi,scalar,&scfCrc[i],i);
	}

	for(int i=0;i<dabExt;i++) assert(crcRead[i]==scfCrc[i]);

	cout<<"end of main"<<endl;
}

void CRC_calcDAB (int nch,int sblimit,
				  unsigned int bit_alloc[2][SBLIMIT],
				  unsigned int scfsi[2][SBLIMIT],
				  unsigned int scalar[2][3][SBLIMIT], unsigned int *crc,int packed)
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
			update_CRCDAB (scalar[k][j][i] >> 3, 3, crc);
		}
		break;
	case 1:
	case 3:
		nb_scalar += 2;
		update_CRCDAB (scalar[k][0][i] >> 3, 3, crc);
		update_CRCDAB (scalar[k][2][i] >> 3, 3, crc);
		break;
	case 2:
		nb_scalar++;
		update_CRCDAB (scalar[k][0][i] >> 3, 3, crc);
			}
}

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