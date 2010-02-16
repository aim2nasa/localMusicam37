/* twolame - Filename: AUDIO.H
 *
 *   Copyright (c) 2006 John Edwards <john.edwards33@ntlworld.com>
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Portions Copyright 2000-2002, Michael Smith <msmith@labyrinth.net.au>
 */


#ifndef AUDIO_H_INCLUDED
#define AUDIO_H_INCLUDED

#include <stdio.h>

#define MAXWAVESIZE     4294967040LU

typedef long (*audio_read_func)(void *src, short int *buffer, int samples);

typedef struct
{
	audio_read_func read_samples;
	
	void *readdata;

	long total_samples_per_channel;
	int channels;
	long rate;
	int samplesize;
	int format;

	char *filename;
} src_proc_opt;

typedef struct
{
	int  (*id_func)(unsigned char *buf, int len); /* Returns true if can load file */
	int  id_data_len; /* Amount of data needed to id whether this can load the file */
	int  (*open_func)(FILE *in, src_proc_opt *opt, unsigned char *oldbuf, int buflen);
	void (*close_func)(void *);
	char *format;
	char *description;
} input_format;


typedef struct {
	unsigned short format;
	unsigned short channels;
	unsigned int samplerate;
	unsigned int bytespersec;
	unsigned short align;
	unsigned short samplesize;
} wav_fmt;

typedef struct {
	short channels;
	short samplesize;
	long  totalsamples;
	long  samplesread;
	FILE  *f;
	short bigendian;
} wavfile;

input_format *open_audio_file(FILE *in, src_proc_opt *opt);

int raw_open(FILE *in, src_proc_opt *opt, unsigned char *buf, int buflen);
int wav_open(FILE *in, src_proc_opt *opt, unsigned char *oldbuf, int buflen);
int wav_id(unsigned char *buf, int len);
void raw_close(void *);
void wav_close(void *);

long wav_read(void *, short int *buffer, int samples);
long wav_ieee_read(void *, short int *buffer, int samples);
long raw_read_stereo(void *, short int *buffer, int samples);

#endif /* AUDIO_H_INCLUDED */
