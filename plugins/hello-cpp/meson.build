if get_option('with_hello_cpp')

add_languages('cpp')

hello_cpp_sources = [
	'hellocppapplicationaddin.cc',
	'hellocppapplicationaddin.h',
	'hellocppplugin.cc',
]

shared_library('hello-cpp-plugin', hello_cpp_sources,
  dependencies: plugin_deps,
  install: true,
  install_dir: plugindir,
)

install_data('hello-cpp.plugin', install_dir: plugindir)

endif
