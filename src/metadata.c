/*  
    This file is part of Restraint.

    Restraint is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Restraint is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Restraint.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "metadata.h"
#include "process.h"
#include "errors.h"
#include "utils.h"

void
restraint_metadata_free (MetaData *metadata)
{
    if (metadata) {
        g_free(metadata->name);
        g_free(metadata->entry_point);
        g_slist_free_full (metadata->dependencies, g_free);
        g_slist_free_full (metadata->repodeps, g_free);
        g_slice_free (MetaData, metadata);
    }
}

MetaData *
restraint_parse_metadata (gchar *filename,
                          gchar *locale,
                          GError **error)
{
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    GKeyFile *keyfile;
    GKeyFileFlags flags;
    GError *tmp_error = NULL;
    MetaData *metadata = g_slice_new0 (MetaData);

    /* Create a new GKeyFile object and a bitwise list of flags. */
    keyfile = g_key_file_new();
    /* This is not really needed since I don't envision writing the file back out */
    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;

    /* Load the GKeyFile from metadata or return. */
    if (!g_key_file_load_from_file(keyfile, filename, flags, &tmp_error)) {
        g_propagate_error(error, tmp_error);
        goto error;
    }

    metadata->name = g_strstrip(g_key_file_get_locale_string(keyfile,
                                                      "General",
                                                      "name",
                                                      locale,
                                                      &tmp_error));
    if (tmp_error != NULL && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);

    gchar *key_entry_point = g_key_file_get_locale_string(keyfile,
                                                      "restraint",
                                                      "entry_point",
                                                      locale,
                                                      &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);
    if (key_entry_point != NULL) {
        metadata->entry_point = key_entry_point;
    }

    gchar *max_time = g_key_file_get_locale_string (keyfile,
                                                    "restraint",
                                                    "max_time",
                                                    locale,
                                                    &tmp_error);
    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);
    if (max_time != NULL) {
        gint64 time = parse_time_string(max_time, &tmp_error);
        g_free(max_time);
        if (tmp_error) {
            g_propagate_error(error, tmp_error);
            goto error;
        }
        // If max_time is set it's because we read it from our run data
        metadata->max_time = time;
    }

    gsize length;
    gchar **dependencies = g_key_file_get_locale_string_list(keyfile,
                                                            "restraint",
                                                            "dependencies",
                                                            locale,
                                                            &length,
                                                            &tmp_error);
    gchar **dependency = dependencies;
    if (dependency) {
      while (*dependency) {
        metadata->dependencies = g_slist_prepend (metadata->dependencies, g_strdup(*dependency));
        dependency++;
      }
      g_strfreev (dependencies);
    }
    else
      metadata->dependencies = NULL;

    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);

    gchar **repodeps = g_key_file_get_locale_string_list(keyfile,
                                                         "restraint",
                                                         "repoRequires",
                                                         locale,
                                                         &length,
                                                         &tmp_error);
    gchar **repodep = repodeps;
    if (repodep) {
      while (*repodep) {
        metadata->repodeps = g_slist_prepend (metadata->repodeps, g_strdup(*repodep));
        repodep++;
      }
      g_strfreev (repodeps);
    }
    else
      metadata->repodeps = NULL;

    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);

    metadata->nolocalwatchdog = g_key_file_get_boolean (keyfile,
                                                    "restraint",
                                                    "no_localwatchdog",
                                                    &tmp_error);

    if (tmp_error && tmp_error->code != G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
        g_propagate_error(error, tmp_error);
        goto error;
    }
    g_clear_error (&tmp_error);

    g_key_file_free(keyfile);

    return metadata;

error:
    restraint_metadata_free(metadata);
    g_key_file_free(keyfile);
    return NULL;
}

