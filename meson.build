project('wavelet-denoise', 'c',
  version: '0.5',
  license: 'GPL-2.0-only',
  meson_version: '>= 0.60.0',
  default_options: ['warning_level=1', 'buildtype=debugoptimized'],
)

gimp_dep = dependency('gimp-3.0', version: '>= 3.0')
gimpui_dep = dependency('gimpui-3.0', version: '>= 3.0')
math_dep = meson.get_compiler('c').find_library('m', required: false)

# GIMP 3 loads each plug-in from a folder of the same name as its binary.
plugindir = get_option('plugindir')
if plugindir == ''
  plugindir = gimp_dep.get_variable(pkgconfig: 'gimplibdir') / 'plug-ins'
endif
plugindir = plugindir / meson.project_name()

executable(meson.project_name(),
  'src/colorspace.c',
  'src/denoise.c',
  'src/interface.c',
  'src/plugin.c',
  'src/wavelet.c',
  dependencies: [gimp_dep, gimpui_dep, math_dep],
  c_args: ['-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name())],
  install: true,
  install_dir: plugindir,
)

subdir('po')
