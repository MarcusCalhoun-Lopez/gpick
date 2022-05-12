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

#include "PaletteFromImage.h"
#include "ColorList.h"
#include "ColorObject.h"
#include "uiUtilities.h"
#include "uiListPalette.h"
#include "GlobalState.h"
#include "ToolColorNaming.h"
#include "dynv/Map.h"
#include "I18N.h"
#include "common/Guard.h"
#include <string.h>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>
using namespace std;

/** \file PaletteFromImage.cpp
 * \brief
 */

/** \struct Node
 * \brief Node is a cube in space with color information
 *
 * Each node can b
 */
struct Node{
	uint32_t n_pixels; /**< Number of colors in current Node and its children */
	uint32_t n_pixels_in; /**< Number of colors in current Node */
	float color[3]; /**< Sum of color values */
	float distance; /**< Squared distances from Node center of colors in Node */
	Node *child[8]; /**< Pointers to child Nodes */
	Node *parent; /**< Pointer to parent Node */
};

/** \struct Cube
 * \brief Cube structure holds all information necessary to define cube size and position in space
 */
struct Cube{
	float x; /**< X position */
	float w; /**< Width */
	float y; /**< Y position */
	float h; /**< Height */
	float z; /**< Z position */
	float d; /**< Depth */
};

struct PaletteFromImageArgs{
	GtkWidget *file_browser;
	GtkWidget *range_colors;
	GtkWidget *merge_threshold;
	GtkWidget *preview_expander;
	string filename;
	uint32_t n_colors;
	string previous_filename;
	Node *previous_node;
	ColorList *color_list;
	ColorList *preview_color_list;
	dynv::Ref options;
	GlobalState* gs;
};

struct PaletteColorNameAssigner: public ToolColorNameAssigner {
	PaletteColorNameAssigner(GlobalState &gs):
		ToolColorNameAssigner(gs) {
		m_index = 0;
	}
	void assign(ColorObject &colorObject, std::string_view fileName, const int index) {
		m_fileName = fileName;
		m_index = index;
		ToolColorNameAssigner::assign(colorObject);
	}
	virtual std::string getToolSpecificName(const ColorObject &colorObject) override {
		m_stream.str("");
		m_stream << m_fileName << " #" << m_index;
		return m_stream.str();
	}
protected:
	std::stringstream m_stream;
	std::string_view m_fileName;
	int m_index;
};

/**
 * Allocate and initialize a new node with specified parent
 * @param[in] parent Parent node
 * @return New node
 */
static Node* node_new(Node *parent){
	Node *n = new Node;
	n->color[0] = n->color[1] = n->color[2] = 0;
	n->distance = 0;
	n->n_pixels = 0;
	n->n_pixels_in = 0;
	n->parent = parent;
	for (int i = 0; i < 8; i++){
		n->child[i] = 0;
	}
	return n;
}

/**
 * Deallocate node and its children
 * @param[in] node Node to deallocate
 */
static void node_delete(Node *node){
	for (int i = 0; i < 8; i++){
		if (node->child[i]){
			node_delete(node->child[i]);
		}
	}

	delete node;
}

/**
 * Copy node and assing new parent
 * @param[in] node Node to copy
 * @param[in] parent Parent of copied node
 * @return A copy of node
 */
static Node* node_copy(Node *node, Node *parent){
	Node *n = node_new(0);
	memcpy(n, node, sizeof(Node));
	n->parent = parent;

	for (int i = 0; i < 8; i++){
		if (node->child[i]){
			n->child[i] = node_copy(node->child[i], n);
		}else{
			n->child[i] = 0;
		}
	}
	return n;
}

/**
 * Get the number of nodes with available color information in them
 * @param[in] node Start from this node
 * @return Number of nodes with available color information in them
 */
static uint32_t node_count_leafs(Node *node){
	uint32_t r = 0;
	if (node->n_pixels_in) r++;
	for (int i = 0; i < 8; i++){
		if (node->child[i])
			r += node_count_leafs(node->child[i]);
	}
	return r;
}

/**
 * Call callback on all nodes with available color information in them
 * @param[in] node Start from this node
 * @param[in] leaf_cb Callback function
 * @param[in] userdata User supplied pointer which is passed when calling callback
 */
static void node_leaf_callback(Node *node, void (*leaf_cb)(Node* node, void* userdata), void* userdata){
	if (node->n_pixels_in > 0) leaf_cb(node, userdata);

	for (int i = 0; i < 8; i++){
		if (node->child[i])
			node_leaf_callback(node->child[i], leaf_cb, userdata);
	}
}

