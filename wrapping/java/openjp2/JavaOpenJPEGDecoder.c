/*
 * Copyright (C) 2018, El Mostafa IDRASSI
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux 
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * Copyright (c) 2007, Patrick Piscaglia (Telemis)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
 
//	To avoid errors due to the use of free, malloc, realloc and calloc
//	because of the pragma in opj_malloc.h that poisons such uses
#define OPJ_SKIP_POISON

#include "opj_apps_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jni.h>
#include <math.h>
#include <time.h>

#if defined _WIN32
#include "windirent.h"
#else
#include <dirent.h>
#endif

#if defined (_WIN32) && (_MSC_VER) && (!__INTEL_COMPILER)
#include <windows.h>
#include <psapi.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <errno.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/types.h>
#include <unistd.h>
#endif /* _WIN32 */

#include "openjpeg.h"
#include "opj_includes.h"
#include "opj_getopt.h"
#include "convert.h"
#include "org_openJpeg_OpenJPEGJavaDecoder.h"

#ifdef OPJ_HAVE_LIBLCMS2
#include <lcms2.h>
#endif
#ifdef OPJ_HAVE_LIBLCMS1
#include <lcms.h>
#endif
#include "color.h"

#include "format_defs.h"
#include "opj_string.h"

typedef struct callback_variables 
{
	JNIEnv *env;
	/** 'jclass' object used to call a Java method from the C */
	jobject *jobj;
	/** 'jclass' object used to call a Java method from the C */
	jmethodID message_mid;
	jmethodID error_mid;
} callback_variables_t;

typedef struct dircnt{
	/** Buffer for holding images read from Directory*/
	char *filename_buf;
	/** Pointer to the buffer*/
	char **filename;
}dircnt_t;


typedef struct img_folder{
	/** The directory path of the folder containing input images*/
	char *imgdirpath;
	/** Output format*/
	char *out_format;
	/** Enable option*/
	char set_imgdir;
	/** Enable Cod Format for output*/
	char set_out_format;

}img_fol_t;

typedef enum opj_prec_mode {
	OPJ_PREC_MODE_CLIP,
	OPJ_PREC_MODE_SCALE
} opj_precision_mode;

typedef struct opj_prec {
	OPJ_UINT32         prec;
	opj_precision_mode mode;
} opj_precision;

// Added isCompressed parameter 
typedef struct opj_decompress_params {
	/** core library parameters */
	opj_dparameters_t core;

	/** whether the input image has been set using setInputStream() */
	int isInputStreamSet;

	/** input file name */
	char infile[OPJ_PATH_LEN];
	/** output file name */
	char outfile[OPJ_PATH_LEN];
	/** input file format 0: J2K, 1: JP2, 2: JPT */
	int decod_format;
	/** output file format 0: PGX, 1: PxM, 2: BMP */
	int cod_format;
	/** index file name */
	char indexfilename[OPJ_PATH_LEN];

	/** Decoding area left boundary */
	OPJ_UINT32 DA_x0;
	/** Decoding area right boundary */
	OPJ_UINT32 DA_x1;
	/** Decoding area up boundary */
	OPJ_UINT32 DA_y0;
	/** Decoding area bottom boundary */
	OPJ_UINT32 DA_y1;
	/** Verbose mode */
	OPJ_BOOL m_verbose;

	/** tile number ot the decoded tile*/
	OPJ_UINT32 tile_index;
	/** Nb of tile to decode */
	OPJ_UINT32 nb_tile_to_decode;

	opj_precision* precision;
	OPJ_UINT32     nb_precision;

	/* force output colorspace to RGB */
	int force_rgb;
	/* upsample components according to their dx/dy values */
	int upsample;
	/* split output components to different files */
	int split_pnm;
	/** number of threads */
	int num_threads;
	/* Quiet */
	int quiet;
	/** number of components to decode */
	OPJ_UINT32 numcomps;
	/** indices of components to decode */
	OPJ_UINT32* comps_indices;
} opj_decompress_parameters;

/* -------------------------------------------------------------------------- */
/* Declarations                                                               */
int get_num_images(char *imgdirpath);
int load_images(dircnt_t *dirptr, char *imgdirpath);
int get_file_format(const char *filename);
char get_next_file(int imageno, dircnt_t *dirptr, img_fol_t *img_fol,
	opj_decompress_parameters *parameters);
static int infile_format(const char *fname);
static int instream_format(const unsigned char *stream, OPJ_SIZE_T streamSize, int isJPT);

int parse_cmdline_decoder(int argc, char **argv,
	opj_decompress_parameters *parameters, img_fol_t *img_fol);
int parse_DA_values(char* inArg, unsigned int *DA_x0, unsigned int *DA_y0,
	unsigned int *DA_x1, unsigned int *DA_y1);

static opj_image_t* convert_gray_to_rgb(opj_image_t* original);
/* -------------------------------------------------------------------------- */

static void decode_help_display(void)
{
	fprintf(stdout,
		"\nThis is the JavaOpenJPEGDecoder utility from the OpenJPEG project.\n"
		"It decompresses JPEG 2000 codestreams to various image formats.\n"
		"It has been compiled against openjp2 library v%s.\n\n", opj_version());

	fprintf(stdout, "Parameters:\n"
		"-----------\n"
		"\n"
		"  -ImgDir <directory> \n"
		"	Image file Directory path \n"
		"  -OutFor <PBM|PGM|PPM|PNM|PAM|PGX|PNG|BMP|TIF|RAW|RAWL|TGA>\n"
		"    REQUIRED only if -ImgDir is used or -s is used\n"
		"		Output format for decompressed images.\n");
	fprintf(stdout, "  -i <compressed file>\n"
		"    REQUIRED only if an Input image directory is not specified\n"
		"    Currently accepts J2K-files, JP2-files and JPT-files. The file type\n"
		"    is identified based on its suffix.\n");
	fprintf(stdout, "  -o <decompressed file>\n"
		"    REQUIRED\n"
		"    Currently accepts formats specified above (see OutFor option)\n"
		"    Binary data is written to the file (not ascii). If a PGX\n"
		"    filename is given, there will be as many output files as there are\n"
		"    components: an indice starting from 0 will then be appended to the\n"
		"    output filename, just before the \"pgx\" extension. If a PGM filename\n"
		"    is given and there are more than one component, only the first component\n"
		"    will be written to the file.\n");
	fprintf(stdout, "  -r <reduce factor>\n"
		"    Set the number of highest resolution levels to be discarded. The\n"
		"    image resolution is effectively divided by 2 to the power of the\n"
		"    number of discarded levels. The reduce factor is limited by the\n"
		"    smallest total number of decomposition levels among tiles.\n"
		"  -l <number of quality layers to decode>\n"
		"    Set the maximum number of quality layers to decode. If there are\n"
		"    less quality layers than the specified number, all the quality layers\n"
		"    are decoded.\n");
	fprintf(stdout, "  -x  \n"
		"    Create an index file *.Idx (-x index_name.Idx) \n"
		"  -d <x0,y0,x1,y1>\n"
		"    OPTIONAL\n"
		"    Decoding area\n"
		"    By default all the image is decoded.\n"
		"  -t <tile_number>\n"
		"    OPTIONAL\n"
		"    Set the tile number of the decoded tile. Follow the JPEG2000 convention from left-up to bottom-up\n"
		"    By default all tiles are decoded.\n");
	fprintf(stdout, "  -p <comp 0 precision>[C|S][,<comp 1 precision>[C|S][,...]]\n"
		"    OPTIONAL\n"
		"    Force the precision (bit depth) of components.\n");
	fprintf(stdout,
		"    There shall be at least 1 value. Theres no limit on the number of values (comma separated, last values ignored if too much values).\n"
		"    If there are less values than components, the last value is used for remaining components.\n"
		"    If 'C' is specified (default), values are clipped.\n"
		"    If 'S' is specified, values are scaled.\n"
		"    A 0 value can be specified (meaning original bit depth).\n");
	fprintf(stdout, "  -c first_comp_index[,second_comp_index][,...]\n"
		"    OPTIONAL\n"
		"    To limit the number of components to decoded.\n"
		"    Component indices are numbered starting at 0.\n");
	fprintf(stdout, "  -force-rgb\n"
		"    Force output image colorspace to RGB\n"
		"  -upsample\n"
		"    Downsampled components will be upsampled to image size\n"
		"  -split-pnm\n"
		"    Split output components to different files when writing to PNM\n");
	if (opj_has_thread_support()) {
		fprintf(stdout, "  -threads <num_threads>\n"
			"    Number of threads to use for decoding.\n");
	}
	fprintf(stdout, "  -quiet\n"
		"    Disable output from the library and other output.\n");
	/* UniPG>> */
#ifdef USE_JPWL
	fprintf(stdout, "  -W <options>\n"
		"    Activates the JPWL correction capability, if the codestream complies.\n"
		"    Options can be a comma separated list of <param=val> tokens:\n"
		"    c, c=numcomps\n"
		"       numcomps is the number of expected components in the codestream\n"
		"       (search of first EPB rely upon this, default is %d)\n",
		JPWL_EXPECTED_COMPONENTS);
#endif /* USE_JPWL */
	/* <<UniPG */
	fprintf(stdout, "\n");
}

/* -------------------------------------------------------------------------- */

static OPJ_BOOL parse_precision(const char* option,
	opj_decompress_parameters* parameters)
{
	const char* l_remaining = option;
	OPJ_BOOL l_result = OPJ_TRUE;

	/* reset */
	if (parameters->precision) {
		free(parameters->precision);
		parameters->precision = NULL;
	}
	parameters->nb_precision = 0U;

	for (;;) {
		int prec;
		char mode;
		char comma;
		int count;

		count = sscanf(l_remaining, "%d%c%c", &prec, &mode, &comma);
		if (count == 1) {
			mode = 'C';
			count++;
		}
		if ((count == 2) || (mode == ',')) {
			if (mode == ',') {
				mode = 'C';
			}
			comma = ',';
			count = 3;
		}
		if (count == 3) {
			if ((prec < 1) || (prec > 32)) {
				fprintf(stderr, "Invalid precision %d in precision option %s\n", prec, option);
				l_result = OPJ_FALSE;
				break;
			}
			if ((mode != 'C') && (mode != 'S')) {
				fprintf(stderr, "Invalid precision mode %c in precision option %s\n", mode,
					option);
				l_result = OPJ_FALSE;
				break;
			}
			if (comma != ',') {
				fprintf(stderr, "Invalid character %c in precision option %s\n", comma, option);
				l_result = OPJ_FALSE;
				break;
			}

			if (parameters->precision == NULL) {
				/* first one */
				parameters->precision = (opj_precision *)malloc(sizeof(opj_precision));
				if (parameters->precision == NULL) {
					fprintf(stderr, "Could not allocate memory for precision option\n");
					l_result = OPJ_FALSE;
					break;
				}
			}
			else {
				OPJ_UINT32 l_new_size = parameters->nb_precision + 1U;
				opj_precision* l_new;

				if (l_new_size == 0U) {
					fprintf(stderr, "Could not allocate memory for precision option\n");
					l_result = OPJ_FALSE;
					break;
				}

				l_new = (opj_precision *)realloc(parameters->precision,
					l_new_size * sizeof(opj_precision));
				if (l_new == NULL) {
					fprintf(stderr, "Could not allocate memory for precision option\n");
					l_result = OPJ_FALSE;
					break;
				}
				parameters->precision = l_new;
			}

			parameters->precision[parameters->nb_precision].prec = (OPJ_UINT32)prec;
			switch (mode) {
			case 'C':
				parameters->precision[parameters->nb_precision].mode = OPJ_PREC_MODE_CLIP;
				break;
			case 'S':
				parameters->precision[parameters->nb_precision].mode = OPJ_PREC_MODE_SCALE;
				break;
			default:
				break;
			}
			parameters->nb_precision++;

			l_remaining = strchr(l_remaining, ',');
			if (l_remaining == NULL) {
				break;
			}
			l_remaining += 1;
		}
		else {
			fprintf(stderr, "Could not parse precision option %s\n", option);
			l_result = OPJ_FALSE;
			break;
		}
	}

	return l_result;
}

/* -------------------------------------------------------------------------- */

int get_num_images(char *imgdirpath)
{
	DIR *dir;
	struct dirent* content;
	int num_images = 0;

	/* Reading the input images from given input directory */

	dir = opendir(imgdirpath);
	if (!dir) {
		fprintf(stderr, "Could not open Folder %s\n", imgdirpath);
		return 0;
	}

	while ((content = readdir(dir)) != NULL) {
		if (strcmp(".", content->d_name) == 0 || strcmp("..", content->d_name) == 0) {
			continue;
		}
		num_images++;
	}
	closedir(dir);
	return num_images;
}

