if get_option('with_autotools')

autotools_sources = [
  'autotools-plugin.c',
  'ide-autotools-application-addin.c',
  'ide-autotools-application-addin.h',
  'ide-autotools-builder.c',
  'ide-autotools-builder.h',
  'ide-autotools-build-system.c',
  'ide-autotools-build-system.h',
  'ide-autotools-build-target.c',
  'ide-autotools-build-target.h',
  'ide-autotools-build-task.c',
  'ide-autotools-build-task.h',
  'ide-autotools-project-miner.c',
  'ide-autotools-project-miner.h',
  'ide-makecache.c',
  'ide-makecache.h',
  'ide-makecache-target.c',
  'ide-makecache-target.h',
]

make_exe_name = 'make'
if find_program('gmake', required: false).found()
  make_exe_name = 'gmake'
endif

shared_library('autotools-plugin', autotools_sources,
  dependencies: plugin_deps,
  c_args: ['-DGNU_MAKE_NAME="@0@"'.format(make_exe_name)],
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('autotools.plugin', install_dir: plugindir)

endif
