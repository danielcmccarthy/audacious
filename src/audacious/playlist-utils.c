/*
 * playlist-utils.c
 * Copyright 2009-2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <dirent.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/audstrings.h>
#include <libaudcore/hook.h>

#include "misc.h"
#include "playlist.h"

static const char * get_basename (const char * filename)
{
    const char * slash = strrchr (filename, '/');

    return (slash == NULL) ? filename : slash + 1;
}

static int filename_compare_basename (const char * a, const char * b)
{
    return str_compare_encoded (get_basename (a), get_basename (b));
}

static int tuple_compare_string (const Tuple * a, const Tuple * b, int field)
{
    char * string_a = tuple_get_str (a, field);
    char * string_b = tuple_get_str (b, field);
    int ret;

    if (string_a == NULL)
        ret = (string_b == NULL) ? 0 : -1;
    else if (string_b == NULL)
        ret = 1;
    else
        ret = str_compare (string_a, string_b);

    str_unref (string_a);
    str_unref (string_b);
    return ret;
}

static int tuple_compare_int (const Tuple * a, const Tuple * b, int field)
{
    if (tuple_get_value_type (a, field) != TUPLE_INT)
        return (tuple_get_value_type (b, field) != TUPLE_INT) ? 0 : -1;
    if (tuple_get_value_type (b, field) != TUPLE_INT)
        return 1;

    int int_a = tuple_get_int (a, field);
    int int_b = tuple_get_int (b, field);

    return (int_a < int_b) ? -1 : (int_a > int_b);
}

static int tuple_compare_title (const Tuple * a, const Tuple * b)
{
    return tuple_compare_string (a, b, FIELD_TITLE);
}

static int tuple_compare_album (const Tuple * a, const Tuple * b)
{
    return tuple_compare_string (a, b, FIELD_ALBUM);
}

static int tuple_compare_artist (const Tuple * a, const Tuple * b)
{
    return tuple_compare_string (a, b, FIELD_ARTIST);
}

static int tuple_compare_date (const Tuple * a, const Tuple * b)
{
    return tuple_compare_int (a, b, FIELD_YEAR);
}

static int tuple_compare_track (const Tuple * a, const Tuple * b)
{
    return tuple_compare_int (a, b, FIELD_TRACK_NUMBER);
}

static int tuple_compare_length (const Tuple * a, const Tuple * b)
{
    return tuple_compare_int (a, b, FIELD_LENGTH);
}

static const PlaylistStringCompareFunc filename_comparisons[] = {
 [PLAYLIST_SORT_PATH] = str_compare_encoded,
 [PLAYLIST_SORT_FILENAME] = filename_compare_basename,
 [PLAYLIST_SORT_TITLE] = NULL,
 [PLAYLIST_SORT_ALBUM] = NULL,
 [PLAYLIST_SORT_ARTIST] = NULL,
 [PLAYLIST_SORT_DATE] = NULL,
 [PLAYLIST_SORT_TRACK] = NULL,
 [PLAYLIST_SORT_FORMATTED_TITLE] = NULL,
 [PLAYLIST_SORT_LENGTH] = NULL};

static const PlaylistTupleCompareFunc tuple_comparisons[] = {
 [PLAYLIST_SORT_PATH] = NULL,
 [PLAYLIST_SORT_FILENAME] = NULL,
 [PLAYLIST_SORT_TITLE] = tuple_compare_title,
 [PLAYLIST_SORT_ALBUM] = tuple_compare_album,
 [PLAYLIST_SORT_ARTIST] = tuple_compare_artist,
 [PLAYLIST_SORT_DATE] = tuple_compare_date,
 [PLAYLIST_SORT_TRACK] = tuple_compare_track,
 [PLAYLIST_SORT_FORMATTED_TITLE] = NULL,
 [PLAYLIST_SORT_LENGTH] = tuple_compare_length};

static const PlaylistStringCompareFunc title_comparisons[] = {
 [PLAYLIST_SORT_PATH] = NULL,
 [PLAYLIST_SORT_FILENAME] = NULL,
 [PLAYLIST_SORT_TITLE] = NULL,
 [PLAYLIST_SORT_ALBUM] = NULL,
 [PLAYLIST_SORT_ARTIST] = NULL,
 [PLAYLIST_SORT_DATE] = NULL,
 [PLAYLIST_SORT_TRACK] = NULL,
 [PLAYLIST_SORT_FORMATTED_TITLE] = str_compare,
 [PLAYLIST_SORT_LENGTH] = NULL};

void playlist_sort_by_scheme (int playlist, int scheme)
{
    if (filename_comparisons[scheme] != NULL)
        playlist_sort_by_filename (playlist, filename_comparisons[scheme]);
    else if (tuple_comparisons[scheme] != NULL)
        playlist_sort_by_tuple (playlist, tuple_comparisons[scheme]);
    else if (title_comparisons[scheme] != NULL)
        playlist_sort_by_title (playlist, title_comparisons[scheme]);
}

void playlist_sort_selected_by_scheme (int playlist, int scheme)
{
    if (filename_comparisons[scheme] != NULL)
        playlist_sort_selected_by_filename (playlist,
         filename_comparisons[scheme]);
    else if (tuple_comparisons[scheme] != NULL)
        playlist_sort_selected_by_tuple (playlist, tuple_comparisons[scheme]);
    else if (title_comparisons[scheme] != NULL)
        playlist_sort_selected_by_title (playlist, title_comparisons[scheme]);
}

/* Fix me:  This considers empty fields as duplicates. */
void playlist_remove_duplicates_by_scheme (int playlist, int scheme)
{
    int entries = playlist_entry_count (playlist);
    int count;

    if (entries < 1)
        return;

    playlist_select_all (playlist, FALSE);

    if (filename_comparisons[scheme] != NULL)
    {
        int (* compare) (const char * a, const char * b) =
         filename_comparisons[scheme];

        playlist_sort_by_filename (playlist, compare);
        char * last = playlist_entry_get_filename (playlist, 0);

        for (count = 1; count < entries; count ++)
        {
            char * current = playlist_entry_get_filename (playlist, count);

            if (compare (last, current) == 0)
                playlist_entry_set_selected (playlist, count, TRUE);

            str_unref (last);
            last = current;
        }

        str_unref (last);
    }
    else if (tuple_comparisons[scheme] != NULL)
    {
        int (* compare) (const Tuple * a, const Tuple * b) =
         tuple_comparisons[scheme];

        playlist_sort_by_tuple (playlist, compare);
        Tuple * last = playlist_entry_get_tuple (playlist, 0, FALSE);

        for (count = 1; count < entries; count ++)
        {
            Tuple * current = playlist_entry_get_tuple (playlist, count, FALSE);

            if (last != NULL && current != NULL && compare (last, current) == 0)
                playlist_entry_set_selected (playlist, count, TRUE);

            if (last)
                tuple_unref (last);
            last = current;
        }

        if (last)
            tuple_unref (last);
    }

    playlist_delete_selected (playlist);
}

