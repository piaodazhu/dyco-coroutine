project(
	'dyco',
	'c',
	version: 'v2.1.1',
	license : 'GPLv3',
	default_options: []
)
project_description = 'a dynamic coroutine framework for C'

header_dir = include_directories('include')

project_headers = [
	'include/dyco/dyco_coroutine.h',
	'include/dyco/dyco_htable.h',
	'include/dyco/dyco_ucontext.h',
	'include/dyco/sys_queue.h',
	'include/dyco/sys_tree.h',
	'include/dyco/uctx_amd64.h',
	'include/dyco/uctx_mips.h',
	'include/dyco/uctx_power.h',
	'include/dyco/uctx_386.h'
]

project_source_files = [
	'src/dyco_channel.c',
	'src/dyco_epoll.c',
	'src/dyco_schedcall.c',
	'src/dyco_signal.c',
	'src/dyco_waitgroup.c',
	'src/dyco_coropool.c',
	'src/dyco_hook.c',
	'src/dyco_schedule.c',
	'src/dyco_socket.c',
	'src/uctx_context.c',
	'src/dyco_coroutine.c',
	'src/dyco_asymcoro.c',
	'src/dyco_pubsub.c',
	'src/dyco_semaphore.c',
	'src/dyco_ssl.c',
	'src/uctx_asm.S'
]

build_args = [
	'-O3',
	'-lpthread',
	'-Wall',
	'-Wno-unused-but-set-variable',
	'-Wno-unused-variable',
	'-Wno-unused-function'
]

build_args += [
  '-DPROJECT_NAME=' + meson.project_name(),
  '-DPROJECT_VERSION=' + meson.project_version(),
]

project_dep = [
]

crypto_dep = dependency('libcrypto', required : false)
ssl_dep = dependency('libssl', required : false)
hiredis_dep = dependency('hiredis', required : false)

if crypto_dep.found() and ssl_dep.found() 
	build_args += [
		'-D DYCO_SSL_OK'
	]
	project_dep += [crypto_dep, ssl_dep]
	subdir('cert')
endif
# # Only make public interfaces visible
# if target_machine.system() == 'windows' or target_machine.system() == 'cygwin'
#   build_args += '-DMYLIB_PUBLIC="__declspec(dllexport)"'
# else
#   build_args += '-DMYLIB_PUBLIC=__attribute__((visibility("default")))'
# endif

libdyco = shared_library(
	meson.project_name(),
	project_source_files,
	dependencies: project_dep,
	install : true,
	c_args : build_args,
	include_directories : header_dir,
)

# Make this library usable as a Meson subproject.
libdyco_dep = declare_dependency(
	include_directories: header_dir,
	link_with : libdyco
)
set_variable(meson.project_name() + '_dep', libdyco_dep)

# Make this library usable from the system's
# package manager.
install_headers(project_headers, subdir : meson.project_name())

pkg_mod = import('pkgconfig')
pkg_mod.generate(
	name : meson.project_name(),
	filebase : meson.project_name(),
	description : project_description,
	libraries : libdyco,
)

subdir('example')