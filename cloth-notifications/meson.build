sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

wlr_protocol_dir = '../wlr-proto/'
cloth_protocol_dir = '../protocol/'

protocols = [
	[wp_protocol_dir, 'unstable/xdg-shell/xdg-shell-unstable-v6.xml'],
	[wp_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	[wlr_protocol_dir, 'wlr-layer-shell-unstable-v1.xml'],
]

xml_files = []

foreach p : protocols
	xml = join_paths(p)
	xml_files += xml
endforeach

dbus_dep = dependency('dbus-c++-1')

protocol_sources = custom_target('gen-notifications-protocols',
    input: xml_files,
    output: ['protocols.hpp', 'protocols.cpp'],
    command: [find_program('wayland-scanner++'), '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'])

sources += protocol_sources

dbus_sources = custom_target('gen-notifications-dbus',
    input: [join_paths(cloth_protocol_dir, 'dbus-notifications.xml')],
    output: ['dbus-notifications-proxy.hpp', 'dbus-notifications-adaptor.hpp'],
    command: [find_program('bash'), '-c', 'dbusxx-xml2cpp @INPUT0@ --proxy=@OUTPUT0@; dbusxx-xml2cpp @INPUT0@ --adaptor=@OUTPUT1@'])

sources += dbus_sources

executable('cloth-notifications', sources, dependencies : [thread_dep, dbus_dep, dbus_c_dep, fmt, wlroots, libinput, wayland_cursor_dep, dep_cloth_common, waylandpp, gtkmm])