void playlist_remove_failed (int playlist)
{
    int entries = playlist_entry_count (playlist);
    int count;

    playlist_select_all (playlist, FALSE);

    for (count = 0; count < entries; count ++)
    {
        char * filename = playlist_entry_get_filename (playlist, count);

        /* vfs_file_test() only works for file:// URIs currently */
        if (! strncmp (filename, "file://", 7) && ! vfs_file_test (filename,
         G_FILE_TEST_EXISTS))
            playlist_entry_set_selected (playlist, count, TRUE);

        str_unref (filename);
    }

    playlist_delete_selected (playlist);
}

void playlist_select_by_patterns (int playlist, const Tuple * patterns)
{
    const int fields[] = {FIELD_TITLE, FIELD_ALBUM, FIELD_ARTIST,
     FIELD_FILE_NAME};

    int entries = playlist_entry_count (playlist);
    int field, entry;

    playlist_select_all (playlist, TRUE);

    for (field = 0; field < ARRAY_LEN (fields); field ++)
    {
        char * pattern = tuple_get_str (patterns, fields[field]);
        GRegex * regex;

        if (! pattern || ! pattern[0] || ! (regex = g_regex_new (pattern,
         G_REGEX_CASELESS, 0, NULL)))
        {
            str_unref (pattern);
            continue;
        }

        for (entry = 0; entry < entries; entry ++)
        {
            if (! playlist_entry_get_selected (playlist, entry))
                continue;

            Tuple * tuple = playlist_entry_get_tuple (playlist, entry, FALSE);
            char * string = tuple ? tuple_get_str (tuple, fields[field]) : NULL;

            if (! string || ! g_regex_match (regex, string, 0, NULL))
                playlist_entry_set_selected (playlist, entry, FALSE);

            str_unref (string);
            if (tuple)
                tuple_unref (tuple);
        }

        g_regex_unref (regex);
        str_unref (pattern);
    }
}

static char * make_playlist_path (int playlist)
{
    if (! playlist)
        return filename_build (get_path (AUD_PATH_USER_DIR), "playlist.xspf");

    SPRINTF (name, "playlist_%02d.xspf", 1 + playlist);
    return filename_build (get_path (AUD_PATH_PLAYLISTS_DIR), name);
}

