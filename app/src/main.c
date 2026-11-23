#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);

/*
 * "Commit the latency output from the original priorities (showing deadline 
 * misses and the miss count) and from the corrected priorities, run with the 
 * same workload and detector."
 * 
 * The latency output using CONTROL_PRIORITY=7 is 40 ms:
 * 
*** Booting Zephyr OS build v4.4.0 ***
[00:00:00.000,000] <inf> homework: === L6 Homework: Runtime Investigation ===
[00:00:00.000,000] <inf> homework: Control work must start within 10 ms
[00:00:00.000,000] <inf> homework: Inspect, measure, trace, explain, and correct the delay
[00:00:00.545,000] <inf> homework: [CONTROL] processed seq=0
[00:00:00.545,000] <wrn> homework: deadline_miss count=1 seq=0 latency_ms=40
[00:00:00.795,000] <inf> homework: [CONTROL] processed seq=1
[00:00:00.795,000] <wrn> homework: deadline_miss count=2 seq=1 latency_ms=40
[00:00:01.045,000] <inf> homework: [CONTROL] processed seq=2
[00:00:01.045,000] <wrn> homework: deadline_miss count=3 seq=2 latency_ms=40
[00:00:01.295,000] <inf> homework: [CONTROL] processed seq=3
[00:00:01.295,000] <wrn> homework: deadline_miss count=4 seq=3 latency_ms=40
[00:00:01.545,000] <inf> homework: [CONTROL] processed seq=4
[00:00:01.545,000] <wrn> homework: deadline_miss count=5 seq=4 latency_ms=40
*/

/*
 * "Capture a CTF trace, decode it with Babeltrace, and commit the relevant 
 * scheduler events (control and maintenance thread switches around 
 * event_ready/event_done), relating them to the delayed control work."
 * 
 * Both tasks become ready when event_timer_expiry is called inside
 * ISR context. As maintenance task has higher priority than control one,
 * it starts execution right after timer ISR returns. It has a heavy workload
 * (~45ms) and only after this period it allows the control task to starts
 * execution by waiting semaphore to be taken.
 * 
[21:00:00.490000000] (+0.000000000) idle: 
 * # Timer ISR -> control and maintenance tasks ready
[21:00:00.500000000] (+0.010000000) isr_enter: 
[21:00:00.500000000] (+0.000000000) timer_expiry_enter: { id = 134628620 }
[21:00:00.500000000] (+0.000000000) msgq_put_enter: { id = 134628696, timeout = 0 }
[21:00:00.500000000] (+0.000000000) thread_sched_ready: { thread_id = 134629056, name = "control" }
[21:00:00.500000000] (+0.000000000) msgq_put_exit: { id = 134628696, timeout = 0, ret = 0 }
[21:00:00.500000000] (+0.000000000) semaphore_give_enter: { id = 134628760 }
[21:00:00.500000000] (+0.000000000) thread_sched_ready: { thread_id = 134628896, name = "maintenance" }
[21:00:00.500000000] (+0.000000000) semaphore_give_exit: { id = 134628760 }
[21:00:00.500000000] (+0.000000000) named_event: { name = "event_ready", arg0 = 0, arg1 = 0 }
[21:00:00.500000000] (+0.000000000) timer_expiry_exit: { id = 134628620 }
[21:00:00.500000000] (+0.000000000) timer_expiry_enter: { id = 134632548 }
[21:00:00.500000000] (+0.000000000) timer_expiry_exit: { id = 134632548 }
[21:00:00.500000000] (+0.000000000) isr_exit: 
 * # Maintenance starts execution (higher priority task)
[21:00:00.500000000] (+0.000000000) thread_switched_out: { thread_id = 134633216, name = "idle" }
[21:00:00.500000000] (+0.000000000) thread_switched_in: { thread_id = 134628896, name = "maintenance" }
[21:00:00.500000000] (+0.000000000) semaphore_take_exit: { id = 134628760, timeout = 4294957296, ret = 0 }
[21:00:00.500000000] (+0.000000000) thread_busy_wait_enter: { usec_to_wait = 45000 }
 * # Heavy workload (~45ms)
[21:00:00.545000000] (+0.005000000) thread_busy_wait_exit: { usec_to_wait = 45000 }
[21:00:00.545000000] (+0.000000000) semaphore_take_enter: { id = 134628760, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) semaphore_take_blocking: { id = 134628760, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) thread_sched_pend: { thread_id = 134628896, name = "maintenance" }
 * # Control starts execution
[21:00:00.545000000] (+0.000000000) thread_switched_out: { thread_id = 134628896, name = "maintenance" }
[21:00:00.545000000] (+0.000000000) thread_switched_in: { thread_id = 134629056, name = "control" }
[21:00:00.545000000] (+0.000000000) msgq_get_exit: { id = 134628696, timeout = 4294957296, ret = 0 }
[21:00:00.545000000] (+0.000000000) timer_start: { id = 134630464, duration = 1000000, period = 0 }
[21:00:00.545000000] (+0.000000000) named_event: { name = "event_done", arg0 = 0, arg1 = 40 }
[21:00:00.545000000] (+0.000000000) msgq_get_enter: { id = 134628696, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) msgq_get_blocking: { id = 134628696, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) thread_sched_pend: { thread_id = 134629056, name = "control" }
 * # Back to idle
[21:00:00.545000000] (+0.000000000) thread_switched_out: { thread_id = 134629056, name = "control" }
[21:00:00.545000000] (+0.000000000) thread_switched_in: { thread_id = 134633216, name = "idle" }
[21:00:00.545000000] (+0.000000000) idle: 
*/

