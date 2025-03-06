deps = [lib_bp, lib_dtpc, lib_ici]
incdir = include_directories('/usr/local/include', is_system: true)

bpmailsend_exe = executable(
    'bpmailsend',
    'bpmailsend.c',
    dependencies: deps,
    include_directories: incdir,
)

bpmailrecv_exe = executable(
    'bpmailrecv',
    'bpmailrecv.c',
    dependencies: deps,
    include_directories: incdir,
)

