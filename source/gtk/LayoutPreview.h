/*
 * Copyright (c) 2009-2016, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GPICK_GTK_LAYOUT_PREVIEW_H_
#define GPICK_GTK_LAYOUT_PREVIEW_H_

#include <gtk/gtk.h>
#include "ColorObject.h"
#include "layout/System.h"
#include "transformation/Chain.h"

#define GTK_TYPE_LAYOUT_PREVIEW (gtk_layout_preview_get_type())
#define GTK_LAYOUT_PREVIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_LAYOUT_PREVIEW, GtkLayoutPreview))
#define GTK_LAYOUT_PREVIEW_CLASS(obj) (G_TYPE_CHECK_CLASS_CAST((obj), GTK_LAYOUT_PREVIEW, GtkLayoutPreviewClass))
#define GTK_IS_LAYOUT_PREVIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_LAYOUT_PREVIEW))
#define GTK_IS_LAYOUT_PREVIEW_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((obj), GTK_TYPE_LAYOUT_PREVIEW))
#define GTK_LAYOUT_PREVIEW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_LAYOUT_PREVIEW, GtkLayoutPreviewClass))

struct GtkLayoutPreview {
	GtkDrawingArea parent;
};
struct GtkLayoutPreviewClass {
	GtkDrawingAreaClass parent_class;
	void (*active_color_changed)(GtkWidget *widget, gint32 active_color, gpointer userdata);
	void (*color_changed)(GtkWidget *widget, gpointer userdata);
	void (*color_activated)(GtkWidget *widget, gpointer userdata);
};
GtkWidget *gtk_layout_preview_new();
GType gtk_layout_preview_get_type();
int gtk_layout_preview_set_system(GtkLayoutPreview *widget, common::Ref<layout::System> system);
int gtk_layout_preview_set_color_at(GtkLayoutPreview *widget, Color *color, gdouble x, gdouble y);
int gtk_layout_preview_set_focus_at(GtkLayoutPreview *widget, gdouble x, gdouble y);
int gtk_layout_preview_set_focus_named(GtkLayoutPreview *widget, const char *name);
int gtk_layout_preview_set_color_named(GtkLayoutPreview *widget, const Color &color, const char *name);
int gtk_layout_preview_get_current_color(GtkLayoutPreview *widget, Color &color);
int gtk_layout_preview_set_current_color(GtkLayoutPreview *widget, const Color &color);
bool gtk_layout_preview_is_selected(GtkLayoutPreview *widget);
bool gtk_layout_preview_is_editable(GtkLayoutPreview *widget);
int gtk_layout_preview_get_current_style(GtkLayoutPreview *widget, common::Ref<layout::Style> &style);
void gtk_layout_preview_set_transformation_chain(GtkLayoutPreview *widget, transformation::Chain *chain);
void gtk_layout_preview_set_fill(GtkLayoutPreview *widget, bool fill);
#endif /* GPICK_GTK_LAYOUT_PREVIEW_H_ */
