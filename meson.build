project(
    'bpmail',
    'c',
    version: '0.1.0',
    default_options: ['warning_level=3', 'c_std=c17'],
)

cc = meson.get_compiler('c')

if get_option('broad_warnings')
    if cc.get_id() == 'gcc'
        add_project_arguments('-Wshadow', language: 'c')
    elif cc.get_id() == 'clang'
        add_project_arguments('-Wshadow-all', language: 'c')
    endif
    add_project_arguments(
        '-Wpointer-arith',
        '-Wcast-qual',
        '-Wcast-align',
        '-Wstrict-prototypes',
        '-Wmissing-prototypes',
        '-Wconversion',
        language: 'c',
    )
endif

add_project_arguments('-D_POSIX_C_SOURCE=202405L', language: 'c')

ion_dep = dependency('ion')
lib_dtpc = cc.find_library(
    'dtpc',
    dirs: [ion_dep.get_variable(pkgconfig: 'libdir')],
    required: true,
)

subdir('src')
subdir('test')

install_man('man/bpmailsend.1', 'man/bpmailrecv.1')
