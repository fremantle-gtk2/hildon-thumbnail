/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <stdio.h>

#include <hildon-thumbnail-factory.h>

#include <unistd.h>

/*
void log_func(const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
    printf("*** %s: %s\n", log_domain, message);
}
*/

static gboolean cancel_check = FALSE;

void thumb_callback(HildonThumbnailFactoryHandle handle, gpointer user_data,
    GdkPixbuf *thumbnail, GError *error)
{
    g_message("Callback invoked, pixbuf=%p, error=%s", thumbnail,
              error ? error->message : "NULL");
              
    // Test running cancel
    if(!cancel_check) {
        printf("-- running_queue cancel test\n");
        hildon_thumbnail_factory_cancel(handle);
        cancel_check = TRUE;
    }
}

void thumb_callback_new (HildonThumbnailFactory *self, GdkPixbuf *thumbnail, GError *error, gpointer user_data)
{
    g_message("Callback invoked, pixbuf=%p, error=%s", thumbnail,
              error ? error->message : "NULL");             
}

gchar *to_uri(gchar *file) {
    gchar *str;
    gchar *cur = g_get_current_dir();
    gchar *filename = g_build_filename(cur, file, NULL);

    str = g_strconcat("file://", filename, NULL);

    g_free(filename);
    g_free(cur);

    return str;
}

// Works only when current directory == top build directory
void test_thumbs_old() {
    HildonThumbnailFactoryHandle h, h1, h2;
    gchar *uri1, *uri2, *uri3,*uri4;
    char *file1 = "tests/images/Debian.jpg";
    char *file2 = "tests/images/Splash-Debian.png";
    char *file3 = "tests/images/error-test.png";
    char *file4 = "tests/images/test.mp3";

    uri1 = to_uri(file1);
    uri2 = to_uri(file2);
    uri3 = to_uri(file3);
    uri4 = to_uri(file4);

    printf("--- Loading tests ---\n");

    h = hildon_thumbnail_factory_load(uri1, "image/png", 100, 100, thumb_callback,
        NULL);

    hildon_thumbnail_factory_cancel(h);

    h = hildon_thumbnail_factory_load(uri1, "image/png", 100, 100, thumb_callback,
        NULL);    
    
    h1 = hildon_thumbnail_factory_load(uri1, "image/png", 100, 100,
        thumb_callback, NULL);

    h2 = hildon_thumbnail_factory_load(uri2, "image/jpeg", 100, 100, thumb_callback,
        NULL);

    hildon_thumbnail_factory_load(uri4, "audio/mp3", 100, 100, thumb_callback,
        NULL);
        
    hildon_thumbnail_factory_cancel(h);
    
    printf("-- double free cancel test\n");
    // Test double-free check
    hildon_thumbnail_factory_cancel(h);

    hildon_thumbnail_factory_wait();

    printf("--- Loading error tests ---\n");

    // Error
    hildon_thumbnail_factory_load(uri3, "image/jpeg", 100, 100, thumb_callback,
        NULL);

    hildon_thumbnail_factory_wait();

    printf("--- Cache tests ---\n");

    // Repeat for cache
    h1 = hildon_thumbnail_factory_load(uri1, "image/png", 100, 100,
        thumb_callback, NULL);

    h2 = hildon_thumbnail_factory_load(uri2, "image/jpeg", 100, 100, thumb_callback,
        NULL);

    hildon_thumbnail_factory_load(uri4, "audio/mp3", 100, 100, thumb_callback,
        NULL);

    hildon_thumbnail_factory_wait();

    printf("--- Cache error tests ---\n");

    hildon_thumbnail_factory_load(uri3, "image/jpeg", 100, 100, thumb_callback,
        NULL);

    // Prevent races
    hildon_thumbnail_factory_wait();

    printf("--- Queue tests ---\n");
    // Queue functionality
    hildon_thumbnail_factory_move_front(h2);
    hildon_thumbnail_factory_move_front(h2);
    hildon_thumbnail_factory_move_front(h1);

    hildon_thumbnail_factory_move_front_all_from(h2);
    hildon_thumbnail_factory_move_front_all_from(h2);
    hildon_thumbnail_factory_move_front_all_from(h1);

    printf("--- Filemanager tests ---\n");
    // File management functionality
    rename(file1, file3);
    hildon_thumbnail_factory_move(uri1, uri3);
    rename(file3, file1);
    hildon_thumbnail_factory_move(uri3, uri1);
    link(file1, file3);
    hildon_thumbnail_factory_copy(uri1, uri3);
    unlink(file3);
    hildon_thumbnail_factory_remove(uri3);

    hildon_thumbnail_factory_wait();

    g_free(uri1);
    g_free(uri2);
    g_free(uri3);
    g_free(uri4);
}

