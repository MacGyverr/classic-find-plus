#pragma once
#include "search.h"
struct _CfCriteria {
    char *name, *extension, *exclude;
    GPatternSpec *exclude_pattern;
    GRegex *exclude_regex;
    gboolean exclude_directories;
    GPatternSpec *pattern;
    GRegex *regex;
    CfNameMode name_mode;
    CfType type;
    CfDateField date_field;
    CfSizeMode size_mode;
    guint64 size_start, size_end;
    gint64 date_start, date_end; /* microseconds since epoch, [start, end) */
    gboolean has_start, has_end, case_sensitive, extension_only,
             recursive, stay_on_filesystem;
};

/* Selected attribute, with Created fallback only when the birth attribute is absent. */
const char *cf_criteria_date_attribute(const CfCriteria *criteria, GFileInfo *info);
