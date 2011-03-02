lgsound2png

Lucas Garron
lucasg@gmx.de
Started May 2010

A tool to generate pretty waveform images from sound files.

Contains small libraries for reading wave files (lgwav.c)
and writing correctly formatted imafe files (lgpng.c).

To compile:
make install

To run:
./lgwav2png -i the_wizard.wav -o the_wizard.png

If you have ffmpeg and ImageMagick:
./lgsound2png the_wizard.mp3