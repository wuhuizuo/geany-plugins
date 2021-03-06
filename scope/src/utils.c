/*
 *  utils.c
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

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common.h"

#ifdef G_OS_UNIX
#include <fcntl.h>

void show_errno(const char *prefix)
{
	show_error(_("%s: %s."), prefix, g_strerror(errno));
}
#else  /* G_OS_UNIX */
#include <windows.h>

void show_errno(const char *prefix)
{
	show_error(_("%s: error %lu."), prefix, (unsigned long) GetLastError());
}
#endif  /* G_OS_UNIX */

gboolean utils_set_nonblock(GPollFD *fd)
{
#ifdef G_OS_UNIX
	int state = fcntl(fd->fd, F_GETFL);
	return state != -1 && fcntl(fd->fd, F_SETFL, state | O_NONBLOCK) != -1;
#else  /* G_OS_UNIX */
	HANDLE h = (HANDLE) _get_osfhandle(fd->fd);
	DWORD state;

	if (h != INVALID_HANDLE_VALUE &&
		GetNamedPipeHandleState(h, &state, NULL, NULL, NULL, NULL, 0))
	{
			state |= PIPE_NOWAIT;
			if (SetNamedPipeHandleState(h, &state, NULL, NULL))
				return TRUE;
	}

	return FALSE;
#endif  /* G_OS_UNIX */
}

void utils_handle_button_press(GtkWidget *widget, GdkEventButton *event)
{
	/* from sidebar.c */
	GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);

	if (widget_class->button_press_event)
		widget_class->button_press_event(widget, event);
}

void utils_handle_button_release(GtkWidget *widget, GdkEventButton *event)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_GET_CLASS(widget);

	if (widget_class->button_release_event)
		widget_class->button_release_event(widget, event);
}

gboolean utils_check_path(const gchar *pathname, gboolean file, int mode)
{
	if (*pathname)
	{
		char *path = utils_get_locale_from_utf8(pathname);
		struct stat buf;
		gboolean result = FALSE;

		if (stat(path, &buf) == 0)
		{
			if (!S_ISDIR(buf.st_mode) == file)
				result = access(path, mode) == 0;
			else
				errno = file ? EISDIR : ENOTDIR;
		}

		g_free(path);
		return result;
	}

	return TRUE;
}

const gchar *utils_skip_spaces(const gchar *text)
{
	while (isspace(*text))
		text++;
	return text;
}

gchar *array_append(GArray *array)
{
	g_array_set_size(array, array->len + 1);
	return array->data + g_array_get_element_size(array) * (array->len - 1);
}

#ifdef G_OS_UNIX
gchar *array_find(GArray *array, const char *key, G_GNUC_UNUSED gboolean filename)
#else
gchar *array_find(GArray *array, const char *key, gboolean filename)
#endif
{
	guint i;
	gchar *data = array->data;
	guint size = g_array_get_element_size(array);
#ifdef G_OS_UNIX
	int (*const compare)(const char *s1, const char *s2) = strcmp;
#else
	int (*compare)(const char *s1, const char *s2) = filename ? utils_str_casecmp : strcmp;
#endif

	for (i = 0; i < array->len; i++, data += size)
		if (!compare(*(const char **) data, key))
			return data;

	return NULL;
}

void array_foreach(GArray *array, GFunc each_func, gpointer gdata)
{
	guint i;
	gchar *data = array->data;
	guint size = g_array_get_element_size(array);

	for (i = 0; i < array->len; i++, data += size)
		each_func(data, gdata);
}

guint array_index(GArray *array, gconstpointer element)
{
	return ((const gchar *) element - array->data) / g_array_get_element_size(array);
}

void array_remove(GArray *array, gconstpointer element)
{
	g_array_remove_index(array, array_index(array, element));
}

static void array_free_element(gpointer element, GFreeFunc free_func)
{
	free_func(element);
}

void array_clear(GArray *array, GFreeFunc free_func)
{
	array_foreach(array, (GFunc) array_free_element, free_func);
	g_array_set_size(array, 0);
}

void array_free(GArray *array, GFreeFunc free_func)
{
	array_foreach(array, (GFunc) array_free_element, free_func);
	g_array_free(array, TRUE);
}

static void utils_clear_sections(GKeyFile *config, const char *prefix, guint i)
{
	gboolean valid;

	do
	{
		char *section = g_strdup_printf("%s_%d", prefix, i++);
		valid = g_key_file_remove_group(config, section, NULL);
		g_free(section);
	} while (valid);
}

