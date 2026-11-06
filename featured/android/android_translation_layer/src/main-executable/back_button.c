#include "back_button.h"

#include "../api-impl-jni/app/android_app_Activity.h"

GtkWidget *back_button;
// the same as an action ("app.back"): a host (orbiter's bar) shows it
static GSimpleAction *back_action;

static void back_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	current_activity_back_pressed();
}

GtkWidget *back_button_new()
{
	back_button = gtk_button_new_from_icon_name("go-previous");

	g_signal_connect(back_button, "clicked", G_CALLBACK(current_activity_back_pressed), NULL);

	back_button_set_sensitive(false);
	return back_button;
}

void back_action_add(GActionMap *map)
{
	back_action = g_simple_action_new("back", NULL);
	g_signal_connect(back_action, "activate", G_CALLBACK(back_activated), NULL);
	g_simple_action_set_enabled(back_action, gtk_widget_get_sensitive(back_button));
	g_action_map_add_action(map, G_ACTION(back_action));
}

void back_button_set_sensitive(bool sensitive)
{
	gtk_widget_set_sensitive(GTK_WIDGET(back_button), sensitive);
	if (back_action)
		g_simple_action_set_enabled(back_action, sensitive);
}
