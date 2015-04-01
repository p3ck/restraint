#include <string.h>
#include <glib.h>
#include <gio/gunixinputstream.h>
#include <libsoup/soup.h>
#include "pipe.h"
#include "multipart.h"
#include "errors.h"

static void next_part_cb (GObject *source, GAsyncResult *async_result, gpointer user_data);
// Taken from libsoup
static char *
generate_boundary (void)
{
        static int counter;
        struct {
                GTimeVal timeval;
                int counter;
        } data;

        /* avoid valgrind warning */
        if (sizeof (data) != sizeof (data.timeval) + sizeof (data.counter))
                memset (&data, 0, sizeof (data));

        g_get_current_time (&data.timeval);
        data.counter = counter++;

        /* The maximum boundary string length is 69 characters, and a
         * stringified SHA256 checksum is 64 bytes long.
         */
        return g_compute_checksum_for_data (G_CHECKSUM_SHA256,
                                            (const guchar *)&data,
                                            sizeof (data));
}

static void
multipart_destroy (MultiPartData *multipart_data)
{
    //GError *error = NULL;
    //g_print ("multipart_destroy: Enter\n");
    if (multipart_data->destroy) {
        multipart_data->destroy (multipart_data->error,
                                 multipart_data->user_data);
    }
    g_clear_error (&multipart_data->error);
    g_slice_free (MultiPartData, multipart_data);
    //g_print ("multipart_destroy: Exit\n");
}

static void
close_cb (GObject *source,
          GAsyncResult *async_result,
          gpointer user_data)
{
    GError *error = NULL;
    g_input_stream_close_finish (G_INPUT_STREAM (source), async_result, &error);
    if (error) {
        g_printerr("close_cb: %s [%s, %d]\n", error->message,
                   g_quark_to_string(error->domain),
                   error->code);
        g_error_free (error);
    }
}

static void
close_base_cb (GObject *source,
          GAsyncResult *async_result,
          gpointer user_data)
{
    GError *error = NULL;
    g_input_stream_close_finish (G_INPUT_STREAM (source), async_result, &error);
    if (error) {
        g_printerr("close_base_cb: %s [%s, %d]\n", error->message,
                   g_quark_to_string(error->domain),
                   error->code);
        g_error_free (error);
    }
}

/*
 *  * Read from g_input_stream until we get 0 bytes read.  Then process
 *   * using the value of stream_type.  Finally try and read another multipart.
 *    */
static void
read_cb (GObject *source, GAsyncResult *async_result, gpointer user_data)
{
    //g_print ("read_cb: Enter\n");
    GInputStream *in = G_INPUT_STREAM (source);
    MultiPartData *multipart_data = (MultiPartData *) user_data;
    SoupMultipartInputStream *multipart = SOUP_MULTIPART_INPUT_STREAM (multipart_data->multipart);
    gssize bytes_read;

    bytes_read = g_input_stream_read_finish (in, async_result, &multipart_data->error);

    /* Read 0 bytes - try to start reading another part. */
    if (bytes_read <= 0) {
        g_input_stream_close_async (in,
                                    G_PRIORITY_DEFAULT,
                                    multipart_data->cancellable,
                                    close_cb,
                                    user_data);
        if (multipart_data->callback) {
            SoupBuffer *soup_buffer;
            //g_print ("callback\n");
            soup_buffer = soup_buffer_new(SOUP_MEMORY_TEMPORARY,
                                (guchar *) multipart_data->buffer->str,
                                multipart_data->buffer->len);
            multipart_data->callback (multipart_data->method,
                                      multipart_data->path,
                                      multipart_data->cancellable,
                                      multipart_data->error,
                                      multipart_data->headers,
                                      soup_buffer,
                                      multipart_data->user_data);
            soup_buffer_free(soup_buffer);
        }
        g_string_free (multipart_data->buffer, TRUE);
        if (multipart_data->error) {
            g_input_stream_close_async (G_INPUT_STREAM (multipart),
                                        G_PRIORITY_DEFAULT,
                                        multipart_data->cancellable,
                                        close_base_cb,
                                        user_data);
            return;
        }
        soup_multipart_input_stream_next_part_async (multipart_data->multipart,
                                                     G_PRIORITY_DEFAULT,
                                                     multipart_data->cancellable,
                                                     next_part_cb,
                                                     user_data);
        return;
    }
    multipart_data->buffer = g_string_append_len (multipart_data->buffer, multipart_data->read_buffer, bytes_read);
    g_input_stream_read_async (in,
                               multipart_data->read_buffer,
                               READ_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT,
                               multipart_data->cancellable,
                               read_cb,
                               user_data);
    //g_print ("read_cb: Exit\n");
}