/* -------------------------------------------------------------------------- */
int load_images(dircnt_t *dirptr, char *imgdirpath)
{
	DIR *dir;
	struct dirent* content;
	int i = 0;

	/* Reading the input images from given input directory */

	dir = opendir(imgdirpath);
	if (!dir) {
		fprintf(stderr, "Could not open Folder %s\n", imgdirpath);
		return 1;
	}
	else {
		fprintf(stderr, "Folder opened successfully\n");
	}

	while ((content = readdir(dir)) != NULL) {
		if (strcmp(".", content->d_name) == 0 || strcmp("..", content->d_name) == 0) {
			continue;
		}

		strcpy(dirptr->filename[i], content->d_name);
		i++;
	}
	closedir(dir);
	return 0;
}

/* -------------------------------------------------------------------------- */
int get_file_format(const char *filename)
{
	unsigned int i;
	static const char *extension[] = { "pgx", "pnm", "pgm", "ppm", "bmp", "tif", "raw", "rawl", "tga", "png", "j2k", "jp2", "jpt", "j2c", "jpc" };
	static const int format[] = { PGX_DFMT, PXM_DFMT, PXM_DFMT, PXM_DFMT, BMP_DFMT, TIF_DFMT, RAW_DFMT, RAWL_DFMT, TGA_DFMT, PNG_DFMT, J2K_CFMT, JP2_CFMT, JPT_CFMT, J2K_CFMT, J2K_CFMT };
	const char * ext = strrchr(filename, '.');
	if (ext == NULL) {
		return -1;
	}
	ext++;
	if (*ext) {
		for (i = 0; i < sizeof(format) / sizeof(*format); i++) {
			if (strcasecmp(ext, extension[i]) == 0) {
				return format[i];
			}
		}
	}

	return -1;
}

/* -------------------------------------------------------------------------- */

#ifdef _WIN32
const char* path_separator = "\\";
#else
const char* path_separator = "/";
#endif

/* -------------------------------------------------------------------------- */

