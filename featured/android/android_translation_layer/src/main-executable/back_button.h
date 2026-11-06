#include <gtk/gtk.h>

GtkWidget *back_button_new();
void back_button_set_sensitive(bool sensitive);
// "back" as an application action, for a host's own back control
void back_action_add(GActionMap *map);
