#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>

#define MOTION_DIRECTORY	"/tmp/motion/"

#define TRIGGER_ENTRY	MOTION_DIRECTORY"trigger"
#define STATISTIC_RESULT_ENTRY	MOTION_DIRECTORY"statistic_result"
#define RAW_RESULT_ENTRY	MOTION_DIRECTORY"raw_result"

#define NSEC_PER_SEC            1000000000
typedef int64_t s64;

enum motion_status_t {
    MOTION_INIT = 0,
    MOTION_FINISH = 1,
    MOTION_IN_PROCESSING = 2
};


static int motion_status;

struct motion_statistic {	/* time unit: ns */
	s64 max;
	s64 min;
	s64 avg;
	s64 repeat;
	s64 *raw_data;
};

struct motion_statistic motion_stat = {
	.max = 0,
	.min = LLONG_MAX,
	.avg = 0,
	.repeat = 0,
	.raw_data = NULL,
};

static inline
void motion_statistic_init(struct motion_statistic *mstat)
{
	mstat->max = 0;
	mstat->min = LLONG_MAX;
	mstat->avg = 0;
}

static inline
s64 timespec_to_ns(const struct timespec ts)
{
	return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static inline int64_t timespec_sub_ns(struct timespec t1, struct timespec t2)
{
	s64 diff;
	diff = NSEC_PER_SEC * (t1.tv_sec - t2.tv_sec);
	diff += (t1.tv_nsec - t2.tv_nsec);
	return diff;
}


pthread_t motion_work_thrid;
pthread_t trigger_thrid;
pthread_t stat_result_thrid;
pthread_t raw_result_thrid;
pthread_mutex_t mutex;

inline static int max(s64 a, s64 b)
{
	return (a > b) ? a : b;
}

inline static int min(s64 a, s64 b)
{
	return (a < b) ? a : b;
}

static void clean_resource(void)
{
	if (motion_stat.raw_data)
		free(motion_stat.raw_data);

	remove(TRIGGER_ENTRY);
	remove(STATISTIC_RESULT_ENTRY);
	remove(RAW_RESULT_ENTRY);
	rmdir(MOTION_DIRECTORY);
}

static void terminate_handler(int ignore)
{
	pthread_cancel(trigger_thrid);
	pthread_cancel(stat_result_thrid);
	pthread_cancel(raw_result_thrid);
	pthread_mutex_destroy(&mutex);

	clean_resource();
}

inline static int atomic_status_get(void)
{
	int ret;

	if ((ret = pthread_mutex_trylock(&mutex)) != 0)
		return ret;
	ret = motion_status;
	pthread_mutex_unlock(&mutex);

	return ret;
}

inline static int atomic_status_set(int s)
{
	int ret;

	if ((ret = pthread_mutex_trylock(&mutex)) != 0)
		return ret;
	motion_status = s;
	pthread_mutex_unlock(&mutex);

	return ret;
}

void* motion_work_handler(void *arg)
{
	int i;
	struct timespec time1, time2;

	motion_statistic_init(&motion_stat);

	for (i = 0 ; i < motion_stat.repeat ; i++) {
		s64 ns;

		clock_gettime(CLOCK_MONOTONIC, &time1);

        // TODO: Insert Algorithm

		clock_gettime(CLOCK_MONOTONIC, &time2);

		ns = timespec_sub_ns(time2, time1);

		motion_stat.max = max(ns, motion_stat.max);
		motion_stat.min = min(ns, motion_stat.min);
		motion_stat.avg += ns;
		motion_stat.raw_data[i] = ns;
	}

	motion_stat.avg = (s64)motion_stat.avg / motion_stat.repeat;

	atomic_status_set(MOTION_FINISH);


	return NULL;
}

static void cleanup_fd(void *arg)
{
	int *fdp = (int *)arg;

	if (*fdp < 0)
		return;

	close(*fdp);
	*fdp = -1;
}

void* trigger_handler(void *arg)
{
	char buff[16];
	int trigger_fd = -1;

	pthread_cleanup_push(cleanup_fd, &trigger_fd);

	if (mkfifo(TRIGGER_ENTRY, 0644) < 0) {
		perror("fail to trigger mkfifo");
		goto trigger_out;
	}

	do {
		ssize_t n;
		pthread_attr_t attr;

		trigger_fd = -1;
		memset(buff, 0, sizeof(buff));

		trigger_fd = open(TRIGGER_ENTRY, O_RDONLY);
		if (trigger_fd < 0) {
			perror("open trigger entry");
			goto trigger_out;
		}


		int sz = 0, remain = 16;
		do {
			n = read(trigger_fd, buff + sz, 16);
			sz += n;
			remain -= sz;
		} while (n > 0 || sz == 0);
		n = strtol(buff, NULL, 10);

		if (n == LONG_MAX) { /* overflow */
			close(trigger_fd);
			continue;
		}

		if (atomic_status_get() == MOTION_IN_PROCESSING)
			continue;

		atomic_status_set(MOTION_INIT);

		if (n == 0) {
			memset(&motion_stat, 0 , sizeof(struct motion_statistic));
			close(trigger_fd);
			continue;
		}

		if (n > motion_stat.repeat)
			motion_stat.raw_data = (s64 *)realloc(motion_stat.raw_data,
							      sizeof(s64) * n);
		motion_stat.repeat = n;
		atomic_status_set(MOTION_IN_PROCESSING);

		pthread_attr_init(&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
		pthread_create(&motion_work_thrid, &attr,
		               motion_work_handler, NULL);
		pthread_attr_destroy(&attr);

		close(trigger_fd);
	} while (1);

	pthread_cleanup_pop(0);
trigger_out:
	return NULL;
}

void* stat_result_handler(void *arg)
{
	char buff[128];
	int result_fd = -1;

	pthread_cleanup_push(cleanup_fd, &result_fd);

	if (mkfifo(STATISTIC_RESULT_ENTRY, 0644) < 0) {
		perror("fail to statistic result mkfifo");
		goto result_out;
	}

	do {
		int status;
		memset(buff, 0, sizeof(buff));

		result_fd = open(STATISTIC_RESULT_ENTRY, O_WRONLY);
		if (result_fd < 0) {
			perror("open result entry");
			goto result_out;
		}

		status = atomic_status_get();
		if (status < 0 ) {
			snprintf(buff, 128, "motion in processing\n");
		} else if (status == MOTION_IN_PROCESSING) {
			/* Output again error messages */
			snprintf(buff, 128, "motion in processing\n");
		} else {
			snprintf(buff, 128, " avg = %lldns\n max = %lldns\n min = %lldns\n",
			         motion_stat.avg,
			         motion_stat.max,
			         motion_stat.min);
		}

		write(result_fd, buff, 128);
		close(result_fd);
		result_fd = -1;
		/* FIXME: Sleep here to give reader the chance to escape */
		usleep(10000);
	} while (1);

	pthread_cleanup_pop(0);
result_out:
	return NULL;
}

void* raw_result_handler(void *arg)
{
	char buff[128];
	int result_fd = -1;

	pthread_cleanup_push(cleanup_fd, &result_fd);

	if (mkfifo(RAW_RESULT_ENTRY, 0644) < 0) {
		perror("fail to raw result mkfifo");
		goto result_out;
	}

	do {
		int status, len;
		memset(buff, 0, sizeof(buff));

		result_fd = open(RAW_RESULT_ENTRY, O_WRONLY);
		if (result_fd < 0) {
			perror("open raw result entry");
			goto result_out;
		}

		status = atomic_status_get();
		if (status < 0 ) {
			len = snprintf(buff, 128, "motion in processing\n");
			write(result_fd, buff, len);
		} else if (status == MOTION_IN_PROCESSING) {
			/* Output again error messages */
			len = snprintf(buff, 128, "motion in processing\n");
			write(result_fd, buff, len);
		} else if (motion_stat.raw_data == NULL) {
			len = snprintf(buff, 128, "raw data is not available\n");
			write(result_fd, buff, len);
		} else {
			int i;
			for (i = 0 ; i < motion_stat.repeat ; i++) {
				len = snprintf(buff, 128, " test_num=%d time=%lld\n",
					       i, motion_stat.raw_data[i]);
				write(result_fd, buff, len);
			}
		}

		close(result_fd);
		result_fd = -1;
		/* FIXME: Sleep here to give reader the chance to escape */
		usleep(10000);
	} while (1);

	pthread_cleanup_pop(0);
result_out:
	return NULL;
}


int main(int argc, char *argv[])
{
	struct sigaction sigact;
	int opt = 0;

	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch(opt) {
		case 'h':
			printf("Usage: %s &\n"
			       "  test trigger: echo num > /tmp/motion/trigger,\n"
			       "                'num' is the test number.\n"
			       "  statistical result : cat /tmp/motion/statistic_result\n"
			       "  raw result: cat /tmp/motion/raw_result",
			       argv[0]);
			return 0;
		default:
			continue;
		}
	}

	if (mkdir(MOTION_DIRECTORY, 0744) < 0) {
		perror("mkdir motion path");
		goto error_out;
	}

	sigact.sa_handler = terminate_handler;
	sigemptyset(&sigact.sa_mask);
	sigaddset(&sigact.sa_mask, SIGTERM);
	sigaddset(&sigact.sa_mask, SIGINT);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGINT, &sigact, NULL);

    // TODO: Init

	pthread_mutex_init(&mutex, NULL);
	pthread_create(&trigger_thrid, NULL, trigger_handler, NULL);
	pthread_create(&stat_result_thrid, NULL, stat_result_handler, NULL);
	pthread_create(&raw_result_thrid, NULL, raw_result_handler, NULL);

	pthread_join(trigger_thrid, NULL);
	pthread_join(stat_result_thrid, NULL);
	pthread_join(raw_result_thrid, NULL);

	return 0;

error_out:
	clean_resource();
	exit(EXIT_FAILURE);
}
