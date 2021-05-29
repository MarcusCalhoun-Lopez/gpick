/*
 * Copyright (c) 2009-2021, Albertas Vyšniauskas
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

#include "ColorPicker.h"
#include "ColorObject.h"
#include "GlobalState.h"
#include "ColorList.h"
#include "ColorSource.h"
#include "ColorSourceManager.h"
#include "Converters.h"
#include "dynv/Map.h"
#include "FloatingPicker.h"
#include "ColorRYB.h"
#include "ColorWheelType.h"
#include "ColorSpaceType.h"
#include "gtk/Swatch.h"
#include "gtk/Zoomed.h"
#include "gtk/ColorComponent.h"
#include "gtk/ColorWidget.h"
#include "Clipboard.h"
#include "uiUtilities.h"
#include "uiColorInput.h"
#include "uiConverter.h"
#include "StandardEventHandler.h"
#include "StandardDragDropHandler.h"
#include "IDroppableColorUI.h"
#include "I18N.h"
#include "color_names/ColorNames.h"
#include "ScreenReader.h"
#include "Sampler.h"
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <optional>
using namespace std;

struct ColorPickerArgs;
static void updateDisplays(ColorPickerArgs *args, GtkWidget *except_widget);

struct ColorPickerArgs {
	ColorSource source;
	GtkWidget* main;
	GtkWidget* expanderRGB;
	GtkWidget* expanderHSV;
	GtkWidget* expanderHSL;
	GtkWidget* expanderCMYK;
	GtkWidget* expanderXYZ;
	GtkWidget* expanderLAB;
	GtkWidget* expanderLCH;
	GtkWidget* expanderInfo;
	GtkWidget* expanderInput;
	GtkWidget* expanderMain;
	GtkWidget* expanderSettings;
	GtkWidget *swatch_display;
	GtkWidget *zoomed_display;
	GtkWidget *color_code;
	GtkWidget *hsl_control;
	GtkWidget *hsv_control;
	GtkWidget *rgb_control;
	GtkWidget *cmyk_control;
	GtkWidget *lab_control;
	GtkWidget *lch_control;
	GtkWidget *color_name;
	GtkWidget *statusbar;
	GtkWidget *contrastCheck;
	GtkWidget *contrastCheckMsg;
	GtkWidget *pick_button;
	GtkWidget *colorWidget;
	GtkWidget *colorInput;
	guint timeout_source_id;
	FloatingPicker floating_picker;
	dynv::Ref options;
	dynv::Ref mainOptions;
	GlobalState* gs;
	bool ignore_callback;

	ColorObject *getActive() {
		Color color;
		gtk_swatch_get_active_color(GTK_SWATCH(swatch_display), &color);
		std::string name = color_names_get(gs->getColorNames(), &color, gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
		return new ColorObject(name, color);
	}
	void getActive(ColorObject &colorObject) {
		Color color;
		gtk_swatch_get_active_color(GTK_SWATCH(swatch_display), &color);
		std::string name = color_names_get(gs->getColorNames(), &color, gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
		colorObject.setName(name);
		colorObject.setColor(color);
	}
	void addToPalette(ColorObject *colorObject) {
		color_list_add_color_object(gs->getColorList(), colorObject, true);
	}
	void addToPalette(const Color &color) {
		auto name = color_names_get(gs->getColorNames(), &color, gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
		auto colorObject = new ColorObject(name, color);
		addToPalette(colorObject);
		colorObject->release();
	}
	void copy(const Color &color) {
		auto name = color_names_get(gs->getColorNames(), &color, gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
		auto colorObject = new ColorObject(name, color);
		clipboard::set(colorObject, gs, Converters::Type::copy);
		colorObject->release();
	}
	void addToPalette() {
		addToPalette(getActive());
	}
	void addAllToPalette() {
		Color color;
		for (int i = 1; i < 7; ++i) {
			gtk_swatch_get_color(GTK_SWATCH(swatch_display), i, &color);
			addToPalette(color);
		}
	}
	void setColor(const ColorObject &colorObject) {
		Color copy = colorObject.getColor();
		gtk_swatch_set_active_color(GTK_SWATCH(swatch_display), &copy);
		updateDisplays(this, nullptr);
	}
	bool testDropAt(int x, int y) {
		gint colorIndex = gtk_swatch_get_color_at(GTK_SWATCH(swatch_display), x, y);
		return colorIndex > 0;
	}
	void setColorAt(const ColorObject &colorObject, int x, int y) {
		gint colorIndex = gtk_swatch_get_color_at(GTK_SWATCH(swatch_display), x, y);
		Color color = colorObject.getColor();
		gtk_swatch_set_color(GTK_SWATCH(swatch_display), colorIndex, &color);
		updateDisplays(this, nullptr);
	}
	struct SwatchEditable: public IEditableColorsUI, public IDroppableColorUI {
		SwatchEditable(ColorPickerArgs *args):
			args(args) {
		}
		virtual ~SwatchEditable() = default;
		virtual void addToPalette(const ColorObject &) override {
			args->addToPalette();
		}
		virtual void addAllToPalette() override {
			args->addAllToPalette();
		}
		virtual void setColor(const ColorObject &colorObject) override {
			args->setColor(colorObject);
		}
		virtual void setColorAt(const ColorObject &colorObject, int x, int y) override {
			args->setColorAt(colorObject, x, y);
		}
		virtual void setColors(const std::vector<ColorObject> &colorObjects) override {
			args->setColor(colorObjects[0]);
		}
		virtual const ColorObject &getColor() override {
			args->getActive(colorObject);
			return colorObject;
		}
		virtual std::vector<ColorObject> getColors(bool selected) override {
			if (selected) {
				args->getActive(colorObject);
				return std::vector<ColorObject> { colorObject };
			} else {
				std::vector<ColorObject> result;
				//TODO: get all six colors
				return result;
			}
		}
		virtual bool isEditable() override {
			return true;
		}
		virtual bool hasColor() override {
			return true;
		}
		virtual bool hasSelectedColor() override {
			return true;
		}
		virtual bool testDropAt(int x, int y) override {
			return args->testDropAt(x, y);
		}
		virtual void dropEnd(bool move) override {
		}
	private:
		ColorPickerArgs *args;
		ColorObject colorObject;
	};
	std::optional<SwatchEditable> swatchEditable;
	struct ColorInputReadonly: public IReadonlyColorUI {
		ColorInputReadonly(ColorPickerArgs *args):
			args(args) {
		}
		virtual ~ColorInputReadonly() = default;
		virtual void addToPalette(const ColorObject &) override {
			Color color;
			gtk_color_get_color(GTK_COLOR(args->colorWidget), &color);
			args->addToPalette(color);
		}
		virtual const ColorObject &getColor() override {
			Color color;
			gtk_color_get_color(GTK_COLOR(args->colorWidget), &color);
			colorObject.setColor(color);
			return colorObject;
		}
		virtual bool hasSelectedColor() override {
			return true;
		}
	private:
		ColorPickerArgs *args;
		ColorObject colorObject;
	};
	std::optional<ColorInputReadonly> colorInputReadonly;
	struct ContrastEditable: public IEditableColorUI {
		ContrastEditable(ColorPickerArgs *args):
			args(args) {
		}
		virtual ~ContrastEditable() = default;
		virtual void addToPalette(const ColorObject &) override {
			Color color;
			gtk_color_get_color(GTK_COLOR(args->contrastCheck), &color);
			args->addToPalette(color);
		}
		virtual const ColorObject &getColor() override {
			Color color;
			gtk_color_get_color(GTK_COLOR(args->contrastCheck), &color);
			colorObject.setColor(color);
			return colorObject;
		}
		virtual void setColor(const ColorObject &colorObject) override {
			gtk_color_set_color(GTK_COLOR(args->contrastCheck), colorObject.getColor(), _("Sample"));
			updateDisplays(args, nullptr);
		}
		virtual bool hasSelectedColor() override {
			return true;
		}
		virtual bool isEditable() override {
			return true;
		}
	private:
		ColorPickerArgs *args;
		ColorObject colorObject;
	};
	std::optional<ContrastEditable> contrastEditable;
};

struct ColorCompItem{
	GtkWidget *widget;
	GtkColorComponentComp component;
	int component_id;
};

static int source_set_color(ColorPickerArgs *args, ColorObject* color);
static int source_set_nth_color(ColorPickerArgs *args, uint32_t color_n, ColorObject* color);
static int source_get_nth_color(ColorPickerArgs *args, uint32_t color_n, ColorObject** color);

static void updateMainColorNow(ColorPickerArgs* args)
{
	if (!args->options->getBool("zoomed_enabled", true)){
		Color c;
		gtk_swatch_get_active_color(GTK_SWATCH(args->swatch_display), &c);
		string text = args->gs->converters().serialize(c, Converters::Type::display);
		gtk_color_set_color(GTK_COLOR(args->color_code), &c, text.c_str());
		gtk_swatch_set_main_color(GTK_SWATCH(args->swatch_display), &c);
	}
}

static gboolean updateMainColor( gpointer data ){
	ColorPickerArgs* args=(ColorPickerArgs*)data;
	GdkScreen *screen;
	GdkModifierType state;
	int x, y;
	gdk_display_get_pointer(gdk_display_get_default(), &screen, &x, &y, &state);
	int monitor = gdk_screen_get_monitor_at_point(screen, x, y);
	GdkRectangle monitor_geometry;
	gdk_screen_get_monitor_geometry(screen, monitor, &monitor_geometry);
	math::Vector2i pointer(x,y);
	math::Rectangle<int> screen_rect(monitor_geometry.x, monitor_geometry.y, monitor_geometry.x + monitor_geometry.width, monitor_geometry.y + monitor_geometry.height);
	auto screen_reader = args->gs->getScreenReader();
	screen_reader_reset_rect(screen_reader);
	math::Rectangle<int> sampler_rect, zoomed_rect, final_rect;
	sampler_get_screen_rect(args->gs->getSampler(), pointer, screen_rect, &sampler_rect);
	screen_reader_add_rect(screen_reader, screen, sampler_rect);
	bool zoomed_enabled = args->options->getBool("zoomed_enabled", true);
	if (zoomed_enabled){
		gtk_zoomed_get_screen_rect(GTK_ZOOMED(args->zoomed_display), pointer, screen_rect, &zoomed_rect);
		screen_reader_add_rect(screen_reader, screen, zoomed_rect);
	}
	screen_reader_update_surface(screen_reader, &final_rect);
	math::Vector2i offset;
	offset = sampler_rect.position() - final_rect.position();
	Color c;
	sampler_get_color_sample(args->gs->getSampler(), pointer, screen_rect, offset, &c);
	string text = args->gs->converters().serialize(c, Converters::Type::display);
	gtk_color_set_color(GTK_COLOR(args->color_code), &c, text.c_str());
	gtk_swatch_set_main_color(GTK_SWATCH(args->swatch_display), &c);
	if (zoomed_enabled){
		offset = final_rect.position() - zoomed_rect.position();
		gtk_zoomed_update(GTK_ZOOMED(args->zoomed_display), pointer, screen_rect, offset, screen_reader_get_surface(screen_reader));
	}
	return TRUE;
}
static gboolean updateMainColorTimer(ColorPickerArgs* args)
{
	updateMainColor(args);
	return true;
}
static void updateComponentText(ColorPickerArgs *args, GtkColorComponent *component, const char *type)
{
	Color transformed_color;
	gtk_color_component_get_transformed_color(component, &transformed_color);
	lua::Script &script = args->gs->script();
	list<string> str = color_space_color_to_text(type, &transformed_color, script, args->gs);
	int j = 0;
	const char *text[4];
	memset(text, 0, sizeof(text));
	for (list<string>::iterator i = str.begin(); i != str.end(); i++){
		text[j] = (*i).c_str();
		j++;
		if (j > 3) break;
	}
	gtk_color_component_set_text(component, text);
}
static void updateDisplays(ColorPickerArgs *args, GtkWidget *except_widget)
{
	updateMainColorNow(args);
	Color c, c2;
	gtk_swatch_get_active_color(GTK_SWATCH(args->swatch_display),&c);
	if (except_widget != args->hsl_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->hsl_control), &c);
	if (except_widget != args->hsv_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->hsv_control), &c);
	if (except_widget != args->rgb_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->rgb_control), &c);
	if (except_widget != args->cmyk_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->cmyk_control), &c);
	if (except_widget != args->lab_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->lab_control), &c);
	if (except_widget != args->lch_control) gtk_color_component_set_color(GTK_COLOR_COMPONENT(args->lch_control), &c);
	updateComponentText(args, GTK_COLOR_COMPONENT(args->hsl_control), "hsl");
	updateComponentText(args, GTK_COLOR_COMPONENT(args->hsv_control), "hsv");
	updateComponentText(args, GTK_COLOR_COMPONENT(args->rgb_control), "rgb");
	updateComponentText(args, GTK_COLOR_COMPONENT(args->cmyk_control), "cmyk");
	updateComponentText(args, GTK_COLOR_COMPONENT(args->lab_control), "lab");
	updateComponentText(args, GTK_COLOR_COMPONENT(args->lch_control), "lch");
	string color_name = color_names_get(args->gs->getColorNames(), &c, true);
	gtk_entry_set_text(GTK_ENTRY(args->color_name), color_name.c_str());
	gtk_color_get_color(GTK_COLOR(args->contrastCheck), &c2);
	gtk_color_set_text_color(GTK_COLOR(args->contrastCheck), &c);
	stringstream ss;
	ss.setf(ios::fixed, ios::floatfield);
	Color c_lab, c2_lab;
	c_lab = c.rgbToLabD50();
	c2_lab = c2.rgbToLabD50();
	const ColorWheelType *wheel = &color_wheel_types_get()[0];
	Color hsl1, hsl2;
	double hue1, hue2;
	hsl1 = c.rgbToHsl();
	hsl2 = c2.rgbToHsl();
	wheel->rgbhue_to_hue(hsl1.hsl.hue, &hue1);
	wheel->rgbhue_to_hue(hsl2.hsl.hue, &hue2);
	double complementary = std::abs(hue1 - hue2);
	complementary -= std::floor(complementary);
	complementary *= std::sin(hsl1.hsl.lightness * math::PI) * std::sin(hsl2.hsl.lightness * math::PI);
	complementary *= std::sin(hsl1.hsl.saturation * math::PI / 2) * std::sin(hsl2.hsl.saturation * math::PI / 2);
	ss << std::setprecision(1) << std::abs(c_lab.lab.L - c2_lab.lab.L) + complementary * 50 << "%";
	auto message = ss.str();
	gtk_label_set_text(GTK_LABEL(args->contrastCheckMsg), message.c_str());
}
static void on_swatch_active_color_changed(GtkWidget *widget, gint32 new_active_color, gpointer data)
{
	ColorPickerArgs* args = (ColorPickerArgs*)data;
	updateDisplays(args, widget);
}
static void on_swatch_color_changed(GtkWidget *widget, gpointer data)
{
	ColorPickerArgs* args = (ColorPickerArgs*)data;
	updateDisplays(args, widget);
}
static void on_swatch_color_activated(GtkWidget *widget, ColorPickerArgs *args) {
	args->addToPalette();
}
static void on_swatch_center_activated(GtkWidget *widget, ColorPickerArgs *args)
{
	floating_picker_activate(args->floating_picker, true, false, nullptr);
}
static void on_picker_toggled(GtkWidget *widget, ColorPickerArgs *args)
{
	if (args->ignore_callback)
		return;
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))){
		if (args->options->getBool("always_use_floating_picker", true)){
			floating_picker_activate(args->floating_picker, false, false, nullptr);
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), false);
		}else{
			gtk_swatch_set_active(GTK_SWATCH(args->swatch_display), true);
		}
	}else{
		gtk_swatch_set_active(GTK_SWATCH(args->swatch_display), false);
	}
}
static gboolean on_swatch_focus_change(GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
	ColorPickerArgs* args = (ColorPickerArgs*)data;
	if (event->in){
		gtk_statusbar_push(GTK_STATUSBAR(args->statusbar), gtk_statusbar_get_context_id(GTK_STATUSBAR(args->statusbar), "swatch_focused"), _("Press Spacebar to sample color under mouse pointer"));
		if (!args->options->getBool("always_use_floating_picker", true)){
			args->ignore_callback = true;
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(args->pick_button), true);
			args->ignore_callback = false;
			gtk_swatch_set_active(GTK_SWATCH(args->swatch_display), true);
		}
	}else{
		gtk_statusbar_pop(GTK_STATUSBAR(args->statusbar), gtk_statusbar_get_context_id(GTK_STATUSBAR(args->statusbar), "swatch_focused"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(args->pick_button), false);
		gtk_swatch_set_active(GTK_SWATCH(args->swatch_display), false);
	}
	return FALSE;
}
static void onColorInputChanged(GtkWidget *entry, ColorPickerArgs *args) {
	ColorObject *colorObject;
	if (args->gs->converters().deserialize((char*)gtk_entry_get_text(GTK_ENTRY(entry)), &colorObject)) {
		auto color = colorObject->getColor();
		auto text = args->gs->converters().serialize(colorObject, Converters::Type::display);
		gtk_color_set_color(GTK_COLOR(args->colorWidget), color, text);
		colorObject->release();
		gtk_widget_set_sensitive(GTK_WIDGET(args->colorWidget), true);
	} else {
		gtk_widget_set_sensitive(GTK_WIDGET(args->colorWidget), false);
	}
}
static void pick(ColorPickerArgs *args) {
	updateMainColor(args);
	gtk_swatch_set_color_to_main(GTK_SWATCH(args->swatch_display));
	if (args->options->getBool("sampler.add_to_palette", true)) {
		Color color;
		gtk_swatch_get_active_color(GTK_SWATCH(args->swatch_display), &color);
		ColorObject *color_object = color_list_new_color_object(args->gs->getColorList(), &color);
		string name=color_names_get(args->gs->getColorNames(), &color, args->gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
		color_object->setName(name);
		color_list_add_color_object(args->gs->getColorList(), color_object, 1);
		color_object->release();
	}
	if (args->options->getBool("sampler.copy_to_clipboard", true)) {
		Color color;
		gtk_swatch_get_active_color(GTK_SWATCH(args->swatch_display), &color);
		clipboard::set(color, args->gs, Converters::Type::copy);
	}
	if (args->options->getBool("sampler.rotate_swatch_after_sample", true)) {
		gtk_swatch_move_active(GTK_SWATCH(args->swatch_display), 1);
	}
	updateDisplays(args, args->swatch_display);
}
static gboolean onSwatchKeyPress(GtkWidget *widget, GdkEventKey *event, ColorPickerArgs *args) {
	guint modifiers = gtk_accelerator_get_default_mod_mask();
	auto key = getKeyval(*event, args->gs->latinKeysGroup);
	switch (key) {
	case GDK_KEY_m: {
		int x, y;
		gdk_display_get_pointer(gdk_display_get_default(), nullptr, &x, &y, nullptr);
		math::Vector<int, 2> position(x, y);
		if ((event->state & modifiers) == GDK_CONTROL_MASK){
			gtk_zoomed_set_mark(GTK_ZOOMED(args->zoomed_display), 1, position);
		}else{
			gtk_zoomed_set_mark(GTK_ZOOMED(args->zoomed_display), 0, position);
		}
	} break;
	case GDK_KEY_1:
	case GDK_KEY_2:
	case GDK_KEY_3:
	case GDK_KEY_4:
	case GDK_KEY_5:
	case GDK_KEY_6:
		gtk_swatch_set_active_index(GTK_SWATCH(args->swatch_display), key - GDK_KEY_1 + 1);
		updateDisplays(args, widget);
		return true;
	case GDK_KEY_Right:
		gtk_swatch_move_active(GTK_SWATCH(args->swatch_display), 1);
		updateDisplays(args, widget);
		return true;
	case GDK_KEY_Left:
		gtk_swatch_move_active(GTK_SWATCH(args->swatch_display), -1);
		updateDisplays(args, widget);
		return true;
	case GDK_KEY_space:
		pick(args);
		return true;
	}
	return false;
}

void color_picker_pick(ColorSource* color_source) {
	auto args = reinterpret_cast<ColorPickerArgs *>(color_source);
	pick(args);
}
void color_picker_add_to_palette(ColorSource* color_source) {
	auto args = reinterpret_cast<ColorPickerArgs *>(color_source);
	updateMainColor(args);
	Color color;
	gtk_swatch_get_main_color(GTK_SWATCH(args->swatch_display), &color);
	args->addToPalette(color);
}
void color_picker_copy(ColorSource* color_source) {
	auto args = reinterpret_cast<ColorPickerArgs *>(color_source);
	updateMainColor(args);
	Color color;
	gtk_swatch_get_main_color(GTK_SWATCH(args->swatch_display), &color);
	args->copy(color);
}
void color_picker_set(ColorSource *color_source, int index) {
	auto args = reinterpret_cast<ColorPickerArgs *>(color_source);
	updateMainColor(args);
	gtk_swatch_set_active_index(GTK_SWATCH(args->swatch_display), index + 1);
	gtk_swatch_set_color_to_main(GTK_SWATCH(args->swatch_display));
	updateDisplays(args, nullptr);
}

static void on_oversample_value_changed(GtkRange *slider, gpointer data){
	ColorPickerArgs* args=(ColorPickerArgs*)data;
	sampler_set_oversample(args->gs->getSampler(), (int)gtk_range_get_value(GTK_RANGE(slider)));
}

static void on_zoom_value_changed(GtkRange *slider, gpointer data){
	ColorPickerArgs* args=(ColorPickerArgs*)data;
	gtk_zoomed_set_zoom(GTK_ZOOMED(args->zoomed_display), static_cast<float>(gtk_range_get_value(GTK_RANGE(slider))));
}

static void color_component_change_value(GtkWidget *widget, Color* c, ColorPickerArgs* args){
	gtk_swatch_set_active_color(GTK_SWATCH(args->swatch_display), c);
	updateDisplays(args, widget);
}

static void color_component_input_clicked(GtkWidget *widget, int component_id, ColorPickerArgs* args){
	dialog_color_component_input_show(GTK_WINDOW(gtk_widget_get_toplevel(args->main)), GTK_COLOR_COMPONENT(widget), component_id, args->options->getOrCreateMap("component_edit"));
}

static void ser_decimal_get(GtkColorComponentComp component, int component_id, Color* color, const char *text){
	double v;
	stringstream ss(text);
	ss >> v;
	switch (component){
		case GtkColorComponentComp::hsv:
		case GtkColorComponentComp::hsl:
			if (component_id == 0){
				(*color)[component_id] = static_cast<float>(v / 360);
			}else{
				(*color)[component_id] = static_cast<float>(v / 100);
			}
			break;
		default:
			(*color)[component_id] = static_cast<float>(v / 100);
	}
}

static string ser_decimal_set(GtkColorComponentComp component, int component_id, Color* color){
	stringstream ss;
	switch (component){
		case GtkColorComponentComp::hsv:
		case GtkColorComponentComp::hsl:
			if (component_id == 0){
				ss << setprecision(0) << fixed << (*color)[component_id] * 360;
			}else{
				ss << setprecision(0) << fixed << (*color)[component_id] * 100;
			}
			break;
		default:
			ss << setprecision(0) << fixed << (*color)[component_id] * 100;
	}
	return ss.str();
}

static struct{
	const char *name;
	const char *human_name;
	void (*get)(GtkColorComponentComp component, int component_id, Color* color, const char *text);
	string (*set)(GtkColorComponentComp component, int component_id, Color* color);
}serial[] = {
	{"decimal", "Decimal", ser_decimal_get, ser_decimal_set},
};



static void color_component_copy(GtkWidget *widget, ColorPickerArgs* args){
	struct ColorCompItem *comp_item = (struct ColorCompItem*)g_object_get_data(G_OBJECT(gtk_widget_get_parent(widget)), "comp_item");
	const char *text = gtk_color_component_get_text(GTK_COLOR_COMPONENT(comp_item->widget), comp_item->component_id);
	if (text){
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text, strlen(text));
		gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), text, strlen(text));
	}
}

static void color_component_paste(GtkWidget *widget, ColorPickerArgs* args){
	Color color;
	struct ColorCompItem *comp_item = (struct ColorCompItem*)g_object_get_data(G_OBJECT(gtk_widget_get_parent(widget)), "comp_item");
	gtk_color_component_get_transformed_color(GTK_COLOR_COMPONENT(comp_item->widget), &color);

	gchar *text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
	if (text){
		serial[0].get(comp_item->component, comp_item->component_id, &color, text);
		gtk_color_component_set_transformed_color(GTK_COLOR_COMPONENT(comp_item->widget), &color);
		g_free(text);

		gtk_color_component_get_color(GTK_COLOR_COMPONENT(comp_item->widget), &color);
		gtk_swatch_set_active_color(GTK_SWATCH(args->swatch_display), &color);
		updateDisplays(args, comp_item->widget);
	}
}

static void color_component_edit(GtkWidget *widget, ColorPickerArgs* args){
	struct ColorCompItem *comp_item = (struct ColorCompItem*)g_object_get_data(G_OBJECT(gtk_widget_get_parent(widget)), "comp_item");
	dialog_color_component_input_show(GTK_WINDOW(gtk_widget_get_toplevel(args->main)), GTK_COLOR_COMPONENT(comp_item->widget), comp_item->component_id, args->options->getOrCreateMap("component_edit"));
}

static void destroy_comp_item(struct ColorCompItem *comp_item){
	delete comp_item;
}

static gboolean color_component_key_up_cb(GtkWidget *widget, GdkEventButton *event, ColorPickerArgs* args){
	if ((event->type == GDK_BUTTON_RELEASE) && (event->button == 3)){
		auto menu = gtk_menu_new();
		struct ColorCompItem *comp_item = new struct ColorCompItem;
		comp_item->widget = widget;
		comp_item->component_id = gtk_color_component_get_component_id_at(GTK_COLOR_COMPONENT(widget), static_cast<int>(event->x), static_cast<int>(event->y));
		comp_item->component = gtk_color_component_get_component(GTK_COLOR_COMPONENT(widget));
		g_object_set_data_full(G_OBJECT(menu), "comp_item", comp_item, (GDestroyNotify)destroy_comp_item);
		auto item = newMenuItem(_("Copy"), GTK_STOCK_COPY);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(color_component_copy), args);
		item = newMenuItem(_("Paste"), GTK_STOCK_PASTE);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(color_component_paste), args);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
		item = newMenuItem(_("Edit"), GTK_STOCK_EDIT);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(color_component_edit), args);
		showContextMenu(menu, event);
		return true;
	}
	return false;
}


static void on_oversample_falloff_changed(GtkWidget *widget, gpointer data) {
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)) {
		GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));

		gint32 falloff_id;
		gtk_tree_model_get(model, &iter, 2, &falloff_id, -1);

		ColorPickerArgs* args = (ColorPickerArgs*)data;
		sampler_set_falloff(args->gs->getSampler(), (SamplerFalloff) falloff_id);

	}
}

static GtkWidget* create_falloff_type_list()
{
	GtkListStore *store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_INT);
	GtkWidget *widget = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	gtk_combo_box_set_add_tearoffs(GTK_COMBO_BOX(widget), 0);
	GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget),renderer, 0);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(widget), renderer, "pixbuf", 0, nullptr);
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(widget), renderer, 0);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(widget), renderer, "text", 1, nullptr);
	g_object_unref(GTK_TREE_MODEL(store));
	struct{
		const char *icon;
		const char *label;
		SamplerFalloff falloff;
	}falloff_types[] = {
		{"gpick-falloff-none", _("None"), SamplerFalloff::none},
		{"gpick-falloff-linear", _("Linear"), SamplerFalloff::linear},
		{"gpick-falloff-quadratic", _("Quadratic"), SamplerFalloff::quadratic},
		{"gpick-falloff-cubic", _("Cubic"), SamplerFalloff::cubic},
		{"gpick-falloff-exponential", _("Exponential"), SamplerFalloff::exponential},
	};
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
	gint icon_size;
	gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, 0, &icon_size);
	for (size_t i = 0; i < sizeof(falloff_types) / sizeof(falloff_types[0]); ++i){
		GError *error = nullptr;
		GdkPixbuf* pixbuf = gtk_icon_theme_load_icon(icon_theme, falloff_types[i].icon, icon_size, GtkIconLookupFlags(0), &error);
		if (error) g_error_free(error);
		GtkTreeIter iter1;
		gtk_list_store_append(store, &iter1);
		gtk_list_store_set(store, &iter1,
			0, pixbuf,
			1, falloff_types[i].label,
			2, falloff_types[i].falloff,
		-1);
		if (pixbuf) g_object_unref (pixbuf);
	}
	return widget;
}
static int source_destroy(ColorPickerArgs *args)
{
	args->options->set("swatch.active_color", gtk_swatch_get_active_index(GTK_SWATCH(args->swatch_display)));
	Color c;
	char tmp[32];
	for (gint i=1; i<7; ++i){
		sprintf(tmp, "swatch.color%d", i);
		gtk_swatch_get_color(GTK_SWATCH(args->swatch_display), i, &c);
		args->options->set(tmp, c);
	}

	args->options->set("sampler.oversample", sampler_get_oversample(args->gs->getSampler()));
	args->options->set("sampler.falloff", static_cast<int>(sampler_get_falloff(args->gs->getSampler())));

	args->options->set<int32_t>("zoom", static_cast<int32_t>(gtk_zoomed_get_zoom(GTK_ZOOMED(args->zoomed_display))));
	args->options->set("zoom_size", gtk_zoomed_get_size(GTK_ZOOMED(args->zoomed_display)));
	args->options->set<bool>("expander.settings", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderSettings)));
	args->options->set<bool>("expander.rgb", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderRGB)));
	args->options->set<bool>("expander.hsv", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderHSV)));
	args->options->set<bool>("expander.hsl", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderHSL)));
	args->options->set<bool>("expander.lab", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderLAB)));
	args->options->set<bool>("expander.lch", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderLCH)));
	args->options->set<bool>("expander.cmyk", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderCMYK)));
	args->options->set<bool>("expander.info", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderInfo)));
	args->options->set<bool>("expander.input", gtk_expander_get_expanded(GTK_EXPANDER(args->expanderInput)));
	gtk_color_get_color(GTK_COLOR(args->contrastCheck), &c);
	args->options->set("contrast.color", c);
	args->options->set("color_input_text", gtk_entry_get_text(GTK_ENTRY(args->colorInput)));
	gtk_widget_destroy(args->main);
	delete args;
	return 0;
}

static int source_get_color(ColorPickerArgs *args, ColorObject** color_object)
{
	Color color;
	gtk_swatch_get_active_color(GTK_SWATCH(args->swatch_display), &color);
	ColorObject *new_color_object = color_list_new_color_object(args->gs->getColorList(), &color);
	string name = color_names_get(args->gs->getColorNames(), &color, args->gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
	new_color_object->setName(name);
	*color_object = new_color_object;
	return 0;
}
static int source_set_nth_color(ColorPickerArgs *args, uint32_t color_n, ColorObject* color_object)
{
	if (color_n < 0 || color_n > 6) return -1;
	Color color = color_object->getColor();
	gtk_swatch_set_color(GTK_SWATCH(args->swatch_display), color_n + 1, &color);
	updateDisplays(args, 0);
	return 0;
}
static int source_get_nth_color(ColorPickerArgs *args, uint32_t color_n, ColorObject** color_object)
{
	if (color_n < 0 || color_n > 6) return -1;
	Color color;
	gtk_swatch_get_color(GTK_SWATCH(args->swatch_display), color_n + 1, &color);
	ColorObject *new_color_object = color_list_new_color_object(args->gs->getColorList(), &color);
	string name = color_names_get(args->gs->getColorNames(), &color, args->gs->settings().getBool("gpick.color_names.imprecision_postfix", false));
	new_color_object->setName(name);
	*color_object = new_color_object;
	return 0;
}
static int source_set_color(ColorPickerArgs *args, ColorObject* color_object)
{
	Color color = color_object->getColor();
	gtk_swatch_set_active_color(GTK_SWATCH(args->swatch_display), &color);
	updateDisplays(args, 0);
	return 0;
}
static int source_activate(ColorPickerArgs *args)
{
	if (args->timeout_source_id > 0) {
		g_source_remove(args->timeout_source_id);
		args->timeout_source_id = 0;
	}
	struct{
		GtkWidget *widget;
		const char *setting;
	}color_spaces[] = {
		{args->expanderCMYK, "color_space.cmyk"},
		{args->expanderHSL, "color_space.hsl"},
		{args->expanderHSV, "color_space.hsv"},
		{args->expanderLAB, "color_space.lab"},
		{args->expanderLCH, "color_space.lch"},
		{args->expanderRGB, "color_space.rgb"},
		{0, 0},
	};
	for (int i = 0; color_spaces[i].setting; i++){
		if (args->options->getBool(color_spaces[i].setting, true))
			gtk_widget_show(color_spaces[i].widget);
		else
			gtk_widget_hide(color_spaces[i].widget);
	}

	bool out_of_gamut_mask = args->options->getBool("out_of_gamut_mask", true);

	gtk_color_component_set_out_of_gamut_mask(GTK_COLOR_COMPONENT(args->lab_control), out_of_gamut_mask);
	gtk_color_component_set_lab_illuminant(GTK_COLOR_COMPONENT(args->lab_control), Color::getIlluminant(args->options->getString("lab.illuminant", "D50")));
	gtk_color_component_set_lab_observer(GTK_COLOR_COMPONENT(args->lab_control), Color::getObserver(args->options->getString("lab.observer", "2")));
	updateComponentText(args, GTK_COLOR_COMPONENT(args->lab_control), "lab");

	gtk_color_component_set_out_of_gamut_mask(GTK_COLOR_COMPONENT(args->lch_control), out_of_gamut_mask);
	gtk_color_component_set_lab_illuminant(GTK_COLOR_COMPONENT(args->lch_control), Color::getIlluminant(args->options->getString("lab.illuminant", "D50")));
	gtk_color_component_set_lab_observer(GTK_COLOR_COMPONENT(args->lch_control), Color::getObserver(args->options->getString("lab.observer", "2")));
	updateComponentText(args, GTK_COLOR_COMPONENT(args->lch_control), "lch");

	auto chain = args->gs->getTransformationChain();
	gtk_swatch_set_transformation_chain(GTK_SWATCH(args->swatch_display), chain);
	gtk_color_set_transformation_chain(GTK_COLOR(args->color_code), chain);
	gtk_color_set_transformation_chain(GTK_COLOR(args->contrastCheck), chain);

	if (args->options->getBool("zoomed_enabled", true)){
		auto refresh_rate = args->mainOptions->getInt32("refresh_rate", 30);
		args->timeout_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, static_cast<int>(1000 / refresh_rate), (GSourceFunc)updateMainColorTimer, args, (GDestroyNotify)nullptr);
	}

	gtk_zoomed_set_size(GTK_ZOOMED(args->zoomed_display), args->options->getInt32("zoom_size", 150));

	gtk_statusbar_push(GTK_STATUSBAR(args->statusbar), gtk_statusbar_get_context_id(GTK_STATUSBAR(args->statusbar), "focus_swatch"), _("Click on swatch area to begin adding colors to palette"));

	return 0;
}
static int source_deactivate(ColorPickerArgs *args) {
	gtk_statusbar_pop(GTK_STATUSBAR(args->statusbar), gtk_statusbar_get_context_id(GTK_STATUSBAR(args->statusbar), "focus_swatch"));
	if (args->timeout_source_id > 0) {
		g_source_remove(args->timeout_source_id);
		args->timeout_source_id = 0;
	}
	return 0;
}
void color_picker_set_floating_picker(ColorSource *color_source, FloatingPicker floating_picker){
	ColorPickerArgs* args = (ColorPickerArgs*)color_source;
	args->floating_picker = floating_picker;
}
static void show_dialog_converter(GtkWidget *widget, ColorPickerArgs *args){
	dialog_converter_show(GTK_WINDOW(gtk_widget_get_toplevel(args->main)), args->gs);
	return;
}
static void on_zoomed_activate(GtkWidget *widget, ColorPickerArgs *args){
	if (args->options->getBool("zoomed_enabled", true)){
		gtk_zoomed_set_fade(GTK_ZOOMED(args->zoomed_display), true);
		args->options->set("zoomed_enabled", false);

		if (args->timeout_source_id > 0){
			g_source_remove(args->timeout_source_id);
			args->timeout_source_id = 0;
		}
	}else{
		gtk_zoomed_set_fade(GTK_ZOOMED(args->zoomed_display), false);
		args->options->set("zoomed_enabled", true);

		if (args->timeout_source_id > 0){
			g_source_remove(args->timeout_source_id);
			args->timeout_source_id = 0;
		}
		auto refresh_rate = args->mainOptions->getInt32("refresh_rate", 30);
		args->timeout_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, static_cast<int>(1000 / refresh_rate), (GSourceFunc)updateMainColorTimer, args, (GDestroyNotify)nullptr);
	}
	return;
}

static ColorSource* source_implement(ColorSource *source, GlobalState *gs, const dynv::Ref &options){
	ColorPickerArgs* args = new ColorPickerArgs;
	args->swatchEditable = ColorPickerArgs::SwatchEditable(args);
	args->colorInputReadonly = ColorPickerArgs::ColorInputReadonly(args);
	args->contrastEditable = ColorPickerArgs::ContrastEditable(args);

	args->options = options;
	args->mainOptions = gs->settings().getOrCreateMap("gpick.picker");
	args->statusbar = gs->getStatusBar();
	args->floating_picker = 0;
	args->ignore_callback = false;

	color_source_init(&args->source, source->identificator, source->hr_name);
	args->source.destroy = (int (*)(ColorSource *source))source_destroy;
	args->source.get_color = (int (*)(ColorSource *source, ColorObject** color))source_get_color;
	args->source.set_color = (int (*)(ColorSource *source, ColorObject* color))source_set_color;
	args->source.get_nth_color = (int (*)(ColorSource *source, size_t color_n, ColorObject** color))source_get_nth_color;
	args->source.set_nth_color = (int (*)(ColorSource *source, size_t color_n, ColorObject* color))source_set_nth_color;
	args->source.activate = (int (*)(ColorSource *source))source_activate;
	args->source.deactivate = (int (*)(ColorSource *source))source_deactivate;

	args->gs = gs;
	args->timeout_source_id = 0;

	GtkWidget *vbox, *widget, *expander, *table, *main_hbox, *scrolled;
	int table_y;

	main_hbox = gtk_hbox_new(false, 5);

		vbox = gtk_vbox_new(false, 0);
		gtk_box_pack_start(GTK_BOX(main_hbox), vbox, false, false, 0);

			args->pick_button = widget = gtk_toggle_button_new_with_label(_("Pick color"));
			gtk_box_pack_start(GTK_BOX(vbox), widget, false, false, 0);
			g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(on_picker_toggled), args);
			g_signal_connect(G_OBJECT(widget), "key_press_event", G_CALLBACK(onSwatchKeyPress), args);
			g_signal_connect(G_OBJECT(widget), "focus-out-event", G_CALLBACK(on_swatch_focus_change), args);
			StandardEventHandler::forWidget(widget, args->gs, &*args->swatchEditable);

			args->swatch_display = widget = gtk_swatch_new();
			gtk_box_pack_start(GTK_BOX(vbox), widget, false, false, 0);
			g_signal_connect(G_OBJECT(widget), "focus-in-event", G_CALLBACK(on_swatch_focus_change), args);
			g_signal_connect(G_OBJECT(widget), "focus-out-event", G_CALLBACK(on_swatch_focus_change), args);
			g_signal_connect(G_OBJECT(widget), "key_press_event", G_CALLBACK(onSwatchKeyPress), args);
			g_signal_connect(G_OBJECT(widget), "active_color_changed", G_CALLBACK(on_swatch_active_color_changed), args);
			g_signal_connect(G_OBJECT(widget), "color_changed", G_CALLBACK(on_swatch_color_changed), args);
			g_signal_connect(G_OBJECT(widget), "color_activated", G_CALLBACK(on_swatch_color_activated), args);
			g_signal_connect(G_OBJECT(widget), "center_activated", G_CALLBACK(on_swatch_center_activated), args);
			StandardEventHandler::forWidget(widget, args->gs, &*args->swatchEditable);
			StandardDragDropHandler::forWidget(widget, args->gs, &*args->swatchEditable);

			gtk_swatch_set_active_index(GTK_SWATCH(widget), options->getInt32("swatch.active_color", 1));

			{
				char tmp[32];
				for (gint i=1; i<7; ++i){
					sprintf(tmp, "swatch.color%d", i);
					Color color = Color((i - 1) / 15.f, 0.8f, 0.5f).hslToRgb();
					color = options->getColor(tmp, color);
					gtk_swatch_set_color(GTK_SWATCH(args->swatch_display), i, color);
				}
			}

			args->color_code = gtk_color_new();
			gtk_box_pack_start (GTK_BOX(vbox), args->color_code, false, true, 0);
			g_signal_connect(G_OBJECT(args->color_code), "activated", G_CALLBACK(show_dialog_converter), args);


			args->zoomed_display = gtk_zoomed_new();
			if (!options->getBool("zoomed_enabled", true)){
				gtk_zoomed_set_fade(GTK_ZOOMED(args->zoomed_display), true);
			}
			gtk_zoomed_set_size(GTK_ZOOMED(args->zoomed_display), options->getInt32("zoom_size", 150));
			gtk_zoomed_set_zoom(GTK_ZOOMED(args->zoomed_display), options->getInt32("zoom", 20));
			gtk_box_pack_start (GTK_BOX(vbox), args->zoomed_display, false, false, 0);
			g_signal_connect(G_OBJECT(args->zoomed_display), "activated", G_CALLBACK(on_zoomed_activate), args);


		scrolled = gtk_scrolled_window_new(0, 0);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start (GTK_BOX(main_hbox), scrolled, true, true, 0);

		vbox = gtk_vbox_new(false, 5);
		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), vbox);

			expander=gtk_expander_new(_("Settings"));
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.settings", false));
			args->expanderSettings=expander;
			gtk_box_pack_start (GTK_BOX(vbox), expander, FALSE, FALSE, 0);

			table = gtk_table_new(6, 2, FALSE);
			table_y=0;

			gtk_container_add(GTK_CONTAINER(expander), table);

				gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Oversample:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
				widget = gtk_hscale_new_with_range (0,16,1);
				g_signal_connect (G_OBJECT (widget), "value-changed", G_CALLBACK (on_oversample_value_changed), args);
				gtk_range_set_value(GTK_RANGE(widget), options->getInt32("sampler.oversample", 0));
				gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
				table_y++;

				gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Falloff:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
				widget = create_falloff_type_list();
				g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK (on_oversample_falloff_changed), args);
				gtk_combo_box_set_active(GTK_COMBO_BOX(widget), options->getInt32("sampler.falloff", static_cast<int>(SamplerFalloff::none)));
				gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
				table_y++;

				gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Zoom:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
				widget = gtk_hscale_new_with_range (0, 100, 1);
				g_signal_connect (G_OBJECT (widget), "value-changed", G_CALLBACK (on_zoom_value_changed), args);
				gtk_range_set_value(GTK_RANGE(widget), options->getInt32("zoom", 20));
				gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
				table_y++;

			expander=gtk_expander_new("HSV");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.hsv", false));
			args->expanderHSV=expander;
			gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::hsv);
				const char *hsv_labels[] = {"H", _("Hue"), "S", _("Saturation"), "V", _("Value"), nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), hsv_labels);
				args->hsv_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander=gtk_expander_new("HSL");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.hsl", false));
			args->expanderHSL = expander;
			gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::hsl);
				const char *hsl_labels[] = {"H", _("Hue"), "S", _("Saturation"), "L", _("Lightness"), nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), hsl_labels);
				args->hsl_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander=gtk_expander_new("RGB");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.rgb", false));
			args->expanderRGB = expander;
			gtk_box_pack_start (GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::rgb);
				const char *rgb_labels[] = {"R", _("Red"), "G", _("Green"), "B", _("Blue"), nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), rgb_labels);
				args->rgb_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander=gtk_expander_new("CMYK");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.cmyk", false));
			args->expanderCMYK = expander;
			gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::cmyk);
				const char *cmyk_labels[] = {"C", _("Cyan"), "M", _("Magenta"), "Y", _("Yellow"), "K", _("Key"), nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), cmyk_labels);
				args->cmyk_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander = gtk_expander_new("Lab");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.lab", false));
			args->expanderLAB = expander;
			gtk_box_pack_start (GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::lab);
				const char *lab_labels[] = {"L", _("Lightness"), "a", "a", "b", "b", nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), lab_labels);
				args->lab_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander = gtk_expander_new("LCH");
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.lch", false));
			args->expanderLCH = expander;
			gtk_box_pack_start (GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				widget = gtk_color_component_new(GtkColorComponentComp::lch);
				const char *lch_labels[] = {"L", _("Lightness"), "C", "Chroma", "H", "Hue", nullptr};
				gtk_color_component_set_label(GTK_COLOR_COMPONENT(widget), lch_labels);
				args->lch_control = widget;
				g_signal_connect(G_OBJECT(widget), "color-changed", G_CALLBACK(color_component_change_value), args);
				g_signal_connect(G_OBJECT(widget), "button_release_event", G_CALLBACK(color_component_key_up_cb), args);
				g_signal_connect(G_OBJECT(widget), "input-clicked", G_CALLBACK(color_component_input_clicked), args);
				gtk_container_add(GTK_CONTAINER(expander), widget);

			expander=gtk_expander_new(_("Info"));
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.info", false));
			args->expanderInfo=expander;
			gtk_box_pack_start (GTK_BOX(vbox), expander, FALSE, FALSE, 0);

				table = gtk_table_new(3, 2, FALSE);
				table_y=0;
				gtk_container_add(GTK_CONTAINER(expander), table);

					gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Color name:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
					widget = gtk_entry_new();
					gtk_table_attach(GTK_TABLE(table), widget,1,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
					gtk_editable_set_editable(GTK_EDITABLE(widget), FALSE);
					args->color_name = widget;
					table_y++;

					gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Contrast:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
					args->contrastCheck = widget = gtk_color_new();
					StandardEventHandler::forWidget(widget, args->gs, &*args->contrastEditable);
					StandardDragDropHandler::forWidget(widget, args->gs, &*args->contrastEditable);
					auto color = options->getColor("contrast.color", Color(1));
					gtk_color_set_color(GTK_COLOR(widget), color, _("Sample"));
					gtk_color_set_rounded(GTK_COLOR(widget), true);
					gtk_color_set_hcenter(GTK_COLOR(widget), true);
					gtk_color_set_roundness(GTK_COLOR(widget), 5);
					gtk_table_attach(GTK_TABLE(table), widget,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);


					gtk_table_attach(GTK_TABLE(table), args->contrastCheckMsg = gtk_label_new(""),2,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,5);

					table_y++;

			expander=gtk_expander_new(_("Input"));
			gtk_expander_set_expanded(GTK_EXPANDER(expander), options->getBool("expander.input", false));
			args->expanderInput=expander;
			gtk_box_pack_start(GTK_BOX(vbox), expander, false, false, 0);

				table = gtk_table_new(3, 2, FALSE);
				table_y=0;
				gtk_container_add(GTK_CONTAINER(expander), table);

					gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Value:"), 0, 0.5, 0, 0), 0, 1, table_y, table_y + 1, GtkAttachOptions(GTK_FILL), GTK_FILL, 5, 5);
					args->colorInput = widget = gtk_entry_new();
					gtk_entry_set_text(GTK_ENTRY(widget), options->getString("color_input_text", "").c_str());
					gtk_table_attach(GTK_TABLE(table), widget, 1, 3, table_y, table_y + 1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_FILL, 5, 0);
					g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(onColorInputChanged), args);
					table_y++;

					gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Color:"), 0, 0.5, 0, 0), 0, 1, table_y, table_y + 1, GtkAttachOptions(GTK_FILL), GTK_FILL, 5, 5);
					args->colorWidget = widget = gtk_color_new();
					gtk_color_set_rounded(GTK_COLOR(widget), true);
					gtk_color_set_hcenter(GTK_COLOR(widget), true);
					gtk_color_set_roundness(GTK_COLOR(widget), 5);
					gtk_widget_set_size_request(widget, 30, 30);
					gtk_table_attach(GTK_TABLE(table), widget, 1, 3, table_y, table_y + 1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_FILL, 5, 0);
					StandardEventHandler::forWidget(widget, args->gs, &*args->colorInputReadonly);
					StandardDragDropHandler::forWidget(widget, args->gs, &*args->colorInputReadonly);
					table_y++;


	onColorInputChanged(args->colorInput, args);
	updateDisplays(args, 0);
	args->main = main_hbox;
	gtk_widget_show_all(main_hbox);
	args->source.widget = main_hbox;
	return (ColorSource*)args;
}

int color_picker_source_register(ColorSourceManager *csm)
{
	ColorSource *color_source = new ColorSource;
	color_source_init(color_source, "color_picker", _("Color picker"));
	color_source->implement = (ColorSource* (*)(ColorSource *source, GlobalState *gs, const dynv::Ref &options))source_implement;
	color_source->single_instance_only = true;
	color_source->default_accelerator = GDK_KEY_c;
	color_source_manager_add_source(csm, color_source);
	return 0;
}
void color_picker_set_current_color(ColorSource* color_source)
{
	ColorPickerArgs* args = (ColorPickerArgs*)color_source;
	updateMainColor(args);
	gtk_swatch_set_color_to_main(GTK_SWATCH(args->swatch_display));
	updateDisplays(args, nullptr);
}
void color_picker_rotate_swatch(ColorSource* color_source)
{
	ColorPickerArgs* args = (ColorPickerArgs*)color_source;
	gtk_swatch_move_active(GTK_SWATCH(args->swatch_display), 1);
	updateDisplays(args, nullptr);
}
void color_picker_focus_swatch(ColorSource *color_source) {
	ColorPickerArgs* args = (ColorPickerArgs*)color_source;
	gtk_widget_grab_focus(args->swatch_display);
}
bool is_color_picker(ColorSource *color_source) {
	return color_source->identificator == "color_picker";
}