/*
 *  * We have two headeers we care about, Content-ID which we use as steam_type and
 *   * Status which we read when stream_type equals STREAM_ERROR.
 *    */
static void
multipart_read_headers (MultiPartData *multipart_data)
{
    multipart_data->headers = soup_multipart_input_stream_get_headers (multipart_data->multipart);
    if (multipart_data->headers) {
        multipart_data->method = soup_message_headers_get_one (multipart_data->headers, "rstrnt-method");
        multipart_data->path = soup_message_headers_get_one (multipart_data->headers, "rstrnt-path");
    }
}

/*
 * Try and read another multipart message. if in is NULL then there are no more
 * messages to read.
 */
static void
next_part_cb (GObject *source, GAsyncResult *async_result, gpointer user_data)
{
    //g_print ("next_part_cb: Enter\n");
    SoupMultipartInputStream *multipart = SOUP_MULTIPART_INPUT_STREAM (source);

    MultiPartData *multipart_data = (MultiPartData *) user_data;

    g_assert (SOUP_MULTIPART_INPUT_STREAM (source) == multipart_data->multipart);

    GInputStream *in = soup_multipart_input_stream_next_part_finish (multipart,
                                                       async_result,
                                                       &multipart_data->error);
    if (!in) {
        g_input_stream_close_async (G_INPUT_STREAM (multipart),
                                    G_PRIORITY_DEFAULT,
                                    multipart_data->cancellable,
                                    close_base_cb,
                                    user_data);
        return;
    }

    // Read the headers here.
    multipart_read_headers (multipart_data);

    multipart_data->buffer = g_string_sized_new(READ_BUFFER_SIZE);

    g_input_stream_read_async (in,
                               multipart_data->read_buffer,
                               READ_BUFFER_SIZE,
                               G_PRIORITY_DEFAULT,
                               multipart_data->cancellable,
                               read_cb,
                               user_data);
    //g_print ("next_part_cb: Exit\n");
}

void
multipart_handler_async (SoupMessage *msg,
                         gint f_in,
                         MultiPartData *multipart_data)
{
    GInputStream *in = g_unix_input_stream_new (f_in, TRUE /* close_fd */);
    multipart_data->multipart = soup_multipart_input_stream_new (msg,
                                                                 in);
    g_object_unref (in);

    soup_multipart_input_stream_next_part_async (multipart_data->multipart,
                                                 G_PRIORITY_DEFAULT,
                                                 multipart_data->cancellable,
                                                 next_part_cb,
                                                 multipart_data);
}

static void
post_data (gint fd, const gchar *remote_path, const gchar *data, gsize data_len)
{
    gchar *boundary = generate_boundary();
    gchar *header = g_strdup_printf ("POST %s HTTP/1.1\r\nContent-Length: %zu\r\nContent-Type: multipart/x-mixed-replace; boundary=--%s\r\n\r\n", remote_path, data_len + strlen(boundary) + 3, boundary);
    write (fd, header, strlen(header));
    write (fd, data, data_len);
    write (fd, "--", 2);
    write (fd, boundary, strlen(boundary));
    write (fd, "\n", 1);
}