static void load_playlists_real (void)
{
    const char * folder = get_path (AUD_PATH_PLAYLISTS_DIR);

    /* old (v3.1 and earlier) naming scheme */

    int count;
    for (count = 0; ; count ++)
    {
        char * path = make_playlist_path (count);

        if (! g_file_test (path, G_FILE_TEST_EXISTS))
        {
            str_unref (path);
            break;
        }

        char * uri = filename_to_uri (path);

        playlist_insert (count);
        playlist_insert_playlist_raw (count, 0, uri);
        playlist_set_modified (count, TRUE);

        str_unref (path);
        str_unref (uri);
    }

    /* unique ID-based naming scheme */

    char * order_path = filename_build (folder, "order");
    char * order_string;
    g_file_get_contents (order_path, & order_string, NULL, NULL);
    str_unref (order_path);

    if (! order_string)
        goto DONE;

    Index * order = str_list_to_index (order_string, " ");
    g_free (order_string);

    for (int i = 0; i < index_count (order); i ++)
    {
        char * number = index_get (order, i);

        SCONCAT2 (name, number, ".audpl");
        char * path = filename_build (folder, name);

        if (! g_file_test (path, G_FILE_TEST_EXISTS))
        {
            str_unref (path);

            SCONCAT2 (name2, number, ".xspf");
            path = filename_build (folder, name2);
        }

        char * uri = filename_to_uri (path);

        playlist_insert_with_id (count + i, atoi (number));
        playlist_insert_playlist_raw (count + i, 0, uri);
        playlist_set_modified (count + i, FALSE);

        if (g_str_has_suffix (path, ".xspf"))
            playlist_set_modified (count + i, TRUE);

        str_unref (path);
        str_unref (uri);
    }

    index_free_full (order, (IndexFreeFunc) str_unref);

DONE:
    if (! playlist_count ())
        playlist_insert (0);

    playlist_set_active (0);
}

static void save_playlists_real (void)
{
    int lists = playlist_count ();
    const char * folder = get_path (AUD_PATH_PLAYLISTS_DIR);

    /* save playlists */

    Index * order = index_new ();
    GHashTable * saved = g_hash_table_new_full (g_str_hash, g_str_equal,
     (GDestroyNotify) str_unref, NULL);

    for (int i = 0; i < lists; i ++)
    {
        int id = playlist_get_unique_id (i);
        char * number = int_to_str (id);

        SCONCAT2 (name, number, ".audpl");

        if (playlist_get_modified (i))
        {
            char * path = filename_build (folder, name);
            char * uri = filename_to_uri (path);

            playlist_save (i, uri);
            playlist_set_modified (i, FALSE);

            str_unref (path);
            str_unref (uri);
        }

        index_insert (order, -1, number);
        g_hash_table_insert (saved, str_get (name), NULL);
    }

    char * order_string = index_to_str_list (order, " ");
    index_free_full (order, (IndexFreeFunc) str_unref);

    GError * error = NULL;
    char * order_path = filename_build (folder, "order");

    char * old_order_string;
    g_file_get_contents (order_path, & old_order_string, NULL, NULL);

    if (! old_order_string || strcmp (old_order_string, order_string))
    {
        if (! g_file_set_contents (order_path, order_string, -1, & error))
        {
            fprintf (stderr, "Cannot write to %s: %s\n", order_path, error->message);
            g_error_free (error);
        }
    }

    str_unref (order_string);
    str_unref (order_path);
    g_free (old_order_string);

    /* clean up deleted playlists and files from old naming scheme */

    char * path = make_playlist_path (0);
    remove (path);
    str_unref (path);

    DIR * dir = opendir (folder);
    if (! dir)
        goto DONE;

    struct dirent * entry;
    while ((entry = readdir (dir)))
    {
        if (! g_str_has_suffix (entry->d_name, ".audpl")
         && ! g_str_has_suffix (entry->d_name, ".xspf"))
            continue;

        if (! g_hash_table_contains (saved, entry->d_name))
        {
            char * path = filename_build (folder, entry->d_name);
            remove (path);
            str_unref (path);
        }
    }

    closedir (dir);

DONE:
    g_hash_table_destroy (saved);
}

static bool_t hooks_added, state_changed;

static void update_cb (void * data, void * user)
{
    if (GPOINTER_TO_INT (data) < PLAYLIST_UPDATE_METADATA)
        return;

    state_changed = TRUE;
}

static void state_cb (void * data, void * user)
{
    state_changed = TRUE;
}

void load_playlists (void)
{
    load_playlists_real ();
    playlist_load_state ();

    state_changed = FALSE;

    if (! hooks_added)
    {
        hook_associate ("playlist update", update_cb, NULL);
        hook_associate ("playlist activate", state_cb, NULL);
        hook_associate ("playlist position", state_cb, NULL);

        hooks_added = TRUE;
    }
}

void save_playlists (bool_t exiting)
{
    save_playlists_real ();

    /* on exit, save resume states */
    if (state_changed || exiting)
    {
        playlist_save_state ();
        state_changed = FALSE;
    }

    if (exiting && hooks_added)
    {
        hook_dissociate ("playlist update", update_cb);
        hook_dissociate ("playlist activate", state_cb);
        hook_dissociate ("playlist position", state_cb);

        hooks_added = FALSE;
    }
}
