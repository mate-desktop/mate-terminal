project(
  'mate-terminal', 'c',
  version: '1.26.1',
  meson_version: '>=0.47.0',
  license: 'GPL2',
  default_options : [ 'warning_level=1', 'buildtype=debugoptimized', 'c_std=gnu11'],
)

package_name = meson.project_name()
package_version = meson.project_version()

# Versionning
version_arr = package_version.split('.')
version_major = version_arr[0].to_int()
version_minor = version_arr[1].to_int()
version_micro = version_arr[2].to_int()

package_string = '@0@ @1@.0'.format(package_name, package_version)

config_h = configuration_data()
config_h.set_quoted('PACKAGE', package_name, description: 'Name of package')
config_h.set_quoted('PACKAGE_NAME', package_name, description: 'Define to the full name of this package.')
config_h.set_quoted('PACKAGE_STRING', package_string, description: 'Define to the full name and version of this package.')
config_h.set_quoted('PACKAGE_VERSION', package_version, description: 'Define to the version of this package.')
config_h.set_quoted('PACKAGE_URL', 'https://mate-desktop.org', description: 'Define to the home page for this package.')
config_h.set_quoted('GETTEXT_PACKAGE', package_name, description: 'Gettext package')
config_h.set_quoted('VERSION', package_version, description: 'Name of package')

# Compiler & Project arguments
add_project_arguments([
  '-DHAVE_CONFIG_H',
  '-I' + meson.build_root(),
  ], language: 'c')

# get suported warning flags
test_args = [
  #'-fstack-protector-strong',
  #'-Waggregate-return',
  #'-Warray-bounds',
  '-Wcast-align',
  '-Wchar-subscripts',
  #'-Wclobbered',
  #'-Wdeclaration-after-statement',
  #'-Wempty-body',
  #'-Wextra',
  #'-Wformat=2',
  #'-Wformat-nonliteral',
  #'-Wformat-security',
  #'-Werror=format-security',
  #'-Werror=format=2',
  #'-Wformat-signedness',
  #'-Wignored-qualifiers',
  #'-Wimplicit-function-declaration',
  #'-Winit-self',
  #'-Winline',
  '-Wmissing-declarations',
  #'-Wmissing-format-attribute',
  #'-Wmissing-include-dirs',
  #'-Wmissing-noreturn',
  #'-Wmissing-parameter-type',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  #'-Wno-discarded-qualifiers',
  #'-Wno-missing-field-initializers',
  #'-Wno-strict-aliasing',
  #'-Wno-suggest-attribute=format',
  #'-Wno-unused-parameter',
  #'-Wold-style-definition',
  #'-Woverride-init',
  #'-Wpacked',
  '-Wpointer-arith',
  #'-Wredundant-decls',
  #'-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  #'-Wstrict-aliasing',
  #'-Wstrict-prototypes',
  #'-Wswitch-default',
  #'-Wtype-limits',
  #'-Wundef',
  #'-Wuninitialized',
  #'-Wunused-but-set-variable',
  #'-Wwrite-strings'
  #'-Wall',
  #'-ansi',
  ]

cc = meson.get_compiler('c')

foreach arg: test_args
  if cc.has_multi_arguments(arg)
    add_project_arguments(arg, language : 'c')
  endif
endforeach

# byte order
if build_machine.endian() == 'big'
  config_h.set('WORDS_BIGENDIAN', 1)
endif

gnome = import('gnome')
i18n = import('i18n')

# Paths
rootdir = include_directories('.')

srcdir = meson.current_source_dir()
builddir = meson.current_build_dir()

prefix = get_option('prefix')

buildtype = get_option('buildtype')

bindir = join_paths(prefix, get_option('bindir'))
libdir = join_paths(prefix, get_option('libdir'))
datadir = join_paths(prefix, get_option('datadir'))
pkgdatadir = join_paths(datadir, package_name)

# Dependencies
glib_req = '>= 2.50.0'
gio_req = '>= 2.50.0'
gtk_req = '>= 3.22.0'
vte_req = '>= 0.48'
dconf_req = '>= 0.13.4'
ice_req = '>= 1.0.0'
sm_req = '>= 1.0.0'

glib_dep = dependency('glib-2.0', version: glib_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
gio_dep = dependency('gio-2.0', version: gio_req)
vte_dep = dependency('vte-2.91', version: vte_req)
dconf_dep = dependency('dconf', version: dconf_req)
x11_dep = dependency('x11')
sm_dep = dependency('sm', version: sm_req, required: false)

term_deps = [
  vte_dep,
  glib_dep,
  gio_dep,
  gtk_dep,
  dconf_dep,
  x11_dep,
]

build_skey = false
if get_option('skey')
  build_skey = true
  config_h.set('ENABLE_SKEY', 1, description: 'Define if s/key support is enabled')
endif

build_sm = false
ice_dep = dependency('ice', version: ice_req, required: false)
if get_option('sm') and sm_dep.found() and ice_dep.found()
  build_sm = true
  config_h.set('HAVE_SMCLIENT', 1)
  mate_submodules_dep = dependency('mate-submodules', version: '1.24.0',
    fallback: ['mate-submodules', 'mate_submodules_dep'])
  term_deps += [mate_submodules_dep]
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

subdir('help')
subdir('src')
subdir('po')

desktop_in = configuration_data()
desktop_in.set('VERSION', meson.project_version())

desktop_file_configured = configure_file(
  input: 'mate-terminal.desktop.in.in',
  output: 'mate-terminal.desktop.in',
  configuration: desktop_in,
)

desktop_file = i18n.merge_file(
  input: desktop_file_configured,
  output: 'mate-terminal.desktop',
  type: 'desktop',
  po_dir: join_paths(srcdir, 'po'),
  install: true,
  install_dir: join_paths(
    datadir,
    'applications',
  )
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: [
      desktop_file.full_path(),
    ]
  )
endif

install_man('mate-terminal.1')

appdata_file_configured = configure_file(
  input: 'mate-terminal.appdata.xml.in.in',
  output: 'mate-terminal.appdata.xml.in',
  configuration: config_h,
)

appdata_file = i18n.merge_file(
  input: appdata_file_configured,
  output: 'mate-terminal.appdata.xml',
  type: 'xml',
  po_dir: join_paths(srcdir, 'po'),
  install: true,
  install_dir: join_paths(
    datadir,
    'metainfo',
  )
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata',
    appstream_util,
    args: [
      'validate-relax',
      '--nonet',
      appdata_file.full_path()
    ]
  )
endif

install_data(
  'mate-terminal.wrapper',
  install_mode: 'rwxr-xr-x',
  install_dir: get_option('bindir')
  )

summary = [
  '',
  'mate-terminal-@0@:'.format(meson.project_version()),
  '',
  '	prefix:                   @0@'.format(prefix),
  '	source code location:     @0@'.format(srcdir),
  '	compiler:                 @0@'.format(cc.get_id()),
  '',
  '	smclient support:         @0@'.format(build_sm),
  '	s/key support:            @0@'.format(build_skey),
  '',
]

meson.add_install_script(
  'meson_post_install.py',
  datadir,
)

message('\n'.join(summary))
