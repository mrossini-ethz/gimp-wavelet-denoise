/* 
 * Wavelet denoise GIMP plugin
 * 
 * interface.h
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

/* colour mode frame */
extern GtkWidget *fr_mode, *mode_radio[3], *mode_vbox;
extern GSList *mode_list;

/* preview select frame */
extern GtkWidget *fr_preview, *preview_radio[3], *preview_vbox, *preview_check;
extern GSList *preview_list;

/* channel select frame */
extern GtkWidget *fr_channel, *channel_radio[4], *channel_vbox;
extern GSList *channel_list;

/* threshold frame */
extern GtkWidget *fr_threshold, *thr_label[2], *thr_spin[2];
extern GtkWidget *thr_hbox[2], *thr_vbox, *thr_scale[2];
extern GtkObject *thr_adj[2];

/* reset buttons */
extern GtkWidget *reset_button[2], *reset_hbox, *reset_align, *reset_button_icon[2];

/* dialog */
extern GtkWidget *dialog, *dialog_hbox, *dialog_vbox, *frame_hbox, *dialog_aspect;
extern GtkWidget *preview, *preview_reset, *preview_hbox, *preview_reset_icon;

extern GtkWidget **radios_labels[];

extern char **names;