static void parse_line(MetaData *metadata,
                       const gchar *line,
                       gsize length,
                       GError **error) {

    g_return_if_fail(metadata != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *tmp_error = NULL;
    gchar **key_value = g_strsplit (line,":",2);
    gchar *key = g_ascii_strup(g_strstrip(key_value[0]), -1);
    gchar *value = g_strdup(g_strstrip(key_value[1]));
    g_strfreev(key_value);

    if (g_strcmp0("TESTTIME",key) == 0) {
        guint64 time = parse_time_string(value, &tmp_error);
        if (tmp_error) {
            g_free(key);
            g_free(value);
            g_propagate_error(error, tmp_error);
            return;
        }
        metadata->max_time = time;
    } else if(g_strcmp0("NAME", key) == 0) {
        metadata->name = g_strdup(g_strstrip(value));
    } else if(g_strcmp0("REQUIRES", key) == 0) {
        gchar **dependencies = g_strsplit_set (value,", ", -1);
        gchar **dependency = dependencies;
        while (*dependency) {
            if (g_strcmp0 (*dependency, "") != 0) {
                metadata->dependencies = g_slist_prepend (metadata->dependencies, g_strdup(*dependency));
            }
            dependency++;
        }
        // We only want to free the array not the values that it's pointing to
        g_strfreev (dependencies);
    } else  if (g_strcmp0("REPOREQUIRES", key) == 0) {
        metadata->repodeps = g_slist_prepend(metadata->repodeps,
                                             g_strndup(value, strlen(value)));
    }
    g_free(key);
    g_free(value);
}

static void flush_parse_buffer(MetaData *metadata, GString *parse_buffer, GError **error) {
    g_return_if_fail(metadata != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *tmp_error = NULL;

    if (parse_buffer->len > 0) {
        parse_line(metadata, parse_buffer->str, parse_buffer->len, &tmp_error);
        g_string_erase(parse_buffer, 0, -1);
        if (tmp_error) {
            g_propagate_error(error, tmp_error);
            return;
        }
    }
}

static void file_parse_data(MetaData *metadata,
                       GString *parse_buffer,
                       const gchar *data,
                       gsize length,
                       GError **error) {
    g_return_if_fail(metadata != NULL);
    g_return_if_fail(error == NULL || *error == NULL);

    GError *parse_error;
    gsize i;

    g_return_if_fail(data != NULL || length == 0);

    parse_error = NULL;
    i = 0;
    while (i < length) {
        if (data[i] == '\n') {
            /* remove old style ascii termintaor */
            if (parse_buffer->len > 0
                && (parse_buffer->str[parse_buffer->len - 1]
                    == '\r'))
                g_string_erase(parse_buffer, parse_buffer->len - 1, 1);
            /* When a newline is encountered flush the parse buffer so that the
             * line can be parsed.  Completely blank lines are skipped.
             */
            if (parse_buffer->len > 0)
                flush_parse_buffer(metadata, parse_buffer, &parse_error);
            if (parse_error) {
                g_propagate_error(error, parse_error);
                return;
            }
            i++;
        } else {
            const gchar *start_of_line;
            const gchar *end_of_line;
            gsize line_length;

            start_of_line = data + i;
            end_of_line = memchr(start_of_line, '\n', length - i);

            if (end_of_line == NULL)
                end_of_line = data + length;

            line_length = end_of_line - start_of_line;

            g_string_append_len(parse_buffer, start_of_line, line_length);
            i += line_length;
        }
    }
}

static gboolean parse_testinfo_from_fd(MetaData *metadata,
                                       gint fd,
                                       GError **error) {
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    GError *tmp_error = NULL;
    gssize bytes_read;
    struct stat stat_buf;
    gchar read_buf[4096];
    GString *parse_buffer = g_string_sized_new(128);

    if (fstat(fd, &stat_buf) < 0) {
        g_set_error_literal(error, G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            g_strerror(errno));
        return FALSE;
    }

    if (!S_ISREG (stat_buf.st_mode)) {
        g_set_error_literal (error, G_FILE_ERROR,
                             RESTRAINT_OPEN,
                             "Not a regular file");
      g_string_free(parse_buffer, TRUE);
      return FALSE;
    }

    do {
        bytes_read = read(fd, read_buf, 4096);

        if (bytes_read == 0) /* End of File */
            break;

        if (bytes_read < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;

            g_set_error_literal(error, G_FILE_ERROR,
                                g_file_error_from_errno(errno),
                                g_strerror(errno));
            g_string_free(parse_buffer, TRUE);
            return FALSE;
        }
        file_parse_data(metadata,
                        parse_buffer,
                        read_buf,
                        bytes_read,
                        &tmp_error);
    } while (!tmp_error);

    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        g_string_free(parse_buffer, TRUE);
        return FALSE;
    }

    flush_parse_buffer(metadata, parse_buffer, &tmp_error);
    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        g_string_free(parse_buffer, TRUE);
        return FALSE;
    }
    g_string_free(parse_buffer, TRUE);
    return TRUE;
}

MetaData *
restraint_parse_testinfo(gchar *filename,
                         GError **error)
{
    g_return_val_if_fail(filename != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GError *tmp_error = NULL;
    gint fd;

    fd = g_open(filename, O_RDONLY, 0);

    if (fd == -1) {
        g_set_error_literal(error, G_FILE_ERROR,
                            g_file_error_from_errno(errno),
                            g_strerror(errno));
        return NULL;
    }

    MetaData *metadata = g_slice_new0(MetaData);
    parse_testinfo_from_fd(metadata, fd, &tmp_error);
    close(fd);

    if (tmp_error) {
        g_propagate_error(error, tmp_error);
        restraint_metadata_free(metadata);
        return NULL;
    }
    return metadata;
}
