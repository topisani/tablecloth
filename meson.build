project('tablecloth', 'cpp', 'c', default_options : ['cpp_std=c++17'])

thread_dep = dependency('threads')
swc_dep = declare_dependency(link_args : '-lswc')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')
wayland_client_dep = dependency('wayland-client')
libweston_desktop = dependency('libweston-desktop-4')
libweston = dependency('libweston-4')
libinput = dependency('libinput')

wayland_protocols = dependency('wayland-protocols')
wayland_wall = dependency('wayland-wall')
waylandpp = dependency('wayland-client++')
waylandpp_extra = dependency('wayland-client-extra++')
gtk = dependency('gtk+-3.0')
gtkmm = dependency('gtkmm-3.0')
wp_protocol_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

wayland_scannerpp = generator(find_program('wayland-scanner++'), output: [
        '@BASENAME@-protocol.hpp',
        '@BASENAME@-protocol.cpp'
    ],
    arguments: ['@INPUT@', '@BUILD_DIR@/@BASENAME@-protocol.hpp', '@BUILD_DIR@/@BASENAME@-protocol.cpp']
)

wayland_scanner_prog = find_program('wayland-scanner')
wayland_scanner_client = generator(wayland_scanner_prog, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(wayland_scanner_prog, output: '@BASENAME@-server-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_code = generator(wayland_scanner_prog, output: '@BASENAME@-protocol.c', arguments: ['code', '@INPUT@', '@OUTPUT@'])

sources = run_command('find', 'src', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scannerpp.process('protocol/weston-desktop-shell.xml'),
    wayland_scannerpp.process(join_paths(wp_protocol_dir, 'unstable', 'input-method', 'input-method-unstable-v1.xml')),
    wayland_scannerpp.process('protocol/workspaces.xml'),
]

executable('tablecloth', sources, dependencies : [thread_dep, xkbcommon_dep, waylandpp, waylandpp_extra, libweston_desktop, libweston, libinput, gtk, gtkmm])
