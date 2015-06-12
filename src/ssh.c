#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include "ssh.h"

#define START_PORT 8091
#define LOCAL_ADDR "127.0.0.1"
#define BUF_SIZE 16384
#define SSH_RETRY_DELAY 15

static void close_ssh_connection(SshData *ssh_data) {
  if (ssh_data->fwdc != NULL) {
    ssh_channel_close(ssh_data->fwdc);
    ssh_channel_free(ssh_data->fwdc);
    ssh_data->fwdc = NULL;
  }
  ssh_disconnect(ssh_data->session);
}

void ssh_kill(SshData *ssh_data)
{
  if (ssh_data->state == SSH_ESTABLISHED) {
    pthread_kill(ssh_data->fwdt, SIGTERM);
  } else {
    ssh_data->state = SSH_QUIT;
    pthread_join(ssh_data->fwdt, NULL);
  }

  close(ssh_data->fsock);
  close(ssh_data->lsock);

  g_free(ssh_data->rhost);
  close_ssh_connection(ssh_data);
  ssh_free(ssh_data->session);
}

static gint ssh_establish_connection(SshData *ssh_data) {
  int rc = 0;
  struct sockaddr_in sin;
  socklen_t slen = 0;

  rc = ssh_connect(ssh_data->session);
  if (rc != SSH_OK) {
    g_printerr("Error connecting to ssh: %s\n", ssh_get_error(ssh_data->session));
    return -1;
  }

  // TODO: Auth server?

  rc = ssh_userauth_publickey_auto(ssh_data->session, NULL, NULL);
  if (rc != SSH_AUTH_SUCCESS) {
    gchar *password = NULL;
    g_printerr("Failed to authenticate by public key: %s\n", ssh_get_error(ssh_data->session));
    password = getpass("Password: ");
    rc = ssh_userauth_password(ssh_data->session, NULL, password);
    free(password);
    if (rc != SSH_AUTH_SUCCESS) {
      g_printerr("Failed to authenticate by password: %s\n", ssh_get_error(ssh_data->session));
      ssh_disconnect(ssh_data->session);
      return -1;
    }
  }

  ssh_data->fwdc = ssh_channel_new(ssh_data->session);
  if (ssh_data->fwdc == NULL) {
    g_printerr("Failed to create ssh channel\n");
    ssh_disconnect(ssh_data->session);
    return -1;
  }

  if (ssh_data->state != SSH_FAILED) {
    // TODO: ipv6?
    ssh_data->lsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    ssh_data->lport = START_PORT;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(ssh_data->lport);
    sin.sin_addr.s_addr = inet_addr(LOCAL_ADDR);
    slen = sizeof(sin);

    while (bind(ssh_data->lsock, (struct sockaddr*)&sin, slen) == -1) {
      ssh_data->lport++;
      sin.sin_port = htons(ssh_data->lport);
    }

    if (listen(ssh_data->lsock, 1)) {
      g_printerr("Failed to start listening\n");
      close(ssh_data->lsock);
      ssh_channel_free(ssh_data->fwdc);
      ssh_data->fwdc = NULL;
      ssh_disconnect(ssh_data->session);
      return -1;
    }
  }

  rc = ssh_channel_open_forward(ssh_data->fwdc, "localhost", ssh_data->rport,
                                "localhost", ssh_data->lport);
  if (rc != SSH_OK) {
    g_printerr("Failed to open forwarding channel\n");
    ssh_channel_free(ssh_data->fwdc);
    ssh_data->fwdc = NULL;
    ssh_disconnect(ssh_data->session);
    return -1;
  }
  ssh_data->state = SSH_ESTABLISHED;

  return 0;
}

static void ssh_connection_fail(SshData *ssh_data) {
  close(ssh_data->fsock);
  close_ssh_connection(ssh_data);
  ssh_data->state = SSH_FAILED;
}

