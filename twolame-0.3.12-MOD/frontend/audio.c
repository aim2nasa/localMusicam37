/* twolame - Filename: AUDIO.C
 *
 * Function: Essentially provides all the wave input routines.
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
 * Portions, (c) Michael Smith <msmith@labyrinth.net.au>
 * and libvorbis examples, (c) Monty <monty@xiph.org>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <math.h>
#include "audio.h"
//#include "i18n.h"
//#include "resample.h"
//#include "misc.h"

/* Macros to read header data */
#define READ_U32_LE(buf) \
	(((buf)[3]<<24)|((buf)[2]<<16)|((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U16_LE(buf) \
	(((buf)[1]<<8)|((buf)[0]&0xff))

#define READ_U32_BE(buf) \
	(((buf)[0]<<24)|((buf)[1]<<16)|((buf)[2]<<8)|((buf)[3]&0xff))

#define READ_U16_BE(buf) \
	(((buf)[0]<<8)|((buf)[1]&0xff))

/* Define the supported formats here */
input_format formats[] = {
	{wav_id, 12, wav_open, wav_close, "wav", "WAV file reader"},
	{NULL, 0, NULL, NULL, NULL, NULL}
};

input_format *open_audio_file(FILE *in, src_proc_opt *opt)
{
	int j=0;
	unsigned char *buf=NULL;
	int buf_size=0, buf_filled=0;
	int size,ret;

	while (formats[j].id_func) {
		size = formats[j].id_data_len;
		if (size >= buf_size) {
			buf = realloc(buf, size);
			buf_size = size;
		}

		if (size > buf_filled) {
			ret = fread(buf+buf_filled, 1, buf_size-buf_filled, in);
			buf_filled += ret;

			if (buf_filled < size) {
				/* File truncated */
				j++;
				continue;
			}
		}

		if (formats[j].id_func(buf, buf_filled)) {
			/* ok, we now have something that can handle the file */
			if (formats[j].open_func(in, opt, buf, buf_filled)) {
				free(buf);
				return &formats[j];
			}
		}
		j++;
	}

	free(buf);

	return NULL;
}

static int seek_forward(FILE *in, int length)
{
	if (fseek(in, length, SEEK_CUR)) {
		/* Failed. Do it the hard way. */
		unsigned char buf[1024];
		int seek_needed = length, seeked;
		while (seek_needed > 0) {
			seeked = fread(buf, 1, seek_needed>1024?1024:seek_needed, in);
			if (!seeked)
				return 0; /* Couldn't read more, can't read file */
			else
				seek_needed -= seeked;
		}
	}
	return 1;
}


static int find_wav_chunk(FILE *in, char *type, unsigned int *len)
{
	unsigned char buf[8];

	while (1) {
		if (fread(buf,1,8,in) < 8) {
			/* Suck down a chunk specifier */
			fprintf(stderr, "Warning: Unexpected EOF in reading WAV header\n");
			return 0; /* EOF before reaching the appropriate chunk */
		}

		if (memcmp(buf, type, 4)) {
			*len = READ_U32_LE(buf+4);
			if (!seek_forward(in, *len))
				return 0;

			buf[4] = 0;
			fprintf(stderr, "Skipping chunk of type \"%s\", length %d\n", buf, *len);
		}
		else {
			*len = READ_U32_LE(buf+4);
			return 1;
		}
	}
}

double read_IEEE80(unsigned char *buf)
{
	int s=buf[0]&0xff;
	int e=((buf[0]&0x7f)<<8)|(buf[1]&0xff);
	double f=((unsigned long)(buf[2]&0xff)<<24)| ((buf[3]&0xff)<<16)| ((buf[4]&0xff)<<8) | (buf[5]&0xff);

	if (e==32767) {
		if (buf[2]&0x80)
			return HUGE_VAL; /* Really NaN, but this won't happen in reality */
		else {
			if (s)
				return -HUGE_VAL;
			else
				return HUGE_VAL;
		}
	}

	f=ldexp(f,32);
	f+= ((buf[6]&0xff)<<24)| ((buf[7]&0xff)<<16)| ((buf[8]&0xff)<<8) | (buf[9]&0xff);

	return ldexp(f, e-16446);
}

int wav_id(unsigned char *buf, int len)
{
	unsigned int flen;
	
	if (len<12) return 0; /* Something screwed up */

	if (memcmp(buf, "RIFF", 4))
		return 0; /* Not wave */

	flen = READ_U32_LE(buf+4); /* We don't use this */

	if (memcmp(buf+8, "WAVE",4))
		return 0; /* RIFF, but not wave */

	return 1;
}

int wav_open(FILE *in, src_proc_opt *opt, unsigned char *oldbuf, int buflen)
{
	unsigned char buf[16];
	unsigned int len;
	int samplesize;
	wav_fmt format;
	wavfile *wav = malloc(sizeof(wavfile));

	/* Ok. At this point, we know we have a WAV file. Now we have to detect
	 * whether we support the subtype, and we have to find the actual data
	 * We don't (for the wav reader) need to use the buffer we used to id this
	 * as a wav file (oldbuf)
	 */

	if (!find_wav_chunk(in, "fmt ", &len))
		return 0; /* EOF */

	if (len < 16) {
		fprintf(stderr, "Warning: Unrecognised format chunk in WAV header\n");
		return 0; /* Weird format chunk */
	}

	/* A common error is to have a format chunk that is not 16 or 18 bytes
	 * in size.  This is incorrect, but not fatal, so we only warn about 
	 * it instead of refusing to work with the file.  Please, if you
	 * have a program that's creating format chunks of sizes other than
	 * 16 or 18 bytes in size, report a bug to the author.
	 */
	if (len!=16 && len!=18)
		fprintf(stderr,"Warning: INVALID format chunk in wav header.\n"
				" Trying to read anyway (may not work)...\n");

	if (fread(buf,1,16,in) < 16) {
		fprintf(stderr, "Warning: Unexpected EOF in reading WAV header\n");
		return 0;
	}

	/* Deal with stupid broken apps. Don't use these programs.
	 */
	if (len - 16 > 0 && !seek_forward(in, len-16))
		return 0;

	format.format =      READ_U16_LE(buf); 
	format.channels =    READ_U16_LE(buf+2); 
	format.samplerate =  READ_U32_LE(buf+4);
	format.bytespersec = READ_U32_LE(buf+8);
	format.align =       READ_U16_LE(buf+12);
	format.samplesize =  READ_U16_LE(buf+14);

	if (!find_wav_chunk(in, "data", &len))
		return 0; /* EOF */

	if (format.format == 1) {
		samplesize = format.samplesize/8;
		opt->read_samples = wav_read;
	}
	else if (format.format == 3) {
		samplesize = 4;
		opt->read_samples = wav_ieee_read;
	}
	else {
		fprintf(stderr, "ERROR: Wav file is unsupported type (must be standard PCM\n"
				" or type 3 floating point PCM)\n");
		return 0;
	}

	if ( format.align == format.channels*samplesize && format.samplesize == samplesize*8 && 
				(format.samplesize == 32 || format.samplesize == 24 ||
				 format.samplesize == 16 || format.samplesize == 8)) {
		/* OK, good - we have the one supported format,
		   now we want to find the size of the file */
		opt->rate = format.samplerate;
		opt->channels = format.channels;
		
		wav->f = in;
		wav->samplesread = 0;
		wav->bigendian = 0;
		wav->channels = format.channels; /* This is in several places. The price
							of trying to abstract stuff. */
		wav->samplesize = format.samplesize;
		opt->samplesize = format.samplesize;

		if (len) {
			opt->total_samples_per_channel = len/(format.channels*samplesize);
		}
		else {
			long pos;
			pos = ftell(in);
			if (fseek(in, 0, SEEK_END) == -1) {
				opt->total_samples_per_channel = 0; /* Give up */
			}
			else {
				opt->total_samples_per_channel = (ftell(in) - pos) / (format.channels*samplesize);
				fseek(in,pos, SEEK_SET);
			}
		}
		wav->totalsamples = opt->total_samples_per_channel;

		opt->readdata = (void *)wav;
		return 1;
	}
	else {
		fprintf(stderr, "ERROR: Wav file is unsupported subformat (must be 8, 16, 24 or 32 bit PCM\n"
				"or floating point PCM)\n");
		return 0;
	}
}

int raw_open(FILE *in, src_proc_opt *opt, unsigned char *buf, int buflen)
{
	wav_fmt format; /* fake wave header ;) */
	wavfile *wav = malloc(sizeof(wavfile));

	/* construct fake wav header ;) */
	format.format =      opt->format; 
	format.channels =    opt->channels;
	format.samplerate =  opt->rate;
	format.samplesize =  opt->samplesize;
	format.bytespersec = opt->channels * opt->rate * opt->samplesize / 8;
	format.align =       format.bytespersec;

	wav->f =             in;
	wav->samplesread =   0;
	wav->bigendian =     0;
	wav->channels =      opt->channels;
	wav->samplesize =    opt->samplesize;
	wav->totalsamples =  0;

	if (opt->format == 3)
		opt->read_samples = wav_ieee_read;
	else
		opt->read_samples = wav_read;
	opt->readdata = (void *)wav;
	opt->total_samples_per_channel = 0; /* raw mode, don't bother */
	return 1;
}

long wav_read(void *in, short int *buffer, int samples)
{

	wavfile *f = (wavfile *)in;
	int sampbyte = f->samplesize / 8;
	signed char *buf = alloca(samples*sampbyte*f->channels);
	long bytes_read = fread(buf, 1, samples*sampbyte*f->channels, f->f);
	int i;
	long realsamples;

	if (f->totalsamples && f->samplesread + bytes_read/(sampbyte*f->channels) > f->totalsamples) {
		bytes_read = sampbyte*f->channels*(f->totalsamples - f->samplesread);
	}

	realsamples = bytes_read/(sampbyte*f->channels);
	f->samplesread += realsamples;
		
	if (f->samplesize==8) {
		unsigned char *bufu = (unsigned char *)buf;
		for (i = 0; i < realsamples*f->channels; i++)
			buffer[i] = ((int)(bufu[i])-128) * 255;
	}
	else if (f->samplesize==16) {
		short *short_sample_buffer = (short *)buf;
		for (i = 0; i < realsamples*f->channels; i++)
			buffer[i] = short_sample_buffer[i];
	}
	else if (f->samplesize==24) {
		int *int_sample_buffer = (int *)buf;
		for (i = 0; i < realsamples*f->channels; i++)
			buffer[i] = int_sample_buffer[i] / 256;
	}
	else if (f->samplesize==32) {
		int *int_sample_buffer = (int *)buf;
		for (i = 0; i < realsamples*f->channels; i++)
			buffer[i] = int_sample_buffer[i] / 65536;
	}
	else {
		fprintf(stderr, "Internal error: attempt to read unsupported "
			"bitdepth %d\n", f->samplesize);
		return 0;
	}

	return realsamples;
}

long wav_ieee_read(void *in, short int *buffer, int samples)
{
	wavfile *f = (wavfile *)in;
	float *buf = alloca(samples*4*f->channels); /* de-interleave buffer */
	long bytes_read = fread(buf,1,samples*4*f->channels, f->f);
	int i;
	long realsamples;


	if (f->totalsamples && f->samplesread + bytes_read/(4*f->channels) > f->totalsamples)
		bytes_read = 4*f->channels*(f->totalsamples - f->samplesread);
	realsamples = bytes_read/(4*f->channels);
	f->samplesread += realsamples;

	for (i=0; i < realsamples * f->channels; i++)
		buffer[i] = buf[i] * 32767.f;

	return realsamples;
}

void wav_close(void *info)
{
	wavfile *f = (wavfile *)info;

	free(f);
}

/*
 * end of audio.c
 */
