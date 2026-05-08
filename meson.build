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

basu = dependency('basu')
scdoc = find_program('scdoc', required: get_option('man-pages'))

dependencies = [
	basu,
]

sources = [
	'main.c',
]

inc = include_directories('include')

executable('fprintd-unlock',
	sources,
	include_directories: [inc],
	dependencies: dependencies,
	install: true,
)

if scdoc.found()
	mandir = get_option('mandir')
	custom_target('fprintd-unlock.1',
		input: 'fprintd-unlock.1.scd',
		output: 'fprintd-unlock.1',
		command: scdoc,
		feed: true,
		capture: true,
		install: true,
		install_dir: mandir / 'man1',
	)
endif
