# Changelog

## 1.0.2

Caja integration now uses an executable Scripts menu launcher without requiring python-caja.
Help/About offers Switch to Scripts for the old CFP Python extension and removes only its
unchanged owned file. Selected/current folders and special-character paths use argv without
a shell. Fresh integrations remain disabled until enabled. The Administrator Mode warning
now explicitly identifies the main application window as the window that must be closed.

The owner verified real Mint administrator authentication, protected searches and explicit
full-root GUI operation with 1.0.1. Other managers' new context-menu GUIs remain provisional.
Caja 1.26's actual background Scripts menu, launcher, correct folder and updated warning
passed in a disposable namespace using the Ubuntu 22.04 SDK; see docs/integrations.md for scope.

## 1.0.1

Adds Find Plus's transient Allow admin search checkbox and a temporary, Polkit-authorized
system D-Bus worker using the shared scanner. All functional filters remain available;
metadata IPC, sender-bound requests, limits, cancellation and a read-only syscall guard keep
the worker narrow. GUI actions/export/transfers retain ordinary permissions.

Adds explicit --allow-root full Administrator Mode with a warning and separate root profile;
plain root GUI launch is still refused. Adds opt-in Nemo, Nautilus, Dolphin, Caja and Thunar
adapters and Help/About integration controls. Fresh installs activate none; upgrades retain
legacy Nemo actions and user choices. Both build systems install required security/resources.

Private protocol, mock-authority, read-only, UI, adapter and disposable package lifecycle
tests passed. Protected traversal and the explicit root GUI passed in isolated UID-0
namespaces; ordinary clients still cannot open protected results. Real desktop Polkit
authentication remains a release verification gate. New context-menu GUI behavior remains
provisional; this source version does not claim those desktop checks passed.
See administrator-search and integration documentation for scope and platform limitations.

## 1.0.0

First public source release baseline. Adds an independent GNU Make build alongside Meson,
conservative source installation/removal, a maintained public-file allowlist, and public
build/install documentation. Application behavior remains that of the completed feature set.

Features include four search tabs and Help/About, shared persistent criteria, wildcard/exact/
RegEx filename matching, Find Plus exclusions with directory pruning, size/date filters,
fifteen configurable columns including exact Bytes/Bits, CSV/TXT export, file actions,
Nemo drag-and-drop and bounded post-transfer results synchronization.

## Earlier development milestones

- 0.6.0: exclusions, exact Bytes/Bits columns and model-driven export availability.
- 0.5.0: CSV and tab-delimited TXT export.
- 0.4.0: extended metadata, selectable date fields and window-position handling.
- 0.3.x: automatic result synchronization and shared persistent search criteria.
- 0.2.x: Find Plus layout, calendar expressions, RegEx and Help/About.
- 0.1.x: native search, file actions, Nemo integration and Debian packaging.
