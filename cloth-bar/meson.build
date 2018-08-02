sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

sources += [
    wayland_scannerpp.process('../protocol/tablecloth-shell.xml'),
]

executable('cloth-bar', sources, dependencies : [thread_dep, fmt, wlroots, wlr_protos, libinput, dep_cloth_common, waylandpp, gtkmm])
