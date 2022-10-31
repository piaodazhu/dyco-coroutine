example_channel = executable(
	'example_channel',
	'channel.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_coropool = executable(
	'example_coropool',
	'coropool.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_epoll = executable(
	'example_epoll',
	'epoll.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_multithread = executable(
	'example_multithread',
	'multithread.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_pubsub = executable(
	'example_pubsub',
	'pubsub.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_semaphore = executable(
	'example_semaphore',
	'semaphore.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_setstack = executable(
	'example_setstack',
	'setstack.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_signal = executable(
	'example_signal',
	'signal.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_sleep = executable(
	'example_sleep',
	'sleep.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_stop_abort = executable(
	'example_stop_abort',
	'stop_abort.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_waitgroup = executable(
	'example_waitgroup',
	'waitgroup.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_socket_client = executable(
	'example_socket_client',
	'socket_client.c',
	include_directories : header_dir,
	link_with : libdyco
)

example_socket_server = executable(
	'example_socket_server',
	'socket_server.c',
	include_directories : header_dir,
	link_with : libdyco
)


if crypto_dep.found() and ssl_dep.found() 
	example_ssl_client = executable(
		'example_ssl_client',
		'ssl_client.c',
		include_directories : header_dir,
		dependencies : [crypto_dep, ssl_dep],
		link_with : libdyco
	)

	example_ssl_server = executable(
		'example_ssl_server',
		'ssl_server.c',
		include_directories : header_dir,
		dependencies : [crypto_dep, ssl_dep],
		link_with : libdyco
	)
else
	message('libssl not found! ssl example will not be compiled.')
endif

if hiredis_dep.found()
	example_network = executable(
		'example_network',
		'network.c',
		include_directories : header_dir,
		dependencies : [hiredis_dep],
		link_with : libdyco
	)
else
	message('libhiredis not found! hiredis example will not be compiled.')
endif