/*
 * gedit-file-chooser-dialog-native.c
 * This file is part of gedit
 *
 * The save dialog as a GtkFileChooserNative: the desktop's file
 * chooser (its portal) picks the file. The document keeps its
 * encoding and line ending: a portal has no place for gedit's
 * own choices. Added for silver's orbiter.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "gedit-file-chooser-dialog-native.h"
#include <gfls/gfls.h>

struct _GeditFileChooserDialogNative
{
	GObject parent_instance;

	GtkFileChooserNative *native;
	GflsEncoding *encoding;
	GtkSourceNewlineType newline_type;
};

static void gedit_file_chooser_dialog_native_chooser_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (GeditFileChooserDialogNative,
                        gedit_file_chooser_dialog_native,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GEDIT_TYPE_FILE_CHOOSER_DIALOG,
                                               gedit_file_chooser_dialog_native_chooser_init))

static void
chooser_set_encoding (GeditFileChooserDialog *dialog,
		      const GflsEncoding     *encoding)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	g_clear_pointer (&self->encoding, gfls_encoding_free);
	self->encoding = encoding != NULL ? gfls_encoding_copy (encoding) : NULL;
}

static GflsEncoding *
chooser_get_encoding (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	return self->encoding != NULL ? gfls_encoding_copy (self->encoding) : NULL;
}

static void
chooser_set_newline_type (GeditFileChooserDialog *dialog,
			  GtkSourceNewlineType    newline_type)
{
	GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog)->newline_type = newline_type;
}

static GtkSourceNewlineType
chooser_get_newline_type (GeditFileChooserDialog *dialog)
{
	return GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog)->newline_type;
}

static void
chooser_set_current_folder (GeditFileChooserDialog *dialog,
			    GFile                  *folder)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);
	gchar *uri;

	if (folder == NULL)
	{
		return;
	}

	uri = g_file_get_uri (folder);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (self->native), uri);
	g_free (uri);
}

static void
chooser_set_current_name (GeditFileChooserDialog *dialog,
			  const gchar            *name)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (self->native), name);
}

static void
chooser_set_file (GeditFileChooserDialog *dialog,
		  GFile                  *file)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_file_chooser_set_file (GTK_FILE_CHOOSER (self->native), file, NULL);
}

static GFile *
chooser_get_file (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	return gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->native));
}

static void
chooser_set_do_overwrite_confirmation (GeditFileChooserDialog *dialog,
				       gboolean                overwrite_confirmation)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->native),
							overwrite_confirmation);
}

static void
chooser_show (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (self->native));
}

static void
chooser_destroy (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_native_dialog_destroy (GTK_NATIVE_DIALOG (self->native));
	g_object_unref (self);
}

static void
chooser_set_modal (GeditFileChooserDialog *dialog,
		   gboolean                is_modal)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (dialog);

	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (self->native), is_modal);
}

/* No window of its own: the desktop shows the chooser. */
static GtkWindow *
chooser_get_window (GeditFileChooserDialog *dialog)
{
	return NULL;
}

static void
gedit_file_chooser_dialog_native_chooser_init (gpointer g_iface,
					       gpointer iface_data)
{
	GeditFileChooserDialogInterface *iface = g_iface;

	iface->set_encoding = chooser_set_encoding;
	iface->get_encoding = chooser_get_encoding;
	iface->set_newline_type = chooser_set_newline_type;
	iface->get_newline_type = chooser_get_newline_type;
	iface->set_current_folder = chooser_set_current_folder;
	iface->set_current_name = chooser_set_current_name;
	iface->set_file = chooser_set_file;
	iface->get_file = chooser_get_file;
	iface->set_do_overwrite_confirmation = chooser_set_do_overwrite_confirmation;
	iface->show = chooser_show;
	iface->destroy = chooser_destroy;
	iface->set_modal = chooser_set_modal;
	iface->get_window = chooser_get_window;
}

static void
gedit_file_chooser_dialog_native_dispose (GObject *object)
{
	GeditFileChooserDialogNative *self = GEDIT_FILE_CHOOSER_DIALOG_NATIVE (object);

	g_clear_object (&self->native);
	g_clear_pointer (&self->encoding, gfls_encoding_free);

	G_OBJECT_CLASS (gedit_file_chooser_dialog_native_parent_class)->dispose (object);
}

static void
gedit_file_chooser_dialog_native_class_init (GeditFileChooserDialogNativeClass *klass)
{
	G_OBJECT_CLASS (klass)->dispose = gedit_file_chooser_dialog_native_dispose;
}

static void
gedit_file_chooser_dialog_native_init (GeditFileChooserDialogNative *self)
{
	self->newline_type = GTK_SOURCE_NEWLINE_TYPE_DEFAULT;
}

/* The native dialog's answer, as gedit's "response" signal. */
static void
native_response_cb (GtkNativeDialog              *native,
		    gint                          response_id,
		    GeditFileChooserDialogNative *self)
{
	g_signal_emit_by_name (self, "response", response_id);
}

GeditFileChooserDialog *
gedit_file_chooser_dialog_native_create (const gchar *title,
					 GtkWindow   *parent,
					 const gchar *accept_label,
					 const gchar *cancel_label)
{
	GeditFileChooserDialogNative *self;

	self = g_object_new (GEDIT_TYPE_FILE_CHOOSER_DIALOG_NATIVE, NULL);
	self->native = gtk_file_chooser_native_new (title,
						    parent,
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    accept_label,
						    cancel_label);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (self->native), FALSE);

	g_signal_connect (self->native,
			  "response",
			  G_CALLBACK (native_response_cb),
			  self);

	return GEDIT_FILE_CHOOSER_DIALOG (self);
}
