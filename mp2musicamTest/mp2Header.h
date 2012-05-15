#ifndef __MP2HEADER_H__
#define __MP2HEADER_H__

#define ID_ISO13818		0
#define ID_ISO11172		1
#define STEREO			0
#define JOINT_STEREO	1
#define DUAL_CHANNEL	2
#define MONO			3

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

#endif//__MP2HEADER_H__