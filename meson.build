project('tablecloth', 'cpp', default_options : ['cpp_std=c++17'])

thread_dep = dependency('threads')
swc_dep = declare_dependency(link_args : '-lswc')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')

sources = run_command('find', 'src', '-name', '*.cpp').stdout().strip().split('\n')

executable('tablecloth', sources, dependencies : [swc_dep, thread_dep, xkbcommon_dep, wayland_server_dep])

