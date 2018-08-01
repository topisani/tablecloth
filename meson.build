project('tablecloth', 'cpp', 'c', default_options : ['cpp_std=c++17'])

thread_dep = dependency('threads')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')
libinput = dependency('libinput')

wayland_protocols = dependency('wayland-protocols')
wayland_wall = dependency('wayland-wall')
wlroots = dependency('wlroots', fallback: ['wlroots', 'wlroots'])
wlr_protos = dependency('wlr_protos', fallback: ['wlroots', 'wlr_protos'])
fmt = dependency('fmt', fallback: ['fmtlib', 'fmt_dep'])
gtk = dependency('gtk+-3.0')
gtkmm = dependency('gtkmm-3.0')
wp_protocol_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

wayland_scanner_prog = find_program('wayland-scanner')
wayland_scanner_client = generator(wayland_scanner_prog, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(wayland_scanner_prog, output: '@BASENAME@-server-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_code = generator(wayland_scanner_prog, output: '@BASENAME@-protocol.c', arguments: ['code', '@INPUT@', '@OUTPUT@'])

waylandpp = dependency('wayland-client++')
wayland_scannerpp = generator(find_program('wayland-scanner++'), output: [
        '@BASENAME@-protocol.hpp',
        '@BASENAME@-protocol.cpp'
    ],
    arguments: ['@INPUT@', '@BUILD_DIR@/@BASENAME@-protocol.hpp', '@BUILD_DIR@/@BASENAME@-protocol.cpp']
)

subdir('common')
subdir('tablecloth')
subdir('cloth-msg')