/**
 * Merge node information into its parent node
 * @param[in] node Node to merge
 */
static void node_prune(Node *node){

	for (int i = 0; i < 8; i++){
		if (node->child[i]){
			node_prune(node->child[i]);
			node->child[i] = 0;

		}
	}

	if (node->parent){
		node->parent->n_pixels_in += node->n_pixels_in;

		node->parent->color[0] += node->color[0];
		node->parent->color[1] += node->color[1];
		node->parent->color[2] += node->color[2];
	}

	node_delete(node);
}

typedef struct PruneData{
	float threshold;
	float min_distance;
	uint32_t n_colors;
	uint32_t n_colors_target;

	Node *prune_node;
	uint32_t distant_nodes;
}PruneData;

static bool node_prune_threshold(Node *node, PruneData *prune_data){
	if (node->distance <= prune_data->threshold){
		uint32_t colors_removed = node_count_leafs(node);
		node_prune(node);
		prune_data->n_colors -= colors_removed;
		return true;
	}

	if (node->distance < prune_data->min_distance){
		prune_data->min_distance = node->distance;
	}

	uint32_t n = node->n_pixels_in;

	for (int i = 0; i < 8; i++){
		if (node->child[i]){
			if (node_prune_threshold(node->child[i], prune_data)){
				node->child[i] = 0;
			}
		}
	}

	if (node->n_pixels_in > 0 && n == 0) prune_data->n_colors++;

	return false;
}

static void node_reduce(Node *node, uint32_t colors){
	PruneData prune_data;
	prune_data.n_colors = node_count_leafs(node);
	prune_data.n_colors_target = colors;
	prune_data.threshold = 0;

	while (prune_data.n_colors > colors){
		prune_data.min_distance = node->distance;

		if (node_prune_threshold(node, &prune_data)) break;

		prune_data.threshold = prune_data.min_distance;
	}
}

static void node_update(Node *node, Color *color, Cube *cube, uint32_t max_depth){
	Cube new_cube;

	new_cube.w = cube->w / 2;
	new_cube.h = cube->h / 2;
	new_cube.d = cube->d / 2;

	node->n_pixels++;

	node->distance += (color->xyz.x - (cube->x + new_cube.w)) * (color->xyz.x - (cube->x + new_cube.w)) +
		(color->xyz.y - (cube->y + new_cube.h)) * (color->xyz.y - (cube->y + new_cube.h)) +
		(color->xyz.z - (cube->z + new_cube.d)) * (color->xyz.z - (cube->z + new_cube.d));

	if (!max_depth){
		node->n_pixels_in++;

		node->color[0] += color->xyz.x;
		node->color[1] += color->xyz.y;
		node->color[2] += color->xyz.z;

	}else{
		int x, y, z;

		if (color->xyz.x - cube->x < new_cube.w)
			x = 0;
		else
			x = 1;

		if (color->xyz.y - cube->y < new_cube.h)
			y = 0;
		else
			y = 1;

		if (color->xyz.z - cube->z < new_cube.d)
			z = 0;
		else
			z = 1;

		new_cube.x = cube->x + new_cube.w * x;
		new_cube.y = cube->y + new_cube.h * y;
		new_cube.z = cube->z + new_cube.d * z;

		int i = x | (y<<1) | (z<<2);

		if (!node->child[i])
			node->child[i] = node_new(node);

		node->n_pixels++;

		node_update(node->child[x | (y<<1) | (z<<2)], color, &new_cube, max_depth - 1);
	}
}

static void leaf_cb(Node *node, void *userdata){
	list<Color> *l = static_cast<list<Color>*>(userdata);

	Color c;
	c.xyz.x = node->color[0] / node->n_pixels_in;
	c.xyz.y = node->color[1] / node->n_pixels_in;
	c.xyz.z = node->color[2] / node->n_pixels_in;

	l->push_back(c);
}

