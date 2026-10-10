#pragma once
#include <gio/gio.h>

typedef enum { CF_NAME_AUTO, CF_NAME_EXACT, CF_NAME_REGEX } CfNameMode;
typedef enum { CF_DATE_ALL, CF_DATE_AFTER, CF_DATE_BEFORE, CF_DATE_BETWEEN,
               CF_DATE_DAYS, CF_DATE_MONTHS, CF_DATE_BOUNDS } CfDateMode;
typedef enum { CF_DATE_MODIFIED, CF_DATE_CREATED, CF_DATE_ACCESSED } CfDateField;
typedef enum { CF_SIZE_ALL, CF_SIZE_GREATER, CF_SIZE_LESS, CF_SIZE_BETWEEN, CF_SIZE_EQUAL } CfSizeMode;
typedef enum { CF_TYPE_ANY, CF_TYPE_FILES, CF_TYPE_FOLDERS, CF_TYPE_IMAGE,
               CF_TYPE_AUDIO, CF_TYPE_VIDEO, CF_TYPE_TEXT } CfType;
typedef struct {
    const char *name, *exclude, *extension, *date_start, *date_end, *recent,
               *size_start, *size_end;
    CfNameMode name_mode;
    CfDateMode date_mode;
    CfDateField date_field;
    CfSizeMode size_mode;
    CfType type;
    guint size_unit; /* 0: bytes, 1: KiB, 2: MiB, 3: GiB */
    gboolean case_sensitive, extension_only, recursive, stay_on_filesystem;
} CfCriteriaInput;
typedef struct _CfCriteria CfCriteria;
CfCriteria *cf_criteria_new(const CfCriteriaInput *input, GDateTime *now, GError **error);
void cf_criteria_free(CfCriteria *criteria);
CfCriteria *cf_criteria_copy(const CfCriteria *criteria);
/* Exclusion ignores inclusion/date/size/type filters; safe before directory descent. */
gboolean cf_criteria_excludes(const CfCriteria *criteria, GFileInfo *info);
gboolean cf_criteria_matches(const CfCriteria *criteria, GFileInfo *info);
gboolean cf_parse_size(const char *text, guint unit, guint64 *value, GError **error);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfCriteria, cf_criteria_free)

typedef struct {
    GFile *file;
    GFileInfo *info;
} CfResult;
void cf_result_free(CfResult *result);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfResult, cf_result_free)

typedef struct _CfSearch CfSearch;
typedef struct {
    guint64 scanned, matched, errors, directories_enumerated, directories_pruned;
    guint queued;
    gboolean finished, cancelled;
    char *first_error;
} CfSearchStats;
/* Takes ownership of criteria, including on failure. No filesystem I/O here. */
CfSearch *cf_search_start(GFile *root, CfCriteria *criteria, GError **error);
CfSearch *cf_search_start_admin(GFile *root, CfCriteria *criteria, GError **error);
void cf_search_cancel(CfSearch *search);
void cf_search_unref(CfSearch *search);
CfResult *cf_search_pop(CfSearch *search);
void cf_search_get_stats(CfSearch *search, CfSearchStats *stats);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CfSearch, cf_search_unref)