/*
 * "Record the kernel shell output (e.g. kernel thread list) showing the priorities 
 * and states of the control and maintenance threads."
 * 
uart:~$ kernel thread list
Scheduler: 0 since last call
Threads:
 0x8064620 maintenance
        options: 0x0, priority: 4 timeout: 0
        state: pending, entry: 0x8049c32
        stack size 2048, unused 2024, usage 24 / 2048 (1 %)

 0x80646c0 control   
        options: 0x0, priority: 7 timeout: 0
        state: pending, entry: 0x8049c58
        stack size 2048, unused 2024, usage 24 / 2048 (1 %)
...
*/

/*
 * "Add a short write-up that defines the response-time guarantee with its start and 
 * end events (and why DEADLINE_MS is 5 ms when the starter states 10 ms), explains 
 * the root cause, and justifies the priority correction."
 * 
 * Start: the timer ISR queues the event (ready_ms / event_ready).
 * End: the control thread takes the event from the queue and starts its work. 
 * In the code, latency is measured just after k_msgq_get and the LOG_INF.
 * So, the response-time is expected to be very low (< 1ms).
 * 
 * Root cause: one timer ISR wakes both threads. A lower number means higher priority, 
 * so maintenance (pri=4) outranks control (pri=7). It busy-waits for 45 ms 
 * without blocking. control can't run until it finishes, so latency is about 45 ms, 
 * which breaks the 10 ms limit on every event.
 * Priority should follow how urgent the deadline is. control has a short deadline and 
 * little work, so it should be higher. maintenance has no short deadline, so being 
 * delayed by control's few milliseconds costs nothing. Its load also still fits well 
 * within the 250 ms period, so it isn't starved.

 * Priority change (7 -> 3):

kernel thread list
Scheduler: 0 since last call
Threads:
 0x8064620 maintenance
        options: 0x0, priority: 4 timeout: 0
        state: pending, entry: 0x8049c32
        stack size 2048, unused 2024, usage 24 / 2048 (1 %)                                                        
                                                                                                                   
 0x80646c0 control                                                                                                 
        options: 0x0, priority: 3 timeout: 0                                                                       
        state: pending, entry: 0x8049c58                                                                           
        stack size 2048, unused 2024, usage 24 / 2048 (1 %)
...

* Log after changing priority:

*** Booting Zephyr OS build v4.4.0 ***
[00:00:00.000,000] <inf> homework: === L6 Homework: Runtime Investigation ===
[00:00:00.000,000] <inf> homework: Control work must start within 10 ms
[00:00:00.000,000] <inf> homework: Inspect, measure, trace, explain, and correct the delay
[00:00:00.500,000] <inf> homework: [CONTROL] processed seq=0
[00:00:00.750,000] <inf> homework: [CONTROL] processed seq=1
[00:00:01.000,000] <inf> homework: [CONTROL] processed seq=2
[00:00:01.250,000] <inf> homework: [CONTROL] processed seq=3
[00:00:01.500,000] <inf> homework: [CONTROL] processed seq=4
[00:00:01.750,000] <inf> homework: [CONTROL] processed seq=5
[00:00:02.000,000] <inf> homework: [CONTROL] processed seq=6
[00:00:02.250,000] <inf> homework: [CONTROL] processed seq=7
[00:00:02.500,000] <inf> homework: [CONTROL] processed seq=8
[00:00:02.750,000] <inf> homework: [CONTROL] processed seq=9
[00:00:03.000,000] <inf> homework: [CONTROL] processed seq=10

* Trace after changing priority:

[21:00:00.490000000] (+0.000000000) idle: 
 * # Timer ISR -> control and maintenance tasks ready
[21:00:00.500000000] (+0.010000000) isr_enter: 
[21:00:00.500000000] (+0.000000000) timer_expiry_enter: { id = 134628620 }
[21:00:00.500000000] (+0.000000000) msgq_put_enter: { id = 134628696, timeout = 0 }
[21:00:00.500000000] (+0.000000000) thread_sched_ready: { thread_id = 134629056, name = "control" }
[21:00:00.500000000] (+0.000000000) msgq_put_exit: { id = 134628696, timeout = 0, ret = 0 }
[21:00:00.500000000] (+0.000000000) semaphore_give_enter: { id = 134628760 }
[21:00:00.500000000] (+0.000000000) thread_sched_ready: { thread_id = 134628896, name = "maintenance" }
[21:00:00.500000000] (+0.000000000) semaphore_give_exit: { id = 134628760 }
[21:00:00.500000000] (+0.000000000) named_event: { name = "event_ready", arg0 = 0, arg1 = 0 }
[21:00:00.500000000] (+0.000000000) timer_expiry_exit: { id = 134628620 }
[21:00:00.500000000] (+0.000000000) timer_expiry_enter: { id = 134632548 }
[21:00:00.500000000] (+0.000000000) timer_expiry_exit: { id = 134632548 }
[21:00:00.500000000] (+0.000000000) isr_exit: 
 * # Now control starts execution first (higher priority task)
[21:00:00.500000000] (+0.000000000) thread_switched_out: { thread_id = 134633216, name = "idle" }
[21:00:00.500000000] (+0.000000000) thread_switched_in: { thread_id = 134629056, name = "control" }
[21:00:00.500000000] (+0.000000000) msgq_get_exit: { id = 134628696, timeout = 4294957296, ret = 0 }
[21:00:00.500000000] (+0.000000000) timer_start: { id = 134630464, duration = 1000000, period = 0 }
[21:00:00.500000000] (+0.000000000) named_event: { name = "event_done", arg0 = 0, arg1 = 0 }
[21:00:00.500000000] (+0.000000000) msgq_get_enter: { id = 134628696, timeout = 4294957296 }
[21:00:00.500000000] (+0.000000000) msgq_get_blocking: { id = 134628696, timeout = 4294957296 }
[21:00:00.500000000] (+0.000000000) thread_sched_pend: { thread_id = 134629056, name = "control" }
 * # Maintenance starts execiction with eavy workload (~45ms)
[21:00:00.500000000] (+0.000000000) thread_switched_out: { thread_id = 134629056, name = "control" }
[21:00:00.500000000] (+0.000000000) thread_switched_in: { thread_id = 134628896, name = "maintenance" }
[21:00:00.500000000] (+0.000000000) semaphore_take_exit: { id = 134628760, timeout = 4294957296, ret = 0 }
[21:00:00.500000000] (+0.000000000) thread_busy_wait_enter: { usec_to_wait = 45000 }
...
[21:00:00.545000000] (+0.005000000) thread_busy_wait_exit: { usec_to_wait = 45000 }
[21:00:00.545000000] (+0.000000000) semaphore_take_enter: { id = 134628760, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) semaphore_take_blocking: { id = 134628760, timeout = 4294957296 }
[21:00:00.545000000] (+0.000000000) thread_sched_pend: { thread_id = 134628896, name = "maintenance" }
 * # Back to idle
[21:00:00.545000000] (+0.000000000) thread_switched_out: { thread_id = 134628896, name = "maintenance" }
[21:00:00.545000000] (+0.000000000) thread_switched_in: { thread_id = 134633216, name = "idle" }
[21:00:00.545000000] (+0.000000000) idle: 
*/
#define STACK_SIZE            2048
#define CONTROL_PRIORITY         3
#define MAINTENANCE_PRIORITY     4
#define EVENT_PERIOD_MS        250
#define MAINTENANCE_LOAD_US  45000
#define LOG_ALL_MISSED_EVENTS    0

