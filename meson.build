project (
	'fprintd-unlock',
	'c',
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=1.1',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')



scdoc = find_program('scdoc', required: get_option('man-pages'))

if get_option('sd-bus-provider') == 'auto'
	sdbus = dependency('libsystemd', 'libelogind', 'basu')
else
	sdbus = dependency(get_option('sd-bus-provider'))
endif
add_project_arguments('-DHAVE_' + sdbus.name().to_upper(), language: 'c')



bsd_args = ['-D_DEFAULT_SOURCE', '-D_BSD_SOURCE']
linux_args = ['-D_GNU_SOURCE']
if cc.has_header_symbol('signal.h', 'sys_signame', args: bsd_args)
	add_project_arguments('-DHAVE_SYS_SIGNAME', language: 'c')
	add_project_arguments(bsd_args, language: 'c')
elif cc.has_function('sigabbrev_np', args: linux_args, prefix: '#include <string.h>')
	add_project_arguments('-DHAVE_SIGABBREV_NP', language: 'c')
	add_project_arguments(linux_args, language: 'c')
else
	warning('Neither sys_signame nor sigabbrev_np is available. Signal name lookup will be disabled, requiring signal numbers only.')
endif



program_name = get_option('program-name')
if program_name == ''
	program_name = meson.project_name()
endif

default_signal = get_option('default-signal')
default_locker = get_option('default-locker')

conf_data = configuration_data()
conf_data.set('program-name', program_name)
conf_data.set('version', meson.project_version())
conf_data.set('default-signal', default_signal)
conf_data.set('default-locker', default_locker)

configure_file(
	input: 'include/config.h.in',
	output: 'config.h',
	configuration: conf_data,
)



dependencies = [
	sdbus,
]

sources = [
	'main.c',
	'signum.c',
]

fpu_inc = include_directories('include')

executable(
	program_name,
	sources,
	dependencies: dependencies,
	include_directories: [fpu_inc],
	install: true,
)



if scdoc.found()
	man_scd = configure_file(
		input: 'fprintd-unlock.1.scd.in',
		output: program_name + '.1.scd',
		configuration: conf_data,
	)

	mandir = get_option('mandir')
	custom_target(
		program_name + '.1',
		input: man_scd,
		output: program_name + '.1',
		command: scdoc,
		feed: true,
		capture: true,
		install: true,
		install_dir: mandir / 'man1',
	)
endif
