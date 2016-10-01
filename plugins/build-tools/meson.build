if get_option('with_build_tools')

build_tools_resources = gnome.compile_resources(
  'gbp-build-resources',
  'gbp-build-tools.gresource.xml',
  c_name: 'gbp_build_tools',
)

build_tools_sources = [
  'gbp-build-configuration-row.c',
  'gbp-build-configuration-row.h',
  'gbp-build-configuration-view.c',
  'gbp-build-configuration-view.h',
  'gbp-build-log-panel.c',
  'gbp-build-log-panel.h',
  'gbp-build-panel.c',
  'gbp-build-panel.h',
  'gbp-build-perspective.c',
  'gbp-build-perspective.h',
  'gbp-build-plugin.c',
  'gbp-build-tool.c',
  'gbp-build-tool.h',
  'gbp-build-workbench-addin.c',
  'gbp-build-workbench-addin.h',
  'ide-environment-editor-row.c',
  'ide-environment-editor-row.h',
  'ide-environment-editor.c',
  'ide-environment-editor.h',
  build_tools_resources[0],
]

shared_library('build-tools-plugin', build_tools_sources,
  dependencies: plugin_deps,
  c_args: ['-DG_LOG_DOMAIN="build-tools"'],
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('build-tools.plugin', install_dir: plugindir)

endif
