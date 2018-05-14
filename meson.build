project('honas', 'c', default_options: ['c_std=c99'])

#########################
#  Build configuration  #
#########################

add_global_arguments('-D_GNU_SOURCE', language : 'c')
compiler = meson.get_compiler('c')

inc = include_directories(['include', '.'])

conf_data = configuration_data()
conf_data.set_quoted('VERSION', '1.0.0')
conf_data.set_quoted('DEFAULT_HONAS_GATHER_CONFIG_PATH', get_option('default_honas_gather_config_path'))
conf_data.set('USE_ASM', get_option('use_asm'))
conf_data.set('HAS_128BIT_INTEGERS', compiler.compiles('unsigned __int128 func(void) { return 1; }', name: '128-bit integer support'))
conf_data.set('HAS_BUILTIN_POPCOUNTLL', compiler.has_function('popcountll'))
conf_data.set('HAS_BUILTIN_POPCOUNTL', compiler.has_function('popcountl'))
conf_data.set('HAS_BUILTIN_POPCOUNT', compiler.has_function('popcount'))
configure_file(
	input : 'include/defines.h.in',
    output : 'defines.h',
    configuration : conf_data
)


##################
#  Dependencies  #
##################

m_dep = compiler.find_library('m', required: false)
yajl_dep = dependency('yajl', version: '>=2.1.0')
check_dep = dependency('check', version: '>=0.9.10')
openssl_dep = dependency('openssl', version: '>=1.0.1')
libevent_dep = dependency('libevent', version: '>=2.1')
fstrm_dep = dependency('libfstrm')

#######################
#  Honas executables  #
#######################

honas_src = ['src/honas_state.c', 'src/bloom.c', 'src/byte_slice.c', 'src/hyperloglog.c', 'src/combinations.c', 'src/logging.c']

gather_src = honas_src + ['src/bin/honas_gather.c']
gather_src += ['src/honas_gather_config.c', 'src/utils.c', 'src/config.c', 'src/read_file.c', 'src/inet.c', 'src/utils.c']
gather_src += ['src/inet.c', 'src/utils.c']
executable('honas-gather', gather_src, include_directories: inc, install: true, dependencies: [m_dep, openssl_dep, libevent_dep, fstrm_dep])

search_src = honas_src + ['src/bin/honas_search.c']
search_src += ['src/json_printer.c', 'src/utils.c']
executable('honas-search', search_src, include_directories: inc, install: true, dependencies: [m_dep, openssl_dep, yajl_dep])

info_src = honas_src + ['src/bin/honas_info.c']
executable('honas-info', info_src, include_directories: inc, install: true, dependencies: [m_dep, openssl_dep])

###############
#  Unittests  #
###############

test_main_src = ['tests/main.c', 'src/logging.c']

test_basics_src = test_main_src + ['tests/basic.c']
test_basics_exe = executable('test_basics', test_basics_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('basic tests', test_basics_exe)

test_bitset_src = test_main_src + ['tests/bitset.c', 'src/byte_slice.c']
test_bitset_exe = executable('test_bitset', test_bitset_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('bitset tests', test_bitset_exe)

test_byte_slice_src = test_main_src + ['tests/byte_slice.c', 'src/byte_slice.c']
test_byte_slice_exe = executable('test_byte_slice', test_byte_slice_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('byte slice tests', test_byte_slice_exe)

test_combinations_src = test_main_src + ['tests/combinations.c', 'src/combinations.c']
test_combinations_exe = executable('test_combinations', test_combinations_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('combinations tests', test_combinations_exe)

test_hyperloglog_src = test_main_src + ['tests/hyperloglog.c', 'src/hyperloglog.c', 'src/byte_slice.c']
test_hyperloglog_exe = executable('test_hyperloglog', test_hyperloglog_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('hyperloglog tests', test_hyperloglog_exe, env: ['CK_DEFAULT_TIMEOUT=90'])

test_bloom_src = test_main_src + ['tests/bloom.c', 'src/bloom.c', 'src/byte_slice.c']
test_bloom_exe = executable('test_bloom', test_bloom_src, include_directories: inc, build_by_default: false, dependencies: [check_dep])
test('bloom tests', test_bloom_exe)

##########################
#  Static code analysis  #
##########################

# If clang-tidy is installed, add target for checking all source files
clang_tidy = find_program('clang-tidy', required: false)
if clang_tidy.found()
	run_target(
		'clang-tidy-all',
		command: ['find', 'src/', 'tests/', '-type', 'f', '-name', '*.c', '-exec',
			clang_tidy.path(), '{}', '--', '-D_GNU_SOURCE', '-Iinclude', '-Ibuild', ';']
	)
endif


##############################
#  Documentation generation  #
##############################

run_target('doc', command: [ 'doxygen' ])
