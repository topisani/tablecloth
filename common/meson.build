sources = run_command('find', '.', '-name', '*.cpp').stdout().strip().split('\n')

lib_cloth_common = static_library('cloth-common', sources, include_directories: include_directories('.'), dependencies : [thread_dep, fmt, wlroots, wlr_protos, libinput])

dep_cloth_common = declare_dependency(link_with: lib_cloth_common, include_directories: include_directories('.'))
