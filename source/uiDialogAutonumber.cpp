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

#include "uiDialogAutonumber.h"
#include "uiListPalette.h"
#include "uiUtilities.h"
#include "dynv/Map.h"
#include "GlobalState.h"
#include "ColorRYB.h"
#include "Noise.h"
#include "GenerateScheme.h"
#include "I18N.h"
#include <math.h>
#include <sstream>
#include <iostream>
using namespace std;

typedef struct DialogAutonumberArgs{
	GtkWidget *name;
	GtkWidget *nplaces;
	GtkWidget *startindex;
	GtkWidget *toggle_decreasing;
	GtkWidget *toggle_append;
	uint32_t selected_count;
	GtkWidget *sample;
	dynv::Ref options;
	GlobalState* gs;
}DialogAutonumberArgs;

static int default_nplaces(uint32_t selected_count)
{
	uint32_t places = 1;
	uint32_t ncolors = selected_count;
	// technically this can be implemented as `places = 1 + (int) (trunc(log (ncolors,10)));`
	// however I don't know the exact function names, and this has minimal dependencies and acceptable speed.
	while (ncolors > 10) {
		ncolors = ncolors / 10;
		places += 1;
	}
	return places;
}
static void update(GtkWidget *widget, DialogAutonumberArgs *args)
{
	int nplaces = static_cast<int>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->nplaces)));
	int startindex = static_cast<int>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->startindex)));
	const char *name = gtk_entry_get_text(GTK_ENTRY(args->name));
	stringstream ss;
	ss << name << "-";
	ss.fill('0');
	ss.width(nplaces);
	ss << right << startindex;
	auto text = ss.str();
	gtk_entry_set_text(GTK_ENTRY(args->sample), text.c_str());
	args->options->set("name", name);
	args->options->set<bool>("append", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->toggle_append)));
	args->options->set<bool>("decreasing", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(args->toggle_decreasing)));
	args->options->set("nplaces", nplaces);
	args->options->set("startindex", startindex);
}
static void update_startindex(GtkWidget *widget, DialogAutonumberArgs *args)
{
	int startindex = static_cast<int>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->startindex)));
	int newindex;
	gdouble min, max;
	gtk_spin_button_get_range(GTK_SPIN_BUTTON(args->startindex), &min, &max);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))){
		if (startindex == 0){
			newindex = args->selected_count;
		}else{
			newindex = args->selected_count + (startindex - 1);
		}
		min = args->selected_count;
	}else{
		newindex = (startindex + 1) - args->selected_count;
		min = 1;
	}
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(args->startindex), min, max);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->startindex), newindex);
	update(widget, args);
}
int dialog_autonumber_show(GtkWindow* parent, size_t selected_count, GlobalState* gs)
{
	DialogAutonumberArgs *args = new DialogAutonumberArgs;
	int return_val;
	args->gs = gs;
	args->options = args->gs->settings().getOrCreateMap("gpick.autonumber");
	args->selected_count = selected_count;
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Autonumber colors"), parent, GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			nullptr);

	gtk_window_set_default_size(GTK_WINDOW(dialog), args->options->getInt32("window.width", -1),
		args->options->getInt32("window.height", -1));

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

	gint table_y;
	GtkWidget *table;
	table = gtk_table_new(4, 4, FALSE);
	table_y=0;

	gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Name:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
	args->name = gtk_entry_new();
	auto name = args->options->getString("name", "autonum");
	gtk_entry_set_text(GTK_ENTRY(args->name), name.c_str());

	g_signal_connect (G_OBJECT (args->name), "changed", G_CALLBACK(update), args);
	gtk_table_attach(GTK_TABLE(table), args->name,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);

	table_y++;

	gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Decimal places:"),0,0,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
	args->nplaces = gtk_spin_button_new_with_range (1, 6, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->nplaces), args->options->getInt32("nplaces", default_nplaces (selected_count)));
	gtk_table_attach(GTK_TABLE(table), args->nplaces,1,4,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
	g_signal_connect(G_OBJECT (args->nplaces), "value-changed", G_CALLBACK (update), args);
	table_y++;

gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Starting number:"),0,0,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
	args->startindex = gtk_spin_button_new_with_range (1, 0x7fffffff, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->startindex), args->options->getInt32("startindex", 1));
	gtk_table_attach(GTK_TABLE(table), args->startindex,1,4,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
	g_signal_connect(G_OBJECT (args->startindex), "value-changed", G_CALLBACK (update), args);
	table_y++;

	args->toggle_decreasing = gtk_check_button_new_with_mnemonic (_("_Decreasing"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(args->toggle_decreasing), args->options->getBool("decreasing", false));
	gtk_table_attach(GTK_TABLE(table), args->toggle_decreasing,1,4,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
	g_signal_connect (G_OBJECT(args->toggle_decreasing), "toggled", G_CALLBACK (update_startindex), args);
	table_y++;

	args->toggle_append = gtk_check_button_new_with_mnemonic (_("_Append"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(args->toggle_append), args->options->getBool("append", false));
	gtk_table_attach(GTK_TABLE(table), args->toggle_append,1,4,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);
	g_signal_connect (G_OBJECT(args->toggle_append), "toggled", G_CALLBACK (update), args);
	table_y++;

	gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Sample:"),0,0.5,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GtkAttachOptions(GTK_FILL | GTK_EXPAND),5,5);
	args->sample = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(args->sample), false);
	gtk_widget_set_sensitive(args->sample, false);

	gtk_table_attach(GTK_TABLE(table), args->sample,1,2,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,5,0);

	update(0, args);

	gtk_widget_show_all(table);
	setDialogContent(dialog, table);

	return_val = gtk_dialog_run(GTK_DIALOG(dialog));

	gint width, height;
	gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);
	args->options->set("window.width", width);
	args->options->set("window.height", height);
	gtk_widget_destroy(dialog);
	delete args;
	return return_val;
}
