#include <gtk/gtk.h>

#include "../defines.h"
#include "../util.h"

#include "WrapperWidget.h"

#include "../generated_headers/android_widget_TextView.h"

static GtkLabel *box_get_label(JNIEnv *env, GtkWidget *box)
{
	GtkWidget *label = gtk_widget_get_last_child(GTK_WIDGET(box));
	if (!GTK_IS_LABEL(label))
		label = gtk_widget_get_prev_sibling(label);
	return GTK_LABEL(label);
}

JNIEXPORT jlong JNICALL Java_android_widget_TextView_native_1constructor(JNIEnv *env, jobject this, jobject context, jobject attrs)
{
	const char *text = attribute_set_get_string(env, attrs, "text", NULL);

	//	_SET_OBJ_FIELD(this, "text", "Ljava/lang/String;", _JSTRING(text)); //TODO: sadly this might be needed, but it's not atm

	GtkWidget *wrapper = g_object_ref(wrapper_widget_new());
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0.f);
	gtk_label_set_yalign(GTK_LABEL(label), 0.f);
	gtk_widget_set_hexpand(label, TRUE);
	gtk_box_append(GTK_BOX(box), label);
	wrapper_widget_set_child(WRAPPER_WIDGET(wrapper), box);
	wrapper_widget_set_jobject(WRAPPER_WIDGET(wrapper), env, this);

	PangoAttrList *pango_attrs = pango_attr_list_new();
	pango_attr_list_insert(pango_attrs, pango_attr_font_features_new("tnum"));
	gtk_label_set_attributes(GTK_LABEL(label), pango_attrs);
	pango_attr_list_unref(pango_attrs);

	return _INTPTR(box);
}

JNIEXPORT void JNICALL Java_android_widget_TextView_native_1setText(JNIEnv *env, jobject this, jobject charseq)
{
	const char *text = charseq ? (*env)->GetStringUTFChars(env, charseq, NULL) : NULL;
	atl_safe_gtk_label_set_text(box_get_label(env, _PTR(_GET_LONG_FIELD(this, "widget"))), text ?: "");
	if (text)
		(*env)->ReleaseStringUTFChars(env, charseq, text);
}

/* we kinda need per-widget css */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
JNIEXPORT void JNICALL Java_android_widget_TextView_native_1setTextColor(JNIEnv *env, jobject this, jint color)
{
	GtkWidget *widget = GTK_WIDGET(_PTR(_GET_LONG_FIELD(this, "widget")));

	GtkStyleContext *style_context = gtk_widget_get_style_context(widget);

	GtkCssProvider *old_provider = g_object_get_data(G_OBJECT(widget), "color_style_provider");
	if (old_provider)
		gtk_style_context_remove_provider(style_context, GTK_STYLE_PROVIDER(old_provider));

	GtkCssProvider *css_provider = gtk_css_provider_new();

	char *css_string = g_markup_printf_escaped("* { color: #%06x%02x; }", color & 0xFFFFFF, (color >> 24) & 0xFF);
	gtk_css_provider_load_from_string(css_provider, css_string);
	g_free(css_string);

	gtk_style_context_add_provider(style_context, GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_set_data(G_OBJECT(widget), "color_style_provider", css_provider);
}
#pragma GCC diagnostic pop

JNIEXPORT void JNICALL Java_android_widget_TextView_setTextSize(JNIEnv *env, jobject this, jfloat size)
{
	GtkLabel *label = box_get_label(env, _PTR(_GET_LONG_FIELD(this, "widget")));
	PangoAttrList *attrs;

	PangoAttrList *old_attrs = gtk_label_get_attributes(label);
	if (old_attrs)
		attrs = pango_attr_list_copy(old_attrs);
	else
		attrs = pango_attr_list_new();

	PangoAttribute *size_attr = pango_attr_size_new(size * PANGO_SCALE);
	pango_attr_list_change(attrs, size_attr);
	gtk_label_set_attributes(label, attrs);

	pango_attr_list_unref(attrs);
}

/* the attributes over the whole text (size, features) are kept */
static gboolean is_span_attr(PangoAttribute *attr, gpointer data)
{
	return attr->start_index != PANGO_ATTR_INDEX_FROM_TEXT_BEGINNING || attr->end_index != PANGO_ATTR_INDEX_TO_TEXT_END;
}

static void add_attr(PangoAttrList *attrs, PangoAttribute *attr, jint start, jint end)
{
	attr->start_index = start;
	attr->end_index = end;
	pango_attr_list_insert(attrs, attr);
}

JNIEXPORT void JNICALL Java_android_widget_TextView_native_1setSpans(JNIEnv *env, jobject this, jintArray runs)
{
	GtkLabel *label = box_get_label(env, _PTR(_GET_LONG_FIELD(this, "widget")));
	PangoAttrList *old_attrs = gtk_label_get_attributes(label);
	PangoAttrList *attrs = old_attrs ? pango_attr_list_copy(old_attrs) : pango_attr_list_new();
	PangoAttrList *gone = pango_attr_list_filter(attrs, is_span_attr, NULL);
	if (gone)
		pango_attr_list_unref(gone);

	jsize n = runs ? (*env)->GetArrayLength(env, runs) : 0;
	jint *r = n ? (*env)->GetIntArrayElements(env, runs, NULL) : NULL;
	for (jsize i = 0; i + 3 < n; i += 4) {
		jint s = r[i], e = r[i + 1], v = r[i + 3];
		guint16 cr = ((v >> 16) & 0xff) * 257, cg = ((v >> 8) & 0xff) * 257, cb = (v & 0xff) * 257;
		switch (r[i + 2]) {
			case 1: add_attr(attrs, pango_attr_weight_new(v), s, e); break;
			case 2: add_attr(attrs, pango_attr_style_new(PANGO_STYLE_ITALIC), s, e); break;
			case 3: add_attr(attrs, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE), s, e); break;
			case 4: add_attr(attrs, pango_attr_strikethrough_new(TRUE), s, e); break;
			case 5:
				add_attr(attrs, pango_attr_foreground_new(cr, cg, cb), s, e);
				add_attr(attrs, pango_attr_foreground_alpha_new(((v >> 24) & 0xff) * 257), s, e);
				break;
			case 6:
				add_attr(attrs, pango_attr_background_new(cr, cg, cb), s, e);
				add_attr(attrs, pango_attr_background_alpha_new(((v >> 24) & 0xff) * 257), s, e);
				break;
			case 7: {
				union { jint i; float f; } bits = {.i = v};
				add_attr(attrs, pango_attr_scale_new(bits.f), s, e);
				break;
			}
			case 8: add_attr(attrs, pango_attr_family_new("monospace"), s, e); break;
			case 9:
				add_attr(attrs, pango_attr_baseline_shift_new(v > 0 ? PANGO_BASELINE_SHIFT_SUPERSCRIPT : PANGO_BASELINE_SHIFT_SUBSCRIPT), s, e);
				add_attr(attrs, pango_attr_font_scale_new(v > 0 ? PANGO_FONT_SCALE_SUPERSCRIPT : PANGO_FONT_SCALE_SUBSCRIPT), s, e);
				break;
		}
	}
	if (r)
		(*env)->ReleaseIntArrayElements(env, runs, r, JNI_ABORT);
	gtk_label_set_attributes(label, attrs);
	pango_attr_list_unref(attrs);
}

