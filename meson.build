project('bpmail', 'c', version: '0.1.0', default_options: ['warning_level=3'])

cc = meson.get_compiler('c')

lib_bp = cc.find_library('bp', dirs: ['/usr/local/lib'], required: true)

exe = executable(
    'bpmail',
    'test.c',
    dependencies: [lib_bp],
    include_directories: include_directories('/usr/local/include'),
)

test('basic', exe)

