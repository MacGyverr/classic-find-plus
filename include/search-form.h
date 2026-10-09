#pragma once
#include "search.h"
#include "settings.h"

#define CF_TYPE_SEARCH_FORM (cf_search_form_get_type())
G_DECLARE_FINAL_TYPE(CfSearchForm, cf_search_form, CF, SEARCH_FORM, GObject)

/* One editable draft per window, shared by every presentation of the controls. */
CfSearchForm *cf_search_form_new(const char *location);
/* Borrowed strings remain valid until the draft changes. Criteria construction copies them. */
const char *cf_search_form_get_location(CfSearchForm *form);
CfCriteriaInput cf_search_form_get_input(CfSearchForm *form);

typedef enum { CF_TAB_NAME, CF_TAB_DATE, CF_TAB_ADVANCED, CF_TAB_PLUS, CF_TAB_HELP } CfSearchTab;
/* Inactive values are neutralized before criteria validation; does not edit the draft. */
gboolean cf_search_form_effective(CfSearchForm *form, int tab, gboolean more_expanded, CfCriteriaInput *input);

void cf_search_form_restore(CfSearchForm *form, CfSettings *settings);
void cf_search_form_track(CfSearchForm *form, CfSettings *settings);
