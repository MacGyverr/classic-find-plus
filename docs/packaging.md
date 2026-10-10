# Native packages

The package identifier is `classic-find-plus`. DEBs conflict with/replace/provide the earlier
`classic-find` package, retain the compatibility executable name, and preserve user settings.
The current source version is read from VERSION; the Debian revision is in debian/changelog.

Build on the intended distribution with the prerequisites in README, plus `dpkg-dev`,
`binutils` and `python3`:

```sh
sh scripts/build-deb.sh
```

The script performs an incremental Meson release build, stages under a marked build-package
folder and writes a root-owned archive to dist. It never installs onto the host. The generated
Depends field comes from dpkg-shlibdeps and actual build libraries, plus runtime icon support, D-Bus, Polkit and GLib tools. Python 3 is a pre-dependency for
the narrow legacy Nemo migration maintscripts.
A Debian-built DEB is not automatically compatible with Ubuntu/Mint; t64 ABI and version
requirements matter. Mint 21.3 uses Ubuntu 22.04/jammy. Build natively there or use an independently
prepared compatible SDK; the optional --sysroot mode currently supports amd64 only and expects
a marked SDK. SDK preparation/internal verification tools are not part of this public release.

Example native commands:

```sh
sudo apt install ./dist/classic-find-plus_VERSION_DISTRIBUTION_ARCH.deb
sudo apt remove classic-find-plus
sudo apt purge classic-find-plus
```

Replace the example filename with the actual generated file. A newer downloaded DEB upgrades
through the same apt install command. Synaptic upgrades need a configured repository containing
the newer package; local files can use the graphical package installer. Removal/purge retains
per-user settings. Avoid mixing source installs with package-owned paths. No RPM spec is supplied;
native source builds on RPM distributions are documented without claiming RPM packaging tests.

Phase 8 packages include the non-setuid worker, restrictive Polkit action/system-bus resources,
and inactive optional adapter templates. Fresh installs activate no menu; upgrades preserve
owned legacy Nemo content and per-user configuration. Migration hooks honor DPKG_ROOT for
safe disposable lifecycle tests and remove only unchanged migrated global content. Python
adapters remain optional; no file manager/desktop stack is automatically installed.

Release source archives use the explicit public allowlist, a versioned top-level directory and
no private tests/build output. Verify the final matching .sha256 with `sha256sum -c`. Public
source builds must succeed using both Make and Meson before a release is considered verified.
