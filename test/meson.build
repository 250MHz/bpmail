py3_exe = import('python').find_installation(
    'python3',
    required: false,
    modules: ['pytest'],
    disabler: true,
)

if not py3_exe.found()
    warning('Could not find python3 or pytest, tests will be skipped')
endif

test(
    'pytest',
    py3_exe,
    args: ['-m', 'pytest', '--capture=tee-sys', meson.project_source_root() + '/test'],
    env: {
        'TEST_BPMAILSEND_BINARY': bpmailsend_exe.full_path(),
        'TEST_BPMAILRECV_BINARY': bpmailrecv_exe.full_path(),
        'TEST_DIR': meson.project_source_root() + '/test',
    },
    timeout: -1,
)

