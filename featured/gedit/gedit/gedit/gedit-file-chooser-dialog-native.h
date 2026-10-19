/*
 * gedit-file-chooser-dialog-native.h
 * This file is part of gedit
 *
 * The save dialog as a GtkFileChooserNative: the desktop's file
 * chooser (its portal) picks the file. Added for silver's orbiter.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef GEDIT_FILE_CHOOSER_DIALOG_NATIVE_H
#define GEDIT_FILE_CHOOSER_DIALOG_NATIVE_H

#include <gtk/gtk.h>
#include "gedit-file-chooser-dialog.h"

G_BEGIN_DECLS

#define GEDIT_TYPE_FILE_CHOOSER_DIALOG_NATIVE (gedit_file_chooser_dialog_native_get_type ())

G_DECLARE_FINAL_TYPE (GeditFileChooserDialogNative, gedit_file_chooser_dialog_native,
		      GEDIT, FILE_CHOOSER_DIALOG_NATIVE,
		      GObject)

GeditFileChooserDialog *	gedit_file_chooser_dialog_native_create	(const gchar *title,
									 GtkWindow   *parent,
									 const gchar *accept_label,
									 const gchar *cancel_label);

G_END_DECLS

#endif /* GEDIT_FILE_CHOOSER_DIALOG_NATIVE_H */
