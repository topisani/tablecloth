project('tablecloth', 'cpp', 'c', default_options : ['cpp_std=c++17'])

thread_dep = dependency('threads')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')
libinput = dependency('libinput')

wayland_protocols = dependency('wayland-protocols')
wayland_wall = dependency('wayland-wall')
wlroots = dependency('wlroots', fallback: ['wlroots', 'wlroots'])
fmt = dependency('fmt', fallback: ['fmtlib', 'fmt_dep'])
gtk = dependency('gtk+-3.0')
gtkmm = dependency('gtkmm-3.0')
wp_protocol_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')
ww_protocol_dir = wayland_wall.get_pkgconfig_variable('pkgdatadir')

wayland_scanner_prog = find_program('wayland-scanner')
wayland_scanner_client = generator(wayland_scanner_prog, output: '@BASENAME@-client-protocol.h', arguments: ['client-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_server = generator(wayland_scanner_prog, output: '@BASENAME@-protocol.h', arguments: ['server-header', '@INPUT@', '@OUTPUT@'])
wayland_scanner_code = generator(wayland_scanner_prog, output: '@BASENAME@-protocol.c', arguments: ['code', '@INPUT@', '@OUTPUT@'])

sources = run_command('find', 'src', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scanner_server.process(join_paths(wp_protocol_dir, 'unstable', 'xdg-shell', 'xdg-shell-unstable-v6.xml')),
    wayland_scanner_code.process(join_paths(wp_protocol_dir, 'unstable', 'xdg-shell', 'xdg-shell-unstable-v6.xml')),
]

executable('tablecloth', sources, dependencies : [thread_dep, fmt, xkbcommon_dep, wlroots, libinput, gtk, gtkmm])
