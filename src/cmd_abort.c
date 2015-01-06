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

#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libsoup/soup.h>
#include "upload.h"
#include "utils.h"
#include "errors.h"


int main(int argc, char *argv[]) {
    SoupSession *session;
    GError *error = NULL;

    gchar *server = NULL;
    gchar *abrt_url = NULL;
    SoupURI *server_uri = NULL;
    guint ret = 0;

    GOptionEntry entries[] = {
        {"server", 's', 0, G_OPTION_ARG_STRING, &server,
            "Server to connect to", "URL" },
        { NULL }
    };
    GOptionContext *context = g_option_context_new(NULL);
    g_option_context_set_summary(context,
            "Aborts currently running task.\n");
    g_option_context_add_main_entries(context, entries, NULL);
    gboolean parse_succeeded = g_option_context_parse(context, &argc, &argv, &error);

    if (!parse_succeeded) {
        goto cleanup;
    }

    if (!server) {
        cmd_usage(context);
        goto cleanup;
    }

    abrt_url = g_strdup_printf ("%s/abort", server);
    server_uri = soup_uri_new(abrt_url);
    if (!server_uri) {
        g_set_error (&error, RESTRAINT_ERROR,
                     RESTRAINT_PARSE_ERROR_BAD_SYNTAX,
                     "Malformed server url: %s", server);
        goto cleanup;
    }
    session = soup_session_new_with_options("timeout", 3600, NULL);
    SoupMessage *msg = soup_message_new_from_uri("GET", server_uri);
    ret = soup_session_send_message(session, msg);
    if (!SOUP_STATUS_IS_SUCCESSFUL(ret)) {
        g_warning ("Failed to abort job, status: %d Message: %s\n", ret,
                   msg->reason_phrase);
    }

    g_object_unref(msg);
    soup_uri_free(server_uri);
    soup_session_abort(session);
    g_object_unref(session);

cleanup:
    g_option_context_free(context);
    if (server_uri != NULL) {
        soup_uri_free (server_uri);
    }

    if (server != NULL) {
        g_free(server);
    }
    if (error) {
        int retcode = error->code;
        g_printerr("%s [%s, %d]\n", error->message,
                g_quark_to_string(error->domain), error->code);
        g_clear_error(&error);
        return retcode;
    } else {
        return EXIT_SUCCESS;
    }
}
