zlib_dep = dependency('zlib')
cares_dep = dependency('libcares')
gmime_dep = dependency('gmime-3.0')
deps = [lib_bp, lib_dtpc, lib_ici, zlib_dep, cares_dep, gmime_dep]
incdir = include_directories('/usr/local/include', is_system: true)

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
