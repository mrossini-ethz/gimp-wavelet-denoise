/*
 * Wavelet denoise GIMP plugin
 *
 * denoise.c
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

#include "plugin.h"

/* number of rows transferred from and to GIMP at once */
#define STRIP_HEIGHT 64

/* progress fractions of reading and writing, the rest is denoising */
#define PROGRESS_READ 0.1
#define PROGRESS_WRITE 0.1

/* The algorithm works on non-linear values in [0,1], as it did on 8-bit
   sRGB data before. Use the matching float format in the colour space of
   the drawable, so that any precision is processed without loss. */
static const Babl *
get_float_format (GimpDrawable * drawable)
{
  const Babl *space = babl_format_get_space (gimp_drawable_get_format
					     (drawable));
  const char *name;

  if (gimp_drawable_is_rgb (drawable))
    name = gimp_drawable_has_alpha (drawable) ? "R'G'B'A float"
      : "R'G'B' float";
  else
    name = gimp_drawable_has_alpha (drawable) ? "Y'A float" : "Y' float";

  return babl_format_with_space (name, space);
}

/* only float images may hold values outside of [0,1] */
static gboolean
drawable_is_float (GimpDrawable * drawable)
{
  const Babl *type = babl_format_get_type (gimp_drawable_get_format
					   (drawable), 0);

  return type == babl_type ("half") || type == babl_type ("float")
    || type == babl_type ("double");
}

static void
free_planes (float **planes, gint n)
{
  gint i;

  for (i = 0; i < n; i++)
    g_free (planes[i]);
}

