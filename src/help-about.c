#include "help-about.h"
#include "app-info.h"
#include "integrations.h"

static void paragraph(GtkBox *box, const char *name, const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_name(label, name);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 65);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_pack_start(box, label, FALSE, FALSE, 0);
}

GtkWidget *cf_help_about_new(void)
{
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(scroll, "help-about");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(scroll), box);
    GtkWidget *identity = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    g_autoptr(GdkPixbuf) image = gdk_pixbuf_new_from_resource_at_scale(
        "/org/classicfind/plus/about.png", 160, 160, TRUE, NULL);
    if (image) {
        GtkWidget *portrait = gtk_image_new_from_pixbuf(image);
        gtk_widget_set_name(portrait, "about-portrait");
        gtk_widget_set_valign(portrait, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(identity), portrait, FALSE, FALSE, 0);
    }
    GtkWidget *credits = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    paragraph(GTK_BOX(credits), "about-name", CF_APP_NAME " " CF_APP_VERSION);
    paragraph(GTK_BOX(credits), "about-creators", "Created by " CF_APP_CREATORS);
    paragraph(GTK_BOX(credits), "about-license", "Free software — GPL-3.0-or-later");
    GtkWidget *link = gtk_link_button_new_with_label(CF_APP_WEBSITE, "GitHub project page");
    gtk_widget_set_name(link, "about-github");
    gtk_widget_set_halign(link, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(credits), link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(identity), credits, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), identity, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), cf_integrations_new(), FALSE, FALSE, 0);
    paragraph(GTK_BOX(box), "help-start", "Searching: choose a folder, enter a filename or pattern and press Find Now (F5). "
        "Tabs share values: Name searches filenames, Date searches dates, Advanced searches type/extension/size, and Find Plus combines eligible filters. Stop or Esc cancels a search. "
        "Search location, Include Subfolders, Stay on this filesystem and Follow symbolic links are shared on every search tab. "
        "Stay on this filesystem limits recursive traversal to the filesystem containing the search root; you can start a search directly on another filesystem. "
        "Aa enables case sensitivity; matching is case-insensitive by default. Filename matching does not search file contents. Containing Text is a disabled placeholder.");
    paragraph(GTK_BOX(box), "help-wildcards", "Partial / wildcard: plain text finds that part of a filename. "
        "If you enter * or ?, the pattern matches the whole filename. * means any number of characters; ? means one character.\n"
        "Examples: *.txt finds text filenames; report* finds names beginning with report; "
        "h?ll0.txt matches hell0.txt or hall0.txt (the final 0 is a digit).");
    paragraph(GTK_BOX(box), "help-exclude", "Exclude is active only in Find Plus, including when More Filters is collapsed. "
        "In Partial / wildcard and Exact name modes, use | to separate multiple exclusions. Any matching expression excludes the file or directory. Matching directories and their subfolders are skipped before enumeration. "
        "Examples: /Movies|/Pictures|cache, /backup|*.tmp|*.bak, /My Movies|old files. "
        "A leading / targets directory components anywhere below the search root, not an absolute path or regex delimiter. Literal /new skips newshows but not brandnew, including in Exact name mode. "
        "Each rule keeps its own directory-only or file/directory behavior. Wildcards match whole component names; plain text uses the selected partial/exact rules. "
        "Non-RegEx escapes: \\| matches a literal pipe; \\\\ represents a literal backslash. Example: file\\|old|cache matches file|old or cache. Other backslashes are preserved. "
        "Spaces (including leading/trailing spaces), colons and semicolons are ordinary characters. Empty terms are ignored; ||| disables non-RegEx exclusion. Aa controls case sensitivity. "
        "In RegEx mode the whole field is one unchanged expression using native | alternation; it is never custom-split or unescaped. Examples: (?:Movies|Pictures)|cache, file\\|old, file:old. "
        "The selected root itself is exempt; symbolic links require Follow enabled and resolved excluded target subtrees remain pruned. Other tabs retain but ignore Exclude. Raw text is saved unchanged across modes.");
    paragraph(GTK_BOX(box), "help-exact", "Exact name: matches the whole filename literally, including any * or ? characters.");
    paragraph(GTK_BOX(box), "help-regex", "RegEx: uses GLib's Perl-compatible regular expressions. "
        "Matches may occur anywhere in a filename; use ^ and $ to require the beginning and end. "
        "A dot matches one character, .* matches any number, [0-9] matches a digit, and {4} repeats four times.\n"
        "Example: ^report-[0-9]{4}\\.txt$ matches report-2026.txt but not report-final.txt. "
        "Use \\. for a literal dot. *.txt is a wildcard, not a valid RegEx; use \\.txt$ instead. "
        "Invalid expressions show an error and retain existing results.");
    paragraph(GTK_BOX(box), "help-filters", "Find Plus combines eligible filters. Size accepts whole nonnegative numbers in bytes, KiB, MiB or GiB; no decimals. "
        "Greater/less than and size ranges exclude their boundaries; equal-to matches the exact byte count. Overflow and reversed ranges show an error. "
        "Dates use yyyy/mm/dd; calendar buttons offer fixed or relative dates, optional time and signed adjustments. "
        "Relative dates evaluate when Find starts. After/Before exclude the specified day (or instant with a time); Between includes both dates or instants. "
        "More Filters has individual enable checkboxes. Collapsing resets all optional values and checkboxes without searching; each new window starts with them off, even when expanded. Main dates and older tabs keep their values. Any/disabled optional Date mode uses main After/Before; active Last N combines with main bounds using AND. Stay/Follow/Admin remain outside the reset.");
    paragraph(GTK_BOX(box), "help-optional", "More Filters: File type, Extension, Date mode, Group, User and Permissions apply only when checked and expanded. "
        "Extension accepts jpg, .jpg, *.jpg or tar.gz and combines with File name without a second checkbox. "
        "Group/User include NSS system/service/root accounts, load asynchronously and are cached until restart; numeric GID/UID matching requires actual ownership metadata. "
        "Permissions matches exactly three/four octal digits, including special bits: 644, 0644, 4755, 2755, 1777. Invalid active input prevents searching. "
        "No Hidden files excludes dot-prefixed entries and skips entire hidden directory trees; the selected root is exempt.");
    paragraph(GTK_BOX(box), "help-relative", "Date Mode: Any date or Last N minutes, hours, days or months. Previous N has no checkbox: it enables automatically for a checked relative mode on the active, expanded Find Plus tab. "
        "Enter a positive whole number (ASCII digits, at least 1); inactive N is ignored. Collapse resets Date Mode off/Any and N to 7. "
        "Date Modified keeps Any, After, Before, Between, After and before, and all four Last N modes. "
        "Find captures one reference instant before authentication. Both edges are inclusive; newer timestamps are excluded, and running searches retain their interval. "
        "Minutes/hours/days subtract elapsed time; a day is 24 hours across DST. Months subtract calendar months from the local clock, clamping month-end (March 31 minus one month becomes February 28/29). "
        "N is bounded by the captured date's distance from year 1 and checked 64-bit arithmetic; months allow at most 12*(year-1)+month-1. See docs/search.md for exact limits. "
        "Main After/Before AND Last N use the same Modified/Created/Accessed field. Nonoverlapping bounds show a validation error.");
    paragraph(GTK_BOX(box), "help-follow", "Follow symbolic links defaults off and is saved independently. When on, native local/mounted directory targets are visited once by filesystem/file identity; cycles cannot recurse indefinitely. "
        "Stay on this filesystem checks actual targets. Exclude and No Hidden files also check resolved target ancestors. Link rows retain link metadata; children use the first eligible alias in bytewise URI traversal order. "
        "Unsafe or unavailable backend identities are reported rather than followed. Account lookup failures leave other filters available.");
    paragraph(GTK_BOX(box), "help-date-metadata", "Date Modified and Find Plus share a saved Modified / Created / Accessed selector. "
        "Modified means last modification; Created means filesystem creation/birth; Accessed means last access. "
        "Created searches fall back to Modified only when birth metadata is unavailable, never when a real birth timestamp fails the filter. "
        "The Created result column always shows actual birth metadata, or stays blank. Accessed may not update after every read on relatime/noatime mounts.");
    paragraph(GTK_BOX(box), "help-input-limits", "Input limits: filename/pattern, Exclude, extension, rename and the disabled content placeholder allow up to 8192 characters; "
        "folder paths 32768; date expressions 1024; numeric fields 32. History keeps 20 names. Settings files are limited to 1 MiB. "
        "Linux names can contain spaces, Unicode, quotes, backslashes and brackets. Rename accepts one basename: no /, empty name, ' . ' or '..'. "
        "Dates require valid calendar values (including leap years); optional time uses 24-hour HH:mm:ss. Adjustments are signed whole numbers within ±1000000; "
        "the evaluated date must remain representable. Incomplete or invalid filters show an error when searching. "
        "RegEx is interpreted only in RegEx mode; matching has work/depth limits, and overly expensive matches can be excluded. Simplify such patterns.");
    paragraph(GTK_BOX(box), "help-admin-search", "Allow admin search: available above More Filters in Find Plus, initially unchecked on every launch and never saved. "
        "Checking does not authenticate. Find validates filters and requests Polkit permission for a temporary read-only worker; the GUI stays your ordinary user. "
        "Other tabs ignore the checkbox. Stop/Esc or closing cancels. Denial, cancellation or an unavailable service never falls back to ordinary searching. "
        "All functional filters apply to local/mounted folders. Status identifies administrator searches. Protected results do not grant permission to open, copy, move or export them. "
        "A system worker, Polkit authority, active session and authentication agent must be installed/configured. See docs/admin-search.md for limits.");
    paragraph(GTK_BOX(box), "help-root-mode", "Root CLI: classic-find-plus runs normally; sudo classic-find-plus is refused. "
        "sudo classic-find-plus --allow-root explicitly runs the whole GUI as root with an Administrator Mode title and startup warning. "
        "Every feature has root permissions and can change system files. Root uses separate configuration/cache/data under its own home. "
        "X11/Wayland access may be refused; CFP never changes display access controls. Prefer Allow admin search when only searching needs elevation.");
    paragraph(GTK_BOX(box), "help-admin-transfers", "Administrative transfers — Linux Mint workflow:\n"
        "1. Open Nemo and press F3 for split panes.\n"
        "2. Navigate one pane to a normal source folder.\n"
        "3. In the other pane press Ctrl+L, enter admin:///opt/ (or another protected destination), and authenticate if prompted.\n"
        "4. Open Classic Find Plus from the normal folder, select results, and drag them into the administrative pane. Authenticate again if prompted.\n"
        "Protected destinations require administrator authorization. Classic Find Plus normally runs as your regular user; keep it that way. "
        "Nemo handles the transfer and authentication. admin:// support depends on the file manager, desktop session, GVfs and Polkit configuration; "
        "behavior varies between distributions.");
    paragraph(GTK_BOX(box), "help-settings", "Filename arrows show the newest 20 unique searches. Choosing fills the fields without searching; Clear History retains the current input. "
        "Search values, relative expressions, last tab, window and column layout are saved in ~/.config/classic-find-plus/settings.ini (or XDG_CONFIG_HOME). "
        "Nemo's directory overrides the saved location. Startup and tab changes never search. Name always stays visible.");
    paragraph(GTK_BOX(box), "help-results", "Results: right-click a header to choose columns, drag headers to reorder and click to sort. "
        "Ctrl/Shift and Ctrl+A select files. Drag selected files into Nemo; Ctrl requests copy and Shift requests move. "
        "Nemo handles transfers and access permissions. Recently dragged results update after filesystem verification. "
        "Rerun Find after authentication delays longer than two minutes or uncorrelated moves. "
        "Right-click results for file actions; Delete requests confirmed Trash.");
    paragraph(GTK_BOX(box), "help-exact-sizes", "Size remains the human-readable file-size column. Bytes shows exact bytes; Bits shows exact bytes multiplied by eight. "
        "Both are initially hidden: enable them in the header menu. Values use comma grouping, no units or rounding, and sort numerically in either direction. "
        "Visible Bytes/Bits export exactly as displayed. Bits is logical file size, not network traffic or transfer time: protocols add overhead, and compression and transfer speed affect real transfers.");
    paragraph(GTK_BOX(box), "help-export", "Exporting Results: Export results is enabled only with one or more displayed rows, including stopped partial searches. It is temporarily disabled while writing and restores according to the current model after success or failure. Export results saves CSV (commas, every field quoted) or TXT (tabs, no field quoting), as UTF-8 with CRLF records. "
        "Both include headings and all rows available when you click Export, regardless of selection. Only visible columns are included, in their current order and displayed sorting; later search or result changes do not change that snapshot. "
        "The Save dialog starts in the search directory that produced the results, even after editing Look in. Choose another folder if it is protected; an inaccessible starting directory falls back to home. "
        "Save asks before replacing an existing file. Export does not modify your source files or results. TXT replaces each embedded tab, CR or LF with a space. "
        "For spreadsheet safety, values starting with tab/CR/LF or whose first non-whitespace/control/BOM character is =, +, - or @ gain a leading apostrophe. Invalid UTF-8 bytes become replacement characters in export only.");
    paragraph(GTK_BOX(box), "help-shortcuts", "Shortcuts: Ctrl+1–5 selects tabs; Ctrl+F focuses Filename; Ctrl+L focuses Look in; "
        "Ctrl+Return or F5 starts Find; Esc stops. Update and removal instructions are in the installed readme.txt.");
    return scroll;
}
