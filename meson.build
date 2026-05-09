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

add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-D_POSIX_C_SOURCE', language: 'c')

basu = dependency('basu')
scdoc = find_program('scdoc', required: get_option('man-pages'))

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
	input: 'config.h.in',
	output: 'config.h',
	configuration: conf_data,
)

dependencies = [
	basu,
]

sources = [
	'main.c',
	'signum.c',
]

executable(
	program_name,
	sources,
	dependencies: dependencies,
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
