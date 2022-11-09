project('DOCA-FLOW-DROP', 'C', 'CPP',
	# Get version number from file.
	# Fallback to "more" for Windows compatibility.
	version: run_command(find_program('cat', 'more'),
		files('VERSION')).stdout().strip(),
	license: 'BSD',
	default_options: ['buildtype=debug',
			  'default_library=static'],
	meson_version: '>= 0.47.1'
)

# set up some global vars for compiler, platform, configuration, etc.
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

add_project_arguments('-DALLOW_EXPERIMENTAL_API', language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')

if host_machine.cpu_family().startswith('x86')
	add_project_arguments('-march=native', language: 'c')
	add_project_arguments('-mavx', language: 'c')
	arch_subdir = 'x86'
elif host_machine.cpu_family().startswith('arm') or host_machine.cpu_family().startswith('aarch')
	arch_subdir = 'arm'
elif host_machine.cpu_family().startswith('ppc')
	arch_subdir = 'ppc'
endif

# Uncomment this line to remove warnings of experimental compile
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: 'c')

libdpdk_dep = dependency('libdpdk', required : true)
if not libdpdk_dep.found()
	error('please set PKG_CONFIG_PATH to dpdk packages path, for further instructions check DOCA manuals')
endif
docacommon_dep = dependency('doca-common', required : true)
docaflow_dep = dependency('doca-flow', required : true)
# thread_dep = dependency('threads', required : true)
# libutils_dep = dependency('doca-utils', required : true)
# libdpi_dep = dependency('doca-dpi', required : true)
# libcommon_dep = dependency('doca-common', required : true)

flow_drop_src = [
	'./flow_drop.c',
	'./../common/flow_common.c',
]

common_dir = include_directories('./../common')

executable('doca_flow_drop', flow_drop_src, dependencies : [docacommon_dep, docaflow_dep, libdpdk_dep], include_directories : [common_dir], install: false)