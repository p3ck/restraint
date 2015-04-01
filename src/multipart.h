#define READ_BUFFER_SIZE 10000

typedef void (*MultiPartCallback)	(const char *method,
					 const char *path,
					 GCancellable *cancellable,
					 GError *error,
					 SoupMessageHeaders *headers,
					 SoupBuffer *buffer,
					 gpointer user_data);
typedef void (*MultiPartDestroy)	(GError *error,
					 gpointer user_data);

typedef enum {
    HANDSHAKE,
    READ_HEADER,
    RUN,
    RUNNING
} ClientState;

typedef struct {
    const gchar *method;
    const gchar *path;
    GCancellable *cancellable;
    GError *error;
    SoupMessageHeaders *headers;
    GString *buffer;
    gchar read_buffer[READ_BUFFER_SIZE];
    MultiPartCallback callback;
    MultiPartDestroy destroy;
    SoupMultipartInputStream *multipart;
    gpointer user_data;
} MultiPartData;

typedef struct {
    const gchar *remote_path;
    gchar *data;
    gsize data_len;
    SoupMessage *msg;
    GString *response_str;
    GIOChannel *io;
    gint f_in;
    gint f_out;
    pid_t pid;
    ClientState state;
    MultiPartData *multipart_data;
} HandshakeData;

void
multipart_start_async (gchar **remote_cmd,
                       gchar *data,
                       gsize data_len,
                       gchar *remote_path,
                       GCancellable *cancellable,
                       MultiPartCallback callback,
                       MultiPartDestroy destroy,
                       gpointer user_data);
