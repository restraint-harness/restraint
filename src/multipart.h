#define READ_BUFFER_SIZE 131072

typedef void (*MultiPartCallback)	(const char *method,
					 const char *path,
					 GCancellable *cancellable,
					 GError **error,
					 SoupMessageHeaders *headers,
					 SoupBuffer *buffer,
					 gpointer user_data);
typedef void (*MultiPartDestroy)	(GError *error,
					 gpointer user_data);

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
    gpointer user_data;
    SoupMultipartInputStream *multipart;
} MultiPartData;

void
multipart_request_send_async (SoupRequest *request,
                              GCancellable *cancellable,
                              MultiPartCallback callback,
                              MultiPartDestroy destroy,
                              gpointer user_data);
