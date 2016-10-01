plugindir = join_paths(get_option('libdir'), 'gnome-builder/plugins')
plugindatadir = join_paths(get_option('datadir'), 'gnome-builder/plugins')
plugin_deps = [
  libpeas_dep,
  libide_dep,
]

plugin_link_deps = join_paths(meson.current_source_dir(), 'plugins.map')
plugin_link_args = [
  '-Wl,--version-script,' + plugin_link_deps,
]

subdir('autotools')
subdir('autotools-templates')
subdir('build-tools')
subdir('c-pack')
subdir('clang')
subdir('color-picker')
subdir('command-bar')
subdir('comment-code')
subdir('contributing')
subdir('create-project')
subdir('ctags')
subdir('devhelp')
subdir('file-search')
subdir('flatpak')
subdir('fpaste')
subdir('gcc')
subdir('gettext')
subdir('git')
subdir('gnome-code-assistance')
#subdir('hello-cpp') # needs libidemm (impossible)
subdir('html-completion')
subdir('html-preview')
subdir('jedi')
subdir('jhbuild')
subdir('mingw')
subdir('project-tree')
subdir('python-gi-imports-completion')
subdir('python-pack')
subdir('quick-highlight')
subdir('support')
subdir('symbol-tree')
subdir('sysmon')
subdir('sysprof')
subdir('terminal')
subdir('todo')
subdir('vala-pack')
subdir('xml-pack')
