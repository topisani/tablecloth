sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

wlr_protocol_dir = '../subprojects/wlroots/protocol/'
cloth_protocol_dir = '../protocol/'

protocols = [
	[wp_protocol_dir, 'unstable/xdg-shell/xdg-shell-unstable-v6.xml'],
	[wp_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	[wp_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
	[wlr_protocol_dir, 'idle.xml'],
	[wlr_protocol_dir, 'screenshooter.xml'],
	[wlr_protocol_dir, 'wlr-export-dmabuf-unstable-v1.xml'],
	[wlr_protocol_dir, 'wlr-input-inhibitor-unstable-v1.xml'],
	[wlr_protocol_dir, 'wlr-layer-shell-unstable-v1.xml'],
	[wlr_protocol_dir, 'wlr-screencopy-unstable-v1.xml'],
	[cloth_protocol_dir, 'tablecloth-shell.xml'],
]

xml_files = []

foreach p : protocols
	xml = join_paths(p)
	xml_files += xml
endforeach

protocol_sources = custom_target('gen-protocols',
    input: xml_files,
    output: ['protocols.hpp', 'protocols.cpp'],
    command: [find_program('wayland-scanner++'), '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'])

sources += protocol_sources

executable('cloth-bar', sources, dependencies : [thread_dep, fmt, wlroots, wlr_protos, libinput, wayland_cursor_dep, dep_cloth_common, waylandpp, gtkmm])
