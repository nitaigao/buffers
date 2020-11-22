project('gfx', 'c', default_options: ['warning_level=3', 'c_std=c17'])

c = meson.get_compiler('c')
vulkan = c.find_library('vulkan', dirs: '/usr/local/lib', required: true)
gbm = c.find_library('gbm', dirs: '/usr/local/lib', required: true)

dependencies = [
  dependency('libdrm'),
  gbm,
  vulkan
]

includes = include_directories('include')

executable_sources = [
	'src/main.c',
	'src/device.c',
	'src/vk_device.c',
	'src/output.c'
]

# generate vulkan shaders
shaders = []
glslang = find_program('glslangValidator')
shader_sources = [
	'shader.vert',
	'shader.frag',
]

foreach shader : shader_sources
	name = shader.underscorify() + '_data'
	args = [glslang, '-V', '@INPUT@', '-o', '@OUTPUT@', '--vn', name]
	header = custom_target(
		shader + '_spv',
		output: shader + '.h',
		input: shader,
		command: args)
	shaders += [header]
endforeach

executable('gfx', [
  executable_sources,
  shaders
], dependencies: dependencies, include_directories: includes)
