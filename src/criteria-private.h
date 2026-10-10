#pragma once
#include "search.h"
typedef struct {
    char *text;
    GPatternSpec *pattern;
    GRegex *regex;
    gboolean directories;
} CfExcludeRule;

struct _CfCriteria {
    char *input_name, *input_exclude, *input_extension;
    char *name, *extension;
    GPtrArray *exclusions; /* Immutable compiled rules; shared by captured copies. */
    GPatternSpec *pattern;
    GRegex *regex;
    CfNameMode name_mode;
    CfType type;
    CfDateField date_field;
    CfSizeMode size_mode;
    guint64 size_start, size_end;
    guint32 uid, gid, permissions;
    gboolean has_uid, has_gid, has_permissions, no_hidden, follow_symlinks;
    gint64 date_start, date_end; /* microseconds since epoch, [start, end) */
    gboolean has_start, has_end, case_sensitive, extension_only,
             recursive, stay_on_filesystem;
};

/* Selected attribute, with Created fallback only when the birth attribute is absent. */
const char *cf_criteria_date_attribute(const CfCriteria *criteria, GFileInfo *info);
