/*
 * Wavelet denoise GIMP plugin
 *
 * interface.c
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

typedef struct
{
  GimpProcedureConfig *config;
  GimpDrawable *drawable;
  GtkWidget *preview;
  GtkWidget *channel_combo;
  GtkWidget *colour_check;
  GtkWidget *channel_labels[4];
  gint channels;
  /* the displayed channel, not remembered: 0 = all, otherwise channel + 1 */
  gint preview_channel;
  gboolean compare;
} dialog_data;

/* names of the image channels in the current colour model */
static const char **
channel_names (dialog_data * data)
{
  if (data->channels < 3)
    return names_gray;

  switch (gimp_procedure_config_get_choice_id (data->config, "color-model"))
    {
    case MODE_LAB:
      return names_lab;
    case MODE_RGB:
      return names_rgb;
    default:
      return names_ycbcr;
    }
}

static void
preview_update (GimpPreview * preview, dialog_data * data)
{
  wavelet_settings settings;

  settings_from_config (data->config, &settings);
  settings.preview_channel = data->preview_channel;
  settings.compare = data->compare;
  denoise (data->drawable, preview, &settings, NULL);
}

static void
update_channel_names (dialog_data * data)
{
  const char **names = channel_names (data);
  GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT (data->channel_combo);
  gint active, c;

  /* rebuild the preview channel list, keeping the selection */
  active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
  g_signal_handlers_block_matched (combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL,
				   NULL, data);
  gtk_combo_box_text_remove_all (combo);
  /* TRANSLATORS: *All* channels (from the preview select frame) */
  gtk_combo_box_text_append_text (combo, _("All"));
  for (c = 0; c < data->channels; c++)
    gtk_combo_box_text_append_text (combo, _(names[c]));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);
  g_signal_handlers_unblock_matched (combo, G_SIGNAL_MATCH_DATA, 0, 0,
				     NULL, NULL, data);

  for (c = 0; c < data->channels; c++)
    gtk_label_set_text (GTK_LABEL (data->channel_labels[c]), _(names[c]));
}

static void
update_colour_check (dialog_data * data)
{
  gint channel = data->preview_channel;

  if (!data->colour_check)
    return;

  /* colour display is only meaningful for single colour channels */
  gtk_widget_set_sensitive (data->colour_check, channel > 0 && channel < 4);
}

static void
preview_channel_changed (GtkComboBox * combo, dialog_data * data)
{
  gint active = gtk_combo_box_get_active (combo);

  if (active >= 0)
    data->preview_channel = active;
  update_colour_check (data);
  gimp_preview_invalidate (GIMP_PREVIEW (data->preview));
}

static void
compare_pressed (GtkButton * button, dialog_data * data)
{
  data->compare = TRUE;
  gimp_preview_invalidate (GIMP_PREVIEW (data->preview));
}

static void
compare_released (GtkButton * button, dialog_data * data)
{
  data->compare = FALSE;
  gimp_preview_invalidate (GIMP_PREVIEW (data->preview));
}

/* collect the radio buttons below widget in their order */
static void
collect_radios (GtkWidget * widget, gpointer list)
{
  if (GTK_IS_RADIO_BUTTON (widget))
    *(GList **) list = g_list_append (*(GList **) list, widget);
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget), collect_radios, list);
}

