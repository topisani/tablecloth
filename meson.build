project('tablecloth', 'cpp', default_options : ['cpp_std=c++17'])

thread_dep = dependency('threads')
swc_dep = declare_dependency(link_args : '-lswc')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')
libweston_desktop = dependency('libweston-desktop-4')
libweston = dependency('libweston-4')
libinput = dependency('libinput')

wayland_scanner = find_program('wayland-scanner')
wayland_protocols = dependency('wayland-protocols')
wayland_wall = dependency('wayland-wall')
wp_protocol_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

wayland_scanner_client = generator(wayland_scanner, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(wayland_scanner, output: '@BASENAME@-server-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_code = generator(wayland_scanner, output: '@BASENAME@-protocol.cpp', arguments: ['code', '@INPUT@', '@OUTPUT@'])

sources = run_command('find', 'src', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scanner_server.process('protocol/weston-desktop-shell.xml'),
    wayland_scanner_code.process('protocol/weston-desktop-shell.xml'),
    wayland_scanner_server.process(join_paths(wp_protocol_dir, 'unstable', 'input-method', 'input-method-unstable-v1.xml')),
    wayland_scanner_code.process(join_paths(wp_protocol_dir, 'unstable', 'input-method', 'input-method-unstable-v1.xml')),
    wayland_scanner_server.process('protocol/workspaces.xml'),
    wayland_scanner_code.process('protocol/workspaces.xml'),
]

executable('tablecloth', sources, dependencies : [thread_dep, xkbcommon_dep, wayland_server_dep, libweston_desktop, libweston, libinput])
