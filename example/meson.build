coropool_example = executable(
	'asymmetric_example',
	'asymmetric.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

channel_example = executable(
	'channel_example',
	'channel.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

coropool_example = executable(
	'coropool_example',
	'coropool.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

epoll_example = executable(
	'epoll_example',
	'epoll.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

multithread_example = executable(
	'multithread_example',
	'multithread.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

pubsub_example = executable(
	'pubsub_example',
	'pubsub.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

semaphore_example = executable(
	'semaphore_example',
	'semaphore.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

setstack_example = executable(
	'setstack_example',
	'setstack.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

signal_example = executable(
	'signal_example',
	'signal.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

sleep_example = executable(
	'sleep_example',
	'sleep.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

stop_abort_example = executable(
	'stop_abort_example',
	'stop_abort.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

waitgroup_example = executable(
	'waitgroup_example',
	'waitgroup.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

socket_client_example = executable(
	'socket_client_example',
	'socket_client.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)

socket_server_example = executable(
	'socket_server_example',
	'socket_server.c',
	include_directories : header_dir,
	c_args : build_args,
	link_with : libdyco
)


if crypto_dep.found() and ssl_dep.found() 
	ssl_client_example = executable(
		'ssl_client_example',
		'ssl_client.c',
		include_directories : header_dir,
		c_args : build_args,
		dependencies : [crypto_dep, ssl_dep],
		link_with : libdyco
	)

	ssl_server_example = executable(
		'ssl_server_example',
		'ssl_server.c',
		include_directories : header_dir,
		c_args : build_args,
		dependencies : [crypto_dep, ssl_dep],
		link_with : libdyco
	)
else
	message('libssl not found! ssl example will not be compiled.')
endif

if hiredis_dep.found()
	network_example = executable(
		'network_example',
		'network.c',
		include_directories : header_dir,
		c_args : build_args,
		dependencies : [hiredis_dep],
		link_with : libdyco
	)
else
	message('libhiredis not found! hiredis example will not be compiled.')
endif