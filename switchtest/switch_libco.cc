/*
* Tencent is pleased to support the open source community by making Libco available.

* Copyright (C) 2014 THL A29 Limited, a Tencent company. All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License"); 
* you may not use this file except in compliance with the License. 
* You may obtain a copy of the License at
*
*	http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, 
* software distributed under the License is distributed on an "AS IS" BASIS, 
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
* See the License for the specific language governing permissions and 
* limitations under the License.
*/

#include "co_routine_specific.h"
#include "co_routine.h"
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <sys/time.h>

using namespace std;

#define TOTAL_SWITCH_COUNTER	10000000
#define SHARED_STACK
static int count;
struct timeval tv1, tv2;

void* cofunc(void* args)
{
	// printf("hello\n");
	while (1) {
		if (count == 0) {
			gettimeofday(&tv1, NULL);
		}
		
		// printf("ok\n");
		++count;
		poll(NULL, 0, 0);
		++count;

		if (count > TOTAL_SWITCH_COUNTER) {
			gettimeofday(&tv2, NULL);
			time_t diff = (tv2.tv_sec - tv1.tv_sec) * 1000000 + (tv2.tv_usec - tv1.tv_usec);
			double speed = 1.0 * diff / TOTAL_SWITCH_COUNTER;
			printf("diff = %ld us, switch speed = %f M/s\n", diff, speed);
			exit(0);
		}
	}
	return NULL;
}

int main()
{
	count = 0;
	for (int i = 0; i < 2; i++)
	{
		args[i].routine_id = i;
		// co_create(&args[i].co, NULL, RoutineFunc, (void*)&args[i]);
		co_create(&args[i].co, NULL, cofunc, NULL);
		coresume(args[i].co);
	}
	co_eventloop(co_get_epoll_ct(), NULL, NULL);
	return 0;
}
