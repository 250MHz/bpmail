third_party_inc = include_directories('.')

dovecot_parser_lib = static_library(
    'dovecot-parser',
    files('dovecot-parser.c'),
    c_args: ['-w', '-D_POSIX_C_SOURCE=200809L'],
    implicit_include_directories: true
)

