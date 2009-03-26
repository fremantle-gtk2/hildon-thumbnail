/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <philip@codeminded.be>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR  PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <utime.h>

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>

#include "utils.h"

#ifdef HAVE_SQLITE3
#include <sqlite3.h>
#endif

#include <hildon-thumbnail-plugin.h>

static gboolean had_init = FALSE;
static gboolean is_active = TRUE;
static GFileMonitor *monitor = NULL;

#ifdef HAVE_SQLITE3
static sqlite3 *db = NULL;
static gint callback (void   *NotUsed, gint    argc, gchar **argv, gchar **azColName) { return 0; }
#endif

void hildon_thumbnail_outplugin_cleanup (const gchar *uri_match, guint since);

void
hildon_thumbnail_outplugin_cleanup (const gchar *uri_match, guint since)
{
#ifdef HAVE_SQLITE3
	sqlite3_stmt *stmt;
	gint result = SQLITE_OK;

	if (!db) {
		gchar *dbfile;
		dbfile = g_build_filename (g_get_home_dir (), ".thumbnails", 
				   "meta.db", NULL);
		if (g_file_test (dbfile, G_FILE_TEST_EXISTS))
			sqlite3_open (dbfile, &db);
		g_free (dbfile);
	}

	if (db) {
		gchar *sql = g_strdup_printf ("select Path from jpegthumbnails where URI LIKE '%s%%' and MTime <= '%u'",
					      uri_match, since);
		result = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);

		while (result == SQLITE_OK  || result == SQLITE_ROW || result == SQLITE_BUSY) {
			const unsigned char *path;

			result = sqlite3_step (stmt);

			if (result == SQLITE_ERROR) {
				sqlite3_reset (stmt);
				result = SQLITE_OK;
				continue;
			}

			if (result == SQLITE_BUSY) {
				g_usleep (10);
				result = SQLITE_OK;
				continue;
			}

			path = sqlite3_column_text (stmt, 0);
			g_unlink ((const gchar *) path);
		}
		g_free (sql);
		sql = g_strdup_printf ("delete from jpegthumbnails where URI LIKE '%s%%' and MTime <= '%u'",
					       uri_match, since);
		sqlite3_exec (db, sql, callback, 0, NULL);
		g_free (sql);
	}
#endif
}

gchar *
hildon_thumbnail_outplugin_get_orig (const gchar *path)
{
#ifdef HAVE_SQLITE3
	gchar *retval = NULL;
	sqlite3_stmt *stmt;
	gint result = SQLITE_OK;


	if (!db) {
		gchar *dbfile;
		dbfile = g_build_filename (g_get_home_dir (), ".thumbnails", 
				   "meta.db", NULL);
		if (g_file_test (dbfile, G_FILE_TEST_EXISTS))
			sqlite3_open (dbfile, &db);
		g_free (dbfile);
	}

	if (db) {
		const unsigned char *tmp;
		gchar *sql = g_strdup_printf ("select URI from jpegthumbnails where Path = '%s'",
					      path);
		result = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
		g_free (sql);

		while (result == SQLITE_OK  || result == SQLITE_ROW || result == SQLITE_BUSY) {
			result = sqlite3_step (stmt);

			if (result == SQLITE_ERROR) {
				sqlite3_reset (stmt);
				result = SQLITE_OK;
				continue;
			}

			if (result == SQLITE_BUSY) {
				g_usleep (10);
				result = SQLITE_OK;
				continue;
			}

			tmp = sqlite3_column_text (stmt, 0);

			if (tmp) {
				retval = g_strdup ((const gchar *) tmp);
				break;
			}
		}
	}

	return retval;
#else
	return NULL;
#endif
}

