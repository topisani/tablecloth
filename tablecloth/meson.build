sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scanner_server.process('../protocol/tablecloth-shell.xml'),
    wayland_scanner_code.process('../protocol/tablecloth-shell.xml'),
]

executable('tablecloth', sources, dependencies : [thread_dep, fmt, wlroots, wlr_protos, libinput, dep_cloth_common, gtkmm])
