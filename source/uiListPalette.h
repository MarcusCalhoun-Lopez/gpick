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

#ifndef GPICK_UI_LIST_PALETTE_H_
#define GPICK_UI_LIST_PALETTE_H_
#include <gtk/gtk.h>
struct GlobalState;
struct ColorObject;
struct ColorList;
GtkWidget* palette_list_new(GlobalState* gs, GtkWidget* count_label);
void palette_list_add_entry(GtkWidget* widget, ColorObject *color_object, bool allowUpdate);
GtkWidget* palette_list_preview_new(GlobalState* gs, bool expander, bool expanded, ColorList* color_list, ColorList** out_color_list);
GtkWidget* palette_list_get_widget(ColorList *color_list);
void palette_list_remove_all_entries(GtkWidget* widget, bool allowUpdate);
void palette_list_remove_selected_entries(GtkWidget* widget, bool allowUpdate);
int palette_list_remove_entry(GtkWidget* widget, ColorObject *color_object, bool allowUpdate);
enum struct PaletteListCallbackResult {
	noUpdate = 0,
	updateRow = 1,
	updateName = 2,
};
using PaletteListCallback = PaletteListCallbackResult (*)(ColorObject* color_object, void *userdata);
using PaletteListReplaceCallback = PaletteListCallbackResult (*)(ColorObject** color_object, void *userdata);
void palette_list_foreach_selected(GtkWidget* widget, PaletteListCallback callback, void *userdata, bool allowUpdate);
void palette_list_foreach_selected(GtkWidget* widget, PaletteListReplaceCallback callback, void *userdata, bool allowUpdate);
void palette_list_forfirst_selected(GtkWidget* widget, PaletteListCallback callback, void *userdata, bool allowUpdate);
void palette_list_foreach(GtkWidget* widget, PaletteListCallback callback, void *userdata, bool allowUpdate);
gint32 palette_list_get_selected_count(GtkWidget* widget);
gint32 palette_list_get_count(GtkWidget* widget);
ColorObject *palette_list_get_first_selected(GtkWidget* widget);
void palette_list_update_first_selected(GtkWidget* widget, bool only_name);
void palette_list_append_copy_menu(GtkWidget* widget, GtkWidget *menu);
void palette_list_after_update(GtkWidget* widget);
#endif /* GPICK_UI_LIST_PALETTE_H_ */
