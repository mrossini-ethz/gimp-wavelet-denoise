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

#include "plugin.h"

G_DEFINE_TYPE (WaveletDenoise, wavelet_denoise, GIMP_TYPE_PLUG_IN)

GIMP_MAIN (WAVELET_DENOISE_TYPE)

/* Global variables declared in plugin.h */
float *fimg[4];
float *buffer[3];
gint channels;

GTimer *timer;

wavelet_settings settings = {
  {0, 0},			/* gray_thresholds */
  {0.0, 0.0},			/* gray_low */
  {0, 0.4, 0.4, 0},			/* colour_thresholds */
  {0.0, 0.0, 0.0, 0.0},		/* colour_low */
  MODE_YCBCR,			/* colour_mode */
  0,				/* preview_channel */
  0,				/* preview_mode */
  TRUE,				/* preview */
  {1.1, 4.5, 1.4},		/* times */
  0, 0				/* winxsize, winysize */
};

char *names_ycbcr[] = { "Y", "Cb", "Cr", N_("Alpha") };
char *names_rgb[] = { "R", "G", "B", N_("Alpha") };
char *names_gray[] = { "Gray", N_("Alpha") };
char *names_lab[] = { "L*", "a*", "b*", N_("Alpha") };

static GList *
wavelet_denoise_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup ("plug-in-wavelet-denoise"));
}

static GimpValueArray *
wavelet_denoise_run (GimpProcedure        *procedure,
                     GimpRunMode           run_mode,
                     GimpImage            *image,
                     GimpDrawable        **drawables,
                     GimpProcedureConfig  *config,
                     gpointer              run_data)
{
  GimpDrawable *drawable;
  gint i;
  gint width, height;
  GimpParasite *parasite;

  if (drawables == NULL || drawables[0] == NULL)
    {
      return gimp_procedure_new_return_values (procedure,
                                                GIMP_PDB_CALLING_ERROR,
                                                NULL);
    }

  drawable = drawables[0];

  if (gimp_drawable_is_rgb (drawable))
    channels = gimp_drawable_has_alpha (drawable) ? 4 : 3;
  else
    channels = gimp_drawable_has_alpha (drawable) ? 2 : 1;

  width = gimp_drawable_get_width (drawable);
  height = gimp_drawable_get_height (drawable);

  timer = g_timer_new ();

  /* restore settings saved in GIMP core parasite */
  parasite = gimp_get_parasite ("plug-in-wavelet-denoise-settings");
  if (parasite)
    {
      guint32 size = 0;
      const void *data = gimp_parasite_get_data (parasite, &size);
      if (size == sizeof (wavelet_settings))
        memcpy (&settings, data, sizeof (wavelet_settings));
      gimp_parasite_free (parasite);
    }

  if (settings.preview_channel > channels - 1)
    settings.preview_channel = 0;

  /* allocate buffers */
  for (i = 0; i < channels; i++)
    {
      fimg[i] = (float *) malloc (width * height * sizeof (float));
    }
  buffer[1] = (float *) malloc (width * height * sizeof (float));
  buffer[2] = (float *) malloc (width * height * sizeof (float));

  /* run GUI if in interactive mode */
  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (!user_interface (drawable))
	{
	  for (i = 0; i < channels; i++)
	    free (fimg[i]);
	  free (buffer[1]);
	  free (buffer[2]);
	  g_timer_destroy (timer);
	  return gimp_procedure_new_return_values (procedure,
						    GIMP_PDB_CANCEL,
						    NULL);
	}
    }

  denoise (drawable, NULL);

  /* free buffers */
  for (i = 0; i < channels; i++)
    {
      free (fimg[i]);
    }
  free (buffer[1]);
  free (buffer[2]);
  g_timer_destroy (timer);

  gimp_displays_flush ();

  /* save settings in GIMP core parasite */
  parasite = gimp_parasite_new ("plug-in-wavelet-denoise-settings",
                                GIMP_PARASITE_PERSISTENT,
                                sizeof (wavelet_settings),
                                &settings);
  gimp_attach_parasite (parasite);
  gimp_parasite_free (parasite);

  return gimp_procedure_new_return_values (procedure,
                                            GIMP_PDB_SUCCESS,
                                            NULL);
}

static GimpProcedure *
wavelet_denoise_create_procedure (GimpPlugIn  *plug_in,
                                  const gchar *name)
{
  GimpProcedure *procedure = NULL;

  if (g_strcmp0 (name, "plug-in-wavelet-denoise") == 0)
    {
      procedure = gimp_image_procedure_new (plug_in, name,
                                            GIMP_PDB_PROC_TYPE_PLUGIN,
                                            wavelet_denoise_run, NULL, NULL);

      gimp_procedure_set_image_types (procedure, "RGB*, GRAY*");
      gimp_procedure_set_sensitivity_mask (procedure,
                                            GIMP_PROCEDURE_SENSITIVE_DRAWABLE);

      /* TRANSLATORS: Menu entry of the plugin. Use under-
         score for identifying hotkey */
      gimp_procedure_set_menu_label (procedure, N_("_Wavelet denoise..."));
      gimp_procedure_add_menu_path (procedure, "<Image>/Filters/Enhance");

      gimp_procedure_set_documentation (procedure,
                                         _("Removes noise in the image using wavelets."),
                                         PLUGIN_HELP,
                                         name);
      gimp_procedure_set_attribution (procedure,
                                       _("Marco Rossini"),
                                       _("Copyright 2008 Marco Rossini"),
                                       "2008");
    }

  return procedure;
}

static gboolean
wavelet_denoise_set_i18n (GimpPlugIn   *plug_in,
                          const gchar  *procedure_name,
                          gchar       **gettext_domain,
                          gchar       **catalog_dir)
{
  *gettext_domain = g_strdup ("gimp20-wavelet-denoise-plug-in");
  *catalog_dir    = g_strdup (LOCALEDIR);
  return TRUE;
}

static void
wavelet_denoise_class_init (WaveletDenoiseClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);

  plug_in_class->query_procedures = wavelet_denoise_query_procedures;
  plug_in_class->create_procedure = wavelet_denoise_create_procedure;
  plug_in_class->set_i18n         = wavelet_denoise_set_i18n;
}

static void
wavelet_denoise_init (WaveletDenoise *denoise)
{
}

