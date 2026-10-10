#pragma once
#include "search.h"
#define CF_ADMIN_BUS "org.classicfind.Plus.Search"
#define CF_ADMIN_PATH "/org/classicfind/Plus/Search"
#define CF_ADMIN_INTERFACE CF_ADMIN_BUS
#define CF_ADMIN_ACTION "org.classicfind.plus.search"
#define CF_ADMIN_PROTOCOL_VERSION 4
#define CF_ADMIN_CRITERIA_TYPE "(usssuuuuttxxbbbbbbbbbuuubbsu)"
#define CF_ADMIN_STATS_TYPE "(tttttbbs)"
#define CF_ADMIN_MAX_MESSAGE (512 * 1024)
#define CF_ADMIN_BATCH 32
GVariant *cf_admin_criteria_encode(const CfCriteria *criteria);
CfCriteria *cf_admin_criteria_decode(GVariant *value, GError **error);
GFile *cf_admin_root(const char *uri, GError **error);
GVariant *cf_admin_result_encode(CfResult *result, GError **error);
CfResult *cf_admin_result_decode(GVariant *value, GError **error);
GVariant *cf_admin_stats_encode(const CfSearchStats *stats);
gboolean cf_admin_stats_decode(GVariant *value, CfSearchStats *stats, GError **error);
/* Runs on a background thread. Result callback takes ownership. */
typedef void (*CfAdminResultFunc)(CfResult *, gpointer);
typedef void (*CfAdminStatsFunc)(const CfSearchStats *, gpointer);
void cf_admin_client_run(GFile *root, CfCriteria *criteria, GCancellable *cancel,
    CfAdminResultFunc result, CfAdminStatsFunc stats, gpointer data, GError **error);
