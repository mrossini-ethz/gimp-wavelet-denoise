/*
 * Wavelet denoise GIMP plugin
 *
 * plugin.c
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

typedef struct _WaveletDenoise WaveletDenoise;
typedef struct _WaveletDenoiseClass WaveletDenoiseClass;

struct _WaveletDenoise
{
  GimpPlugIn parent_instance;
};

struct _WaveletDenoiseClass
{
  GimpPlugInClass parent_class;
};

#define WAVELET_DENOISE_TYPE (wavelet_denoise_get_type ())

GType wavelet_denoise_get_type (void);

static GList *wavelet_denoise_query_procedures (GimpPlugIn * plug_in);
static GimpProcedure *wavelet_denoise_create_procedure (GimpPlugIn * plug_in,
							const gchar * name);
static GimpValueArray *wavelet_denoise_run (GimpProcedure * procedure,
					    GimpRunMode run_mode,
					    GimpImage * image,
					    GimpDrawable ** drawables,
					    GimpProcedureConfig * config,
					    gpointer run_data);

G_DEFINE_TYPE (WaveletDenoise, wavelet_denoise, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (WAVELET_DENOISE_TYPE)

const char *names_ycbcr[] = { "Y", "Cb", "Cr", N_("Alpha") };
const char *names_rgb[] = { "R", "G", "B", N_("Alpha") };
const char *names_gray[] = { N_("Gray"), N_("Alpha") };
const char *names_lab[] = { "L*", "a*", "b*", N_("Alpha") };

const char *threshold_props[] =
  { "threshold-1", "threshold-2", "threshold-3", "threshold-alpha" };
const char *low_props[] =
  { "softness-1", "softness-2", "softness-3", "softness-alpha" };

/* default thresholds of the first three channels and alpha */
static const double default_thresholds[] = { 0.0, 0.4, 0.4, 0.0 };

static void
wavelet_denoise_class_init (WaveletDenoiseClass * klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = wavelet_denoise_query_procedures;
  plug_in_class->create_procedure = wavelet_denoise_create_procedure;
}

static void
wavelet_denoise_init (WaveletDenoise * wavelet_denoise)
{
}

static GList *
wavelet_denoise_query_procedures (GimpPlugIn * plug_in)
{
  return g_list_append (NULL, g_strdup (PLUG_IN_PROC));
}

