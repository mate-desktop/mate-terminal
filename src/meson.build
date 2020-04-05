version_data = configuration_data()
version_data.set('TERMINAL_MAJOR_VERSION', version_major)
version_data.set('TERMINAL_MINOR_VERSION', version_minor)
version_data.set('TERMINAL_MICRO_VERSION', version_micro)

version_h = configure_file(
  input: 'terminal-version.h.in',
  output: 'terminal-version.h',
  configuration: version_data,
)

cargs = [
  '-DG_LOG_DOMAIN="marco"',
  '-DTERMINAL_COMPILATION',
  '-DTERMINAL_RESOURCES_PATH_PREFIX="/org/mate/terminal"',
  '-DTERM_DATADIR="@0@"'.format(datadir),
  '-DTERM_LOCALEDIR="@0@/locale"'.format(datadir),
  '-DTERM_PKGDATADIR="@0@"'.format(pkgdatadir),
  '-DTERM_HELPDIR="@0@/help"'.format(datadir),
  '-DSN_API_NOT_YET_FROZEN',
  '-DGDK_MULTIHEAD_SAFE',
  '-DG_DISABLE_SINGLE_INCLUDES',
  '-DPANGO_DISABLE_SINGLE_INCLUDES',
  '-DATK_DISABLE_SINGLE_INCLUDES',
  '-DGDK_DISABLE_SINGLE_INCLUDES',
  '-DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES',
  '-DGTK_DISABLE_SINGLE_INCLUDES',
  '-DEGG_SM_CLIENT_BACKEND_XSMP',
]

sources = [
  'eggshell.c',
  'eggshell.h',
  'profile-editor.c',
  'profile-editor.h',
  'terminal.c',
  'terminal-accels.c',
  'terminal-accels.h',
  'terminal-app.c',
  'terminal-app.h',
  'terminal-close-button.h',
  'terminal-close-button.c',
  'terminal-debug.c',
  'terminal-debug.h',
  'terminal-encoding.c',
  'terminal-encoding.h',
  'terminal-info-bar.c',
  'terminal-info-bar.h',
  'terminal-intl.h',
  'terminal-options.c',
  'terminal-options.h',
  'terminal-profile.c',
  'terminal-profile.h',
  'terminal-screen.c',
  'terminal-screen.h',
  'terminal-screen-container.c',
  'terminal-screen-container.h',
  'terminal-search-dialog.c',
  'terminal-search-dialog.h',
  'terminal-tab-label.c',
  'terminal-tab-label.h',
  'terminal-tabs-menu.c',
  'terminal-tabs-menu.h',
  'terminal-util.c',
  'terminal-util.h',
  'terminal-window.c',
  'terminal-window.h',
  version_h,
]

if build_skey
  subdir('skey')
  sources += [
    'skey-popup.c',
    'skey-popup.h',
  ]
endif

schemas_data = configuration_data()
schemas_data.set('GETTEXT_PACKAGE', meson.project_name())

schemas_in = [
  'org.mate.terminal.gschema.xml',
]

schemas = []
foreach s: schemas_in
  schemas += configure_file(
    input: '@0@.in'.format(s),
    output: '@0@'.format(s),
    configuration: schemas_data,
  )
endforeach

install_data(schemas,
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas'),
)

source_enum = gnome.mkenums('terminal-type-builtins',
  sources: 'terminal-profile.h',
  h_template: 'terminal-type-builtins.h.template',
  c_template: 'terminal-type-builtins.c.template',
  )

source_resources = gnome.compile_resources(
  'terminal-resources',
  'terminal.gresource.xml',
  source_dir : '.',
  c_name : 'terminal'
  )

marshalers = gnome.genmarshal('terminal-marshal',
  sources: 'terminal-marshal.list',
  prefix: '_terminal_marshal',
)

executable('mate-terminal',
  sources, source_enum, source_resources, marshalers,
  include_directories : [
    include_directories('.'),
    include_directories('..'),
    ],
  dependencies : term_deps,
  link_with : [
    libskey,
    ],
  c_args : cargs,
  install: true,
)
