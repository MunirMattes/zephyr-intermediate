#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

/*
 * -------------------------------------------------------------------
 * TASK2_PROTECT_WITH_MUTEX = 0:
 *
 * *** Booting Zephyr OS build v4.4.0 ***
 * [00:00:00.000,000] <inf> demo: Counting started...
 * [00:00:00.053,000] <inf> demo: Finished worker_a
 * [00:00:00.066,000] <inf> demo: Finished worker_b
 * [00:00:00.066,000] <inf> demo: Final value: 1598209
 * [00:00:00.066,000] <err> demo: Error, race condition detected
 * [00:00:00.066,000] <inf> demo: Execution time: 66 ms
 * 
 * In this case, the following condition may happen:
 * - worker_a is running and read the counter value (counter=10);
 * - Scheduler runs worker_b and worker_a stays ready;
 * - worker_b reads the counter value (counter=10, yet) and increment it (counter=11);
 * - worker_b may continue running and incrementing it until counter=55, for example;
 * - Scheduler runs worker_a again and worker_b stays ready;
 * - worker_a increments the read counter value (counter was 10 -> counter=11) and
 * store it in the counter itself;
 * - Note that, in this case, worker_b increments were lost (10 -> 55);
 * - The same effect could happen if worker_a starts running when worker_b has just
 * read the counter value and didn't increment it yet.
 * 
 * -------------------------------------------------------------------
 * TASK2_PROTECT_WITH_MUTEX = 1:
 *
 * *** Booting Zephyr OS build v4.4.0 ***
 * [00:00:00.000,000] <inf> demo: Counting started...
 * [00:00:15.428,000] <inf> demo: Finished worker_a
 * [00:00:15.448,000] <inf> demo: Finished worker_b
 * [00:00:15.448,000] <inf> demo: Final value: 2000000
 * [00:00:15.448,000] <inf> demo: Success, no race condition detected
 * [00:00:15.448,000] <inf> demo: Execution time: 15448 ms
 * 
 * In this case, the mutex prevents that worker_b starts running while
 * worker_a is performing a read-modify-write operation in counter or vice-versa.
*/
#define TASK2_PROTECT_WITH_MUTEX    1

#define STACK_SIZE 1024

/* Task priority */
#define PRIO_WORKER 5

/* Number of increments */
#define INCREMENTS  1000000

static volatile uint32_t counter = 0;

static struct k_sem done_sem;

static K_MUTEX_DEFINE(counter_mutex);

void worker_fn(void *p1, void *p2, void *p3)
{
    const char* name = k_thread_name_get(k_current_get());

    for (size_t i = 0; i < INCREMENTS; i++)
    {
    #if TASK2_PROTECT_WITH_MUTEX
        /* Protect with mutex to guarantee that the
         * counter won't lose any event */
        k_mutex_lock(&counter_mutex, K_FOREVER);
    #endif

        counter++;

    #if TASK2_PROTECT_WITH_MUTEX
        /* Unlock mutex, allowing other threads that were waiting for it to run */
        k_mutex_unlock(&counter_mutex);
    #endif
    }

    LOG_INF("Finished %s", name);

    /* Notify that worker has been finished */
    k_sem_give(&done_sem);
}

K_THREAD_DEFINE(worker_a, STACK_SIZE, worker_fn,
                NULL, NULL, NULL, PRIO_WORKER, 0, 0);
K_THREAD_DEFINE(worker_b, STACK_SIZE, worker_fn,
                NULL, NULL, NULL, PRIO_WORKER, 0, 0);

int main(void)
{
    int64_t time = k_uptime_get();

    LOG_INF("Counting started...");

    k_sem_init(&done_sem, 0, 2);

    /* Wait workers to finish */
    k_sem_take(&done_sem, K_FOREVER);
    k_sem_take(&done_sem, K_FOREVER);
    
    LOG_INF("Final value: %d", counter);

    if (counter == (INCREMENTS * 2))
    {
        LOG_INF("Success, no race condition detected");
    }
    else
    {
        LOG_ERR("Error, race condition detected");
    }

    LOG_INF("Execution time: %lld ms", k_uptime_delta(&time));

    return 0;
}
