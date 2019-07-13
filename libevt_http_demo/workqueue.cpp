/**
 * Multithreaded work queue.
 * Copyright (c) 2012 Ronald Bennett Cemer
 * This software is licensed under the BSD license.
 * See the accompanying LICENSE.txt for details.
 */
#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "workqueue.h"

#define LL_ADD(item, list) { \
	item->prev = NULL; \
	item->next = list; \
	list = item; \
}

#define LL_REMOVE(item, list) { \
	if (item->prev != NULL) item->prev->next = item->next; \
	if (item->next != NULL) item->next->prev = item->prev; \
	if (list == item) list = item->next; \
	item->prev = item->next = NULL; \
}

void worker_function(void *ptr) {
	worker_t *worker = (worker_t *)ptr;
	job_t *job;

	while (1) {
		/* Wait until we get notified. */
		{
			//worker->workqueue->jobs_mutex->lock();
			std::unique_lock<std::mutex> lk(*worker->workqueue->jobs_mutex);

			while (worker->workqueue->waiting_jobs == NULL) {
				/* If we're supposed to terminate, break out of our continuous loop. */
				if (worker->terminate) break;
				worker->workqueue->jobs_cond->wait(lk);
			}

			/* If we're supposed to terminate, break out of our continuous loop. */
			if (worker->terminate) break;

			job = worker->workqueue->waiting_jobs;
			if (job != NULL) {
				LL_REMOVE(job, worker->workqueue->waiting_jobs);
			}
			//worker->workqueue->jobs_mutex->unlock();
		}
		/* If we didn't get a job, then there's nothing to do at this time. */
		if (job == NULL) continue;

		/* Execute the job. */
		job->job_function(job);
	}

	free(worker);
}

int workqueue_init(workqueue_t *workqueue, int numWorkers) {
	int i;
	worker_t *worker = nullptr;
	std::condition_variable blank_cond;

	if (numWorkers < 1) numWorkers = 1;
	memset(workqueue, 0, sizeof(*workqueue));
	workqueue->jobs_mutex = make_shared<mutex>();
	workqueue->jobs_cond = make_shared<std::condition_variable>();

	for (i = 0; i < numWorkers; i++) {
		if ((worker = new worker_t) == NULL) {
			perror("Failed to allocate all workers");
			return 1;
		}
		memset(worker, 0, sizeof(*worker));
		worker->workqueue = workqueue;
		worker->pthread = new thread(worker_function, worker);
		worker->pthread->join(); 

		LL_ADD(worker, worker->workqueue->workers);
	}

	return 0;
}

void workqueue_shutdown(workqueue_t *workqueue) {
	worker_t *worker = NULL;

	/* Set all workers to terminate. */
	for (worker = workqueue->workers; worker != NULL; worker = worker->next) {
		worker->terminate = 1;
	}

	/* Remove all workers and jobs from the work queue.
	 * wake up all workers so that they will terminate. */
	{
		std::lock_guard<std::mutex> guard(*workqueue->jobs_mutex);	//自解锁
		workqueue->workers = NULL;
		workqueue->waiting_jobs = NULL;
		workqueue->jobs_cond->notify_all();
	}
}

void workqueue_add_job(workqueue_t *workqueue, job_t *job) {
	/* Add the job to the job queue, and notify a worker. */
	std::lock_guard<std::mutex> guard(*workqueue->jobs_mutex);	//自解锁
	LL_ADD(job, workqueue->waiting_jobs);
	workqueue->jobs_cond->notify_one();
}
