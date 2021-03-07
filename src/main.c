#include "../include/main.h"
#include "../include/audio.h"
#include "../include/g722_encoder.h"
#include "../include/g722_decoder.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#define BUFFER_SIZE 80

static void
usage(const char *argv0)
{
    fprintf(stderr, "usage: %s [--sln16k] [--bend] file.g722 file.raw\n"
      "       %s --encode [--sln16k] [--bend] file.raw file.g722\n", argv0,
      argv0);
    exit (1);
}

int main(int argc, char const *argv[])
{
	FILE *fi, *fo;
	uint8_t ibuf[BUFFER_SIZE];
	int16_t obuf[BUFFER_SIZE];
	G722_DEC_CTX *g722_dctx;
	G722_ENC_CTX *g722_ectx;
	int i, srate, ch, enc, bend;
	size_t oblen;

	/* options descriptor */
	static struct option longopts[] = 
	{
	    { "sln16k", no_argument, NULL, 256 },
	    { "encode", no_argument, NULL, 257 },
	    { "bend",   no_argument, NULL, 258 },
	    { NULL,     0,           NULL,  0  }
	};

	srate = G722_SAMPLE_RATE_8000;
	oblen = sizeof(obuf);
	enc = bend = 0;
	while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) 
	{
	   switch (ch) 
	   {
		   case 256:
		   		printf("1\n");
		       srate &= ~G722_SAMPLE_RATE_8000;
		       break;

		   case 257:
		       enc = 1;
		       break;

		   case 258:
		       bend = 1;
		       break;

		   default:
		       usage(argv[0]);
	   }
	}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
	    usage(argv[-optind]);
	}

	fi = fopen(argv[0], "r");
	if (fi == NULL) {
	    fprintf(stderr, "cannot open %s\n", argv[0]);
	    exit (1);
	}
	fo = fopen(argv[1], "w");
	if (fo == NULL) {
	    fprintf(stderr, "cannot open %s\n", argv[1]);
	    exit (1);
	}

	if (enc == 0) 
	{
	    g722_dctx = g722_decoder_new(64000, srate);
	    if (g722_dctx == NULL) {
	        fprintf(stderr, "g722_decoder_new() failed\n");
	        exit (1);
	    }

	    while (fread(ibuf, sizeof(ibuf), 1, fi) == 1) 
	    {
	        g722_decode(g722_dctx, ibuf, sizeof(ibuf), obuf);
	        for (i = 0; i < (oblen / sizeof(obuf[0])); i++) 
	        {
	            if (bend == 0) 
	            {
	                obuf[i] = htole16(obuf[i]);
	            } 
	            else 
	            {
	                obuf[i] = htobe16(obuf[i]);
	            }
	        }
	        fwrite(obuf, oblen, 1, fo);
	        fflush(fo);
	    }
	} 
	else 
	{
	    g722_ectx = g722_encoder_new(64000, srate);

	    if (g722_ectx == NULL) 
	    {
	        fprintf(stderr, "g722_encoder_new() failed\n");
	        exit (1);
	    }

	    while (fread(obuf, oblen, 1, fi) == 1) 
	    {
	        for (i = 0; i < (oblen / sizeof(obuf[0])); i++) 
	        {
	            if (bend == 0) 
	            {
	                obuf[i] = le16toh(obuf[i]);
	            } 
	            else 
	            {
	                obuf[i] = be16toh(obuf[i]);
	            }
	        }
	        g722_encode(g722_ectx, obuf, (oblen / sizeof(obuf[0])), ibuf);
	        fwrite(ibuf, sizeof(ibuf), 1, fo);
	        fflush(fo);
	    }
	}

	fclose(fi);
	fclose(fo);

	exit(0);


	/*
	printf("INIT ALSA LOOP \n");
	init_alsa();
	while(1)
	{
	}
	return 0;
	*/
}