gboolean
hildon_thumbnail_outplugin_needs_out (HildonThumbnailPluginOutType type, guint64 mtime, const gchar *uri)
{
	gboolean retval;
	gchar *large, *normal, *cropped, *filen;

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
					       NULL, NULL, NULL, FALSE);

	switch (type) {
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE:
			filen = large;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL:
			filen = normal;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED:
			filen = cropped;
		break;
	}

	retval = FALSE;

	if (g_file_test (filen, G_FILE_TEST_EXISTS)) {
		struct stat st;
		g_stat (filen, &st);
		if (st.st_mtime != (gint64) mtime)
			retval = TRUE;
	} else
		retval = TRUE;

	g_free (normal);
	g_free (large);
	g_free (cropped);

	return retval;
}

void
hildon_thumbnail_outplugin_out (const guchar *rgb8_pixmap, 
				guint width, guint height,
				guint rowstride, guint bits_per_sample,
				gboolean has_alpha,
				HildonThumbnailPluginOutType type,
				guint64 mtime, 
				const gchar *uri, 
				GError **error)
{
	GdkPixbuf *pixbuf;
	gchar *large, *normal, *cropped, *filen;
	struct utimbuf buf;
	GError *nerror = NULL;

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
					       NULL, NULL, NULL, FALSE);

	switch (type) {
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE:
			filen = large;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL:
			filen = normal;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED:
			filen = cropped;
		break;
	}

	pixbuf = gdk_pixbuf_new_from_data ((const guchar*) rgb8_pixmap, 
					   GDK_COLORSPACE_RGB, has_alpha, 
					   bits_per_sample, width, height, rowstride,
					   NULL, NULL);

	gdk_pixbuf_save (pixbuf, filen, "jpeg", 
			 &nerror, NULL);

	g_object_unref (pixbuf);

	if (!nerror) {
#ifdef HAVE_SQLITE3
		gboolean create = FALSE;

		if (!db) {
			gchar *dbfile;
			dbfile = g_build_filename (g_get_home_dir (), ".thumbnails", 
					   "meta.db", NULL);
			create = !g_file_test (dbfile, G_FILE_TEST_EXISTS);
			sqlite3_open (dbfile, &db);
			g_free (dbfile);
		}

		if (db) {
			gchar *sql;
			if (create) {
				sqlite3_exec (db, "create table jpegthumbnails (Path, URI, MTime)" , 
					      callback, 0, NULL);
			}
			sql = g_strdup_printf ("delete from jpegthumbnails where Path = '%s'",
					       filen);
			sqlite3_exec (db, sql, callback, 0, NULL);
			g_free (sql);
			sql = g_strdup_printf ("insert into jpegthumbnails (Path, URI, MTime) values ('%s', '%s', %Lu)",
					       filen, uri, mtime);
			sqlite3_exec (db, sql, callback, 0, NULL);
			g_free (sql);
		}

#endif

		buf.actime = buf.modtime = mtime;
		utime (filen, &buf);
	} else
		g_propagate_error (error, nerror);


	g_free (normal);
	g_free (large);
	g_free (cropped);

	return;
}



static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		is_active = TRUE;
		g_key_file_free (keyfile);
		return;
	}

	is_active = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "IsActive", &error);

	if (error) {
		is_active = TRUE;
		g_error_free (error);
	}

	g_key_file_free (keyfile);
}


static void 
on_file_changed (GFileMonitor *monitor_, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
		gchar *config = g_file_get_path (file);
		reload_config (config);
		g_free (config);
	}
}

gboolean
hildon_thumbnail_outplugin_stop (void) 
{
	if (monitor)
		g_object_unref (monitor);
#ifdef HAVE_SQLITE3
	if (db)
		sqlite3_close (db);
	db = NULL;
#endif
	return FALSE;
}

gboolean
hildon_thumbnail_outplugin_is_active (void) 
{
	if (!had_init) {
		gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gdkpixbuf-jpeg-output-plugin.conf", NULL);
		GFile *file = g_file_new_for_path (config);

		monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

		g_signal_connect (G_OBJECT (monitor), "changed", 
				  G_CALLBACK (on_file_changed), NULL);

		/* g_object_unref (monitor); */
		g_object_unref (file);

		reload_config (config);

		g_free (config);
		had_init = TRUE;
	}

	return is_active;
}