static Node* process_image(PaletteFromImageArgs *args, const char *filename, Node* node){

	if (args->previous_filename == filename){
		if (args->previous_node)
			return node_copy(args->previous_node, 0);
		else
			return 0;
	}

	args->previous_filename = filename;
	if (args->previous_node){
		node_delete(args->previous_node);
		args->previous_node = 0;
	}

	GError *error = nullptr;
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (error){
		cout << error->message << endl;
		g_error_free(error);
		return 0;
	}

	int channels = gdk_pixbuf_get_n_channels(pixbuf);
	int width = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
	guchar *image_data = gdk_pixbuf_get_pixels(pixbuf);
	guchar *ptr = image_data;

	Cube cube;
	cube.x = 0;
	cube.y = 0;
	cube.z = 0;
	cube.w = 1;
	cube.h = 1;
	cube.d = 1;

	Color color;

	args->previous_node = node_new(0);

	for (int y = 0; y < height; y++){
		ptr = image_data + rowstride * y;
		for (int x = 0; x < width; x++){

			color.xyz.x = ptr[0] / 255.0f;
			color.xyz.y = ptr[1] / 255.0f;
			color.xyz.z = ptr[2] / 255.0f;

			node_update(args->previous_node, &color, &cube, 5);

			ptr += channels;
		}
	}
	g_object_unref(pixbuf);
	node_reduce(args->previous_node, 200);
	return node_copy(args->previous_node, 0);
}

static void get_settings(PaletteFromImageArgs *args){

	gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(args->file_browser));
	if (filename){
		args->filename = filename;
		g_free(filename);
	}else{
		args->filename.clear();
	}

	args->n_colors = static_cast<int>(gtk_spin_button_get_value(GTK_SPIN_BUTTON(args->range_colors)));
}

static void save_settings(PaletteFromImageArgs *args){
	args->options->set("colors", static_cast<int32_t>(args->n_colors));
	gchar *current_folder = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(args->file_browser));
	if (current_folder){
		args->options->set("current_folder", current_folder);
		g_free(current_folder);
	}
	GtkFileFilter *filter = gtk_file_chooser_get_filter(GTK_FILE_CHOOSER(args->file_browser));
	if (filter){
		const char *filter_name = static_cast<const char*>(g_object_get_data(G_OBJECT(filter), "name"));
		args->options->set("filter", filter_name);
	}
}

static void calc(PaletteFromImageArgs *args, bool preview, int limit){

	Node *root_node = 0;
	int index = 0;
	gchar *name = g_path_get_basename(args->filename.c_str());
	PaletteColorNameAssigner name_assigner(*args->gs);
	if (!args->filename.empty())
		root_node = process_image(args, args->filename.c_str(), root_node);

	ColorList *color_list;

	if (preview)
		color_list = args->preview_color_list;
	else
		color_list = args->gs->getColorList();

	list<Color> tmp_list;

	if (root_node){
		node_reduce(root_node, args->n_colors);
		node_leaf_callback(root_node, leaf_cb, &tmp_list);
		node_delete(root_node);
	}

	common::Guard colorListGuard(color_list_start_changes(color_list), color_list_end_changes, color_list);
	for (list<Color>::iterator i = tmp_list.begin(); i != tmp_list.end(); i++){
		i->alpha = 1;
		ColorObject *color_object = color_list_new_color_object(color_list, &(*i));
		name_assigner.assign(*color_object, name, index);
		color_list_add_color_object(color_list, color_object, 1);
		color_object->release();
		index++;
	}
}

static void update(GtkWidget *widget, PaletteFromImageArgs *args ){
	color_list_remove_all(args->preview_color_list);
	get_settings(args);
	calc(args, true, 100);
}

static void destroy_cb(GtkWidget* widget, PaletteFromImageArgs *args){

	if (args->previous_node) node_delete(args->previous_node);
	color_list_destroy(args->preview_color_list);
	delete args;
}

static void response_cb(GtkWidget* widget, gint response_id, PaletteFromImageArgs *args){

	get_settings(args);
	save_settings(args);

	gint width, height;
	gtk_window_get_size(GTK_WINDOW(widget), &width, &height);

	args->options->set("window.width", width);
	args->options->set("window.height", height);
	args->options->set<bool>("show_preview", gtk_expander_get_expanded(GTK_EXPANDER(args->preview_expander)));

	switch (response_id){
		case GTK_RESPONSE_APPLY:
			calc(args, false, 0);
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			break;
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(widget);
			break;
	}
}

