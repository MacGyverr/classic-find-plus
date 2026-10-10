project('classic-find-plus', 'c', version: run_command('cat', 'VERSION', check: true).stdout().strip(), license: 'GPL-3.0-or-later', meson_version: '>=0.61',
  default_options: ['c_std=c11', 'warning_level=3', 'buildtype=debug'])
gtk = dependency('gtk+-3.0', version: '>=3.22')
gio = dependency('gio-2.0', version: '>=2.70')
dependency('glib-2.0', version: '>=2.70')
add_project_arguments('-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_70',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_70', language: 'c')
configuration = configuration_data()
configuration.set_quoted('CF_APP_VERSION', meson.project_version())
configuration.set_quoted('CF_INTEGRATION_HELPER', get_option('prefix') / get_option('datadir') / 'classic-find-plus/integration-manager.py')
configure_file(output: 'config.h', configuration: configuration)
inc = include_directories('.', 'include')
search_sources = files('src/criteria.c', 'src/date-expression.c', 'src/search.c', 'src/admin-protocol.c', 'src/admin-client.c')
result_sources = files('src/export-dialog.c', 'src/result-export.c', 'src/metadata.c', 'src/settings.c', 'src/results.c', 'src/results-sync.c', 'src/result-columns.c', 'src/column-preferences.c', 'src/selection.c', 'src/file-operations.c', 'src/result-actions.c')
ui_sources = files('src/integrations.c', 'src/window-position.c', 'src/search-history.c', 'src/window.c', 'src/preferences.c', 'src/search-form.c', 'src/date-input.c', 'src/date-dialog.c', 'src/help-about.c') + result_sources
artwork_sources = import('gnome').compile_resources('classic-find-plus-resources',
  'data/classic-find-plus.gresource.xml', source_dir: 'data', c_name: 'classic_find_plus')
# Only generated resource strings exceed ISO C's minimum supported literal length.
artwork = static_library('classic-find-plus-artwork', artwork_sources,
  dependencies: gtk, c_args: ['-Wno-overlength-strings'])
source_manifest = import('fs').read('sources.txt').strip().split('\n')
app = executable('classic-find-plus', source_manifest,
  include_directories: inc, dependencies: gtk, link_whole: artwork, install: true)
worker = executable('classic-find-plus-search-worker', 'worker/main.c', 'worker/read-only.c',
  'src/metadata.c', search_sources, include_directories: [inc, include_directories('worker')],
  dependencies: gio, install: true, install_dir: get_option('libexecdir'))
service_config = configuration_data()
worker_path = get_option('prefix') / get_option('libexecdir') / 'classic-find-plus-search-worker'
service_config.set('WORKER_EXEC', '"' + worker_path.replace('\\', '\\\\').replace('"', '\\"') + '"')
configure_file(input: 'data/privileged/org.classicfind.Plus.Search.service.in',
  output: 'org.classicfind.Plus.Search.service', configuration: service_config,
  install_dir: get_option('datadir') / 'dbus-1/system-services')
install_data('data/privileged/org.classicfind.Plus.Search.conf', install_dir: get_option('datadir') / 'dbus-1/system.d')
install_data('data/privileged/org.classicfind.plus.search.policy', install_dir: get_option('datadir') / 'polkit-1/actions')
install_data('scripts/integration-manager.py', install_dir: get_option('datadir') / 'classic-find-plus')
integration_files = ['classic-find-plus.nemo_action', 'classic-find-plus-nautilus.py', 'classic-find-plus-caja.py',
  'classic-find-plus.desktop', 'classic-find-plus-thunar.xml', 'classic-find-plus-caja-script.py']
foreach adapter : integration_files
  install_data('data/integrations' / adapter, install_dir: get_option('datadir') / 'classic-find-plus/integrations')
endforeach
install_data('data/desktop/org.classicfind.Plus.desktop', install_dir: get_option('datadir') / 'applications')
install_data('data/artwork/icon.png', rename: 'classic-find-plus.png',
  install_dir: get_option('datadir') / 'icons/hicolor/16x16/apps')
install_symlink('classic-find', pointing_to: 'classic-find-plus', install_dir: get_option('bindir'))
install_data('LICENSE', 'README.md', 'readme.txt', install_dir: get_option('datadir') / 'doc/classic-find-plus')
install_data('CHANGELOG.md', 'THIRD_PARTY.md', install_dir: get_option('datadir') / 'doc/classic-find-plus')
public_docs = ['admin-search.md', 'integrations.md', 'search.md', 'file-actions.md', 'permissions.md', 'settings.md', 'results-sync.md', 'export.md', 'exclusion.md', 'install.md', 'publishing.md', 'packaging.md']
foreach doc : public_docs
  install_data('docs' / doc, install_dir: get_option('datadir') / 'doc/classic-find-plus/docs')
endforeach
# Development tests stay private; a public export contains no tests directory.
if import('fs').is_file('tests/meson.build')
  subdir('tests')
endif