gboolean
denoise (GimpDrawable * drawable, GimpPreview * preview,
	 const wavelet_settings * settings, GError ** error)
{
  float *fimg[4] = { NULL, NULL, NULL, NULL };
  float *buffer[3] = { NULL, NULL, NULL };
  float *strip;
  GeglBuffer *src_buffer, *dest_buffer;
  const Babl *format;
  gint x1, y1, width, height, y, c, channels, rows, pch;
  gsize i, size, bufsize;
  gboolean clip_colour, alloc_ok;
  gint channels_to_denoise, channels_denoised;
  double progress_step;

  if (preview)
    {
      gimp_preview_get_position (preview, &x1, &y1);
      gimp_preview_get_size (preview, &width, &height);
    }
  else if (!gimp_drawable_mask_intersect (drawable, &x1, &y1, &width,
					  &height))
    return TRUE;

  if (width <= 0 || height <= 0)
    return TRUE;

  format = get_float_format (drawable);
  channels = babl_format_get_n_components (format);
  clip_colour = !drawable_is_float (drawable);

  /* the displayed channel in single channel preview, -1 = all */
  pch = preview ? settings->preview_channel - 1 : -1;

  /* count the channels to denoise for the progress bar */
  channels_to_denoise = 0;
  for (c = 0; c < channels; c++)
    if (settings->thresholds[settings_index (channels, c)] > 0)
      channels_to_denoise++;

  /* nothing to do: leave the drawable exactly as it is */
  if (!preview && channels_to_denoise == 0)
    return TRUE;

  /* allocate one plane per channel plus two for the wavelet transform;
     the colour conversions and the wavelet code count pixels in int */
  if (!g_size_checked_mul (&size, width, height) || size > G_MAXINT
      || !g_size_checked_mul (&bufsize, size, sizeof (float)))
    {
      g_set_error (error, GIMP_PLUG_IN_ERROR, 0,
		   _("Image dimensions too large: width %d x height %d"),
		   width, height);
      return FALSE;
    }
  alloc_ok = TRUE;
  for (c = 0; c < channels; c++)
    alloc_ok = alloc_ok && (fimg[c] = g_try_malloc (bufsize)) != NULL;
  alloc_ok = alloc_ok && (buffer[1] = g_try_malloc (bufsize)) != NULL;
  alloc_ok = alloc_ok && (buffer[2] = g_try_malloc (bufsize)) != NULL;
  strip = alloc_ok ? g_try_malloc ((gsize) width * STRIP_HEIGHT * channels
				   * sizeof (float)) : NULL;
  if (!strip)
    {
      free_planes (fimg, channels);
      free_planes (buffer + 1, 2);
      g_set_error (error, GIMP_PLUG_IN_ERROR, 0,
		   _("There was not enough memory to complete the "
		     "operation."));
      return FALSE;
    }

  progress_step = (1.0 - PROGRESS_READ - PROGRESS_WRITE)
    / MAX2 (channels_to_denoise, 1);

  /* read the image from GIMP */
  if (!preview)
    /* TRANSLATORS: This is the message displayed while denoising is in
       progress */
    gimp_progress_init (_("Wavelet denoising..."));
  src_buffer = gimp_drawable_get_buffer (drawable);
  for (y = 0; y < height; y += STRIP_HEIGHT)
    {
      rows = MIN2 (STRIP_HEIGHT, height - y);
      gegl_buffer_get (src_buffer, GEGL_RECTANGLE (x1, y1 + y, width, rows),
		       1.0, format, strip, GEGL_AUTO_ROWSTRIDE,
		       GEGL_ABYSS_NONE);
      for (i = 0; i < (gsize) rows * width; i++)
	for (c = 0; c < channels; c++)
	  fimg[c][y * width + i] = strip[i * channels + c];
      if (!preview)
	gimp_progress_update (PROGRESS_READ * (y + rows) / height);
    }
  g_object_unref (src_buffer);

  /* do colour model conversion sRGB[0,1] -> whatever */
  if (channels > 2)
    {
      if (settings->colour_mode == MODE_YCBCR)
	srgb2ycbcr (fimg, size);
      else if (settings->colour_mode == MODE_LAB)
	srgb2lab (fimg, size);
      else if (settings->colour_mode == MODE_RGB)
	srgb2rgb (fimg, size);
    }

  /* denoise the channels individually */
  channels_denoised = 0;
  for (c = 0; c < channels; c++)
    {
      gint index = settings_index (channels, c);
      double a, b;

      if (settings->thresholds[index] <= 0)
	continue;
      /* in preview mode only process the displayed channel */
      if (pch >= 0 && pch != c)
	continue;
      /* leave the displayed channels alone while comparing */
      if (preview && settings->compare)
	continue;
      buffer[0] = fimg[c];
      a = PROGRESS_READ + channels_denoised * progress_step;
      b = preview ? 0.0 : progress_step;
      wavelet_denoise (buffer, width, height,
		       (float) settings->thresholds[index],
		       settings->low[index], a, b);
      channels_denoised++;
    }

  /* retransform the image data */
  if (channels > 2)
    {
      int pc = 0;
      if (pch >= 0 && !settings->preview_colour)
	pc = pch + 1;
      else if (pch >= 0)
	pc = pch + 4;

      if (settings->colour_mode == MODE_YCBCR)
	ycbcr2srgb (fimg, size, pc);
      else if (settings->colour_mode == MODE_LAB)
	lab2srgb (fimg, size, pc);
      else if (settings->colour_mode == MODE_RGB)
	rgb2srgb (fimg, size, pc);
    }

  if (pch >= 0 && channels % 2 == 0)
    {
      /* if alpha channel preview, display alpha as gray */
      if (pch == channels - 1)
	for (c = 0; c < channels - 1; c++)
	  memcpy (fimg[c], fimg[channels - 1], bufsize);

      /* show single channels at full opacity */
      for (i = 0; i < size; i++)
	fimg[channels - 1][i] = 1.0;
    }

  /* clip the values */
  for (c = 0; c < channels; c++)
    {
      if (!clip_colour && !(channels % 2 == 0 && c == channels - 1))
	continue;
      for (i = 0; i < size; i++)
	fimg[c][i] = CLIP (fimg[c][i], 0.0, 1.0);
    }

  if (preview)
    {
      /* the preview is drawn from 8-bit data of the drawable layout */
      guchar *line = g_malloc ((gsize) width * height * channels);

      for (i = 0; i < size; i++)
	for (c = 0; c < channels; c++)
	  line[i * channels + c] = (guchar)
	    (CLIP (fimg[c][i], 0.0, 1.0) * 255.0 + 0.5);
      gimp_preview_draw_buffer (preview, line, width * channels);
      g_free (line);
    }
  else
    {
      /* write the image back to GIMP */
      dest_buffer = gimp_drawable_get_shadow_buffer (drawable);
      for (y = 0; y < height; y += STRIP_HEIGHT)
	{
	  rows = MIN2 (STRIP_HEIGHT, height - y);
	  for (i = 0; i < (gsize) rows * width; i++)
	    for (c = 0; c < channels; c++)
	      strip[i * channels + c] = fimg[c][y * width + i];
	  gegl_buffer_set (dest_buffer,
			   GEGL_RECTANGLE (x1, y1 + y, width, rows), 0,
			   format, strip, GEGL_AUTO_ROWSTRIDE);
	  gimp_progress_update (1.0 - PROGRESS_WRITE
				+ PROGRESS_WRITE * (y + rows) / height);
	}
      g_object_unref (dest_buffer);

      gimp_drawable_merge_shadow (drawable, TRUE);
      gimp_drawable_update (drawable, x1, y1, width, height);
    }

  g_free (strip);
  free_planes (fimg, channels);
  free_planes (buffer + 1, 2);

  return TRUE;
}