char get_next_file(int imageno, dircnt_t *dirptr, img_fol_t *img_fol,
	opj_decompress_parameters *parameters)
{
	char image_filename[OPJ_PATH_LEN], infilename[OPJ_PATH_LEN],
		outfilename[OPJ_PATH_LEN], temp_ofname[OPJ_PATH_LEN];
	char *temp_p, temp1[OPJ_PATH_LEN] = "";

	strcpy(image_filename, dirptr->filename[imageno]);
	fprintf(stderr, "File Number %d \"%s\"\n", imageno, image_filename);
	sprintf(infilename, "%s%s%s", img_fol->imgdirpath, path_separator,
		image_filename);
	parameters->decod_format = infile_format(infilename);
	if (parameters->decod_format == -1) {
		return 1;
	}
	if (opj_strcpy_s(parameters->infile, sizeof(parameters->infile),
		infilename) != 0) {
		return 1;
	}

	/*Set output file*/
	strcpy(temp_ofname, strtok(image_filename, "."));
	while ((temp_p = strtok(NULL, ".")) != NULL) {
		strcat(temp_ofname, temp1);
		sprintf(temp1, ".%s", temp_p);
	}
	if (img_fol->set_out_format == 1) {
		sprintf(outfilename, "%s/%s.%s", img_fol->imgdirpath, temp_ofname,
			img_fol->out_format);
		if (opj_strcpy_s(parameters->outfile, sizeof(parameters->outfile),
			outfilename) != 0) {
			return 1;
		}
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
#define JP2_RFC3745_MAGIC "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a"
#define JP2_MAGIC "\x0d\x0a\x87\x0a"
/* position 45: "\xff\x52" */
#define J2K_CODESTREAM_MAGIC "\xff\x4f\xff\x51"

static int infile_format(const char *fname)
{
	FILE *reader;
	const char *s, *magic_s;
	int ext_format, magic_format;
	unsigned char buf[12];
	OPJ_SIZE_T l_nb_read;

	reader = fopen(fname, "rb");

	if (reader == NULL) {
		return -2;
	}

	memset(buf, 0, 12);
	l_nb_read = fread(buf, 1, 12, reader);
	fclose(reader);
	if (l_nb_read != 12) {
		return -1;
	}

	ext_format = get_file_format(fname);

	if (ext_format == JPT_CFMT) {
		return JPT_CFMT;
	}

	if (memcmp(buf, JP2_RFC3745_MAGIC, 12) == 0 || memcmp(buf, JP2_MAGIC, 4) == 0) {
		magic_format = JP2_CFMT;
		magic_s = ".jp2";
	}
	else if (memcmp(buf, J2K_CODESTREAM_MAGIC, 4) == 0) {
		magic_format = J2K_CFMT;
		magic_s = ".j2k or .jpc or .j2c";
	}
	else {
		return -1;
	}

	if (magic_format == ext_format) {
		return ext_format;
	}

	s = fname + strlen(fname) - 4;

	fputs("\n===========================================\n", stderr);
	fprintf(stderr, "The extension of this file is incorrect.\n"
		"FOUND %s. SHOULD BE %s\n", s, magic_s);
	fputs("===========================================\n", stderr);

	return magic_format;
}

/** Determines the type of the stream (only J2K, JP2 are supported - no JPIP) */
static int instream_format(const unsigned char *stream, OPJ_SIZE_T streamSize, int isJPT)
{
	int magic_format;
	unsigned char buf[12];

	if (streamSize < 12)
		return -1;

	memset(buf, 0, 12);
	memcpy(buf, stream, 12);

	if (isJPT == 1)
		return JPT_CFMT;

	if (memcmp(buf, JP2_RFC3745_MAGIC, 12) == 0 || memcmp(buf, JP2_MAGIC, 4) == 0)
	{
		magic_format = JP2_CFMT;
		// magic_s = ".jp2";
	}
	else if (memcmp(buf, J2K_CODESTREAM_MAGIC, 4) == 0)
	{
		magic_format = J2K_CFMT;
		// magic_s = ".j2k or .jpc or .j2c";
	}
	else
	{
		return -1;
	}

	return magic_format;
}

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/**
 * Parse the command line
 */
 /* -------------------------------------------------------------------------- */

int parse_cmdline_decoder(int argc, char **argv,
	opj_decompress_parameters *parameters, img_fol_t *img_fol)
{
	/* parse the command line */
	int totlen, c;
	opj_option_t long_option[] = {
		{"ImgDir",			REQ_ARG, NULL, 'y'},
		{"OutFor",			REQ_ARG, NULL, 'O'},
		{"force-rgb",		NO_ARG,  NULL, 1},
		{"upsample",		NO_ARG,  NULL, 1},
		{"split-pnm",		NO_ARG,  NULL, 1},
		{"threads",			REQ_ARG, NULL, 'T'},
		{"quiet",			NO_ARG,  NULL, 1},
	};

	const char optlist[] = "i:o:r:l:x:d:t:p:c:"

		/* UniPG>> */
#ifdef USE_JPWL
		"W:"
#endif /* USE_JPWL */
		/* <<UniPG */
		"h";

	long_option[2].flag = &(parameters->force_rgb);
	long_option[3].flag = &(parameters->upsample);
	long_option[4].flag = &(parameters->split_pnm);
	long_option[6].flag = &(parameters->quiet);
	totlen = sizeof(long_option);
	opj_reset_options_reading();
	img_fol->set_out_format = 0;
	do {
		c = opj_getopt_long(argc, argv, optlist, long_option, totlen);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 0: /* long opt with flag */
			break;
		case 'i': {         /* input file */
			char *infile = opj_optarg;
			parameters->decod_format = infile_format(infile);
			switch (parameters->decod_format) {
			case J2K_CFMT:
				break;
			case JP2_CFMT:
				break;
			case JPT_CFMT:
				break;
			case -2:
				fprintf(stderr,
					"!! infile cannot be read: %s !!\n\n",
					infile);
				return 1;
			default:
				fprintf(stderr,
					"[ERROR] Unknown input file format: %s \n"
					"        Known file formats are *.j2k, *.jp2, *.jpc or *.jpt\n",
					infile);
				return 1;
			}
			if (opj_strcpy_s(parameters->infile, sizeof(parameters->infile), infile) != 0) {
				fprintf(stderr, "[ERROR] Path is too long\n");
				return 1;
			}
		}
				  break;

				  /* ----------------------------------------------------- */

		case 'o': {         /* output file */
			char *outfile = opj_optarg;
			parameters->cod_format = get_file_format(outfile);
			switch (parameters->cod_format) {
			case PGX_DFMT:
				break;
			case PXM_DFMT:
				break;
			case BMP_DFMT:
				break;
			case TIF_DFMT:
				break;
			case RAW_DFMT:
				break;
			case RAWL_DFMT:
				break;
			case TGA_DFMT:
				break;
			case PNG_DFMT:
				break;
			default:
				fprintf(stderr,
					"Unknown output format image %s [only *.png, *.pnm, *.pgm, *.ppm, *.pgx, *.bmp, *.tif, *.raw or *.tga]!!\n",
					outfile);
				return 1;
			}
			if (opj_strcpy_s(parameters->outfile, sizeof(parameters->outfile),
				outfile) != 0) {
				fprintf(stderr, "[ERROR] Path is too long\n");
				return 1;
			}
		}
				  break;

				  /* ----------------------------------------------------- */

		case 'O': {         /* output format */
			char outformat[50];
			char *of = opj_optarg;
			sprintf(outformat, ".%s", of);
			img_fol->set_out_format = 1;
			parameters->cod_format = get_file_format(outformat);
			switch (parameters->cod_format) {
			case PGX_DFMT:
				img_fol->out_format = "pgx";
				break;
			case PXM_DFMT:
				img_fol->out_format = "ppm";
				break;
			case BMP_DFMT:
				img_fol->out_format = "bmp";
				break;
			case TIF_DFMT:
				img_fol->out_format = "tif";
				break;
			case RAW_DFMT:
				img_fol->out_format = "raw";
				break;
			case RAWL_DFMT:
				img_fol->out_format = "rawl";
				break;
			case TGA_DFMT:
				img_fol->out_format = "raw";
				break;
			case PNG_DFMT:
				img_fol->out_format = "png";
				break;
			default:
				fprintf(stderr,
					"Unknown output format image %s [only *.png, *.pnm, *.pgm, *.ppm, *.pgx, *.bmp, *.tif, *.raw or *.tga]!!\n",
					outformat);
				return 1;
				break;
			}
		}
				  break;

				  /* ----------------------------------------------------- */


		case 'r': {     /* reduce option */
			sscanf(opj_optarg, "%u", &(parameters->core.cp_reduce));
		}
				  break;

				  /* ----------------------------------------------------- */


		case 'l': {     /* layering option */
			sscanf(opj_optarg, "%u", &(parameters->core.cp_layer));
		}
				  break;

				  /* ----------------------------------------------------- */

		case 'h':           /* display an help description */
			decode_help_display();
			return 1;

			/* ----------------------------------------------------- */

		case 'y': {         /* Image Directory path */
			img_fol->imgdirpath = (char*)malloc(strlen(opj_optarg) + 1);
			if (img_fol->imgdirpath == NULL) {
				return 1;
			}
			strcpy(img_fol->imgdirpath, opj_optarg);
			img_fol->set_imgdir = 1;
		}
				  break;

				  /* ----------------------------------------------------- */

		case 'd': {         /* Input decode ROI */
			size_t size_optarg = (size_t)strlen(opj_optarg) + 1U;
			char *ROI_values = (char*)malloc(size_optarg);
			if (ROI_values == NULL) {
				fprintf(stderr, "[ERROR] Couldn't allocate memory\n");
				return 1;
			}
			ROI_values[0] = '\0';
			memcpy(ROI_values, opj_optarg, size_optarg);
			/*printf("ROI_values = %s [%d / %d]\n", ROI_values, strlen(ROI_values), size_optarg ); */
			parse_DA_values(ROI_values, &parameters->DA_x0, &parameters->DA_y0,
				&parameters->DA_x1, &parameters->DA_y1);

			free(ROI_values);
		}
				  break;

				  /* ----------------------------------------------------- */

		case 't': {         /* Input tile index */
			sscanf(opj_optarg, "%u", &parameters->tile_index);
			parameters->nb_tile_to_decode = 1;
		}
				  break;

				  /* ----------------------------------------------------- */

		case 'x': {         /* Creation of index file */
			if (opj_strcpy_s(parameters->indexfilename, sizeof(parameters->indexfilename),
				opj_optarg) != 0) {
				fprintf(stderr, "[ERROR] Path is too long\n");
				return 1;
			}
		}
				  break;

				  /* ----------------------------------------------------- */
		case 'p': { /* Force precision */
			if (!parse_precision(opj_optarg, parameters)) {
				return 1;
			}
		}
				  break;

				  /* ----------------------------------------------------- */
		case 'c': { /* Componenets */
			const char* iter = opj_optarg;
			while (1) {
				parameters->numcomps++;
				parameters->comps_indices = (OPJ_UINT32*)realloc(
					parameters->comps_indices,
					parameters->numcomps * sizeof(OPJ_UINT32));
				parameters->comps_indices[parameters->numcomps - 1] =
					(OPJ_UINT32)atoi(iter);
				iter = strchr(iter, ',');
				if (iter == NULL) {
					break;
				}
				iter++;
			}
		}
				  break;
				  /* ----------------------------------------------------- */

				  /* UniPG>> */
#ifdef USE_JPWL

		case 'W': {         /* activate JPWL correction */
			char *token = NULL;

			token = strtok(opj_optarg, ",");
			while (token != NULL) {

				/* search expected number of components */
				if (*token == 'c') {

					static int compno;

					compno = JPWL_EXPECTED_COMPONENTS; /* predefined no. of components */

					if (sscanf(token, "c=%d", &compno) == 1) {
						/* Specified */
						if ((compno < 1) || (compno > 256)) {
							fprintf(stderr, "ERROR -> invalid number of components c = %d\n", compno);
							return 1;
						}
						parameters->jpwl_exp_comps = compno;

					}
					else if (!strcmp(token, "c")) {
						/* default */
						parameters->jpwl_exp_comps = compno; /* auto for default size */

					}
					else {
						fprintf(stderr, "ERROR -> invalid components specified = %s\n", token);
						return 1;
					};
				}

				/* search maximum number of tiles */
				if (*token == 't') {

					static int tileno;

					tileno = JPWL_MAXIMUM_TILES; /* maximum no. of tiles */

					if (sscanf(token, "t=%d", &tileno) == 1) {
						/* Specified */
						if ((tileno < 1) || (tileno > JPWL_MAXIMUM_TILES)) {
							fprintf(stderr, "ERROR -> invalid number of tiles t = %d\n", tileno);
							return 1;
						}
						parameters->jpwl_max_tiles = tileno;

					}
					else if (!strcmp(token, "t")) {
						/* default */
						parameters->jpwl_max_tiles = tileno; /* auto for default size */

					}
					else {
						fprintf(stderr, "ERROR -> invalid tiles specified = %s\n", token);
						return 1;
					};
				}

				/* next token or bust */
				token = strtok(NULL, ",");
			};
			parameters->jpwl_correct = OPJ_TRUE;
			if (!(parameter->quiet)) {
				fprintf(stdout, "JPWL correction capability activated\n");
				fprintf(stdout, "- expecting %d components\n", parameters->jpwl_exp_comps);
			}
		}
				  break;
#endif /* USE_JPWL */
				  /* <<UniPG */

				  /* ----------------------------------------------------- */
		case 'T': { /* Number of threads */
			if (strcmp(opj_optarg, "ALL_CPUS") == 0) {
				parameters->num_threads = opj_get_num_cpus();
				if (parameters->num_threads == 1) {
					parameters->num_threads = 0;
				}
			}
			else {
				sscanf(opj_optarg, "%d", &parameters->num_threads);
			}
		}
				  break;

				  /* ----------------------------------------------------- */

		default:
			fprintf(stderr, "[WARNING] An invalid option has been ignored.\n");
			break;
		}
	} while (c != -1);

	/* check for possible errors */

	//	If setCompressed() has been called 
	if (parameters->isInputStreamSet == 1)
	{
		if (img_fol->set_imgdir == 1)
		{
			fprintf(stderr, "[ERROR] Cannot call setCompressed() and set option -ImgDir at the same time.\n");
			return 1;
		}

		if (!(parameters->infile[0] == 0) || !(img_fol->set_out_format == 0) || !(parameters->outfile[0] == 0))
		{
			fprintf(stderr, "[ERROR] Cannot call setCompressed() and set options -i or -o or -OutFor at the same time.\n");
			return 1;
		}
	}

	//	If -ImgDir set
	else if (img_fol->set_imgdir == 1)
	{
		if (!(parameters->infile[0] == 0))
		{
			fprintf(stderr, "[ERROR] options -ImgDir and -i cannot be used together.\n");
			return 1;
		}
		if (img_fol->set_out_format == 0)
		{
			fprintf(stderr,
				"[ERROR] When -ImgDir is used, -OutFor <FORMAT> must be used.\n");
			fprintf(stderr, "Only one format allowed.\n"
				"Valid format are PGM, PPM, PNM, PGX, BMP, TIF, RAW and TGA.\n");
			return 1;
		}
		if (!((parameters->outfile[0] == 0)))
		{
			fprintf(stderr, "[ERROR] options -ImgDir and -o cannot be used together.\n");
			return 1;
		}
	}
	else
	{
		if (((parameters->isInputStreamSet == 0) && (parameters->infile[0] == 0)) || (parameters->outfile[0] == 0))
		{
			fprintf(stderr, "[ERROR] Required parameters are missing\n"
				"Example: %s -i image.j2k -o image.pgm\n", argv[0]);
			fprintf(stderr, "   Help: %s -h\n", argv[0]);
			return 1;
		}
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
/**
 * Parse decoding area input values
 * separator = ","
 */
 /* -------------------------------------------------------------------------- */
int parse_DA_values(char* inArg, unsigned int *DA_x0, unsigned int *DA_y0,
	unsigned int *DA_x1, unsigned int *DA_y1)
{
	int it = 0;
	int values[4];
	char delims[] = ",";
	char *result = NULL;
	result = strtok(inArg, delims);

	while ((result != NULL) && (it < 4)) {
		values[it] = atoi(result);
		result = strtok(NULL, delims);
		it++;
	}

	if (it != 4) {
		return EXIT_FAILURE;
	}
	else {
		*DA_x0 = (OPJ_UINT32)values[0];
		*DA_y0 = (OPJ_UINT32)values[1];
		*DA_x1 = (OPJ_UINT32)values[2];
		*DA_y1 = (OPJ_UINT32)values[3];
		return EXIT_SUCCESS;
	}
}

OPJ_FLOAT64 opj_clock(void)
{
#ifdef _WIN32
	/* _WIN32: use QueryPerformance (very accurate) */
	LARGE_INTEGER freq, t;
	/* freq is the clock speed of the CPU */
	QueryPerformanceFrequency(&freq);
	/* cout << "freq = " << ((double) freq.QuadPart) << endl; */
	/* t is the high resolution performance counter (see MSDN) */
	QueryPerformanceCounter(&t);
	return freq.QuadPart ? ((OPJ_FLOAT64)t.QuadPart / (OPJ_FLOAT64)freq.QuadPart) :
		0;
#elif defined(__linux)
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return ((OPJ_FLOAT64)ts.tv_sec + (OPJ_FLOAT64)ts.tv_nsec * 1e-9);
#else
	/* Unix : use resource usage */
	/* FIXME: this counts the total CPU time, instead of the user perceived time */
	struct rusage t;
	OPJ_FLOAT64 procTime;
	/* (1) Get the rusage data structure at this moment (man getrusage) */
	getrusage(0, &t);
	/* (2) What is the elapsed time ? - CPU time = User time + System time */
	/* (2a) Get the seconds */
	procTime = (OPJ_FLOAT64)(t.ru_utime.tv_sec + t.ru_stime.tv_sec);
	/* (2b) More precisely! Get the microseconds part ! */
	return (procTime + (OPJ_FLOAT64)(t.ru_utime.tv_usec + t.ru_stime.tv_usec) *
		1e-6);
#endif
}

/* -------------------------------------------------------------------------- */

/**
error callback returning the message to Java andexpecting a callback_variables_t client object
*/
void error_callback(const char *msg, void *client_data) {
	callback_variables_t* vars = (callback_variables_t*) client_data;
	JNIEnv *env = vars->env;
	jstring jbuffer;

	jbuffer = (*env)->NewStringUTF(env, msg);
	(*env)->ExceptionClear(env);
	(*env)->CallVoidMethod(env, *(vars->jobj), vars->error_mid, jbuffer);

	if ((*env)->ExceptionOccurred(env)) {
		fprintf(stderr,"C: Exception during call back method\n");
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	(*env)->DeleteLocalRef(env, jbuffer);
}
/**
warning callback returning the message to Java andexpecting a callback_variables_t client object
*/
void warning_callback(const char *msg, void *client_data) {
	callback_variables_t* vars = (callback_variables_t*) client_data;
	JNIEnv *env = vars->env;
	jstring jbuffer;

	jbuffer = (*env)->NewStringUTF(env, msg);
	(*env)->ExceptionClear(env);
	(*env)->CallVoidMethod(env, *(vars->jobj), vars->message_mid, jbuffer);
	
	if ((*env)->ExceptionOccurred(env)) {
		fprintf(stderr,"C: Exception during call back method\n");
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	(*env)->DeleteLocalRef(env, jbuffer);
}
/**
information callback returning the message to Java andexpecting a callback_variables_t client object
*/
void info_callback(const char *msg, void *client_data) {
	callback_variables_t* vars = (callback_variables_t*) client_data;
	JNIEnv *env = vars->env;
	jstring jbuffer;

	jbuffer = (*env)->NewStringUTF(env, msg);
	(*env)->ExceptionClear(env);
	(*env)->CallVoidMethod(env, *(vars->jobj), vars->message_mid, jbuffer);

	if ((*env)->ExceptionOccurred(env)) {
		fprintf(stderr,"C: Exception during call back method\n");
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
	}
	(*env)->DeleteLocalRef(env, jbuffer);
}

/* -------------------------------------------------------------------------- */

static void set_default_parameters(opj_decompress_parameters* parameters)
{
	if (parameters) {
		memset(parameters, 0, sizeof(opj_decompress_parameters));

		/* default decoding parameters (command line specific) */
		parameters->decod_format = -1;
		parameters->cod_format = -1;

		/* default decoding parameters (core) */
		opj_set_default_decoder_parameters(&(parameters->core));
	}
}

static void destroy_parameters(opj_decompress_parameters* parameters)
{
	if (parameters) {
		if (parameters->precision) {
			free(parameters->precision);
			parameters->precision = NULL;
		}

		free(parameters->comps_indices);
		parameters->comps_indices = NULL;
	}
}

/* -------------------------------------------------------------------------- */

static opj_image_t* convert_gray_to_rgb(opj_image_t* original)
{
	OPJ_UINT32 compno;
	opj_image_t* l_new_image = NULL;
	opj_image_cmptparm_t* l_new_components = NULL;

	l_new_components = (opj_image_cmptparm_t*)malloc((original->numcomps + 2U) *
		sizeof(opj_image_cmptparm_t));
	if (l_new_components == NULL) {
		fprintf(stderr,
			"ERROR -> opj_decompress: failed to allocate memory for RGB image!\n");
		opj_image_destroy(original);
		return NULL;
	}

	l_new_components[0].bpp = l_new_components[1].bpp = l_new_components[2].bpp =
		original->comps[0].bpp;
	l_new_components[0].dx = l_new_components[1].dx = l_new_components[2].dx =
		original->comps[0].dx;
	l_new_components[0].dy = l_new_components[1].dy = l_new_components[2].dy =
		original->comps[0].dy;
	l_new_components[0].h = l_new_components[1].h = l_new_components[2].h =
		original->comps[0].h;
	l_new_components[0].w = l_new_components[1].w = l_new_components[2].w =
		original->comps[0].w;
	l_new_components[0].prec = l_new_components[1].prec = l_new_components[2].prec =
		original->comps[0].prec;
	l_new_components[0].sgnd = l_new_components[1].sgnd = l_new_components[2].sgnd =
		original->comps[0].sgnd;
	l_new_components[0].x0 = l_new_components[1].x0 = l_new_components[2].x0 =
		original->comps[0].x0;
	l_new_components[0].y0 = l_new_components[1].y0 = l_new_components[2].y0 =
		original->comps[0].y0;

	for (compno = 1U; compno < original->numcomps; ++compno) {
		l_new_components[compno + 2U].bpp = original->comps[compno].bpp;
		l_new_components[compno + 2U].dx = original->comps[compno].dx;
		l_new_components[compno + 2U].dy = original->comps[compno].dy;
		l_new_components[compno + 2U].h = original->comps[compno].h;
		l_new_components[compno + 2U].w = original->comps[compno].w;
		l_new_components[compno + 2U].prec = original->comps[compno].prec;
		l_new_components[compno + 2U].sgnd = original->comps[compno].sgnd;
		l_new_components[compno + 2U].x0 = original->comps[compno].x0;
		l_new_components[compno + 2U].y0 = original->comps[compno].y0;
	}

	l_new_image = opj_image_create(original->numcomps + 2U, l_new_components,
		OPJ_CLRSPC_SRGB);
	free(l_new_components);
	if (l_new_image == NULL) {
		fprintf(stderr,
			"ERROR -> opj_decompress: failed to allocate memory for RGB image!\n");
		opj_image_destroy(original);
		return NULL;
	}

	l_new_image->x0 = original->x0;
	l_new_image->x1 = original->x1;
	l_new_image->y0 = original->y0;
	l_new_image->y1 = original->y1;

	l_new_image->comps[0].factor = l_new_image->comps[1].factor =
		l_new_image->comps[2].factor = original->comps[0].factor;
	l_new_image->comps[0].alpha = l_new_image->comps[1].alpha =
		l_new_image->comps[2].alpha = original->comps[0].alpha;
	l_new_image->comps[0].resno_decoded = l_new_image->comps[1].resno_decoded =
		l_new_image->comps[2].resno_decoded = original->comps[0].resno_decoded;

	memcpy(l_new_image->comps[0].data, original->comps[0].data,
		original->comps[0].w * original->comps[0].h * sizeof(OPJ_INT32));
	memcpy(l_new_image->comps[1].data, original->comps[0].data,
		original->comps[0].w * original->comps[0].h * sizeof(OPJ_INT32));
	memcpy(l_new_image->comps[2].data, original->comps[0].data,
		original->comps[0].w * original->comps[0].h * sizeof(OPJ_INT32));

	for (compno = 1U; compno < original->numcomps; ++compno) {
		l_new_image->comps[compno + 2U].factor = original->comps[compno].factor;
		l_new_image->comps[compno + 2U].alpha = original->comps[compno].alpha;
		l_new_image->comps[compno + 2U].resno_decoded =
			original->comps[compno].resno_decoded;
		memcpy(l_new_image->comps[compno + 2U].data, original->comps[compno].data,
			original->comps[compno].w * original->comps[compno].h * sizeof(OPJ_INT32));
	}
	opj_image_destroy(original);
	return l_new_image;
}

/* -------------------------------------------------------------------------- */

static opj_image_t* upsample_image_components(opj_image_t* original)
{
	opj_image_t* l_new_image = NULL;
	opj_image_cmptparm_t* l_new_components = NULL;
	OPJ_BOOL l_upsample_need = OPJ_FALSE;
	OPJ_UINT32 compno;

	for (compno = 0U; compno < original->numcomps; ++compno) {
		if (original->comps[compno].factor > 0U) {
			fprintf(stderr,
				"ERROR -> opj_decompress: -upsample not supported with reduction\n");
			opj_image_destroy(original);
			return NULL;
		}
		if ((original->comps[compno].dx > 1U) || (original->comps[compno].dy > 1U)) {
			l_upsample_need = OPJ_TRUE;
			break;
		}
	}
	if (!l_upsample_need) {
		return original;
	}
	/* Upsample is needed */
	l_new_components = (opj_image_cmptparm_t*)malloc(original->numcomps * sizeof(
		opj_image_cmptparm_t));
	if (l_new_components == NULL) {
		fprintf(stderr,
			"ERROR -> opj_decompress: failed to allocate memory for upsampled components!\n");
		opj_image_destroy(original);
		return NULL;
	}

	for (compno = 0U; compno < original->numcomps; ++compno) {
		opj_image_cmptparm_t* l_new_cmp = &(l_new_components[compno]);
		opj_image_comp_t*     l_org_cmp = &(original->comps[compno]);

		l_new_cmp->bpp = l_org_cmp->bpp;
		l_new_cmp->prec = l_org_cmp->prec;
		l_new_cmp->sgnd = l_org_cmp->sgnd;
		l_new_cmp->x0 = original->x0;
		l_new_cmp->y0 = original->y0;
		l_new_cmp->dx = 1;
		l_new_cmp->dy = 1;
		l_new_cmp->w =
			l_org_cmp->w; /* should be original->x1 - original->x0 for dx==1 */
		l_new_cmp->h =
			l_org_cmp->h; /* should be original->y1 - original->y0 for dy==0 */

		if (l_org_cmp->dx > 1U) {
			l_new_cmp->w = original->x1 - original->x0;
		}

		if (l_org_cmp->dy > 1U) {
			l_new_cmp->h = original->y1 - original->y0;
		}
	}

	l_new_image = opj_image_create(original->numcomps, l_new_components,
		original->color_space);
	free(l_new_components);
	if (l_new_image == NULL) {
		fprintf(stderr,
			"ERROR -> opj_decompress: failed to allocate memory for upsampled components!\n");
		opj_image_destroy(original);
		return NULL;
	}

	l_new_image->x0 = original->x0;
	l_new_image->x1 = original->x1;
	l_new_image->y0 = original->y0;
	l_new_image->y1 = original->y1;

	for (compno = 0U; compno < original->numcomps; ++compno) {
		opj_image_comp_t* l_new_cmp = &(l_new_image->comps[compno]);
		opj_image_comp_t* l_org_cmp = &(original->comps[compno]);

		l_new_cmp->factor = l_org_cmp->factor;
		l_new_cmp->alpha = l_org_cmp->alpha;
		l_new_cmp->resno_decoded = l_org_cmp->resno_decoded;

		if ((l_org_cmp->dx > 1U) || (l_org_cmp->dy > 1U)) {
			const OPJ_INT32* l_src = l_org_cmp->data;
			OPJ_INT32*       l_dst = l_new_cmp->data;
			OPJ_UINT32 y;
			OPJ_UINT32 xoff, yoff;

			/* need to take into account dx & dy */
			xoff = l_org_cmp->dx * l_org_cmp->x0 - original->x0;
			yoff = l_org_cmp->dy * l_org_cmp->y0 - original->y0;
			if ((xoff >= l_org_cmp->dx) || (yoff >= l_org_cmp->dy)) {
				fprintf(stderr,
					"ERROR -> opj_decompress: Invalid image/component parameters found when upsampling\n");
				opj_image_destroy(original);
				opj_image_destroy(l_new_image);
				return NULL;
			}

			for (y = 0U; y < yoff; ++y) {
				memset(l_dst, 0U, l_new_cmp->w * sizeof(OPJ_INT32));
				l_dst += l_new_cmp->w;
			}

			if (l_new_cmp->h > (l_org_cmp->dy -
				1U)) { /* check subtraction overflow for really small images */
				for (; y < l_new_cmp->h - (l_org_cmp->dy - 1U); y += l_org_cmp->dy) {
					OPJ_UINT32 x, dy;
					OPJ_UINT32 xorg;

					xorg = 0U;
					for (x = 0U; x < xoff; ++x) {
						l_dst[x] = 0;
					}
					if (l_new_cmp->w > (l_org_cmp->dx -
						1U)) { /* check subtraction overflow for really small images */
						for (; x < l_new_cmp->w - (l_org_cmp->dx - 1U); x += l_org_cmp->dx, ++xorg) {
							OPJ_UINT32 dx;
							for (dx = 0U; dx < l_org_cmp->dx; ++dx) {
								l_dst[x + dx] = l_src[xorg];
							}
						}
					}
					for (; x < l_new_cmp->w; ++x) {
						l_dst[x] = l_src[xorg];
					}
					l_dst += l_new_cmp->w;

					for (dy = 1U; dy < l_org_cmp->dy; ++dy) {
						memcpy(l_dst, l_dst - l_new_cmp->w, l_new_cmp->w * sizeof(OPJ_INT32));
						l_dst += l_new_cmp->w;
					}
					l_src += l_org_cmp->w;
				}
			}
			if (y < l_new_cmp->h) {
				OPJ_UINT32 x;
				OPJ_UINT32 xorg;

				xorg = 0U;
				for (x = 0U; x < xoff; ++x) {
					l_dst[x] = 0;
				}
				if (l_new_cmp->w > (l_org_cmp->dx -
					1U)) { /* check subtraction overflow for really small images */
					for (; x < l_new_cmp->w - (l_org_cmp->dx - 1U); x += l_org_cmp->dx, ++xorg) {
						OPJ_UINT32 dx;
						for (dx = 0U; dx < l_org_cmp->dx; ++dx) {
							l_dst[x + dx] = l_src[xorg];
						}
					}
				}
				for (; x < l_new_cmp->w; ++x) {
					l_dst[x] = l_src[xorg];
				}
				l_dst += l_new_cmp->w;
				++y;
				for (; y < l_new_cmp->h; ++y) {
					memcpy(l_dst, l_dst - l_new_cmp->w, l_new_cmp->w * sizeof(OPJ_INT32));
					l_dst += l_new_cmp->w;
				}
			}
		}
		else {
			memcpy(l_new_cmp->data, l_org_cmp->data,
				l_org_cmp->w * l_org_cmp->h * sizeof(OPJ_INT32));
		}
	}
	opj_image_destroy(original);
	return l_new_image;
}

/* memset implementation which counters agressive dead-code elimination by some compilers */
volatile void* secure_memset(void *ptr, const int value, size_t num)
{
	volatile unsigned char *buf;
	buf = (volatile unsigned char*)ptr;

	while (num)
		buf[--num] = (unsigned char)value;

	return (volatile void*)ptr;
}
volatile void* secure_memclr(void *ptr, size_t num) {
	return (secure_memset(ptr, 0, num));
}

/* For locking */
static int opj_lock(void *addr, size_t len)
{
#if defined (_WIN32) && (_MSC_VER) && (!__INTEL_COMPILER)
	return (0 != VirtualLock(addr, len));
#else
	return (0 == mlock((const void *)addr, len));
#endif
}

/* For unlocking */
static int opj_unlock(void *addr, size_t len)
{
#if defined (_WIN32) && (_MSC_VER) && (!__INTEL_COMPILER)
	return (0 != VirtualUnlock(addr, len));
#else
	return (0 == munlock((const void *)addr, len));
#endif
}

/* -------------------------------------------------------------------------------------- */
/* -------------- Used instead of Files when setInputStream() is used --------------------*/

typedef struct stream_t
{
	unsigned char *originalStreamBytes;		/* Holds a reference to the beginning of the stream */
	unsigned char *currentStreamBytes;		/* Holds a reference to the current element referenced by currentPosition */
	OPJ_SIZE_T currentPosition;
	OPJ_SIZE_T totalSize;
	OPJ_SIZE_T actualSize;
	int isAllocated;
	int isLocked;
}Stream;

/* typedef OPJ_SIZE_T(* opj_stream_read_fn)(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data) ; */
/* typedef OPJ_SIZE_T(* opj_stream_write_fn)(void * p_buffer,  OPJ_SIZE_T p_nb_bytes, void * p_user_data) ; */
/* typedef OPJ_OFF_T(*opj_stream_skip_fn)(OPJ_OFF_T p_nb_bytes, void * p_user_data); */
/* typedef OPJ_BOOL(*opj_stream_seek_fn)(OPJ_OFF_T p_nb_bytes, void * p_user_data); */
/* typedef void (* opj_stream_free_user_data_fn)(void * p_user_data) ;*/

/** Reads data from stream into p_buffer, which should have already been allocated enough memory */
static OPJ_SIZE_T opj_read_from_stream(void * p_buffer, OPJ_SIZE_T p_nb_bytes, Stream *stream)
{
	OPJ_SIZE_T l_nb_read = 0;

	//	We're out of data => no more data to be read => return -1
	if (stream->currentPosition == stream->totalSize)
		return (OPJ_SIZE_T)-1;

	//	The amount of data to be read exceeds the amount of data available => set l_nb_read accordingly
	else if (stream->currentPosition + p_nb_bytes > stream->totalSize)
		l_nb_read = stream->totalSize - stream->currentPosition;

	//	Amount of available data is sufficient
	else
		l_nb_read = p_nb_bytes;

	//	Copy starting from the current position
	memcpy(p_buffer, stream->currentStreamBytes, l_nb_read);

	//	Increment current position accordingly
	stream->currentPosition += l_nb_read;

	//	Increment the reference to the current element as well
	stream->currentStreamBytes += l_nb_read;

	return l_nb_read;
}

/** Writes data from p_buffer into stream (allocates 512 bytes + p_nb_bytes bytes of memory each time it is needed) */
/** N.B : Never called as imagetobmp_stream() writes directly to Java code instead of writing to a Stream */
static OPJ_SIZE_T opj_write_to_stream(void * p_buffer, OPJ_SIZE_T p_nb_bytes, Stream *stream)
{
	//	Nothing to write => return -1
	if (p_nb_bytes == 0)
		return (OPJ_SIZE_T)-1;

	//	No available space => malloc 512 bytes + p_nb_bytes bytes
	if (stream->totalSize == 0)
	{
		stream->totalSize += 512 + p_nb_bytes;
		stream->originalStreamBytes = (unsigned char*)calloc(stream->totalSize, sizeof(unsigned char));

		stream->currentStreamBytes = stream->originalStreamBytes;	//	Set current element to the beginning of the stream
	}

	//	Amount of data to write exceeds available space => realloc + 512 bytes + p_nb_bytes bytes
	else if (stream->currentPosition + p_nb_bytes > stream->totalSize)
	{
		opj_unlock(stream->originalStreamBytes, stream->totalSize);

		stream->totalSize += 512 + p_nb_bytes;
		stream->originalStreamBytes = realloc(stream->originalStreamBytes, stream->totalSize);

		stream->currentStreamBytes = stream->originalStreamBytes + stream->currentPosition; //	Point the current element to the beginning + current position indicator
	}

	stream->isAllocated = 1;
	if (!opj_lock(stream->originalStreamBytes, stream->totalSize))
	{
		secure_memclr(stream->originalStreamBytes, stream->totalSize);
		free(stream->originalStreamBytes);
		stream->originalStreamBytes = NULL;
		stream->currentStreamBytes = NULL;
		return (OPJ_SIZE_T)-1;
	}

	stream->isLocked = 1;

	//	Copy into the current element and increment indicator
	memcpy(stream->currentStreamBytes, p_buffer, p_nb_bytes);

	stream->currentPosition += p_nb_bytes;
	stream->currentStreamBytes += p_nb_bytes;

	//	Set the actual size to the value of current position indicator
	stream->actualSize = stream->currentPosition;

	return p_nb_bytes;
}

//	Skips starting from current position
static OPJ_OFF_T opj_skip_from_stream(OPJ_OFF_T p_nb_bytes, Stream *stream)
{
	if (stream->currentPosition + p_nb_bytes > stream->totalSize)
		return -1;

	stream->currentPosition += p_nb_bytes;
	stream->currentStreamBytes = stream->originalStreamBytes + stream->currentPosition;
	return p_nb_bytes;
}

//	Skips starting from position 0
static OPJ_BOOL opj_seek_from_stream(OPJ_OFF_T p_nb_bytes, Stream *stream)
{
	if (p_nb_bytes > stream->totalSize)
		return OPJ_FALSE;

	stream->currentPosition = p_nb_bytes;
	stream->currentStreamBytes = stream->originalStreamBytes + stream->currentPosition;

	return OPJ_TRUE;
}

//	Unlocks, frees then sets the whole struct to 0
static void opj_free_from_stream(Stream *stream)
{
	if (stream != NULL)
	{
		if (stream->isLocked)
			opj_unlock(stream->originalStreamBytes, stream->totalSize);

		if (stream->isAllocated)
			free(stream->originalStreamBytes);

		secure_memclr(stream, sizeof(Stream));
	}
}

/* Equivalent to opj_stream_create_file_stream() but using a Stream instead of a File */
static opj_stream_t* opj_stream_create_file_stream_from_input_stream(
	Stream *inputStream,
	OPJ_SIZE_T size,
	OPJ_BOOL is_read_stream)
{
	opj_stream_t* l_stream = 00;

	l_stream = opj_stream_create(size, is_read_stream);

	if (!l_stream) 
	{
		return NULL;
	}

	opj_stream_set_user_data(l_stream, inputStream, (opj_stream_free_user_data_fn)opj_free_from_stream);
	opj_stream_set_user_data_length(l_stream, inputStream->totalSize);
	opj_stream_set_read_function(l_stream, (opj_stream_read_fn)opj_read_from_stream);
	opj_stream_set_write_function(l_stream, NULL);
	opj_stream_set_skip_function(l_stream, (opj_stream_skip_fn)opj_skip_from_stream);
	opj_stream_set_seek_function(l_stream, (opj_stream_seek_fn)opj_seek_from_stream);

	return l_stream;
}

/* Equivalent to opj_stream_create_default_file_stream() but using a Stream instead of a File */
static opj_stream_t* opj_stream_create_default_file_stream_from_input_stream(
	Stream *inputStream,
	OPJ_BOOL is_read_stream)
{
	return opj_stream_create_file_stream_from_input_stream(inputStream, OPJ_J2K_STREAM_CHUNK_SIZE, is_read_stream);
}

/* Equivalent to imagetobmp() but writing directly to the Java code */
static int imagetobmp_stream(opj_image_t * image, callback_variables_t msgErrorCallback_vars)
{
	int w = 0, h = 0;
	int i = 0, pad = 0;
	int adjustR = 0, adjustG = 0, adjustB = 0;
	char *tmp;
	OPJ_SIZE_T tmpSize = 0;
	int failed = 0;
	jstring message = NULL;
	char err[128];

	JNIEnv *env = msgErrorCallback_vars.env;
	jobject obj = *msgErrorCallback_vars.jobj;
	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = NULL;								/* Java method call */
	jbyteArray jba = NULL;

	/* Method to write to outputStream in JAVA */
	mid = (*env)->GetMethodID(env, cls, "writeToOutputStream", "([BI)I");

	tmp = (char*)calloc(4, sizeof(char));
	if (tmp == NULL)
	{
		message = (*env)->NewStringUTF(env, "[ERROR] imagetobmp_stream: Out of memory for tmp");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		failed = 1;
		goto fin;
	}

	jba = (*env)->NewByteArray(env, 4);
	if (jba == NULL)
	{
		message = (*env)->NewStringUTF(env, "[ERROR] imagetobmp_stream: Out of memory for jba");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		failed = 1;
		goto fin;
	}

	if (image->comps[0].prec < 8)
	{
		snprintf(err, 128, "[ERROR] imagetobmp_stream: Unsupported precision: %d", image->comps[0].prec);
		message = (*env)->NewStringUTF(env, err);
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		failed = 1;
		goto fin;
	}

	snprintf(err, 128, "[INFO] imagetobmp_stream: Precision %d is supported", image->comps[0].prec);
	message = (*env)->NewStringUTF(env, err);
	(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

	if (image->numcomps >= 3 && image->comps[0].dx == image->comps[1].dx
		&& image->comps[1].dx == image->comps[2].dx
		&& image->comps[0].dy == image->comps[1].dy
		&& image->comps[1].dy == image->comps[2].dy
		&& image->comps[0].prec == image->comps[1].prec
		&& image->comps[1].prec == image->comps[2].prec
		&& image->comps[0].sgnd == image->comps[1].sgnd
		&& image->comps[1].sgnd == image->comps[2].sgnd)
	{

		/* -->> -->> -->> -->>
		24 bits color
		<<-- <<-- <<-- <<-- */

		w = (int)image->comps[0].w;
		h = (int)image->comps[0].h;

		tmpSize = snprintf(tmp, 4, "BM");
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing BM to output stream: ");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* FILE HEADER */
		/* ------------- */

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)(h * w * 3 + 3 * h * (w % 2) + 54) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2) + 54) >> 8) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2) + 54) >> 16) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2) + 54) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(1) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(0) & 0xff,
			((0) >> 8) & 0xff,
			((0) >> 16) & 0xff,
			((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(2) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(54) & 0xff,
			((54) >> 8) & 0xff,
			((54) >> 16) & 0xff,
			((54) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(3) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* INFO HEADER   */
		/* ------------- */

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(40) & 0xff,
			((40) >> 8) & 0xff,
			((40) >> 16) & 0xff,
			((40) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(1) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)((w) & 0xff),
			(OPJ_UINT8)((w) >> 8) & 0xff,
			(OPJ_UINT8)((w) >> 16) & 0xff,
			(OPJ_UINT8)((w) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(2) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)((h) & 0xff),
			(OPJ_UINT8)((h) >> 8) & 0xff,
			(OPJ_UINT8)((h) >> 16) & 0xff,
			(OPJ_UINT8)((h) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(3) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c", (1) & 0xff, ((1) >> 8) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(4) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c", (24) & 0xff, ((24) >> 8) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(5) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff, ((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(6) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)(3 * h * w + 3 * h * (w % 2)) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2)) >> 8) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2)) >> 16) & 0xff,
			(OPJ_UINT8)((h * w * 3 + 3 * h * (w % 2)) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(7) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(7834) & 0xff,
			((7834) >> 8) & 0xff,
			((7834) >> 16) & 0xff,
			((7834) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(8) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(7834) & 0xff,
			((7834) >> 8) & 0xff,
			((7834) >> 16) & 0xff,
			((7834) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(9) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(0) & 0xff,
			((0) >> 8) & 0xff,
			((0) >> 16) & 0xff,
			((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(10) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(0) & 0xff,
			((0) >> 8) & 0xff,
			((0) >> 16) & 0xff,
			((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(11) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		if (image->comps[0].prec > 8)
		{
			adjustR = (int)image->comps[0].prec - 8;

			snprintf(err, 128, "[WARNING] BMP CONVERSION: Truncating component 0 from %d bits to 8 bits\n", image->comps[0].prec);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		else {
			adjustR = 0;
		}
		if (image->comps[1].prec > 8)
		{
			adjustG = (int)image->comps[1].prec - 8;

			snprintf(err, 128, "[WARNING] BMP CONVERSION: Truncating component 1 from %d bits to 8 bits\n", image->comps[1].prec);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		else {
			adjustG = 0;
		}
		if (image->comps[2].prec > 8)
		{
			adjustB = (int)image->comps[2].prec - 8;

			snprintf(err, 128, "[WARNING] BMP CONVERSION: Truncating component 2 from %d bits to 8 bits\n", image->comps[2].prec);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		else {
			adjustB = 0;
		}

		for (i = 0; i < w * h; i++) {
			OPJ_UINT8 rc, gc, bc;
			int r, g, b;

			r = image->comps[0].data[w * h - ((i) / (w)+1) * w + (i) % (w)];
			r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
			if (adjustR > 0) {
				r = ((r >> adjustR) + ((r >> (adjustR - 1)) % 2));
			}
			if (r > 255) {
				r = 255;
			}
			else if (r < 0) {
				r = 0;
			}
			rc = (OPJ_UINT8)r;

			g = image->comps[1].data[w * h - ((i) / (w)+1) * w + (i) % (w)];
			g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
			if (adjustG > 0) {
				g = ((g >> adjustG) + ((g >> (adjustG - 1)) % 2));
			}
			if (g > 255) {
				g = 255;
			}
			else if (g < 0) {
				g = 0;
			}
			gc = (OPJ_UINT8)g;

			b = image->comps[2].data[w * h - ((i) / (w)+1) * w + (i) % (w)];
			b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
			if (adjustB > 0) {
				b = ((b >> adjustB) + ((b >> (adjustB - 1)) % 2));
			}
			if (b > 255) {
				b = 255;
			}
			else if (b < 0) {
				b = 0;
			}
			bc = (OPJ_UINT8)b;

			tmpSize = snprintf(tmp, 4, "%c%c%c", bc, gc, rc);
			(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
			if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
			{
				snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing for loop data to output stream:");
				message = (*env)->NewStringUTF(env, err);
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				failed = 1;
				goto fin;
			}

			if ((i + 1) % w == 0) {
				for (pad = ((3 * w) % 4) ? (4 - (3 * w) % 4) : 0; pad > 0; pad--)
				{ /* ADD */

					tmpSize = snprintf(tmp, 4, "%c", 0);
					(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
					if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
					{
						snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing ADD for loop data to output stream:");
						message = (*env)->NewStringUTF(env, err);
						(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
						failed = 1;
						goto fin;
					}
				}
			}
		}
	}
	else
	{
		/* Gray-scale */

		/* -->> -->> -->> -->>
		8 bits non code (Gray scale)
		<<-- <<-- <<-- <<-- */

		if (image->numcomps > 1)
		{
			snprintf(err, 128, "[WARNING] imagetobmp_stream: only first component of %d is used.\n", image->numcomps);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		w = (int)image->comps[0].w;
		h = (int)image->comps[0].h;

		tmpSize = snprintf(tmp, 4, "BM");
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing BM to output stream: ");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* FILE HEADER */
		/* ------------- */

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)(h * w + 54 + 1024 + h * (w % 2)) & 0xff,
			(OPJ_UINT8)((h * w + 54 + 1024 + h * (w % 2)) >> 8) & 0xff,
			(OPJ_UINT8)((h * w + 54 + 1024 + h * (w % 2)) >> 16) & 0xff,
			(OPJ_UINT8)((h * w + 54 + 1024 + w * (w % 2)) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(1) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(0) & 0xff,
			((0) >> 8) & 0xff,
			((0) >> 16) & 0xff,
			((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(2) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(54 + 1024) & 0xff,
			((54 + 1024) >> 8) & 0xff,
			((54 + 1024) >> 16) & 0xff,
			((54 + 1024) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing header(3) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* INFO HEADER */
		/* ------------- */

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(40) & 0xff,
			((40) >> 8) & 0xff,
			((40) >> 16) & 0xff,
			((40) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(1) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)((w) & 0xff),
			(OPJ_UINT8)((w) >> 8) & 0xff,
			(OPJ_UINT8)((w) >> 16) & 0xff,
			(OPJ_UINT8)((w) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(2) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)((h) & 0xff),
			(OPJ_UINT8)((h) >> 8) & 0xff,
			(OPJ_UINT8)((h) >> 16) & 0xff,
			(OPJ_UINT8)((h) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(3) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c",
			(1) & 0xff,
			((1) >> 8) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(4) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c", (8) & 0xff, ((8) >> 8) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(5) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c", (0) & 0xff, ((0) >> 8) & 0xff, ((0) >> 16) & 0xff, ((0) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(6) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(OPJ_UINT8)(h * w + h * (w % 2)) & 0xff,
			(OPJ_UINT8)((h * w + h * (w % 2)) >> 8) & 0xff,
			(OPJ_UINT8)((h * w + h * (w % 2)) >> 16) & 0xff,
			(OPJ_UINT8)((h * w + h * (w % 2)) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(7) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c",
			(7834) & 0xff,
			((7834) >> 8) & 0xff,
			((7834) >> 16) & 0xff,
			((7834) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(8) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c", (7834) & 0xff, ((7834) >> 8) & 0xff, ((7834) >> 16) & 0xff, ((7834) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(9) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c", (256) & 0xff, ((256) >> 8) & 0xff, ((256) >> 16) & 0xff, ((256) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(10) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		tmpSize = snprintf(tmp, 4, "%c%c%c%c", (256) & 0xff, ((256) >> 8) & 0xff, ((256) >> 16) & 0xff, ((256) >> 24) & 0xff);
		(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
		if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
		{
			snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing info header(11) to output stream:");
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		if (image->comps[0].prec > 8) {
			adjustR = (int)image->comps[0].prec - 8;

			snprintf(err, 128, "[WARNING] BMP CONVERSION: Truncating component 0 from %d bits to 8 bits", image->comps[0].prec);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		else {
			adjustR = 0;
		}

		for (i = 0; i < 256; i++)
		{
			tmpSize = snprintf(tmp, 4, "%c%c%c%c", i, i, i, 0);
			(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
			if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
			{
				snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing for loop 1 data to output stream:");
				message = (*env)->NewStringUTF(env, err);
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				failed = 1;
				goto fin;
			}
		}

		for (i = 0; i < w * h; i++) {
			int r;

			r = image->comps[0].data[w * h - ((i) / (w)+1) * w + (i) % (w)];
			r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
			if (adjustR > 0) {
				r = ((r >> adjustR) + ((r >> (adjustR - 1)) % 2));
			}
			if (r > 255) {
				r = 255;
			}
			else if (r < 0) {
				r = 0;
			}

			tmpSize = snprintf(tmp, 4, "%c", (OPJ_UINT8)r);
			(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
			if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
			{
				snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing for loop 2 data to output stream:");
				message = (*env)->NewStringUTF(env, err);
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				failed = 1;
				goto fin;
			}

			if ((i + 1) % w == 0) {
				for (pad = (w % 4) ? (4 - w % 4) : 0; pad > 0; pad--)
				{ /* ADD */

					tmpSize = snprintf(tmp, 4, "%c", 0);
					(*env)->SetByteArrayRegion(env, jba, 0, (jsize)tmpSize, (jbyte*)tmp);
					if (tmpSize != (int)((*env)->CallIntMethod(env, obj, mid, jba, (int)tmpSize)))
					{
						snprintf(err, 128, "[ERROR] imagetobmp_stream: Error while writing ADD for loop data to output stream:");
						message = (*env)->NewStringUTF(env, err);
						(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
						failed = 1;
						goto fin;
					}
				}
			}
		}
	}

	snprintf(err, 128, "[INFO] imagetobmp_stream: Output ready");
	message = (*env)->NewStringUTF(env, err);
	(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

fin:
	if (tmp != NULL)
	{
		secure_memclr(tmp, 4);
		free(tmp);
		tmp = NULL;
	}

	if (jba != NULL)
	{
		(*env)->DeleteLocalRef(env, jba);
		jba = NULL;
	}

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

/* --------------------------------------------------------------------------
   --------------------   MAIN METHOD, CALLED BY JAVA -----------------------*/
JNIEXPORT jint JNICALL Java_org_openJpeg_OpenJPEGJavaDecoder_internalDecodeJ2KtoImage(JNIEnv *env, jobject obj, jobjectArray javaParameters, jobject stream)
{
	/*	To simulate the command line parameters (taken from the javaParameters variable) and be able to re-use the */
	/*		'parse_cmdline_decoder' method taken from the j2k_to_image project */
	int argc;
	char **argv;

	/* ==> Access variables to the Java member variables*/
	jsize		arraySize = 0, inputStreamSize = 0;
	jclass		cls = NULL;
	jobject		argvObject = NULL;
	jboolean	isCopy = 0;
	jfieldID	fid = NULL;
	jbyteArray	jbaInputStreamByteArray = NULL;
	jstring		message = NULL, jOutputFormatStr = NULL;
	callback_variables_t msgErrorCallback_vars;

	opj_decompress_parameters parameters;           /* decompression parameters */
	opj_image_t* image = NULL;
	opj_stream_t *l_stream = NULL;					/* Stream */
	opj_codec_t* l_codec = NULL;					/* Handle to a decompressor */
	opj_codestream_index_t* cstr_index = NULL;

	OPJ_INT32 num_images = 0, imageno = 0;
	img_fol_t img_fol = { NULL, NULL, 0 };
	dircnt_t *dirptr = NULL;
	int failed = 0;
	OPJ_FLOAT64 t, tCumulative = 0;
	OPJ_UINT32 numDecompressedImages = 0;
	OPJ_UINT32 cp_reduce = 0;

	opj_event_mgr_t event_mgr;						/* event manager */
	unsigned char *inputStreamBytes = NULL;
	char *outformat = NULL;
	int i = 0;
	OPJ_BOOL usingInputStream = OPJ_FALSE;
	Stream inputStream = { NULL, NULL, 0, 0, 0, 0 };
	char err[128];

	/* ------------------------------------------------------------------------------------------------------------ */

	/* JNI reference to the calling class */
	cls = (*env)->GetObjectClass(env, obj);

	/* Pointers to be able to call Java logging methods */
	msgErrorCallback_vars.env = env;
	msgErrorCallback_vars.jobj = &obj;
	msgErrorCallback_vars.message_mid = (*env)->GetMethodID(env, cls, "logMessage", "(Ljava/lang/String;)V");
	msgErrorCallback_vars.error_mid = (*env)->GetMethodID(env, cls, "logError", "(Ljava/lang/String;)V");

	/* Configure the event callbacks */
	secure_memclr(&event_mgr, sizeof(opj_event_mgr_t));
	event_mgr.error_handler = error_callback;
	event_mgr.warning_handler = warning_callback;
	event_mgr.info_handler = info_callback;

	/* Get the String[] containing the parameters, and converts it into a char** to simulate command line arguments. */
	arraySize = (*env)->GetArrayLength(env, javaParameters);
	argc = (int)arraySize + 1;
	argv = (char**)opj_malloc(argc * sizeof(char*));
	if (argv == NULL)
	{
		message = (*env)->NewStringUTF(env, "[ERROR] argv init : OUT OF MEMORY");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		fprintf(stderr, "%s:%d:\n\tMEMORY OUT\n", __FILE__, __LINE__);
		failed = 1;
		goto fin;
	}
	argv[0] = "OpenJPEGJavaDecoder";	/* The program name: useless*/
	for (i = 1; i < argc; i++)
	{
		argvObject = (*env)->GetObjectArrayElement(env, javaParameters, i - 1);
		argv[i] = (char*)(*env)->GetStringUTFChars(env, argvObject, &isCopy);
	}

	/* Get the value of skippedResolutions variable from Java code */
	fid = (*env)->GetFieldID(env, cls, "skippedResolutions", "I");
	jint skippedResolutions = (*env)->GetIntField(env, obj, fid);

	/* Get the value of isInputJPT variable from Java code */
	fid = (*env)->GetFieldID(env, cls, "isInputJPT", "I");
	jint isInputJPT = (*env)->GetIntField(env, obj, fid);
	message = (*env)->NewStringUTF(env, isInputJPT == 1 ? "[INFO] isInputJPT = Yes" : "[INFO] isInputJPT = No");
	(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

	/* Get the value of quiet variable from Java code */
	fid = (*env)->GetFieldID(env, cls, "quiet", "I");
	jint quiet = (*env)->GetIntField(env, obj, fid);
	message = (*env)->NewStringUTF(env, quiet == 1 ? "[INFO] quiet = Yes" : "[INFO] quiet = No");
	(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

	/* Set decoding parameters to default values */
	set_default_parameters(&parameters);
	parameters.core.cp_reduce = (int)skippedResolutions;
	parameters.quiet = (int)quiet;

	/* Set cp_reduce from the parameters */
	cp_reduce = parameters.core.cp_reduce;

	/* Initialize img_fol */
	secure_memclr(&img_fol, sizeof(img_fol_t));

	/* If no parameter is passed, caller should have already called setInputStream() + setOutputFormat() and maybe setQuiet() / setInputJPT() */
	/* We prepare variables / parameters then proceed to decode the input stream */
	if (argc == 1)
	{
		message = (*env)->NewStringUTF(env, "[INFO] argc == 1");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		fid = (*env)->GetFieldID(env, cls, "inputStream", "[B");
		jbaInputStreamByteArray = (*env)->GetObjectField(env, obj, fid);
		if (jbaInputStreamByteArray == NULL)
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Input Stream = NULL");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}
		inputStreamSize = (*env)->GetArrayLength(env, jbaInputStreamByteArray);
		if ((int)inputStreamSize == 0)
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Input Stream length = 0");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* Access the same memory region that is accessible to Java code via the buffer object passed to the native function */
		/* No need to lock / unlock or to free */
		inputStreamBytes = (unsigned char*)((*env)->GetDirectBufferAddress(env, stream));

		/* Set isInputStreamSet parameter */
		parameters.isInputStreamSet = 1;
		usingInputStream = parameters.isInputStreamSet;

		/* Form the InputStream struct variable : the unsigned char* will only point to the buffer in Java code => not locked nor mallocated */
		secure_memclr(&inputStream, sizeof(Stream));
		inputStream.originalStreamBytes = inputStreamBytes;
		inputStream.currentStreamBytes = inputStreamBytes;
		inputStream.totalSize = inputStreamSize;
		inputStream.actualSize = inputStreamSize;
		inputStream.isAllocated = 0;
		inputStream.isLocked = 0;

		message = (*env)->NewStringUTF(env, "[INFO] InputStreamBytes and InputStream initialization OK");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		/* Set the decod_format parameter */
		parameters.decod_format = instream_format(inputStreamBytes, (OPJ_SIZE_T)inputStreamSize, isInputJPT);
		switch (parameters.decod_format)
		{
		case J2K_CFMT:
			if (!parameters.quiet)
			{
				message = (*env)->NewStringUTF(env, "[INFO] Input format = J2K");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			}
			break;
		case JP2_CFMT:
			if (!parameters.quiet)
			{
				message = (*env)->NewStringUTF(env, "[INFO] Input format = JP2");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			}
			break;
		case JPT_CFMT:
			if (!parameters.quiet)
			{
				message = (*env)->NewStringUTF(env, "[INFO] Input format = JPT");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			}
			break;
		default:
			message = (*env)->NewStringUTF(env, "[ERROR] Unknown input stream format - Known stream formats are *.j2k, *.jp2, *.jpc or *.jpt (J2K, JP2 or JPT/JPIP code streams)");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* Read the output format, Get reference to the string then Convert jstring to native string which will be passed to get_file_format(), finally release */
		fid = (*env)->GetFieldID(env, cls, "outputFormat", "Ljava/lang/String;");
		jOutputFormatStr = (*env)->GetObjectField(env, obj, fid);
		outformat = (char*)((*env)->GetStringUTFChars(env, jOutputFormatStr, &isCopy));

		/* Set the cod_format and out_format parameters */
		img_fol.set_out_format = 1;
		parameters.cod_format = get_file_format(outformat);
		switch (parameters.cod_format)
		{
		case BMP_DFMT:
			img_fol.out_format = "bmp";
			if (!parameters.quiet)
			{
				message = (*env)->NewStringUTF(env, "[INFO] Output format = BMP");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			}
			break;
		case TIF_DFMT:
		case PNG_DFMT:
		case PGX_DFMT:
		case PXM_DFMT:
		case RAW_DFMT:
		case RAWL_DFMT:
		case TGA_DFMT:
		default:
			message = (*env)->NewStringUTF(env, "[ERROR] Unknown output format [only *.bmp is supported]!!");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		/* Create l_stream from inputStream */
		l_stream = opj_stream_create_default_file_stream_from_input_stream(&inputStream, 1);

		if (!l_stream)
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Failed to create the l_stream from the input stream");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
			goto fin;
		}

		message = (*env)->NewStringUTF(env, "[INFO] l_stream created from the input stream");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		/* Initialize image */
		image = NULL;

		/* Start decoding the JPEG2000 stream */

		switch (parameters.decod_format)
		{
		case J2K_CFMT: { /* JPEG-2000 codestream */
			/* Get a decoder handle */
			l_codec = opj_create_decompress(OPJ_CODEC_J2K);
			message = (*env)->NewStringUTF(env, "[INFO] Codec is OPJ_CODEC_J2K");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			break;
		}
		case JP2_CFMT: { /* JPEG 2000 compressed image data */
			/* Get a decoder handle */
			l_codec = opj_create_decompress(OPJ_CODEC_JP2);
			message = (*env)->NewStringUTF(env, "[INFO] Codec is OPJ_CODEC_JP2");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			break;
		}
		case JPT_CFMT: { /* JPEG 2000, JPIP */
			/* Get a decoder handle */
			l_codec = opj_create_decompress(OPJ_CODEC_JPT);
			message = (*env)->NewStringUTF(env, "[INFO] Codec is OPJ_CODEC_JPT");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			break;
		}
		default:
			message = (*env)->NewStringUTF(env, "[ERROR] Decode format parameter unexpected..");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			destroy_parameters(&parameters);
			opj_stream_destroy(l_stream);
			failed = 1;
			goto fin;
		}

		t = opj_clock();

		/* Setup the decoder decoding parameters using user parameters */
		if (!opj_setup_decoder(l_codec, &(parameters.core)))
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Failed to setup the decoder");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			opj_stream_destroy(l_stream);
			opj_destroy_codec(l_codec);
			failed = 1;
			goto fin;
		}

		message = (*env)->NewStringUTF(env, "[INFO] Decoder setup");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		if (parameters.num_threads >= 1 && !opj_codec_set_threads(l_codec, parameters.num_threads))
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Failed to set number of threads");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			opj_stream_destroy(l_stream);
			opj_destroy_codec(l_codec);
			failed = 1;
			goto fin;
		}

		message = (*env)->NewStringUTF(env, "[INFO] Number of threads set");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		/* Read the main header of the codestream and if necessary the JP2 boxes */
		if (!opj_read_header(l_stream, l_codec, &image))
		{
			message = (*env)->NewStringUTF(env, "[ERROR] Failed to read the header");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			opj_destroy_codec(l_codec);
			opj_stream_destroy(l_stream);
			opj_image_destroy(image);
			failed = 1;
			goto fin;
		}

		message = (*env)->NewStringUTF(env, "[INFO] Header read");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		if (parameters.numcomps)
		{
			if (!opj_set_decoded_components(l_codec,
				parameters.numcomps,
				parameters.comps_indices,
				OPJ_FALSE))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] Failed to set the component indices!");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			message = (*env)->NewStringUTF(env, "[INFO] Component indices set");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}

		if (getenv("USE_OPJ_SET_DECODED_RESOLUTION_FACTOR") != NULL)
		{
			/* For debugging/testing purposes, and also an illustration on how to */
			/* use the alternative API opj_set_decoded_resolution_factor() instead */
			/* of setting parameters.cp_reduce */
			if (!opj_set_decoded_resolution_factor(l_codec, cp_reduce))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] Failed to set the resolution factor tile!");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			message = (*env)->NewStringUTF(env, "[INFO] Resolution factor tile set");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}

		if (!parameters.nb_tile_to_decode)
		{
			if (getenv("SKIP_OPJ_SET_DECODE_AREA") != NULL &&
				parameters.DA_x0 == 0 &&
				parameters.DA_y0 == 0 &&
				parameters.DA_x1 == 0 &&
				parameters.DA_y1 == 0)
			{
				/* For debugging/testing purposes, */
				/* do nothing if SKIP_OPJ_SET_DECODE_AREA env variable */
				/* is defined and no decoded area has been set */
			}
			/* Optional if you want decode the entire image */
			else if (!opj_set_decode_area(l_codec, image, (OPJ_INT32)parameters.DA_x0,
				(OPJ_INT32)parameters.DA_y0, (OPJ_INT32)parameters.DA_x1,
				(OPJ_INT32)parameters.DA_y1))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] Failed to set the decoded area");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			message = (*env)->NewStringUTF(env, "[INFO] Decoded area set");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

			/* Get the decoded image */
			if (!(opj_decode(l_codec, l_stream, image) && opj_end_decompress(l_codec, l_stream)))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] Failed to decode image!");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			message = (*env)->NewStringUTF(env, "[INFO] Image decoded");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}
		else {
			if (!(parameters.DA_x0 == 0 &&
				parameters.DA_y0 == 0 &&
				parameters.DA_x1 == 0 &&
				parameters.DA_y1 == 0)) {
				if (!(parameters.quiet))
				{
					message = (*env)->NewStringUTF(env, "WARNING: -d option ignored when used together with -t");
					(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				}
			}

			if (!opj_get_decoded_tile(l_codec, l_stream, image, parameters.tile_index))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] Failed to decode tile!");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			snprintf(err, 128, "[INFO] Tile %d is decoded!\n\n", parameters.tile_index);
			message = (*env)->NewStringUTF(env, err);
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}

		/* FIXME? Shouldn't that situation be considered as an error of */
		/* opj_decode() / opj_get_decoded_tile() ? */
		if (image->comps[0].data == NULL)
		{
			message = (*env)->NewStringUTF(env, "[ERROR] No image data!");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			opj_destroy_codec(l_codec);
			opj_stream_destroy(l_stream);
			opj_image_destroy(image);
			failed = 1;
			goto fin;
		}

		tCumulative += opj_clock() - t;
		numDecompressedImages++;

		/* Close the byte stream */
		opj_stream_destroy(l_stream);
		l_stream = NULL;

		message = (*env)->NewStringUTF(env, "[INFO] l_stream cleared");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		if (image->color_space != OPJ_CLRSPC_SYCC
			&& image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy
			&& image->comps[1].dx != 1)
		{
			image->color_space = OPJ_CLRSPC_SYCC;
		}
		else if (image->numcomps <= 2)
		{
			image->color_space = OPJ_CLRSPC_GRAY;
		}

		if (image->color_space == OPJ_CLRSPC_SYCC)
		{
			color_sycc_to_rgb(image);
		}
		else if ((image->color_space == OPJ_CLRSPC_CMYK) &&
			(parameters.cod_format != TIF_DFMT))
		{
			color_cmyk_to_rgb(image);
		}
		else if (image->color_space == OPJ_CLRSPC_EYCC)
		{
			color_esycc_to_rgb(image);
		}

		if (image->icc_profile_buf) {
#if defined(OPJ_HAVE_LIBLCMS1) || defined(OPJ_HAVE_LIBLCMS2)
			if (image->icc_profile_len)
			{
				color_apply_icc_profile(image);
			}
			else
			{
				color_cielab_to_rgb(image);
			}
#endif
			free(image->icc_profile_buf);
			image->icc_profile_buf = NULL;
			image->icc_profile_len = 0;
		}

		/* =============== Return the image to Java code ===============*/

		switch (parameters.cod_format)
		{
		case BMP_DFMT:          /* BMP */
			if (imagetobmp_stream(image, msgErrorCallback_vars))
			{
				message = (*env)->NewStringUTF(env, "[ERROR] BMP Output stream not generated");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
				failed = 1;
			}
			else if (!(parameters.quiet))
			{
				message = (*env)->NewStringUTF(env, "[INFO] Generated BMP Output stream");
				(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			}
			break;

		case PXM_DFMT:          /* PNM PGM PPM */
		case PGX_DFMT:          /* PGX */
#ifdef OPJ_HAVE_LIBTIFF
		case TIF_DFMT:          /* TIFF */
#endif /* OPJ_HAVE_LIBTIFF */
		case RAW_DFMT:          /* RAW */
		case RAWL_DFMT:         /* RAWL */
		case TGA_DFMT:          /* TGA */
#ifdef OPJ_HAVE_LIBPNG
		case PNG_DFMT:          /* PNG */
#endif /* OPJ_HAVE_LIBPNG */
			/* Can happen if output file is TIFF or PNG
				* and OPJ_HAVE_LIBTIF or OPJ_HAVE_LIBPNG is undefined
			*/
		default:
			message = (*env)->NewStringUTF(env, "[ERROR] Output stream not generated");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
			failed = 1;
		}

		/* free remaining structures */
		if (l_codec)
		{
			opj_destroy_codec(l_codec);
			message = (*env)->NewStringUTF(env, "[INFO] Coded cleared");
			(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);
		}

		/* free image data structure */
		opj_image_destroy(image);
		image = NULL;

		message = (*env)->NewStringUTF(env, "[INFO] Image cleared");
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		/* destroy the codestream index */
		opj_destroy_cstr_index(&cstr_index);

		if (failed)
		{
			(void)remove(parameters.outfile);    /* ignore return value */
		}
	}

	else
	{
		/* Parse input and get user encoding parameters */
		int parsingResult = parse_cmdline_decoder(argc, argv, &parameters, &img_fol);

		/* If input parsing error => quit */
		if (parsingResult == 1)
		{
			failed = 1;
			goto fin;
		}

		/* Initialize reading of directory */
		if (img_fol.set_imgdir == 1)
		{
			int it_image;
			num_images = get_num_images(img_fol.imgdirpath);

			dirptr = (dircnt_t*)malloc(sizeof(dircnt_t));
			if (!dirptr) {
				destroy_parameters(&parameters);
				return EXIT_FAILURE;
			}
			dirptr->filename_buf = (char*)malloc((size_t)num_images * OPJ_PATH_LEN * sizeof(
				char)); /* Stores at max 10 image file names*/
			if (!dirptr->filename_buf) {
				failed = 1;
				goto fin;
			}

			dirptr->filename = (char**)malloc((size_t)num_images * sizeof(char*));

			if (!dirptr->filename) {
				failed = 1;
				goto fin;
			}
			for (it_image = 0; it_image < num_images; it_image++) {
				dirptr->filename[it_image] = dirptr->filename_buf + it_image * OPJ_PATH_LEN;
			}

			if (load_images(dirptr, img_fol.imgdirpath) == 1) {
				failed = 1;
				goto fin;
			}
			if (num_images == 0) {
				fprintf(stderr, "Folder is empty\n");
				failed = 1;
				goto fin;
			}
		}
		else
		{
			num_images = 1;
		}

		/* Decoding image one by one */
		for (imageno = 0; imageno < num_images; imageno++)
		{
			image = NULL;
			if (!parameters.quiet)
				fprintf(stderr, "\n");

			if (img_fol.set_imgdir == 1)
			{
				if (get_next_file(imageno, dirptr, &img_fol, &parameters))
				{
					fprintf(stderr, "skipping file...\n");
					destroy_parameters(&parameters);
					continue;
				}
			}

			//	If -i set, read the input file
			if (parameters.infile && parameters.infile[0] != '\0')
			{
				l_stream = opj_stream_create_default_file_stream(parameters.infile, 1);
				if (!l_stream)
				{
					fprintf(stderr, "ERROR -> failed to create the stream from the file %s\n",
						parameters.infile);
					failed = 1;
					goto fin;
				}
			}

			/* decode the JPEG2000 stream */
			/* ---------------------- */

			switch (parameters.decod_format)
			{
			case J2K_CFMT: { /* JPEG-2000 codestream */
				/* Get a decoder handle */
				l_codec = opj_create_decompress(OPJ_CODEC_J2K);
				break;
			}
			case JP2_CFMT: { /* JPEG 2000 compressed image data */
				/* Get a decoder handle */
				l_codec = opj_create_decompress(OPJ_CODEC_JP2);
				break;
			}
			case JPT_CFMT: { /* JPEG 2000, JPIP */
				/* Get a decoder handle */
				l_codec = opj_create_decompress(OPJ_CODEC_JPT);
				break;
			}
			default:
				fprintf(stderr, "skipping file..\n");
				destroy_parameters(&parameters);
				opj_stream_destroy(l_stream);
				continue;
			}

			t = opj_clock();

			/* Setup the decoder decoding parameters using user parameters */
			if (!opj_setup_decoder(l_codec, &(parameters.core)))
			{
				fprintf(stderr, "ERROR -> opj_decompress: failed to setup the decoder\n");
				opj_stream_destroy(l_stream);
				opj_destroy_codec(l_codec);
				failed = 1;
				goto fin;
			}

			if (parameters.num_threads >= 1 && !opj_codec_set_threads(l_codec, parameters.num_threads))
			{
				fprintf(stderr, "ERROR -> opj_decompress: failed to set number of threads\n");
				opj_stream_destroy(l_stream);
				opj_destroy_codec(l_codec);
				failed = 1;
				goto fin;
			}

			/* Read the main header of the codestream and if necessary the JP2 boxes */
			if (!opj_read_header(l_stream, l_codec, &image))
			{
				fprintf(stderr, "ERROR -> opj_decompress: failed to read the header\n");
				opj_stream_destroy(l_stream);
				opj_destroy_codec(l_codec);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			if (parameters.numcomps)
			{
				if (!opj_set_decoded_components(l_codec,
					parameters.numcomps,
					parameters.comps_indices,
					OPJ_FALSE))
				{
					fprintf(stderr,
						"ERROR -> opj_decompress: failed to set the component indices!\n");
					opj_destroy_codec(l_codec);
					opj_stream_destroy(l_stream);
					opj_image_destroy(image);
					failed = 1;
					goto fin;
				}
			}

			if (getenv("USE_OPJ_SET_DECODED_RESOLUTION_FACTOR") != NULL)
			{
				/* For debugging/testing purposes, and also an illustration on how to */
				/* use the alternative API opj_set_decoded_resolution_factor() instead */
				/* of setting parameters.cp_reduce */
				if (!opj_set_decoded_resolution_factor(l_codec, cp_reduce))
				{
					fprintf(stderr,
						"ERROR -> opj_decompress: failed to set the resolution factor tile!\n");
					opj_destroy_codec(l_codec);
					opj_stream_destroy(l_stream);
					opj_image_destroy(image);
					failed = 1;
					goto fin;
				}
			}

			if (!parameters.nb_tile_to_decode)
			{
				if (getenv("SKIP_OPJ_SET_DECODE_AREA") != NULL &&
					parameters.DA_x0 == 0 &&
					parameters.DA_y0 == 0 &&
					parameters.DA_x1 == 0 &&
					parameters.DA_y1 == 0)
				{
					/* For debugging/testing purposes, */
					/* do nothing if SKIP_OPJ_SET_DECODE_AREA env variable */
					/* is defined and no decoded area has been set */
				}
				/* Optional if you want decode the entire image */
				else if (!opj_set_decode_area(l_codec, image, (OPJ_INT32)parameters.DA_x0,
					(OPJ_INT32)parameters.DA_y0, (OPJ_INT32)parameters.DA_x1,
					(OPJ_INT32)parameters.DA_y1))
				{
					fprintf(stderr, "ERROR -> opj_decompress: failed to set the decoded area\n");
					opj_stream_destroy(l_stream);
					opj_destroy_codec(l_codec);
					opj_image_destroy(image);
					failed = 1;
					goto fin;
				}

				/* Get the decoded image */
				if (!(opj_decode(l_codec, l_stream, image) &&
					opj_end_decompress(l_codec, l_stream)))
				{
					fprintf(stderr, "ERROR -> opj_decompress: failed to decode image!\n");
					opj_destroy_codec(l_codec);
					opj_stream_destroy(l_stream);
					opj_image_destroy(image);
					failed = 1;
					goto fin;
				}
			}
			else {
				if (!(parameters.DA_x0 == 0 &&
					parameters.DA_y0 == 0 &&
					parameters.DA_x1 == 0 &&
					parameters.DA_y1 == 0)) {
					if (!(parameters.quiet))
					{
						fprintf(stderr, "WARNING: -d option ignored when used together with -t\n");
					}
				}

				if (!opj_get_decoded_tile(l_codec, l_stream, image, parameters.tile_index))
				{
					fprintf(stderr, "ERROR -> opj_decompress: failed to decode tile!\n");
					opj_destroy_codec(l_codec);
					opj_stream_destroy(l_stream);
					opj_image_destroy(image);
					failed = 1;
					goto fin;
				}
				if (!(parameters.quiet))
				{
					fprintf(stdout, "tile %d is decoded!\n\n", parameters.tile_index);
				}
			}

			/* FIXME? Shouldn't that situation be considered as an error of */
			/* opj_decode() / opj_get_decoded_tile() ? */
			if (image->comps[0].data == NULL)
			{
				fprintf(stderr, "ERROR -> opj_decompress: no image data!\n");
				opj_destroy_codec(l_codec);
				opj_stream_destroy(l_stream);
				opj_image_destroy(image);
				failed = 1;
				goto fin;
			}

			tCumulative += opj_clock() - t;
			numDecompressedImages++;

			/* Close the byte stream */
			opj_stream_destroy(l_stream);

			if (image->color_space != OPJ_CLRSPC_SYCC
				&& image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy
				&& image->comps[1].dx != 1) {
				image->color_space = OPJ_CLRSPC_SYCC;
			}
			else if (image->numcomps <= 2) {
				image->color_space = OPJ_CLRSPC_GRAY;
			}

			if (image->color_space == OPJ_CLRSPC_SYCC) {
				color_sycc_to_rgb(image);
			}
			else if ((image->color_space == OPJ_CLRSPC_CMYK) &&
				(parameters.cod_format != TIF_DFMT)) {
				color_cmyk_to_rgb(image);
			}
			else if (image->color_space == OPJ_CLRSPC_EYCC) {
				color_esycc_to_rgb(image);
			}

			if (image->icc_profile_buf) {
#if defined(OPJ_HAVE_LIBLCMS1) || defined(OPJ_HAVE_LIBLCMS2)
				if (image->icc_profile_len) {
					color_apply_icc_profile(image);
				}
				else {
					color_cielab_to_rgb(image);
				}
#endif
				free(image->icc_profile_buf);
				image->icc_profile_buf = NULL;
				image->icc_profile_len = 0;
			}

			/* Force output precision */
			/* ---------------------- */
			if (parameters.precision != NULL)
			{
				OPJ_UINT32 compno;
				for (compno = 0; compno < image->numcomps; ++compno) {
					OPJ_UINT32 precno = compno;
					OPJ_UINT32 prec;

					if (precno >= parameters.nb_precision) {
						precno = parameters.nb_precision - 1U;
					}

					prec = parameters.precision[precno].prec;
					if (prec == 0) {
						prec = image->comps[compno].prec;
					}

					switch (parameters.precision[precno].mode) {
					case OPJ_PREC_MODE_CLIP:
						clip_component(&(image->comps[compno]), prec);
						break;
					case OPJ_PREC_MODE_SCALE:
						scale_component(&(image->comps[compno]), prec);
						break;
					default:
						break;
					}

				}
			}

			/* Upsample components */
			/* ------------------- */
			if (parameters.upsample)
			{
				image = upsample_image_components(image);
				if (image == NULL) {
					fprintf(stderr,
						"ERROR -> opj_decompress: failed to upsample image components!\n");
					opj_destroy_codec(l_codec);
					failed = 1;
					goto fin;
				}
			}

			/* Force RGB output */
			/* ---------------- */
			if (parameters.force_rgb)
			{
				switch (image->color_space) {
				case OPJ_CLRSPC_SRGB:
					break;
				case OPJ_CLRSPC_GRAY:
					image = convert_gray_to_rgb(image);
					break;
				default:
					fprintf(stderr,
						"ERROR -> opj_decompress: don't know how to convert image to RGB colorspace!\n");
					opj_image_destroy(image);
					image = NULL;
					break;
				}
				if (image == NULL) {
					fprintf(stderr, "ERROR -> opj_decompress: failed to convert to RGB image!\n");
					opj_destroy_codec(l_codec);
					failed = 1;
					goto fin;
				}
			}

			/* create output image.
				If the -o parameter is given in the JavaParameters, write the decoded version into a file.
				Implemented for debug purpose. */
				/* ---------------------------------- */
				/* we don't need the Java part since we get the result in a file */
			if (parameters.outfile && parameters.outfile[0] != '\0')
			{
				switch (parameters.cod_format) {
				case PXM_DFMT:          /* PNM PGM PPM */
					if (imagetopnm(image, parameters.outfile, parameters.split_pnm)) {
						fprintf(stderr, "[ERROR] Outfile %s not generated\n", parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;

				case PGX_DFMT:          /* PGX */
					if (imagetopgx(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Outfile %s not generated\n", parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;

				case BMP_DFMT:          /* BMP */
					if (imagetobmp(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Outfile %s not generated\n", parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;
#ifdef OPJ_HAVE_LIBTIFF
				case TIF_DFMT:          /* TIFF */
					if (imagetotif(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Outfile %s not generated\n", parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;
#endif /* OPJ_HAVE_LIBTIFF */
				case RAW_DFMT:          /* RAW */
					if (imagetoraw(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Error generating raw file. Outfile %s not generated\n",
							parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;

				case RAWL_DFMT:         /* RAWL */
					if (imagetorawl(image, parameters.outfile)) {
						fprintf(stderr,
							"[ERROR] Error generating rawl file. Outfile %s not generated\n",
							parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;

				case TGA_DFMT:          /* TGA */
					if (imagetotga(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Error generating tga file. Outfile %s not generated\n",
							parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;
#ifdef OPJ_HAVE_LIBPNG
				case PNG_DFMT:          /* PNG */
					if (imagetopng(image, parameters.outfile)) {
						fprintf(stderr, "[ERROR] Error generating png file. Outfile %s not generated\n",
							parameters.outfile);
						failed = 1;
					}
					else if (!(parameters.quiet)) {
						fprintf(stdout, "[INFO] Generated Outfile %s\n", parameters.outfile);
					}
					break;
#endif /* OPJ_HAVE_LIBPNG */
					/* Can happen if output file is TIFF or PNG
						* and OPJ_HAVE_LIBTIF or OPJ_HAVE_LIBPNG is undefined
					*/
				default:
					fprintf(stderr, "[ERROR] Outfile %s not generated\n", parameters.outfile);
					failed = 1;
				}
			}

			/* free remaining structures */
			if (l_codec)
			{
				opj_destroy_codec(l_codec);
			}

			/* free image data structure */
			opj_image_destroy(image);

			/* destroy the codestream index */
			opj_destroy_cstr_index(&cstr_index);

			if (failed)
			{
				(void)remove(parameters.outfile);    /* ignore return value */
			}
		}
	}

fin:
	/* Release the Java arguments array*/
	if (argv != NULL)
	{
		//	Only if argc > 1
		for (i = 1; i < argc; i++)
		{
			argvObject = (*env)->GetObjectArrayElement(env, javaParameters, i - 1);
			(*env)->ReleaseStringUTFChars(env, argvObject, argv[i]);
		}

		opj_free(argv);
		argv = NULL;
	}

	/* Free the memory containing the input stream */
	if (usingInputStream)
	{
		inputStreamBytes = NULL;
		opj_free_from_stream(&inputStream);

		if (jOutputFormatStr != NULL)
		{
			/* Release output format */
			(*env)->ReleaseStringUTFChars(env, jOutputFormatStr, outformat);
			(*env)->DeleteLocalRef(env, jOutputFormatStr);
			outformat = NULL;
			jOutputFormatStr = NULL;
		}
	}

	destroy_parameters(&parameters);
	if (failed && img_fol.imgdirpath)
	{
		free(img_fol.imgdirpath);
	}
	if (dirptr) {
		if (dirptr->filename)
		{
			free(dirptr->filename);
		}
		if (dirptr->filename_buf)
		{
			free(dirptr->filename_buf);
		}
		free(dirptr);
	}
	if (numDecompressedImages && !failed && !(parameters.quiet))
	{
		snprintf(err, 128, "[INFO] Decode time: %d ms\n",
			(int)((tCumulative * 1000.0) / (OPJ_FLOAT64)numDecompressedImages));
		message = (*env)->NewStringUTF(env, err);
		(*env)->CallVoidMethod(env, obj, msgErrorCallback_vars.message_mid, message);

		fprintf(stdout, "[INFO] Decode time: %d ms\n",
			(int)((tCumulative * 1000.0) / (OPJ_FLOAT64)numDecompressedImages));
	}

	if (message != NULL)
	{
		(*env)->DeleteLocalRef(env, message);
		message = NULL;
	}

	return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
/*end main*/

