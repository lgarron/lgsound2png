
/*
 * lgpng.c
 * Lucas Garron
 *
 * Started June 2010
 * 
 * PNG, ZLIB, DEFLATE, CRC, and ADLER32 deconstructed using
 * http://www.libpng.org/pub/png/
 * http://www.w3.org/TR/PNG/
 * http://www.ietf.org/rfc/rfc1950.txt
 * http://www.ietf.org/rfc/rfc1951.txt
 * http://en.wikipedia.org/wiki/Cyclic_redundancy_check
 * http://en.wikipedia.org/wiki/Adler-32
 *
 * This generates simple (read: large, essentially bitmap) images
 * without using any non-standard includes or libraries.
 *
 * Tested only on Mac OSX Snow Leopard,
 * using "-pedantic -ansi" for GCC flags.
 *
 * Intended to be a used with lgwav2png/lgsound2png tool to generate
 * pretty waveform images from sound files.
 *
 * Changelog:
 *
 * 1.1
 * - Split header and source files.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lgpng.h"

/*
 * Width first, as in PNG format.
 *
 */
struct image* image_alloc(int width, int height)
{
	struct image* img = malloc(sizeof(struct image));
	img->width = width;
	img->height = height;
	img->data = calloc(width*height, sizeof(struct pixel));
	return img;
}

void image_free(struct image* img)
{
	free(img->data);
	free(img);
}

/*
 * Row first, like array access.
 *
 */
struct pixel* pixel_at(struct image* img, int row, int col)
{
	return ((struct pixel*)(img->data))+row*(img->width)+col;
}


int set_pixel_at(struct image* img, int row, int col, int r, int g, int b, int a)
{
	struct pixel* p = pixel_at(img, row, col);
	
	p->r = r;
	p->g = g;
	p->b = b;
	p->a = a;
	
	return 0;
}

int image_sanity_test(struct image* img) {
	if (img->width >= 2) {
		/* Make sure pixel data is contiguous and streamable. */
		return ((unsigned long)pixel_at(img, 0, 1) - (unsigned long)pixel_at(img, 0, 0) == 4);
	}
	else if ((img->width == 1)&&(img->height >= 2)) {
		return ((unsigned long)pixel_at(img, 1, 0) - (unsigned long)pixel_at(img, 0, 0) == 4);
	}
	
	return 1;
}


/********************************************
 *
 * CRC
 * Code from http://www.w3.org/TR/PNG/
 *
 */

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

/* Make the table for a fast CRC. */
void make_crc_table(void)
{
	unsigned long c;
	int n, k;
	
	for (n = 0; n < 256; n++) {
		c = (unsigned long) n;
		for (k = 0; k < 8; k++) {
			if (c & 1)
				c = 0xedb88320L ^ (c >> 1);
			else
				c = c >> 1;
		}
		crc_table[n] = c;
	}
	crc_table_computed = 1;
}

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
 should be initialized to all 1's, and the transmitted value
 is the 1's complement of the final running CRC (see the
 crc() routine below). */

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
{
	unsigned long c = crc;
	int n;
	
	if (!crc_table_computed)
		make_crc_table();
	for (n = 0; n < len; n++) {
		
		c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
	}
	
	return c;
}

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len)
{
	return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
}

/********************************************
 *
 * ADLER
 * From http://en.wikipedia.org/wiki/Adler-32
 *
 */

#define MOD_ADLER 65521

