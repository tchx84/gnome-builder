if get_option('with_gcc')

gcc_sources = [
  'gbp-gcc-build-result-addin.c',
  'gbp-gcc-build-result-addin.h',
  'gbp-gcc-plugin.c',
]

shared_library('gcc-plugin', gcc_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('gcc.plugin', install_dir: plugindir)

endif