struct control_event {
    uint32_t seq;
    uint32_t ready_ms;
};

K_MSGQ_DEFINE(control_queue, sizeof(struct control_event), 4, 4);
K_SEM_DEFINE(maintenance_start, 0, 1);

/* ================================================================== */
/*  Timer expiry: creates one control event                           */
/* ================================================================== */

static void event_timer_expiry(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    static uint32_t seq;
    struct control_event event = {
        .seq = seq++,
        .ready_ms = k_uptime_get_32(),
    };

    /* Timer expiry runs in interrupt context, so never wait here. */
    int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);

    if (ret != 0) {
        return;
    }

    /* Both threads become ready when the timer interrupt returns. */
    k_sem_give(&maintenance_start);

    /* TODO: Add an application trace event for this sequence. */
    sys_trace_named_event("event_ready", event.seq, 0);
}

K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);

/* ================================================================== */
/*  Control thread                                                   */
/* ================================================================== */

static void control_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (true) {
        struct control_event event;
        int ret = k_msgq_get(&control_queue, &event, K_FOREVER);

        if (ret != 0) {
            LOG_ERR("[CONTROL] receive failed: %d", ret);
            continue;
        }

        LOG_INF("[CONTROL] processed seq=%u", event.seq);

        /* TODO: Define a response-time guarantee. */
        /* Control work must start within 10 ms */
        #define DEADLINE_MS     10
        uint32_t latency_ms = (k_uptime_get_32() - event.ready_ms);
        static uint32_t deadline_misses = 0;

    #if LOG_ALL_MISSED_EVENTS
        /* TODO: Measure latency and count every deadline miss. */
        if (latency_ms > DEADLINE_MS) {
            deadline_misses++;
            LOG_WRN(
                "deadline_miss count=%u seq=%u latency_ms=%u",
                deadline_misses, event.seq, latency_ms);
        }
    #else
        /* TODO: Rate-limit repeated warning messages. */
        if (latency_ms > DEADLINE_MS) {
            deadline_misses++;
            LOG_WRN_RATELIMIT(
                "deadline_miss count=%u seq=%u latency_ms=%u",
                deadline_misses, event.seq, latency_ms);
        }
    #endif

        /* TODO: Add an application trace event for completion. */
        sys_trace_named_event("event_done", event.seq, latency_ms);
    }
}

/* ================================================================== */
/*  Background maintenance thread                                    */
/* ================================================================== */

static void maintenance_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (true) {
        k_sem_take(&maintenance_start, K_FOREVER);

        /* This work is important, but it has no short deadline. */
        k_busy_wait(MAINTENANCE_LOAD_US);
    }
}

K_THREAD_DEFINE(control, STACK_SIZE, control_fn,
                NULL, NULL, NULL, CONTROL_PRIORITY, 0, 0);

K_THREAD_DEFINE(maintenance, STACK_SIZE, maintenance_fn,
                NULL, NULL, NULL, MAINTENANCE_PRIORITY, 0, 0);

int main(void)
{
    LOG_INF("=== L6 Homework: Runtime Investigation ===");
    LOG_INF("Control work must start within 10 ms");
    LOG_INF("Inspect, measure, trace, explain, and correct the delay");

    k_timer_start(&event_timer, K_MSEC(500), K_MSEC(EVENT_PERIOD_MS));

    return 0;
}
