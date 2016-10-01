if get_option('with_xml_pack')

xml_pack_sources = [
  'ide-xml-highlighter.c',
  'ide-xml-highlighter.h',
  'ide-xml-indenter.c',
  'ide-xml-indenter.h',
  'ide-xml.c',
  'ide-xml.h',
  'xml-pack-plugin.c',
]

shared_library('xml-pack-plugin', xml_pack_sources,
  dependencies: plugin_deps,
  link_args: plugin_link_args,
  link_depends: plugin_link_deps,
  install: true,
  install_dir: plugindir,
)

install_data('xml-pack.plugin', install_dir: plugindir)

endif
