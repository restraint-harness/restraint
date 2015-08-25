#ifndef _SSH_H
#define _SSH_H

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include <glib.h>

#define DEFAULT_PORT 8081

typedef enum {
  SSH_IDLE,
  SSH_STARTING,
  SSH_ESTABLISHED,
  SSH_TUNNELING,
  SSH_FAILED,
  SSH_QUIT
} ssh_state;

typedef struct SshData_t {
  ssh_session session;
  ssh_channel fwdc;
  pthread_t fwdt;
  guint lport;
  gchar *rhost;
  guint rport;
  int lsock;
  int fsock;
  ssh_state state;
  gboolean soup_disc;
} SshData;

SshData *ssh_start(char *host);
void ssh_kill(SshData *ssh_data);

#endif
