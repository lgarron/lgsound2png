
/*
 * lgpng.h
 * Lucas Garron
 *
 * Started June 2010
 *
 * This generates simple (read: large, essentially bitmap) images
 * without using any non-standard includes or libraries.
 *
 * Intended to be a used with lgwav2png/lgsound2png tool to generate
 * pretty waveform images from sound files.
 *
 */


#ifndef LGPNG_H
#define LGPNG_H


#define LGPNG_VERSION "1.1"

/********************************************
 *
 * Image
 *
 */

typedef struct pixel
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;	
} pixel;

typedef struct image
{
	int width;
	int height;
	struct pixel** data;
} image;



struct image* image_alloc(int width, int height);

void image_free(struct image* img);

struct pixel* pixel_at(struct image* img, int row, int col);

int set_pixel_at(struct image* img, int row, int col, int r, int g, int b, int a);

int image_sanity_test(struct image* img);

int write_PNG_image_to_file(FILE* file, struct image* img);



#endif //LGPNG_H