void test_thumbs_new() {
    HildonThumbnailFactory *f;
    HildonThumbnailRequest *h1, *h2, *h3, *h4;

    gchar *uri1, *uri2, *uri3,*uri4;
    char *file1 = "tests/images/Debian.jpg";
    char *file2 = "tests/images/Splash-Debian.png";
    char *file3 = "tests/images/error-test.png";
    char *file4 = "tests/images/test.mp3";

    uri1 = to_uri(file1);
    uri2 = to_uri(file2);
    uri3 = to_uri(file3);
    uri4 = to_uri(file4);

    printf("--- Loading new tests ---\n \t%s\n\t%s\n\t%s\n\t%s\n",
		   	uri1, uri2, uri3, uri4);

    f = hildon_thumbnail_factory_get_instance ();

    h1 = hildon_thumbnail_factory_request_pixbuf (f, uri1, 
		100, 100, FALSE, "image/png", thumb_callback_new, 
		NULL, NULL);
    h2 = hildon_thumbnail_factory_request_pixbuf (f, uri2, 
		100, 100, FALSE,"image/png", thumb_callback_new, 
		NULL, NULL);
    h3 = hildon_thumbnail_factory_request_pixbuf (f, uri3, 
		100, 100, TRUE, "image/png", thumb_callback_new, 
		NULL, NULL);
    h4 = hildon_thumbnail_factory_request_pixbuf (f, uri4, 
		100, 100, FALSE, "image/png", thumb_callback_new, 
		NULL, NULL);
          


    hildon_thumbnail_factory_join (f);

	g_object_unref (h1);
    g_object_unref (h2);
    g_object_unref (h3);
    g_object_unref (h4);

	g_object_unref (f);

    printf("--- Filemanager tests ---\n");
    // File management functionality
    rename(file1, file3);
    hildon_thumbnail_factory_move(uri1, uri3);
    rename(file3, file1);
    hildon_thumbnail_factory_move(uri3, uri1);
    link(file1, file3);
    hildon_thumbnail_factory_copy(uri1, uri3);
    unlink(file3);
    hildon_thumbnail_factory_remove(uri3);

    g_free(uri1);
    g_free(uri2);
    g_free(uri3);
    g_free(uri4);
}


void test_clean()
{
    printf("--- Clean test ---\n");
    // Delete all
    hildon_thumbnail_factory_clean_cache(0, 0);
}

int main() {
    printf("Running tests...\n");

    g_type_init ();

    if(!g_file_test("tests/images", G_FILE_TEST_IS_DIR)) {
        g_error("Tester can't find test images in tests/images directory");

        return 1;
    }

	GMainLoop *loop = g_main_loop_new (NULL, FALSE);

    hildon_thumbnail_factory_set_debug(TRUE);

    /*
    g_log_set_handler (NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, log_func, NULL);

    g_log_set_default_handler(log_func, NULL);
    */

    g_message("Message test");
    g_warning("Warning test");

    test_clean();
    test_thumbs_old();

    test_clean();
    test_thumbs_new();

	g_timeout_add_seconds (10, (GSourceFunc) g_main_loop_quit, loop);

	g_main_loop_run (loop);

	g_main_loop_unref (loop);

    printf("Done!\n");
    return 0;
}
