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
        "Aa enables case sensitivity; matching is case-insensitive by default. Filename matching does not search file contents. Containing Text searches stored file bytes only in Find Plus.");
    paragraph(GTK_BOX(box), "help-content", "Containing text: an AND filter only in Find Plus; empty means no content scan. "
        "Hex off searches literal UTF-8 text: error.*failed matches those exact characters, independently of filename RegEx/wildcards. Aa controls case; off uses Unicode case folding (STRASSE matches Straße). "
        "Hex on searches exact bytes: 48656C6C6F20776F726C6421 means Hello world!; 00 FF 10 A2 includes NUL/high-bit bytes. Mixed-case digits and spaces/tabs between complete byte pairs are accepted. "
        "Odd digits, invalid characters, 0x prefixes and whitespace-only Hex show a validation error on Find. Aa has no effect on Hex. Maximum decoded/text/case-folded pattern is 64 KiB; raw Hex allows 256 KiB. "
        "Hex never persists and starts unchecked; toggling preserves the saved text without searching. More Filters collapse does not reset it. "
        "Regular binary files are eligible regardless of MIME or size; 32 KiB streaming reads detect boundary matches, stop at first match, and have no automatic file-size cutoff. No PDF/Office/archive text extraction. "
        "Stop checks between reads and bounded processing batches; blocked kernel/network/FUSE I/O may delay it. Devices, FIFOs, sockets, directories and unsafe actual kernel pseudo-filesystems are never content-scanned; regular /dev/shm files remain eligible. "
        "Follow enables eligible regular symlink targets, with the same protections in root mode. Insensitive text treats malformed UTF-8/NUL as barriers; normalization and non-UTF-8 encodings are unsupported. "
        "Admin content matching requires Polkit; paths/metadata only cross IPC, but protected match presence still reveals information. Existing worker resource/time limits apply. See docs/content-search.md.");
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
    paragraph(GTK_BOX(box), "help-result-names", "Copy Name to Clipboard copies stems without the final extension; Copy Name.EXT to Clipboard copies whole basenames; Copy Full Path to Clipboard copies native filesystem paths without URI escaping. "
        "Right-click a selected result to copy all highlighted rows, or an unselected result to select/copy only that row. The keyboard menu uses the current selection. Text entries follow displayed sort order with newline separators, no added trailing newline and no duplicate-name removal. "
        "archive.tar.gz becomes archive.tar; .bashrc, file. and directory names stay intact. Empty-space menus disable text copying. "
        "F2 with one selected result or Rename… opens the inline Name editor without filesystem permission checks, even for protected files. Select/copy text with normal Ctrl+C. The stem is selected; directories select all. "
        "Enter commits only changed text; unchanged Enter closes without a filesystem operation. Escape or clicking elsewhere cancels. A failed changed rename reports an error, closes editing and keeps the original result. "
        "A second single click on the same Name cell, after GTK's double-click interval and within 500 ms, starts renaming. If the interval is 500 ms or longer, use F2 or Rename…. Fast double-click still opens. "
        "Rename waits for searching to finish, never overwrites existing destinations and uses ordinary permissions, including for administrator search results. Protected items are not granted extra rename privileges. "
        "Unsupported safe rename locations are refused. Pinned identity checks reduce replacement races; the previously documented source-name replacement race remains. "
        "Renamed rows update paths/sorting or leave results if filters no longer match. Children of renamed directories leave the results; Find Now refreshes them. "
        "CSV/TXT Full Path and In Folder use native local paths, preserving spaces, Unicode and literal %20 rather than URI encoding. Remote results keep URIs; existing CSV quoting and TXT delimiters remain unchanged.");
    paragraph(GTK_BOX(box), "help-input-limits", "Input limits: filename/pattern, Exclude, extension and rename allow up to 8192 characters; "
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
    paragraph(GTK_BOX(box), "help-additional-managers", "Enable PCManFM (GTK) and XFE separately in File Manager Integrations. "
        "GTK PCManFM uses libfm custom actions for one local folder; install your distribution's libfm-modules if missing. PCManFM-Qt is a different application. "
        "XFE uses Scripts > Find with Classic Find Plus: select one folder to search it, one regular file to search its parent, XFE1.45 requires a selection to show Scripts. "
        "Restart the manager after changes. PCManFM and XFE administrator access/transfers are unverified; CFP supplies no administrator action for them.");
    paragraph(GTK_BOX(box), "help-nautilus", "GNOME Files is Nautilus. Enable its single entry in File Manager Integrations for Find with Classic Find Plus and Open as Administrator. "
        "One extension supports API 3.0 (before 43) and API 4.x (43+) without GTK3 imports into GTK4. Selected local folders and folder backgrounds are supported; Find opens without searching. "
        "Recognized compatible independent administrator extensions are preserved and suppress CFP's fallback. Unknown extensions remain untouched; detection cannot prove their loading. "
        "Refresh checks matching Python/GI/native provider support. Configured (unavailable) explains missing support; Disable remains usable. "
        "Install matching distribution python3-nautilus, python3-gi and Nautilus typelib packages yourself when required. CFP installs none. "
        "Repair actions upgrades unchanged older CFP extensions; Disable/Remove preserves unrelated providers. Finish transfers and restart Nautilus or log out/in to reload. "
        "A passing support probe is not visual menu verification. See docs/integrations.md for diagnostics and coexistence limits.");
    paragraph(GTK_BOX(box), "help-admin-transfers", "Administrative transfers — Linux Mint workflow:\n"
        "CFP supports Nemo, GNOME Files (Nautilus), Caja, Thunar, Dolphin, GTK PCManFM and XFE. GNOME Files is Nautilus, using the same adapter. "
        "Nemo, GNOME Files and Caja administrative destinations accept CFP drops on the owner's tested systems.\n"
        "Thunar Enable installs Find with Classic Find Plus and Open as Administrator together; older enabled installs offer Repair actions. "
        "Disable/Remove deletes only unchanged CFP actions, preserving unrelated administrator actions. "
        "Right-click one selected local folder for Open as Administrator. Thunar has been confirmed to open admin:///opt; new administrative transfers need desktop acceptance.\n"
        "1. Locate the protected destination in your file manager.\n"
        "2. Open it as Administrator using its context action (GNOME Files/Thunar: enable CFP integration first). Where supported, enter admin:///opt/ in the location bar.\n"
        "3. Authenticate through GVfs/Polkit if requested.\n"
        "4. Search in CFP as your regular user and select results.\n"
        "5. Drag results into the administrator-opened destination.\n"
        "6. Let the receiving file manager copy/move and handle further authorization.\n"
        "CFP remains a normal-user application; ordinary drag-and-drop is unchanged. GVfs administrative access does not run the entire file manager as root. "
        "Support depends on the distribution, manager, GVfs admin backend and desktop authentication agent. "
        "If unavailable, consult distribution GVfs packages (Ubuntu/Mint commonly gvfs-backends) and authentication-agent setup; install missing packages yourself. "
        "CFP installs no dependencies and captures no credentials. Nautilus/Thunar/GVfs handle denied/cancelled prompts and location errors. "
        "Dolphin administrative actions/transfers are outside this change and unverified. See docs/integrations.md and docs/permissions.md.");
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