void array_save(GArray *array, GKeyFile *config, const gchar *prefix, ASaveFunc save_func)
{
	guint i, n = 0;
	gchar *data = array->data;

	for (i = 0; i < array->len; i++, data += g_array_get_element_size(array))
	{
		char *section = g_strdup_printf("%s_%d", prefix, n);
		n += save_func(config, section, data);
		g_free(section);
	}

	utils_clear_sections(config, prefix, n);
}

gboolean model_find(GtkTreeModel *model, GtkTreeIter *iter, guint column, const char *key)
{
	gboolean string = gtk_tree_model_get_column_type(model, column) == G_TYPE_STRING;
	gboolean valid = gtk_tree_model_get_iter_first(model, iter);
	gint i = atoi(key);

	while (valid)
	{
		union
		{
			const gchar *s;
			gint i;
		} data;

		gtk_tree_model_get(model, iter, column, &data, -1);

		if (string ? g_strcmp0(data.s, key) == 0 : data.i == i)
			return TRUE;

		valid = gtk_tree_model_iter_next(model, iter);
	}

	return FALSE;
}

void model_foreach(GtkTreeModel *model, GFunc each_func, gpointer gdata)
{
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid)
	{
		each_func(&iter, gdata);
		valid = gtk_tree_model_iter_next(model, &iter);
	}
}

void model_save(GtkTreeModel *model, GKeyFile *config, const gchar *prefix,
	gboolean (*save_func)(GKeyFile *config, const char *section, GtkTreeIter *iter))
{
	guint i = 0;
	GtkTreeIter iter;
	gboolean valid = gtk_tree_model_get_iter_first(model, &iter);

	while (valid)
	{
		char *section = g_strdup_printf("%s_%d", prefix, i);

		i += save_func(config, section, &iter);
		valid = gtk_tree_model_iter_next(model, &iter);
		g_free(section);
	}

	utils_clear_sections(config, prefix, i);
}

gint model_string_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gint column)
{
	const gchar *s1, *s2;

	gtk_tree_model_get(model, a, column, &s1, -1);
	gtk_tree_model_get(model, b, column, &s2, -1);
	return g_strcmp0(s1, s2);
}

static int model_atoint(GtkTreeModel *model, GtkTreeIter *iter, gpointer gdata)
{
	const gchar *s;
	gtk_tree_model_get(model, iter, GPOINTER_TO_INT(gdata), &s, -1);
	return utils_atoi0(s);
}

gint model_gint_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer gdata)
{
	return model_atoint(model, a, gdata) - model_atoint(model, b, gdata);
}

gint model_seek_compare(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
	G_GNUC_UNUSED gpointer gdata)
{
	gint result = model_string_compare(model, a, b, COLUMN_FILE);

	if (!result)
	{
		gint i1, i2;

		gtk_tree_model_get(model, a, COLUMN_LINE, &i1, -1);
		gtk_tree_model_get(model, b, COLUMN_LINE, &i2, -1);
		result = i1 - i2;
	}

	return result;
}

void utils_load(GKeyFile *config, const gchar *prefix,
	gboolean (*load_func)(GKeyFile *config, const char *section))
{
	guint i = 0;
	gboolean valid;

	do
	{
		char *section = g_strdup_printf("%s_%d", prefix, i++);
		valid = FALSE;

		if (g_key_file_has_group(config, section))
		{
			if (load_func(config, section))
				valid = TRUE;
			else
				msgwin_status_add(_("Scope: error reading [%s]."), section);
		}

		g_free(section);

	} while (valid);
}

void utils_stash_group_free(StashGroup *group)
{
	stash_group_free_settings(group);
	stash_group_free(group);
}

const char *const SCOPE_OPEN = "scope_open";
const char *const SCOPE_LOCK = "scope_lock";
gint utils_sci_marker_first;

