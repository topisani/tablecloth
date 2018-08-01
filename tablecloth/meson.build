sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scanner_server.process('../protocol/workspaces.xml'),
    wayland_scanner_code.process('../protocol/workspaces.xml'),
]

executable('tablecloth', sources, dependencies : [thread_dep, fmt, wlroots, wlr_protos, libinput, dep_cloth_common])
