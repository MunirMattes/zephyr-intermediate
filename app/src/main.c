#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

/* Enable to run task 2 */
#define TASK2_MUTEX     1

#if defined(TASK2_MUTEX)

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
        /* Protect with mutex to guarantee that the
         * counter won't lose any event */
        k_mutex_lock(&counter_mutex, K_FOREVER);
        counter++;
        k_mutex_unlock(&counter_mutex);
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

#else

#define PRIO_COOP   -1
#define PRIO_HIGH   3
#define PRIO_MED    5
#define PRIO_LOW    7

void t_coop_fn(void *p1, void *p2, void *p3)
{
    LOG_INF("T_COOP Heavy work");

    for (size_t i = 0; i < 5; i++)
    {
        /* Heavy work */
        k_busy_wait(75000);
        LOG_INF("T_COOP step %d/5", (i + 1));
    }

    LOG_INF("T_COOP k_yield");
    k_yield();
    
    LOG_INF("T_COOP done");
}

void t_high_fn(void *p1, void *p2, void *p3)
{
    while (1) {
        k_msleep(100);
        LOG_INF("T_HIGH running");
    }
}

void t_med_fn(void *p1, void *p2, void *p3)
{
    while (1) {
        k_msleep(200);
        LOG_INF("T_MED running");
    }
}

void t_low_fn(void *p1, void *p2, void *p3)
{
    while (1) {
        k_msleep(300);
        LOG_INF("T_LOW running");
    }
}

K_THREAD_DEFINE(thread_coop, STACK_SIZE, t_coop_fn,
                NULL, NULL, NULL, PRIO_COOP, 0, 0);
K_THREAD_DEFINE(thread_high, STACK_SIZE, t_high_fn,
                NULL, NULL, NULL, PRIO_HIGH, 0, 0);
K_THREAD_DEFINE(thread_med, STACK_SIZE, t_med_fn,
                NULL, NULL, NULL, PRIO_MED, 0, 0);
K_THREAD_DEFINE(thread_low, STACK_SIZE, t_low_fn,
                NULL, NULL, NULL, PRIO_LOW, 0, 0);

int main(void)
{
    return 0;
}

#endif