void tools_palette_from_image_show(GtkWindow* parent, GlobalState* gs)
{
	PaletteFromImageArgs *args = new PaletteFromImageArgs;
	args->previous_filename = "";
	args->gs = gs;
	args->options = args->gs->settings().getOrCreateMap("gpick.tools.palette_from_image");
	args->previous_node = 0;
	GtkWidget *table, *table_m, *widget;
	GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Palette from image"), parent, GtkDialogFlags(GTK_DIALOG_DESTROY_WITH_PARENT), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, GTK_STOCK_ADD, GTK_RESPONSE_APPLY, nullptr);
	gtk_window_set_default_size(GTK_WINDOW(dialog), args->options->getInt32("window.width", -1),
		args->options->getInt32("window.height", -1));
	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog), GTK_RESPONSE_APPLY, GTK_RESPONSE_CLOSE, -1);

	GtkWidget *frame;
	gint table_y, table_m_y;

	table_m = gtk_table_new(3, 1, FALSE);
	table_m_y = 0;
	frame = gtk_frame_new(_("Image"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;

	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);

	args->file_browser = widget = gtk_file_chooser_button_new(_("Image file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widget), args->options->getString("current_folder", "").c_str());
	gtk_table_attach(GTK_TABLE(table), widget, 0, 3, table_y, table_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	g_signal_connect(G_OBJECT(args->file_browser), "file-set", G_CALLBACK(update), args);
	table_y++;

	auto selected_filter = args->options->getString("filter", "all_images");
	GtkFileFilter *filter;
	GtkFileFilter *all_image_filter;

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, _("All files"));
	gtk_file_filter_add_pattern(filter, "*");
	g_object_set_data_full(G_OBJECT(filter), "name", (void*)"all_files", GDestroyNotify(nullptr));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
	if ("all_files" == selected_filter) gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(widget), filter);


	all_image_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(all_image_filter, _("All images"));
	g_object_set_data_full(G_OBJECT(all_image_filter), "name", (void*)"all_images", GDestroyNotify(nullptr));
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), all_image_filter);
	if ("all_images" == selected_filter) gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(widget), all_image_filter);

	stringstream ss;
	GSList *formats = gdk_pixbuf_get_formats();
	GSList *i = formats;
	while (i){
		GdkPixbufFormat *format = static_cast<GdkPixbufFormat*>(g_slist_nth_data(i, 0));

		filter = gtk_file_filter_new();
		gtk_file_filter_set_name(filter, gdk_pixbuf_format_get_description(format));

		gchar **extensions = gdk_pixbuf_format_get_extensions(format);
		if (extensions){
			for (int j = 0; extensions[j]; j++){
				ss.str("");
				ss << "*." << extensions[j];
				auto pattern = ss.str();
				gtk_file_filter_add_pattern(filter, pattern.c_str());
				gtk_file_filter_add_pattern(all_image_filter, pattern.c_str());
			}
			g_strfreev(extensions);
		}
		g_object_set_data_full(G_OBJECT(filter), "name", gdk_pixbuf_format_get_name(format), GDestroyNotify(nullptr));
		gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(widget), filter);
		if (gdk_pixbuf_format_get_name(format) == selected_filter) gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(widget), filter);

		i = g_slist_next(i);
	}
	if (formats) g_slist_free(formats);

	frame = gtk_frame_new(_("Options"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_table_attach(GTK_TABLE(table_m), frame, 0, 1, table_m_y, table_m_y+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL), 5, 5);
	table_m_y++;
	table = gtk_table_new(5, 3, FALSE);
	table_y=0;
	gtk_container_add(GTK_CONTAINER(frame), table);

	gtk_table_attach(GTK_TABLE(table), gtk_label_aligned_new(_("Colors:"),0,0,0,0),0,1,table_y,table_y+1,GtkAttachOptions(GTK_FILL),GTK_FILL,5,5);
	args->range_colors = widget = gtk_spin_button_new_with_range (1, 100, 1);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(args->range_colors), args->options->getInt32("colors", 3));
	gtk_table_attach(GTK_TABLE(table), widget,1,3,table_y,table_y+1,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GTK_FILL,3,3);
	g_signal_connect(G_OBJECT(args->range_colors), "value-changed", G_CALLBACK(update), args);
	table_y++;

	ColorList* preview_color_list = nullptr;
	gtk_table_attach(GTK_TABLE(table_m), args->preview_expander = palette_list_preview_new(gs, true, args->options->getBool("show_preview", true), gs->getColorList(), &preview_color_list), 0, 1, table_m_y, table_m_y+1 , GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND), 5, 5);
	table_m_y++;

	args->preview_color_list = preview_color_list;

	gtk_widget_show_all(table_m);
	setDialogContent(dialog, table_m);

	g_signal_connect(G_OBJECT(dialog), "destroy", G_CALLBACK(destroy_cb), args);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(response_cb), args);

	gtk_widget_show(dialog);
}