void utils_seek(const char *file, gint line, gboolean focus, SeekerType seeker)
{
	GeanyDocument *doc = NULL;

	if (file)
	{
		GeanyDocument *old_doc = document_get_current();
		ScintillaObject *sci;

		doc = document_find_by_real_path(file);

		if (doc)
		{
			sci = doc->editor->sci;
			gtk_notebook_set_current_page(GTK_NOTEBOOK(geany->main_widgets->notebook),
				document_get_notebook_page(doc));

			if (seeker == SK_EXEC_MARK)
				sci_set_marker_at_line(sci, line - 1, MARKER_EXECUTE);
		}
		else if ((doc = document_open_file(file, FALSE, NULL, NULL)) != NULL)
		{
			sci = doc->editor->sci;
			if (seeker == SK_EXECUTE || seeker == SK_EXEC_MARK)
				g_object_set_data(G_OBJECT(sci), SCOPE_OPEN, utils_seek);
		}

		if (doc)
		{
			if (line)
			{
				if (seeker == SK_DEFAULT && pref_seek_with_navqueue)
					navqueue_goto_line(old_doc, doc, line);
				else
				{
					scintilla_send_message(sci, SCI_SETYCARETPOLICY,
						pref_sci_caret_policy, pref_sci_caret_slop);
					sci_goto_line(sci, line - 1, TRUE);
					scintilla_send_message(sci, SCI_SETYCARETPOLICY, CARET_EVEN, 0);
				}
			}

			if (focus)
				gtk_widget_grab_focus(GTK_WIDGET(sci));
		}
	}

	if (!doc && (seeker == SK_EXECUTE || seeker == SK_EXEC_MARK))
		dc_error("thread %s at %s:%d", thread_id, file, line + 1);
}

void utils_mark(const char *file, gint line, gboolean mark, gint marker)
{
	if (line)
	{
		GeanyDocument *doc = document_find_by_real_path(file);

		if (doc)
		{
			if (mark)
				sci_set_marker_at_line(doc->editor->sci, line - 1, marker);
			else
				sci_delete_marker_at_line(doc->editor->sci, line - 1, marker);
		}
	}
}

gboolean utils_source_filetype(GeanyFiletype *ft)
{
	if (ft)
	{
		static const filetype_id ft_id[] = { GEANY_FILETYPES_C, GEANY_FILETYPES_CPP,
			GEANY_FILETYPES_D, GEANY_FILETYPES_OBJECTIVEC, GEANY_FILETYPES_FORTRAN,
			GEANY_FILETYPES_JAVA, /* GEANY_FILETYPES_OPENCL_C, */ GEANY_FILETYPES_PASCAL,
			/* GEANY_FILETYPES_S, */ GEANY_FILETYPES_ASM, /* GEANY_FILETYPES_MODULA_2, */
			GEANY_FILETYPES_ADA };

		guint i;

		for (i = 0; i < sizeof ft_id / sizeof ft_id[0]; i++)
			if (ft_id[i] == ft->id)
				return TRUE;
	}

	return FALSE;
}

gboolean utils_source_document(GeanyDocument *doc)
{
	return doc->real_path && utils_source_filetype(doc->file_type);
}

enum { GCS_CURRENT_LINE = 7 };  /* from highlighting.c */

static void line_mark_unmark(GeanyDocument *doc, gboolean lock)
{
	if (pref_unmark_current_line)
	{
		scintilla_send_message(doc->editor->sci, SCI_SETCARETLINEVISIBLE, lock ? FALSE :
			highlighting_get_style(GEANY_FILETYPES_NONE, GCS_CURRENT_LINE)->bold, 0);
	}
}

static void doc_lock_unlock(GeanyDocument *doc, gboolean lock)
{
	scintilla_send_message(doc->editor->sci, SCI_SETREADONLY, lock, 0);
	doc->readonly = lock;
	document_set_text_changed(doc, doc->changed);  /* to redraw tab and update sidebar */
}

void utils_lock(GeanyDocument *doc)
{
	if (utils_source_document(doc))
	{
		if (!doc->readonly)
		{
			doc_lock_unlock(doc, TRUE);
			g_object_set_data(G_OBJECT(doc->editor->sci), SCOPE_LOCK, utils_lock);
		}

		line_mark_unmark(doc, TRUE);
		tooltip_attach(doc->editor);
	}
}

void utils_unlock(GeanyDocument *doc)
{
	if (utils_attrib(doc, SCOPE_LOCK))
	{
		doc_lock_unlock(doc, FALSE);
		g_object_steal_data(G_OBJECT(doc->editor->sci), SCOPE_LOCK);
	}

	line_mark_unmark(doc, FALSE);
	tooltip_remove(doc->editor);
}

void utils_lock_unlock(GeanyDocument *doc, gboolean lock)
{
	if (lock)
		utils_lock(doc);
	else
		utils_unlock(doc);
}

void utils_lock_all(gboolean lock)
{
	guint i;

	foreach_document(i)
		utils_lock_unlock(documents[i], lock);
}

