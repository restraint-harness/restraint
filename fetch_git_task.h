#include <libsoup/soup-uri.h>

#define GIT_PORT 9418
#define GIT_BRANCH "master"
#define HDR_LEN_SIZE 4
#define LARGE_PACKET_MAX 65520
#define TASK_LOCATION "./tmp"

typedef struct {
    gchar buf[LARGE_PACKET_MAX];
    SoupURI *uri;
    GSocketConnection *connection;
    GSocketClient *client;
    GInputStream *istream;
    GOutputStream *ostream;
} GitArchData;

char *restraint_fetch_git_task(char *uri_string);