void* fwddata(void *data) {
  SshData *ssh_data = (SshData*) data;
  struct sockaddr_in sin;
  socklen_t slen = 0;
  int rc;
  gint64 len;
  char buf[BUF_SIZE];
  fd_set fds;
  struct timeval tout;

  while (1) {
    if (ssh_data->state == SSH_QUIT) {
      return NULL;
    } else if (ssh_data->state == SSH_FAILED) {
      sleep(SSH_RETRY_DELAY);
      // According to libssh docs ssh_disconnect should be enough to recreate a
      // session, but it's not, so we are creating a new session and copying
      // all of the options over to it.
      ssh_session session = ssh_new();
      ssh_options_copy(ssh_data->session, &session);
      ssh_free(ssh_data->session);
      ssh_data->session = session;
      ssh_establish_connection(ssh_data);
    } else if (ssh_data->state == SSH_ESTABLISHED) {
      ssh_data->fsock = accept(ssh_data->lsock, (struct sockaddr*)&sin, &slen);
      if (ssh_data->fsock == -1) {
        g_printerr("Failed to accept connection\n");
        close(ssh_data->lsock);
        return NULL;
      }
      ssh_data->soup_disc = FALSE;
      ssh_data->state = SSH_TUNNELING;
    } else if (ssh_data->state == SSH_IDLE) {
      if (ssh_data->soup_disc) {
        ssh_data->state = SSH_FAILED;
      } else {
        sleep(1);
      }
    } else {
      if (ssh_data->soup_disc) {
        close(ssh_data->fsock);
        ssh_data->state = SSH_ESTABLISHED;
        continue;
      }
      FD_ZERO(&fds);
      FD_SET(ssh_data->fsock, &fds);
      tout.tv_sec = 0;
      tout.tv_usec = 10000;
      rc = select(ssh_data->fsock + 1, &fds, NULL, NULL, &tout);
      if (rc > 0 && FD_ISSET(ssh_data->fsock, &fds)) {
        int wr = 0;
        memset(buf, 0, BUF_SIZE);
        len = recv(ssh_data->fsock, buf, BUF_SIZE, 0);
        if (len == 0) {
          close(ssh_data->fsock);
          ssh_data->state = SSH_ESTABLISHED;
          continue;
        }

        while (wr < len) {
          int w = ssh_channel_write(ssh_data->fwdc, buf, len);
          if (w != SSH_ERROR) {
            wr += w;
          } else {
            g_printerr("Error writing to ssh channel\n");
            ssh_connection_fail(ssh_data);
            break;
          }
        }
      }
      while (1) {
        int wr = 0;
        memset(buf, 0, BUF_SIZE);
        len = ssh_channel_read_nonblocking(ssh_data->fwdc, buf, BUF_SIZE, 0);
        if (len == 0) {
          break;
        } else if (len == SSH_ERROR) {
          g_printerr("Error reading from ssh channel\n");
          ssh_connection_fail(ssh_data);
          break;
        }
        while (wr < len) {
          int w = send(ssh_data->fsock, buf + wr, len - wr, 0);
          if (w > 0) {
            wr += w;
          }
        }

        if (ssh_channel_is_eof(ssh_data->fwdc)) {
          ssh_connection_fail(ssh_data);
          if (!ssh_data->soup_disc) {
            ssh_data->state = SSH_IDLE;
          }
          break;
        }
      }
    }
  }

  return NULL;
}

SshData *ssh_start(char *host)
{
  SshData *ssh_data = g_slice_new0(SshData);
  ssh_data->session = ssh_new();
  GRegex *regex;
  GMatchInfo *match_info;

  if (ssh_data->session == NULL) {
    g_printerr("Failed to create ssh session\n");
    g_slice_free(SshData, ssh_data);
    return NULL;
  }

  regex = g_regex_new("^((\\w+)@)?([\\w\\.\\-]+)(:(\\d+))?(\\/(\\d+))?$", 0, 0, NULL);
  if (g_regex_match(regex, host, 0, &match_info)) {
    while (g_match_info_matches(match_info)) {
      gchar *user = g_match_info_fetch(match_info, 2);
      gchar *lhost = g_match_info_fetch(match_info, 3);
      gchar *rport = g_match_info_fetch(match_info, 5);
      gchar *sport = g_match_info_fetch(match_info, 7);

      ssh_options_set(ssh_data->session, SSH_OPTIONS_HOST, lhost);
      ssh_data->rhost = lhost;

      ssh_options_parse_config(ssh_data->session, NULL);

      if (user != NULL && g_strcmp0(user, "") != 0) {
        ssh_options_set(ssh_data->session, SSH_OPTIONS_USER, user);
      }

      if (rport != NULL && g_strcmp0(rport, "") != 0) {
        ssh_data->rport = atoi(rport);
      } else {
        ssh_data->rport = DEFAULT_PORT;
      }

      if (sport != NULL && g_strcmp0(sport, "") != 0) {
        ssh_options_set(ssh_data->session, SSH_OPTIONS_PORT_STR, sport);
      }

      g_free(user);
      g_free(rport);
      g_free(sport);
      g_match_info_next(match_info, NULL);
    }
  } else {
    g_printerr("Malformed host: %s, see help for reference\n", host);
    g_match_info_free(match_info);
    g_regex_unref(regex);
    ssh_free(ssh_data->session);
    g_slice_free(SshData, ssh_data);
    return NULL;
  }

  g_match_info_free(match_info);
  g_regex_unref(regex);

  ssh_data->state = SSH_STARTING;

  if (ssh_establish_connection(ssh_data) < 0) {
    ssh_free(ssh_data->session);
    g_slice_free(SshData, ssh_data);
    return NULL;
  }

  if (pthread_create(&ssh_data->fwdt, NULL, fwddata, (void*)ssh_data)) {
    g_printerr("Failed to start forwarding thread\n");
    close_ssh_connection(ssh_data);
    ssh_free(ssh_data->session);
    g_slice_free(SshData, ssh_data);
    return NULL;
  }

  return ssh_data;
}