gboolean
user_interface (GimpProcedure * procedure, GimpProcedureConfig * config,
		GimpDrawable * drawable)
{
  static dialog_data data;
  GimpProcedureDialog *dialog;
  GtkWidget *widget, *box, *button;
  GList *controls = NULL, *radios = NULL;
  gchar *id;
  gint c, index;
  gboolean run;

  data.config = config;
  data.drawable = drawable;
  data.channels = (gimp_drawable_is_rgb (drawable) ? 3 : 1)
    + (gimp_drawable_has_alpha (drawable) ? 1 : 0);
  data.compare = FALSE;
  data.colour_check = NULL;
  data.channel_combo = NULL;

  /* always start with the preview of all channels */
  data.preview_channel = 0;

  gimp_ui_init (PLUG_IN_BINARY);

  dialog = GIMP_PROCEDURE_DIALOG (gimp_procedure_dialog_new (procedure,
							     config,
							     PLUGIN_NAME));

  /* prepare the preview */
  data.preview = gimp_procedure_dialog_get_drawable_preview (dialog,
							       "preview",
							       drawable);
  /* size required for proper noise profiling */
  gtk_widget_set_size_request (data.preview, 300, 304);
  g_signal_connect (data.preview, "invalidated", G_CALLBACK (preview_update),
		    &data);
  g_signal_connect_object (config, "notify",
			   G_CALLBACK (gimp_preview_invalidate), data.preview,
			   G_CONNECT_SWAPPED);

  /* prepare the colour model frame */
  if (data.channels > 2)
    {
      const char *tips[] = { TT_MODEL_YCBCR, TT_MODEL_LAB, TT_MODEL_RGB };
      GList *iter;

      widget = gimp_procedure_dialog_get_widget (dialog, "color-model",
						 GIMP_TYPE_INT_RADIO_FRAME);
      collect_radios (widget, &radios);
      for (iter = radios, c = 0; iter && c < 3; iter = iter->next, c++)
	gtk_widget_set_tooltip_text (iter->data, tips[c]);
      g_list_free (radios);
      controls = g_list_append (controls, g_strdup ("color-model"));
    }

  /* prepare the preview select frame */
  if (data.channels > 1)
    {
      if (data.channels > 2)
	{
	  data.colour_check =
	    gimp_procedure_dialog_get_widget (dialog, "preview-in-color",
					      GTK_TYPE_CHECK_BUTTON);
	  box = gimp_procedure_dialog_fill_box (dialog, "preview-box",
						"preview-in-color", NULL);
	}
      else
	{
	  /* a box without any item would get all arguments, so it holds a
	     hidden empty label */
	  GtkWidget *label =
	    gimp_procedure_dialog_get_label (dialog, "preview-placeholder", "",
					     FALSE, FALSE);
	  box = gimp_procedure_dialog_fill_box (dialog, "preview-box",
						"preview-placeholder", NULL);
	  gtk_widget_set_no_show_all (label, TRUE);
	  gtk_widget_hide (label);
	}

      data.channel_combo = gtk_combo_box_text_new ();
      gtk_widget_set_tooltip_text (data.channel_combo, TT_SELECT);
      gtk_box_pack_start (GTK_BOX (box), data.channel_combo, FALSE, FALSE, 0);
      gtk_box_reorder_child (GTK_BOX (box), data.channel_combo, 0);
      gtk_widget_show (data.channel_combo);

      button = gtk_button_new_with_label (_("Hold to compare"));
      gtk_widget_set_tooltip_text (button, TT_COMPARE);
      g_signal_connect (button, "pressed", G_CALLBACK (compare_pressed),
			&data);
      g_signal_connect (button, "released", G_CALLBACK (compare_released),
			&data);
      gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
      gtk_widget_show (button);

      gimp_procedure_dialog_get_label (dialog, "preview-title",
				       _("Preview channel"), FALSE, FALSE);
      gimp_procedure_dialog_fill_frame (dialog, "preview-frame",
					"preview-title", FALSE, "preview-box");
      controls = g_list_append (controls, g_strdup ("preview-frame"));
    }

  /* prepare one settings frame per channel */
  for (c = 0; c < data.channels; c++)
    {
      gchar *box_id = g_strdup_printf ("channel-box-%d", c);
      gchar *title_id = g_strdup_printf ("channel-title-%d", c);

      index = settings_index (data.channels, c);

      widget = gimp_procedure_dialog_get_spin_scale (dialog,
						     threshold_props[index],
						     1.0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 2);
      gtk_spin_button_set_increments (GTK_SPIN_BUTTON (widget), 0.01, 0.1);
      gimp_help_set_help_data (widget, data.channels > 1
			       ? TT_THR_AMOUNT_COLOUR : TT_THR_AMOUNT_GRAY,
			       NULL);

      widget = gimp_procedure_dialog_get_spin_scale (dialog,
						     low_props[index], 1.0);
      gtk_spin_button_set_digits (GTK_SPIN_BUTTON (widget), 2);
      gtk_spin_button_set_increments (GTK_SPIN_BUTTON (widget), 0.01, 0.1);
      gimp_help_set_help_data (widget, TT_THR_DETAIL_COLOUR, NULL);

      gimp_procedure_dialog_fill_box (dialog, box_id, threshold_props[index],
				      low_props[index], NULL);
      data.channel_labels[c] =
	gimp_procedure_dialog_get_label (dialog, title_id, "", FALSE, FALSE);

      id = g_strdup_printf ("channel-frame-%d", c);
      gimp_procedure_dialog_fill_frame (dialog, id, title_id, FALSE, box_id);
      controls = g_list_append (controls, id);

      g_free (box_id);
      g_free (title_id);
    }

  /* fill in the channel names of the colour model and follow changes */
  if (data.channel_combo)
    {
      update_channel_names (&data);
      gtk_combo_box_set_active (GTK_COMBO_BOX (data.channel_combo),
				data.preview_channel);
      g_signal_connect (data.channel_combo, "changed",
			G_CALLBACK (preview_channel_changed), &data);
      update_colour_check (&data);
    }
  else
    gtk_label_set_text (GTK_LABEL (data.channel_labels[0]), _(names_gray[0]));
  g_signal_connect_swapped (config, "notify::color-model",
			    G_CALLBACK (update_channel_names), &data);

  /* prepare the dialog: preview on the left, settings on the right */
  gimp_procedure_dialog_fill_box_list (dialog, "controls-box", controls);
  box = gimp_procedure_dialog_fill_box (dialog, "main-box", "preview",
					"controls-box", NULL);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (box),
				  GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (box), 12);
  gimp_procedure_dialog_fill (dialog, "main-box", NULL);

  run = gimp_procedure_dialog_run (dialog);
  /* the config outlives the dialog */
  g_signal_handlers_disconnect_by_data (config, &data);
  gtk_widget_destroy (GTK_WIDGET (dialog));

  g_list_free_full (controls, g_free);
  return run;
}