static gboolean
handshake_io_cb (GIOChannel *io, GIOCondition condition, gpointer user_data)
{
    HandshakeData *handshake_data = (HandshakeData *) user_data;
    GError *tmp_error = NULL;
    gchar *s = NULL;
    gsize length = 0;
    gsize terminator_pos = 0;
    if (condition & G_IO_IN) {
        switch (g_io_channel_read_line (io, &s, &length, &terminator_pos, &tmp_error)) {
            case G_IO_STATUS_NORMAL:
                switch (handshake_data->state) {
                    case HANDSHAKE:
                        // Look for the handshake first before posting job
                        // User-Agent: restraint/11
                        // We might want to check the protocol number in the future
                        if (g_str_has_prefix (s, "User-Agent: restraint/")) {
                            post_data(handshake_data->f_out, handshake_data->remote_path,
                                      handshake_data->data, handshake_data->data_len);
                            handshake_data->state = READ_HEADER;
                        }
                        break;
                    case READ_HEADER:
                        // Read response and setup multi-part handling
                        if (terminator_pos == 0) {
                            soup_headers_parse_response (handshake_data->response_str->str,
                                                         handshake_data->response_str->len,
                                                         handshake_data->msg->response_headers,
                                                         NULL,
                                                         &handshake_data->msg->status_code,
                                                         &handshake_data->msg->reason_phrase);
                            handshake_data->state = RUN;
                            multipart_handler_async (handshake_data->msg,
                                                     handshake_data->f_in,
                                                     handshake_data->multipart_data);
                            // remove this I/O handler
                            return FALSE;
                        } else {
                            g_string_append_len (handshake_data->response_str, s, length);
                        }
                        break;
                    default:
                        // we shouldn't get here..
                        g_print ("client:%s", s);
                        break;
                }
                return TRUE;
            case G_IO_STATUS_ERROR:
                g_printerr("IO error: %s\n", tmp_error->message);
                g_clear_error (&tmp_error);
                return FALSE;
            case G_IO_STATUS_EOF:
                g_print ("finished!\n");
                return FALSE;
            case G_IO_STATUS_AGAIN:
                g_warning ("Not ready.. try again.");
                return TRUE;
            default:
                g_return_val_if_reached (FALSE);
                break;
        }
    }
    return FALSE;
}

void
handshake_pid_callback (GPid pid, gint status, gpointer user_data)
{
    HandshakeData *handshake_data = (HandshakeData *) user_data;
    g_print ("handshake_pid_callback\n");
    if (handshake_data->f_in != -1) {
        close (handshake_data->f_in);
        handshake_data->f_in = -1;
    }
    if (handshake_data->f_out != -1) {
        close (handshake_data->f_out);
        handshake_data->f_out = -1;
    }
    multipart_destroy (handshake_data->multipart_data);
    g_free (handshake_data->data);
    g_slice_free (HandshakeData, handshake_data);
}

void
multipart_start_async (gchar **remote_cmd,
                       gchar *data,
                       gsize data_len,
                       gchar *remote_path,
                       GCancellable *cancellable,
                       MultiPartCallback callback,
                       MultiPartDestroy destroy,
                       gpointer user_data)
{
    HandshakeData *handshake_data = g_slice_new0 (HandshakeData);
    SoupMessage *msg;
    msg = g_object_new (SOUP_TYPE_MESSAGE, NULL);
    handshake_data->msg = msg;
    handshake_data->response_str = g_string_new (NULL);
    handshake_data->data = g_strdup(data);
    handshake_data->data_len = data_len;
    handshake_data->remote_path = remote_path;

    MultiPartData *multipart_data = g_slice_new0 (MultiPartData);
    multipart_data->callback = callback;
    multipart_data->destroy = destroy;
    multipart_data->user_data = user_data;
    multipart_data->cancellable = cancellable;
    multipart_data->buffer = NULL;
    handshake_data->multipart_data = multipart_data;

    handshake_data->pid = piped_child (remote_cmd, &handshake_data->f_in, &handshake_data->f_out);
    g_child_watch_add_full (G_PRIORITY_DEFAULT,
                            handshake_data->pid,
                            handshake_pid_callback,
                            handshake_data,
                            NULL);

    handshake_data->io = g_io_channel_unix_new (handshake_data->f_in);
    g_io_channel_set_flags (handshake_data->io, G_IO_FLAG_NONBLOCK, NULL);
    g_io_add_watch_full (handshake_data->io,
                         G_PRIORITY_DEFAULT,
                         G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
                         handshake_io_cb,
                         handshake_data,
                         NULL);
}
