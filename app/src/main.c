#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_DBG);

#define STACK_SIZE 1024

/* ================================================================== */
/*  Shared message type                                               */
/* ================================================================== */

struct sensor_msg {
    uint32_t timestamp_ms;
    int32_t value;
    uint8_t seq;
};

/* ================================================================== */
/*  Part 1 - Thread-to-thread pipeline                                */
/* ================================================================== */

#define P1_QUEUE_DEPTH 6

K_MSGQ_DEFINE(p1_q, sizeof(struct sensor_msg), P1_QUEUE_DEPTH, 4);

static void p1_producer(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "p1_prod");

    int i = 0;

    while (1) {
        struct sensor_msg msg = {
            .timestamp_ms = k_uptime_get_32(),
            .value = 100 + i,
            .seq = (uint8_t)i,
        };

        int ret = k_msgq_put(&p1_q, &msg, K_MSEC(200));
        if (ret == 0) {
            LOG_INF("[P1-PROD] sent seq=%u val=%d q=%u/%d",
                    msg.seq,
                    msg.value,
                    k_msgq_num_used_get(&p1_q),
                    P1_QUEUE_DEPTH);
        } else {
            LOG_WRN("[P1-PROD] put failed ret=%d", ret);
        }

        k_msleep(500);

        i++;
    }

    LOG_INF("[P1-PROD] done");
}

void task_wdt_callback(int channel_id, void *user_data)
{
    LOG_WRN("[TASK-WDT] Watchdog fired @ channel%d", channel_id);
}

static void p1_consumer(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "p1_cons");

    task_wdt_init(NULL); /* NULL = software-only; pass hw wdt device for fallback */

    /* Register a channel per monitored thread */
    int wdt_chan = task_wdt_add(
            1000, /* timeout ms */
            task_wdt_callback, /* called if thread misses feed */
            (void *)k_current_get());

    while (true) {
        struct sensor_msg msg = {0};

        int ret = k_msgq_get(&p1_q, &msg, K_MSEC(1500));
        if (ret != 0) {
            LOG_WRN("[P1-CONS] timeout waiting for message");
            continue;
        }

        uint32_t latency = k_uptime_get_32() - msg.timestamp_ms;

        LOG_INF("[P1-CONS] got seq=%u val=%d q=%u/%d latency=%ums",
                msg.seq,
                msg.value,
                k_msgq_num_used_get(&p1_q),
                P1_QUEUE_DEPTH,
                latency);

        task_wdt_feed(wdt_chan); /* In the thread: feed regularly */

        k_msleep(2000);
    }

    LOG_INF("[P1-CONS] done");
}

static void health_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    k_thread_name_set(k_current_get(), "health");

    while (1) {
        uint32_t used = k_msgq_num_used_get(&p1_q);
        uint32_t free_slots = k_msgq_num_free_get(&p1_q);
        uint32_t total_depth = used + free_slots;

        k_msleep(100);
        
        if (used > (total_depth * 3 / 4)) {
            LOG_WRN("[HEALTH] Queue at %d/%d", used, total_depth);
        }
    }

    LOG_INF("[HEALTH] done");
}

/* ================================================================== */
/*  Runtime threads                                                   */
/* ================================================================== */

K_THREAD_STACK_DEFINE(p1_prod_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(p1_cons_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(p1_health_stack, STACK_SIZE);

static struct k_thread p1_prod_thread;
static struct k_thread p1_cons_thread;
static struct k_thread p1_health_thread;

/* ================================================================== */
/*  Main                                                              */
/* ================================================================== */

int main(void)
{
    LOG_INF("=== L5 Task: Producer/consumer pipeline ===");

    k_msgq_purge(&p1_q);

    k_thread_create(&p1_prod_thread, p1_prod_stack,
                    K_THREAD_STACK_SIZEOF(p1_prod_stack), p1_producer, 
                    NULL, NULL, NULL, 5, 0, K_NO_WAIT);

    /*
    * Let producer get ahead.
    * This makes the queue buffer real messages instead of direct handoff.
    */
    k_msleep(180);

    k_thread_create(&p1_cons_thread,
                    p1_cons_stack,
                    K_THREAD_STACK_SIZEOF(p1_cons_stack), p1_consumer,
                    NULL, NULL, NULL, 5, 0, K_NO_WAIT);


    k_thread_create(&p1_health_thread,
                    p1_health_stack,
                    K_THREAD_STACK_SIZEOF(p1_health_stack), health_thread_fn,
                    NULL, NULL, NULL, 5, 0, K_NO_WAIT);

    return 0;
}


