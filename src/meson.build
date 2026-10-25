zlib_dep = dependency('zlib')
cares_dep = dependency('libcares')
gmime_dep = dependency('gmime-3.0')
deps = [ion_dep, lib_dtpc, zlib_dep, cares_dep, gmime_dep]
incdir = include_directories(
    ion_dep.get_variable(pkgconfig: 'includedir'),
    is_system: true,
)

bpmailsend_exe = executable(
    'bpmailsend',
    'bpmailsend.c',
    dependencies: deps,
    include_directories: incdir,
    install: true,
)

bpmailrecv_exe = executable(
    'bpmailrecv',
    'bpmailrecv.c',
    dependencies: deps,
    include_directories: incdir,
    install: true,
)
