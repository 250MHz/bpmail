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

lib_bp = cc.find_library('bp', dirs: ['/usr/local/lib'], required: true)

exe = executable(
    'bpmail',
    'test.c',
    dependencies: [lib_bp],
    include_directories: include_directories('/usr/local/include'),
)

test('basic', exe)