void utils_move_mark(ScintillaObject *sci, gint line, gint start, gint delta, gint marker)
{
	sci_delete_marker_at_line(sci, delta > 0 || start - delta <= line ? line + delta : start,
		marker);
	sci_set_marker_at_line(sci, line, marker);
}

#define marker_delete_all(doc, marker) \
	scintilla_send_message((doc)->editor->sci, SCI_MARKERDELETEALL, (marker), 0)

void utils_remark(GeanyDocument *doc)
{
	if (doc)
	{
		if (debug_state() != DS_INACTIVE)
		{
			marker_delete_all(doc, MARKER_EXECUTE);
			threads_mark(doc);
		}

		marker_delete_all(doc, MARKER_BREAKPT);
		marker_delete_all(doc, MARKER_BREAKPT + TRUE);
		breaks_mark(doc);
	}
}

guint utils_parse_sci_color(const gchar *string)
{
	GdkColor color;

	gdk_color_parse(string, &color);
	return ((color.blue >> 8) << 16) + (color.green & 0xFF00) + (color.red >> 8);
}

gboolean utils_key_file_write_to_file(GKeyFile *config, const char *configfile)
{
	gchar *data = g_key_file_to_data(config, NULL, NULL);
	gint error = utils_write_file(configfile, data);

	g_free(data);
	if (error)
		msgwin_status_add(_("Scope: %s: %s."), configfile, g_strerror(error));

	return !error;
}

gchar *utils_key_file_get_string(GKeyFile *config, const char *section, const char *key)
{
	gchar *string = utils_get_setting_string(config, section, key, NULL);

	if (!validate_column(string, TRUE))
	{
		g_free(string);
		string = NULL;
	}

	return string;
}

gchar *utils_get_utf8_basename(const char *file)
{
	gchar *utf8 = utils_get_utf8_from_locale(file);
	gchar *base = g_path_get_basename(utf8);
	g_free(utf8);
	return base;
}

static void utils_7bit_text_to_locale(const char *text, char *locale)
{
	while (*text)
	{
		if (*text == '\\' && (guint) (text[1] - '0') <= 3 &&
			(guint) (text[2] - '0') <= 7 && (guint) (text[3] - '0') <= 7)
		{
			unsigned char c = (text[1] - '0') * 64 + (text[2] - '0') * 8 + text[3] - '0';

			if (isgraph(c))
			{
				*locale++ = c;
				text += 4;
				continue;
			}
		}

		*locale++ = *text++;
	}

	*locale = '\0';
}

char *utils_7bit_to_locale(char *text)
{
	if (text)
		utils_7bit_text_to_locale(text, text);

	return text;
}

char *utils_get_locale_from_7bit(const char *text)
{
	char *locale;

	if (text)
	{
		locale = g_malloc(strlen(text) + 1);
		utils_7bit_text_to_locale(text, locale);
	}
	else
		locale = NULL;

	return locale;
}

char *utils_get_locale_from_display(const gchar *display, gint hb_mode)
{
	return opt_hb_mode(hb_mode) == HB_LOCALE ? utils_get_locale_from_utf8(display) :
		g_strdup(display);
}

gchar *utils_get_display_from_7bit(const char *text, gint hb_mode)
{
	gchar *display;

	if (opt_hb_mode(hb_mode) == HB_7BIT)
		display = g_strdup(text);
	else
	{
		char *locale = utils_get_locale_from_7bit(text);
		display = utils_get_display_from_locale(locale, hb_mode);
		g_free(locale);
	}

	return display;
}

gchar *utils_get_display_from_locale(const char *locale, gint hb_mode)
{
	return opt_hb_mode(hb_mode) == HB_LOCALE ? utils_get_utf8_from_locale(locale) :
		g_strdup(locale);
}

gchar *utils_verify_selection(gchar *text)
{
	if (text)
	{
		gchar *s;

		for (s = text; (s = strchr(s, '=')) != NULL; s++)
		{
			if (s[1] == '=')
				s++;
			else if (s < text + 2 || !strchr("<>", s[-1]) || s[-1] == s[-2])
			{
				g_free(text);
				return NULL;
			}
		}
	}

	return text;
}

gchar *utils_get_default_selection(void)
{
	GeanyDocument *doc = document_get_current();
	gchar *text = NULL;

	if (doc && utils_source_document(doc))
		text = editor_get_default_selection(doc->editor, TRUE, NULL);

	return utils_verify_selection(text);
}

