project('tablecloth', 'cpp', 'c', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
cpp_args = []
cpp_link_args = []

if false # libc++
    cpp_args += ['-stdlib=libc++']
    cpp_link_args += ['-stdlib=libc++', '-lc++abi']

    cpp_link_args += ['-lc++fs']
else
    # TODO: For std::filesystem in libstdc++. Still unstable? Or why is it not in libstdc++ proper yet?
    cpp_link_args += ['-lstdc++fs']
endif

add_global_arguments(cpp_args, language : 'cpp')
add_global_link_arguments(cpp_link_args, language : 'cpp')


thread_dep = dependency('threads')
xkbcommon_dep = dependency('xkbcommon ')
wayland_server_dep = dependency('wayland-server')
wayland_client_dep = dependency('wayland-client')
wayland_cursor_dep = dependency('wayland-cursor')
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

dbus_dep = dependency('dbus-c++-1')
dbus_proxy = generator(find_program('dbusxx-xml2cpp'), output: '@BASENAME@-proxy.hpp', arguments: ['@INPUT@', '--proxy=@OUTPUT@'])
dbus_adaptor = generator(find_program('dbusxx-xml2cpp'), output: '@BASENAME@-adaptor.hpp', arguments: ['@INPUT@', '--adaptor=@OUTPUT@'])

waylandpp = dependency('wayland-client++')
wayland_scannerpp = generator(find_program('wayland-scanner++'), output: [
        '@BASENAME@-protocol.hpp',
        '@BASENAME@-protocol.cpp'
    ],
    arguments: ['@INPUT@', '@BUILD_DIR@/@BASENAME@-protocol.hpp', '@BUILD_DIR@/@BASENAME@-protocol.cpp']
)

subdir('common')
#subdir('tablecloth')
#subdir('cloth-msg')
#subdir('cloth-bar')
subdir('cloth-notifications')
subdir('cloth-lock')
#subdir('cloth-kbd')
#subdir('cloth-outputs')