JNIEXPORT void JNICALL Java_android_widget_TextView_native_1setCompoundDrawables(JNIEnv *env, jobject this, jlong widget_ptr, jlong left, jlong top, jlong right, jlong bottom)
{
	GtkWidget *box = GTK_WIDGET(_PTR(widget_ptr));
	gtk_orientable_set_orientation(GTK_ORIENTABLE(box), (left || right) ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

	GdkPaintable *paintable = _PTR(left ?: top); // paintable before text
	GtkWidget *picture = gtk_widget_get_first_child(box);
	if (GTK_IS_PICTURE(picture)) {
		gtk_picture_set_paintable(GTK_PICTURE(picture), paintable);
	} else if (paintable) {
		picture = gtk_picture_new_for_paintable(paintable);
		gtk_widget_insert_after(picture, box, NULL);
	}

	paintable = _PTR(right ?: bottom); // paintable after text
	picture = gtk_widget_get_last_child(box);
	if (GTK_IS_PICTURE(picture)) {
		gtk_picture_set_paintable(GTK_PICTURE(picture), paintable);
	} else if (paintable) {
		picture = gtk_picture_new_for_paintable(paintable);
		gtk_widget_insert_before(picture, box, NULL);
	}
}

// a button centres its label's box, a text view its text
JNIEXPORT void JNICALL Java_android_widget_TextView_native_1setGravity(JNIEnv *env, jobject this, jlong widget_ptr, jint h, jint v)
{
	static const GtkAlign aligns[] = {GTK_ALIGN_START, GTK_ALIGN_CENTER, GTK_ALIGN_END};
	GtkWidget *widget = GTK_WIDGET(_PTR(widget_ptr));
	GtkWidget *box = widget;
	if (GTK_IS_BUTTON(widget)) {
		box = gtk_button_get_child(GTK_BUTTON(widget));
		if (!GTK_IS_BOX(box))
			return;
		gtk_widget_set_halign(box, aligns[h]);
		gtk_widget_set_valign(box, aligns[v]);
	}
	if (!GTK_IS_BOX(box))
		return;
	GtkWidget *label = gtk_widget_get_last_child(box);
	if (label && !GTK_IS_LABEL(label))
		label = gtk_widget_get_prev_sibling(label);
	if (!GTK_IS_LABEL(label))
		return;
	gtk_label_set_xalign(GTK_LABEL(label), h * 0.5f);
	gtk_label_set_yalign(GTK_LABEL(label), v * 0.5f);
	gtk_label_set_justify(GTK_LABEL(label), h == 1 ? GTK_JUSTIFY_CENTER : h == 2 ? GTK_JUSTIFY_RIGHT : GTK_JUSTIFY_LEFT);
}