static void on_insert_text(GtkEditable *editable, gchar *new_text, gint new_text_length,
	G_GNUC_UNUSED gint *position, gpointer gdata)
{
	gboolean valid = TRUE;
	
	if (new_text_length == -1)
		new_text_length = (gint) strlen(new_text);

	if (GPOINTER_TO_INT(gdata) == VALIDATOR_VARFRAME)
	{
		gchar *s = gtk_editable_get_chars(editable, 0, 1);

		if (*s == '\0' && new_text_length == 1 && (*new_text == '*' || *new_text == '@'))
			new_text_length = 0;
		else if (*s == '*' || *s == '@')
			valid = !new_text_length;

		g_free(s);
	}

	while (new_text_length-- > 0 && valid)
	{
		switch (GPOINTER_TO_INT(gdata))
		{
			case VALIDATOR_NUMERIC : valid = isdigit(*new_text); break;
			case VALIDATOR_NOSPACE : valid = !isspace(*new_text); break;
			case VALIDATOR_VARFRAME :
			{
				valid = isxdigit(*new_text) || tolower(*new_text) == 'x';
				break;
			}
			default : valid = FALSE;
		}
		new_text++;
	}

	if (!valid)
		g_signal_stop_emission_by_name(editable, "insert-text");
}

void validator_attach(GtkEditable *editable, gint validator)
{
	g_signal_connect(editable, "insert-text", G_CALLBACK(on_insert_text),
		GINT_TO_POINTER(validator));
}

static gchar *validate_string(gchar *text)
{
	char *s = text + strlen(text);

	while (--s >= text && isspace(*s));
	s[1] = '\0';
	return *text ? text : NULL;
}

static gchar *validate_number(gchar *text)
{
	char *s;

	if (*text == '+') text++;
	while (*text == '0') text++;

	for (s = text; isdigit(*s); s++);
	*s = '\0';
	return *text && (s - text < 10 ||
		(s - text == 10 && strcmp(text, "2147483648")) < 0) ? text : NULL;
}

gchar *validate_column(gchar *text, gboolean string)
{
	if (text)
	{
		const gchar *s = utils_skip_spaces(text);
		memmove(text, s, strlen(s) + 1);
		return string ? validate_string(text) : validate_number(text);
	}

	return NULL;
}

static void on_dialog_response(GtkDialog *dialog, gint response, G_GNUC_UNUSED gpointer gdata)
{
	if (response)
		gtk_widget_hide(GTK_WIDGET(dialog));
}

GtkWidget *dialog_connect(const char *name)
{
	GtkWidget *dialog = get_widget(name);

	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(geany->main_widgets->window));
	g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), NULL);
	return dialog;
}

gchar *utils_text_buffer_get_text(GtkTextBuffer *text, gint maxlen)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter(text, &start);
	gtk_text_buffer_get_iter_at_offset(text, &end, maxlen);
	return gtk_text_buffer_get_text(text, &start, &end, FALSE);
}

static gboolean on_widget_key_press(G_GNUC_UNUSED GtkWidget *widget, GdkEventKey *event,
	GtkWidget *button)
{
	if (!ui_is_keyval_enter_or_return(event->keyval))
		return FALSE;

	if (gtk_widget_get_sensitive(button))
		gtk_button_clicked(GTK_BUTTON(button));

	return TRUE;
}

void utils_enter_to_clicked(GtkWidget *widget, GtkWidget *button)
{
	g_signal_connect(widget, "key-press-event", G_CALLBACK(on_widget_key_press), button);
}

void utils_tree_set_cursor(GtkTreeSelection *selection, GtkTreeIter *iter, gdouble alignment)
{
	GtkTreeView *tree = gtk_tree_selection_get_tree_view(selection);
	GtkTreePath *path = gtk_tree_model_get_path(gtk_tree_view_get_model(tree), iter);

	if (alignment >= 0)
		gtk_tree_view_scroll_to_cell(tree, path, NULL, TRUE, alignment, 0);

	gtk_tree_view_set_cursor(tree, path, NULL, FALSE);
	gtk_tree_path_free(path);
}

void utils_finalize(void)
{
	guint i;
	DebugState state = debug_state();

	foreach_document(i)
	{
		g_object_steal_data(G_OBJECT(documents[i]->editor->sci), SCOPE_OPEN);

		if (state != DS_INACTIVE)
			utils_unlock(documents[i]);
	}
}
