/*
 * lgpng_test.c
 * Lucas Garron
 * lucasg@gmx.de
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lgpng.h"

/********************************************
 *
 * HARNESS
 *
 */
 
#define LGPNG_TEST_VERSION "0.0.1"

 
int main(int argc, char* argv[]) {
 struct image* img;
 int w, h;
 int i, j;
 char* outfile;
 FILE* file;
 
 printf("lgpng_test - Lucas Garron - version %s\n", LGPNG_TEST_VERSION);
 
 w = 256;
 h = 256;
 img = image_alloc(w, h);
 
 for (i=0; i<h; i++) {
   for (j=0; j<w; j++) {
	   struct pixel* curPixel = pixel_at(img, i, j);
	   curPixel->r = 255-(i*256/h);
	   curPixel->g = (j*256/w);
	   curPixel->b = (i-j+255)/2;
	   curPixel->a = (j-i+255)/2;
   }
 }
 
 outfile = "lgpng_test.png";
 if (argc > 1)
 outfile = argv[1];
 
 printf("Writing to file %s\n", outfile);
 file = fopen(outfile, "w");
 assert(file != NULL);
 
 write_image_to_file(file, img);
 
 fclose(file);
 
 return 0;
 }