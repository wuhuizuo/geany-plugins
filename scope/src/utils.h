/*
 *  utils.h
 *
 *  Copyright 2012 Dimitar Toshkov Zhekov <dimitar.zhekov@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILS_H

#define show_error(...) dialogs_show_msgbox(GTK_MESSAGE_ERROR, __VA_ARGS__)
void show_errno(const char *prefix);

gboolean utils_set_nonblock(GPollFD *fd);
void utils_handle_button_press(GtkWidget *widget, GdkEventButton *event);
void utils_handle_button_release(GtkWidget *widget, GdkEventButton *event);
gboolean utils_check_path(const char *pathname, gboolean file, int mode);  /* "" ok */
const gchar *utils_skip_spaces(const gchar *text);

#define array_new(type, start) g_array_sized_new(FALSE, FALSE, sizeof(type), (start))
gchar *array_append(GArray *array);
gchar *array_find(GArray *array, const char *key, gboolean filename);
void array_foreach(GArray *array, GFunc each_func, gpointer gdata);
guint array_index(GArray *array, const void *data);
void array_remove(GArray *array, const void *data);
void array_clear(GArray *array, GFreeFunc free_func);
void array_free(GArray *array, GFreeFunc free_func);
typedef gboolean (*ASaveFunc)(GKeyFile *config, const char *section, void *data);
void array_save(GArray *array, GKeyFile *config, const char *prefix, ASaveFunc save_func);

#define iff(expr, ...) if (G_UNLIKELY(!(expr))) dc_error(__VA_ARGS__); else
#define utils_atoi0(s) ((s) ? atoi(s) : 0)  /* note: 2 references to s */

gboolean model_find(GtkTreeModel *model, GtkTreeIter *iter, guint column, const char *key);
void model_foreach(GtkTreeModel *model, GFunc each_func, gpointer gdata);
void model_save(GtkTreeModel *model, GKeyFile *config, const char *prefix,
	gboolean (*save_func)(GKeyFile *config, const char *section, GtkTreeIter *iter));
gint model_string_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column);
gint model_gint_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer gdata);
gint model_seek_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer gdata);

void utils_load(GKeyFile *config, const char *prefix,
	gboolean (*load_func)(GKeyFile *config, const char *section));
void utils_stash_group_free(StashGroup *group);

typedef enum _SeekerType
{
	SK_EXECUTE,  /* if opening a file, mark it "scope open" */
	SK_EXEC_MARK,  /* set marker execute if file already open */
	SK_DEFAULT
} SeekerType;

extern const char *const SCOPE_OPEN;
extern const char *const SCOPE_LOCK;
#define MARKER_BREAKPT (pref_sci_marker_first + 0)  /* disabled bp, +1 is enabled */
#define MARKER_EXECUTE (pref_sci_marker_first + 2)

void utils_seek(const char *file, gint line, gboolean focus, SeekerType seeker);
void utils_mark(const char *file, gint line, gboolean mark, gint marker);

gboolean utils_source_filetype(GeanyFiletype *ft);
gboolean utils_source_document(GeanyDocument *doc);

/* also (un)mark current line */
void utils_lock(GeanyDocument *doc);
void utils_unlock(GeanyDocument *doc);
void utils_lock_unlock(GeanyDocument *doc, gboolean lock);
void utils_lock_all(gboolean lock);

#define utils_attrib(doc, attrib) (g_object_get_data(G_OBJECT((doc)->editor->sci), (attrib)))
#define utils_current_line(doc) (sci_get_current_line((doc)->editor->sci) + 1)
void utils_move_mark(ScintillaObject *sci, gint line, gint start, gint delta, gint marker);
void utils_remark(GeanyDocument *doc);  /* NULL -> nop */

guint utils_parse_sci_color(const gchar *string);
gboolean utils_key_file_write_to_file(GKeyFile *config, const char *configfile);
gchar *utils_key_file_get_string(GKeyFile *config, const char *section, const char *key);
gchar *utils_get_utf8_basename(const char *file);

char *utils_7bit_to_locale(char *text);  /* == text */
char *utils_get_locale_from_7bit(const char *text);
char *utils_get_locale_from_display(const gchar *display, gint hb_mode);
gchar *utils_get_display_from_7bit(const char *text, gint hb_mode);
gchar *utils_get_display_from_locale(const char *locale, gint hb_mode);

gchar *utils_verify_selection(gchar *text);
gchar *utils_get_default_selection(void);  /* +verify */

enum
{
	VALIDATOR_NUMERIC,
	VALIDATOR_NOSPACE,
	VALIDATOR_VARFRAME
};

void validator_attach(GtkEditable *editable, gint validator);
gchar *validate_column(gchar *text, gboolean string);
GtkWidget *dialog_connect(const char *name);

gchar *utils_text_buffer_get_text(GtkTextBuffer *text, gint maxlen);
void utils_enter_to_clicked(GtkWidget *widget, GtkWidget *button);
void utils_tree_set_cursor(GtkTreeSelection *selection, GtkTreeIter *iter, gdouble alignment);

void utils_finalize(void);

#define UTILS_H 1
#endif
