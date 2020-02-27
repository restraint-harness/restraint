#pragma once

#include <gio/gio.h>
#include <libsoup/soup.h>

#include <stdbool.h>

#define RSTRNT_TYPE_LOG_MANAGER rstrnt_log_manager_get_type ()

G_DECLARE_FINAL_TYPE (RstrntLogManager, rstrnt_log_manager, RSTRNT, LOG_MANAGER, GObject)

typedef enum
{
    RSTRNT_LOG_TYPE_TASK,
    RSTRNT_LOG_TYPE_HARNESS,
} RstrntLogType;

typedef struct RstrntServerAppData RstrntServerAppData;
typedef struct RstrntTask RstrntTask;


void              rstrnt_upload_logs              (const RstrntTask    *task,
                                                   RstrntServerAppData *app_data,
                                                   SoupSession         *session,
                                                   GCancellable        *cancellable);

void              rstrnt_log_bytes                (const RstrntTask    *task,
                                                   RstrntLogType        type,
                                                   const char          *message,
                                                   size_t               message_length);

void              rstrnt_log                      (const RstrntTask    *task,
                                                   RstrntLogType        type,
                                                   const char          *format,
                                                   ...) G_GNUC_PRINTF (3, 4);

RstrntLogManager *rstrnt_log_manager_get_instance (void);
