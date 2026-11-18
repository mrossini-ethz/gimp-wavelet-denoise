/*
 * Wavelet denoise GIMP plugin
 *
 * plugin.h
 * Copyright 2008 by Marco Rossini
 *
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 */

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <glib/gi18n.h>

#include "messages.h"

#define PLUG_IN_PROC "plug-in-wavelet-denoise"
#define PLUG_IN_BINARY "wavelet-denoise"
#define PLUG_IN_VERSION "0.5"

#define MAX2(x,y) ((x) > (y) ? (x) : (y))
#define MIN2(x,y) ((x) < (y) ? (x) : (y))
#define CLIP(x,min,max) MAX2((min), MIN2((x), (max)))

#define MODE_YCBCR 0
#define MODE_RGB 1
#define MODE_LAB 2

/* index of the alpha channel settings, for colour and grayscale images */
#define ALPHA_INDEX 3

/* the settings of one denoising run, read from the procedure config */
typedef struct
{
  gint colour_mode;
  gdouble thresholds[4];
  gdouble low[4];
  /* 0 = all channels, otherwise the displayed channel + 1 */
  gint preview_channel;
  gboolean preview_colour;
  /* show the displayed channels undenoised in the preview (hold to
     compare) */
  gboolean compare;
} wavelet_settings;

void wavelet_denoise (float *fimg[3], unsigned int width,
			     unsigned int height, float threshold, double low,
			     float a, float b);
gboolean denoise (GimpDrawable * drawable, GimpPreview * preview,
		  const wavelet_settings * settings, GError ** error);
void settings_from_config (GimpProcedureConfig * config,
			   wavelet_settings * settings);
gint settings_index (gint channels, gint c);
gboolean user_interface (GimpProcedure * procedure,
			 GimpProcedureConfig * config,
			 GimpDrawable * drawable);

void srgb2rgb (float **fimg, int size);
void rgb2srgb (float **fimg, int size, int pc);
void srgb2ycbcr (float **fimg, int size);
void ycbcr2srgb (float **fimg, int size, int pc);
void srgb2lab (float **fimg, int size);
void lab2srgb (float **fimg, int size, int pc);
void srgb2xyz (float **fimg, int size);
void xyz2srgb (float **fimg, int size, int pc);

extern const char *names_ycbcr[];
extern const char *names_rgb[];
extern const char *names_gray[];
extern const char *names_lab[];

/* names of the settings properties, indexed like wavelet_settings */
extern const char *threshold_props[];
extern const char *low_props[];

#endif /* __PLUGIN_H__ */
