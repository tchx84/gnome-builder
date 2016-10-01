if get_option('with_flatpak')

flatpak_sources = [
  'gbp-flatpak-runtime-provider.c',
  'gbp-flatpak-runtime-provider.h',
  'gbp-flatpak-runtime.c',
  'gbp-flatpak-runtime.h',
  'gbp-flatpak-plugin.c',
]

flatpak_deps = plugin_deps + [
  dependency('flatpak', version: '>=0.6.0'),
]

shared_library('flatpak-plugin', flatpak_sources,
  dependencies: flatpak_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('flatpak.plugin', install_dir: plugindir)

endif
