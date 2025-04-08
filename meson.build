project(
    'bpmail',
    'c',
    version: '0.1.0',
    default_options: ['warning_level=3', 'c_std=c17'],
)

cc = meson.get_compiler('c')

if get_option('broad_warnings')
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

if build_machine.system() == 'freebsd'
    add_project_arguments('-D__BSD_VISIBLE', language: 'c')
endif

lib_bp = cc.find_library('bp', dirs: ['/usr/local/lib'], required: true)
lib_dtpc = cc.find_library('dtpc', dirs: ['/usr/local/lib'], required: true)
lib_ici = cc.find_library('ici', dirs: ['/usr/local/lib'], required: true)
lib_z = cc.find_library('z', required: true)

subdir('src')
subdir('test')
