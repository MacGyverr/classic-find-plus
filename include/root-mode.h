#pragma once
#include <gio/gio.h>
#include <sys/types.h>
/* Trusted account home/UID only, never command-line or inherited HOME. */
gboolean cf_root_environment(const char *home, uid_t owner, GError **error);
