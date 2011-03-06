# Makefile
# lgwav2png
# Lucas Garron

PROG = lgwav2png

# Remember to use -O3 for performance.
CFLAGS += -O3 -Wall -pedantic -ansi $(WARNINGS)

PROG_SRC = lgpng.c lgwav.c lgwav2png.c
PROG_H = lgpng.h lgwav.h

lgwav2png: $(PROG_SRC) $(PROG_H)
	gcc $(CFLAGS) $(PROG_SRC) -o $(PROG)

lgpng_test: lgpng.c lgpng.h lgpng_test.c
	gcc $(CFLAGS) lgpng.c lgpng_test.c -o lgpng_test

all: $(PROG)

clean:
	rm $(PROG)