uint32_t adler32(uint8_t *data, size_t len) /* data: Pointer to the data to be summed; len is in bytes */
{
	uint32_t a = 1, b = 0;
	size_t index;
	
	/* Loop over each byte of data, in order */
	for (index = 0; index < len; ++index)
	{
		a = (a + data[index]) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	
	return (b << 16) | a;
}


typedef struct adler32_data {
	uint32_t a;
	uint32_t b;
} adler32_data;

int adler32_init(struct adler32_data* ad) /* data: Pointer to the data to be summed; len is in bytes */
{
	(ad->a) = 1;
	(ad->b) = 0;
	return 0;
}

int adler32_update(struct adler32_data* ad, uint8_t *data, size_t len)
{
	size_t index;
	for (index = 0; index < len; ++index)
	{
		(ad->a) = ((ad->a) + data[index]) % MOD_ADLER;
		(ad->b) = ((ad->b) + (ad->a)) % MOD_ADLER;
	}
	return 0;
}

int adler32_sum(struct adler32_data* ad)
{
	return ((ad->b) << 16) | (ad->a);
}

/********************************************
 *
 * PNG
 *
 */

const unsigned char PNG_HEADER[] = {137, 80, 78, 71, 13, 10, 26, 10, 0};
const int TRUE_COLOR_WITH_ALPHA = 6;
const int BYTE_LENGTH = 8;
const int COL_BIT_DEPTH = 8;
const unsigned char bKGD[] = {0, 255, 0, 255, 0, 255};
const int bKGD_LEN = 6;
#define PNG_CHUNK_TYPE_LENGTH 4 /* Necessary to use as char[] length in struct */

const int MAX_DATA_BYTES_PER_DEFLATE_BLOCK = 65535;
const unsigned char ZLIB_DEFLATE_HEADER[] = {8, 29, 0};
const int DEFLATE_OVERHEAD = 5; /* Header */


typedef struct png_chunk {
	char type[PNG_CHUNK_TYPE_LENGTH+1]; /*Four chars + terminating null*/
	int len;
	char* data;
} png_chunk;

typedef struct int_as_four_chars {
	char i[4];
} int_as_four_chars;

struct png_chunk* png_chunk_alloc()
{
	struct png_chunk* chunk;
	chunk = malloc(sizeof(struct png_chunk));
	assert(chunk != 0);
	chunk->type[0] = '\0';
	chunk->len = 0;
	chunk->data = NULL;
	return chunk;
}

void png_chunk_free(struct png_chunk* chunk) {
	if (chunk->data) {
		free(chunk->data);
	}
	free(chunk);
}

int write_chunk_type(struct png_chunk* chunk, char* type)
{
	strncpy(chunk->type, type, PNG_CHUNK_TYPE_LENGTH);	
	return 0;
}

/*
 * FUN WITH ENDIAN-NESS
 */
void int_to_four_chars(unsigned long i, struct int_as_four_chars* p_i)
{	
	int j;
	
	for (j=0; j<4; j++) {
		p_i->i[j] = (char)((i >> BYTE_LENGTH*(3-j)) % 256);
	}
}

/*
 * FUN WITH ENDIAN-NESS
 */
void int_to_four_chars_rev(unsigned long i, struct int_as_four_chars* p_i)
{	
	int j;
	
	for (j=0; j<4; j++) {
		p_i->i[j] = (char)((i >> BYTE_LENGTH*(j)) % 256);
	}
}

/*
 * Two bytes and complement 2 bytes
 */
void int_to_four_chars_zlibchunk(unsigned long i, struct int_as_four_chars* p_i)
{	
	int j;
	
	for (j=0; j<2; j++) {
		p_i->i[j] = (char)((i >> BYTE_LENGTH*j) % 256);
	}
	
	for (j=0; j<2; j++) {
		p_i->i[2+j] = (char)(255-((i >> BYTE_LENGTH*j) % 256));
	}
}

int write_chunk(FILE* file, struct png_chunk* chunk)
{
	struct int_as_four_chars i;
	unsigned long chunk_crc;
	
	int_to_four_chars(chunk->len, &i);
	fwrite(&i, sizeof(int), 1, file);
	
	fwrite(&(chunk->type), sizeof(char), PNG_CHUNK_TYPE_LENGTH, file);
	
	fwrite(chunk->data, sizeof(char), chunk->len, file);
	
	chunk_crc = 0xffffffffL;
	chunk_crc = update_crc(chunk_crc, (unsigned char*)chunk->type, PNG_CHUNK_TYPE_LENGTH);
	chunk_crc = update_crc(chunk_crc, (unsigned char*)chunk->data, chunk->len);
	chunk_crc = chunk_crc ^ 0xffffffffL;
	
	int_to_four_chars(chunk_crc, &i);
	fwrite(&i, sizeof(int), 1, file);
	
	return 0;
}

int write_and_free_chunk(FILE* file, struct png_chunk* chunk)
{
	int i;
	
	i = write_chunk(file, chunk);
	png_chunk_free(chunk);
	
	return i;
}

struct png_chunk* create_chunk(char* type, int len, char* data)
{
	struct png_chunk* chunk;
	
	chunk = png_chunk_alloc();
	assert(chunk != NULL);
	
	
	strncpy(chunk->type, type, PNG_CHUNK_TYPE_LENGTH);
	chunk->len = len;
	chunk->data = malloc(len);
	assert(chunk->data != NULL);
	memcpy(chunk->data, data, len);
	
	return chunk;
}

struct png_chunk* create_IHDR_DATA(int width, int height, int depth, int col_type, int compression, int filter, int interlace)
{
	struct png_chunk* ihdr_chunk;
	int len;
	struct int_as_four_chars w, h;
	char* data;
	
	int_to_four_chars((unsigned long)width, &w);
	int_to_four_chars((unsigned long)height, &h);
	
	len = 13;
	data = malloc(len);
	memcpy(&data[0], &w, sizeof(struct int_as_four_chars));
	memcpy(&data[4], &h, sizeof(struct int_as_four_chars));
	data[8] = (char)depth;
	data[9] = (char)col_type;
	data[10] = (char)compression;
	data[11] = (char)filter;
	data[12] = (char)interlace;
	
	ihdr_chunk = create_chunk("IHDR", len, data);
	free(data);
	
	return ihdr_chunk;
}

int write_zlib_chunk(FILE* file, struct image* img)
{
	int bytes_per_row, num_DEFLATE_blocks_per_row, num_DEFLATE_bytes_per_row, num_DEFLATE_bytes, num_chunk_bytes;
	char* buffer;
	struct int_as_four_chars ival;
	int i, j, carat, curblock_len;
	char lastQ;
	
	uint32_t chunk_crc;
	struct adler32_data ad;
	
	/* Current implementation requires this. */
	assert(image_sanity_test(img));
	
	
	bytes_per_row = (img->width)*(sizeof(struct pixel))+1;
	num_DEFLATE_blocks_per_row = (bytes_per_row-1)/MAX_DATA_BYTES_PER_DEFLATE_BLOCK+1;
	num_DEFLATE_bytes_per_row = bytes_per_row + DEFLATE_OVERHEAD*num_DEFLATE_blocks_per_row;
	num_DEFLATE_bytes = (img->height)*num_DEFLATE_bytes_per_row;
	num_chunk_bytes = 2+num_DEFLATE_bytes+4;
	
	
	chunk_crc = 0xffffffffL;
	adler32_init(&ad);
	
	buffer = malloc(bytes_per_row);
	assert(buffer != NULL);
	
	/* IDAT size */
	int_to_four_chars(num_chunk_bytes, &ival);
	fwrite(&ival, sizeof(struct int_as_four_chars), 1, file);
	
	/* IDAT type */
	fprintf(file, "IDAT");
	chunk_crc = update_crc(chunk_crc, (unsigned char*)"IDAT", PNG_CHUNK_TYPE_LENGTH);
	
	/* ZLIB header */
	fprintf(file, "%2s", ZLIB_DEFLATE_HEADER);
	chunk_crc = update_crc(chunk_crc, (unsigned char*)ZLIB_DEFLATE_HEADER, 2);
	
	for (i=0; i<(img->height); i++)
	{
		buffer[0] = '\0';
		memcpy((char*)&buffer[1], pixel_at(img, i, 0), bytes_per_row-1);
		adler32_update(&ad, (unsigned char*)buffer, bytes_per_row);
		
		carat = 0;
		for (j=0; j<num_DEFLATE_blocks_per_row; j++)
		{
			curblock_len = (j==num_DEFLATE_blocks_per_row-1) ? (bytes_per_row % MAX_DATA_BYTES_PER_DEFLATE_BLOCK) : MAX_DATA_BYTES_PER_DEFLATE_BLOCK;
			
			lastQ = (char)0;
			if (i==(img->height)-1 && j==num_DEFLATE_blocks_per_row-1) {
				lastQ = (char)1;
			}
			
			chunk_crc = update_crc(chunk_crc, (unsigned char*)&lastQ, 1);
			fwrite(&lastQ, sizeof(char), 1, file);
			
			int_to_four_chars_zlibchunk(curblock_len, &ival);
			chunk_crc = update_crc(chunk_crc, (unsigned char*)&ival, sizeof(struct int_as_four_chars));
			fwrite(&ival, sizeof(struct int_as_four_chars), 1, file);
			
			chunk_crc = update_crc(chunk_crc, (unsigned char*)(buffer+carat), curblock_len);
			carat += fwrite(buffer+carat, sizeof(char), curblock_len, file);
		}
	}
	
	free(buffer);
	
	/* ADLER32 */
	int_to_four_chars(adler32_sum(&ad), &ival);
	fwrite(&ival, sizeof(struct int_as_four_chars), 1, file);
	chunk_crc = update_crc(chunk_crc, (unsigned char*)&ival, sizeof(struct int_as_four_chars));
	
	/* CRC */
	int_to_four_chars(chunk_crc ^ 0xffffffffL, &ival);
	fwrite(&ival, sizeof(struct int_as_four_chars), 1, file);
	
	return 0;
}

/*
 * Writes a full PNG image to specified file stream.
 *
 */
int write_PNG_image_to_file(FILE* file, struct image* img)
{
	struct png_chunk* chunk;
	
	fprintf(file, "%s", PNG_HEADER);
	
	chunk = create_IHDR_DATA(img->width, img->height, COL_BIT_DEPTH, TRUE_COLOR_WITH_ALPHA, 0, 0, 0);
	write_and_free_chunk(file, chunk);
	
	chunk = create_chunk("sRGB", 1, "\0");
	write_and_free_chunk(file, chunk);
	
	chunk = create_chunk("bKGD", bKGD_LEN, (char*)bKGD);
	write_and_free_chunk(file, chunk);
	
	write_zlib_chunk(file, img);
	
	chunk = create_chunk("IEND", 0, "");
	write_and_free_chunk(file, chunk);
	
	return 0;
}
