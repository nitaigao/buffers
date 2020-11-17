project('gfx', 'c', default_options: ['warning_level=3', 'c_std=c17'])

dependencies = [
  dependency('libdrm')
]

includes = include_directories('include')

sources = [
	'src/main.c',
	'src/device.c',
	'src/vk_device.c',
	'src/output.c'
]

executable('gfx', sources, dependencies: dependencies, include_directories: includes)
