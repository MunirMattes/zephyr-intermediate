#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

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