static GimpProcedure *
wavelet_denoise_create_procedure (GimpPlugIn * plug_in, const gchar * name)
{
  GimpProcedure *procedure = NULL;
  gint i;

  /* TRANSLATORS: Blurbs of the per-channel settings for scripts. The
     first channel is Y, L* or R of colour images, or gray. */
  const char *threshold_blurbs[] = {
    N_("Threshold of the first channel (Y, L*, R or gray)"),
    N_("Threshold of the second channel (Cb, a* or G)"),
    N_("Threshold of the third channel (Cr, b* or B)"),
    N_("Threshold of the alpha channel")
  };
  const char *low_blurbs[] = {
    N_("Softness of the first channel (Y, L*, R or gray)"),
    N_("Softness of the second channel (Cb, a* or G)"),
    N_("Softness of the third channel (Cr, b* or B)"),
    N_("Softness of the alpha channel")
  };

  if (strcmp (name, PLUG_IN_PROC))
    return NULL;

  procedure = gimp_image_procedure_new (plug_in, name,
					GIMP_PDB_PROC_TYPE_PLUGIN,
					wavelet_denoise_run, NULL, NULL);

  gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
  gimp_procedure_set_sensitivity_mask (procedure,
				       GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

  /* TRANSLATORS: Menu entry of the plugin. Use underscore for identifying
     hotkey */
  gimp_procedure_set_menu_label (procedure, _("_Wavelet denoise ..."));
  gimp_procedure_add_menu_path (procedure, "<Image>/Filters/Enhance");

  gimp_procedure_set_documentation (procedure,
				    _("Removes noise in the image using "
				      "wavelets."), PLUGIN_HELP, name);
  gimp_procedure_set_attribution (procedure, "Marco Rossini",
				  "Copyright 2008 Marco Rossini", "2008");

  gimp_procedure_add_choice_argument (procedure, "color-model",
				      _("Color _model"),
				      _("Color model in which the channels "
					"are denoised. Ignored for grayscale "
					"images."),
				      gimp_choice_new_with_values
				      ("ycbcr", MODE_YCBCR, "YCbCr", NULL,
				       "lab", MODE_LAB, "CIELAB", NULL,
				       "rgb", MODE_RGB, "RGB", NULL,
				       NULL), "ycbcr", G_PARAM_READWRITE);

  for (i = 0; i < 4; i++)
    {
      gimp_procedure_add_double_argument (procedure, threshold_props[i],
					  _("_Threshold"),
					  _(threshold_blurbs[i]),
					  0.0, 10.0, default_thresholds[i],
					  G_PARAM_READWRITE);
      gimp_procedure_add_double_argument (procedure, low_props[i],
					  _("So_ftness"),
					  _(low_blurbs[i]),
					  0.0, 1.0, 0.0, G_PARAM_READWRITE);
    }

  /* dialog state, remembered between runs but not a script argument */
  gimp_procedure_add_boolean_aux_argument (procedure, "preview-in-color",
					   _("Sho_w in color"),
					   TT_PREVIEW_SEL_COLOUR,
					   FALSE, G_PARAM_READWRITE);

  return procedure;
}

/* map channel c of an image with the given number of channels to the
   index of its settings (grayscale alpha shares the colour alpha settings) */
gint
settings_index (gint channels, gint c)
{
  if (channels % 2 == 0 && c == channels - 1)
    return ALPHA_INDEX;
  return c;
}

void
settings_from_config (GimpProcedureConfig * config,
		      wavelet_settings * settings)
{
  gint i;

  settings->colour_mode =
    gimp_procedure_config_get_choice_id (config, "color-model");
  for (i = 0; i < 4; i++)
    g_object_get (config,
		  threshold_props[i], &settings->thresholds[i],
		  low_props[i], &settings->low[i], NULL);
  g_object_get (config, "preview-in-color", &settings->preview_colour, NULL);
  settings->preview_channel = 0;
  settings->compare = FALSE;
}

static GimpValueArray *
wavelet_denoise_run (GimpProcedure * procedure, GimpRunMode run_mode,
		     GimpImage * image, GimpDrawable ** drawables,
		     GimpProcedureConfig * config, gpointer run_data)
{
  GimpDrawable *drawable;
  wavelet_settings settings;
  GError *error = NULL;

  gegl_init (NULL, NULL);

  if (gimp_core_object_array_get_length ((GObject **) drawables) != 1)
    {
      g_set_error (&error, GIMP_PLUG_IN_ERROR, 0,
		   _("Procedure '%s' only works with one drawable."),
		   PLUG_IN_PROC);
      return gimp_procedure_new_return_values (procedure,
					       GIMP_PDB_CALLING_ERROR, error);
    }
  drawable = drawables[0];

  /* GIMP does not check the image types for calls from scripts, and the
     menu entry is also active for layer groups */
  if (gimp_drawable_is_indexed (drawable))
    g_set_error (&error, GIMP_PLUG_IN_ERROR, 0,
		 _("Procedure '%s' does not work with indexed images."),
		 PLUG_IN_PROC);
  else if (gimp_item_is_group (GIMP_ITEM (drawable)))
    g_set_error (&error, GIMP_PLUG_IN_ERROR, 0,
		 _("Cannot modify the pixels of layer groups."));
  else if (gimp_item_get_lock_content (GIMP_ITEM (drawable)))
    g_set_error (&error, GIMP_PLUG_IN_ERROR, 0,
		 _("The pixels of the layer are locked."));
  if (error)
    return gimp_procedure_new_return_values (procedure,
					     GIMP_PDB_CALLING_ERROR, error);

  /* run GUI if in interactive mode */
  if (run_mode == GIMP_RUN_INTERACTIVE
      && !user_interface (procedure, config, drawable))
    return gimp_procedure_new_return_values (procedure, GIMP_PDB_CANCEL,
					     NULL);

  settings_from_config (config, &settings);

  if (!denoise (drawable, NULL, &settings, &error))
    return gimp_procedure_new_return_values (procedure,
					     GIMP_PDB_EXECUTION_ERROR, error);

  if (run_mode != GIMP_RUN_NONINTERACTIVE)
    gimp_displays_flush ();

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS,
					   NULL);
}
