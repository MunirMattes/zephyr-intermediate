/*
 * temp_alarm - Error-path unit tests (homework skeleton)
 *
 * Task 2: fill in the stubs according to TEST_SPEC.md. Each stub calls
 * ztest_test_skip(), so the binary builds and runs cleanly until the test is
 * implemented.
 *
 * Run:
 *   west twister -T tests/temp_alarm -p native_sim
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/* Same before() hook as in test_basic.c: a clean fake and an initialised module. */
static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	sensor_fake_reset();
	sensor_fake_init();

	zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
		   "precondition: temp_alarm_init must succeed");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_sensor_errors
 *
 * Failures from the sensor driver, injected through the fakes.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
	/* TODO(l8-task2): a failed fetch is reported,
	 * and the module does not go on to read the channel.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #1.
	 */
	int ret;
	struct temp_alarm_status status;

	fake_sensor_sample_fetch_fake.custom_fake = NULL;
	fake_sensor_sample_fetch_fake.return_val = -EIO;

	ret = sensor_sample_fetch(&fake_sensor_dev);
	zassume_equal(ret, -EIO, "precondition: sample_fetch must be -EIO, got %d", ret);
	
	ret = temp_alarm_check();
	zassert_equal(ret, -EIO, "check must propagate -EIO, got %d", ret);

	zassume_ok(temp_alarm_get_status(&status), "precondition: get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d", status.last_error);

	zassert_equal(fake_sensor_channel_get_fake.call_count, 0, 
		"fake_sensor_channel_get must not be called after fetch error, got %u calls",
		fake_sensor_channel_get_fake.call_count);
}

ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
	/* TODO(l8-task2): a failed channel read is reported.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #2.
	 */
	int ret;
	struct sensor_value val;
	struct temp_alarm_status status;

	fake_sensor_channel_get_fake.custom_fake = NULL;
	fake_sensor_channel_get_fake.return_val = -EIO;

	ret = sensor_channel_get(&fake_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	zassume_equal(ret, -EIO, "precondition: sensor_channel_get must be -EIO, got %d", ret);
	
	ret = temp_alarm_check();
	zassert_equal(ret, -EIO, "check must propagate -EIO, got %d", ret);

	zassume_ok(temp_alarm_get_status(&status), "precondition: get_status must succeed");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d", status.last_error);

	zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1, 
		"fake_sensor_sample_fetch must be called 1 time, got %u calls",
		fake_sensor_sample_fetch_fake.call_count);
}

ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
	/* TODO(l8-task2): the first fetch fails and the second one succeeds.
	 * The module must recover on the second check.
	 * See TEST_SPEC.md "Suite temp_alarm_sensor_errors" #3.
	 */
	int ret;
	struct temp_alarm_status status;

	/* First try: -EIO */
	fake_sensor_sample_fetch_fake.custom_fake = NULL;
	fake_sensor_sample_fetch_fake.return_val = -EIO;

	ret = sensor_sample_fetch(&fake_sensor_dev);
	zassume_equal(ret, -EIO, "precondition: sample_fetch must be -EIO, got %d", ret);
	
	ret = temp_alarm_check();
	zassert_equal(ret, -EIO, "check must propagate -EIO, got %d", ret);

	/* Second try: 0 @ 20 C */
	fake_sensor_sample_fetch_fake.return_val = 0;
	sensor_fake_set_temperature(20);
	
	zassert_ok(temp_alarm_check(), "check must succeed");

	zassume_ok(temp_alarm_get_status(&status), "precondition: get_status must succeed");
	
	zassert_equal(status.last_temp, 20, "last_temp must be 20 after set to 20 C, got %d",
			status.last_temp);
	zassert_false(status.is_alarming, "is_alarming must be false with 20 C");
	zassert_equal(status.last_error, -EIO, "last_error must be -EIO, got %d", status.last_error);
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_invalid_input
 *
 * Arguments the public API must reject.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
	/* TODO(l8-task2): a NULL status pointer is rejected.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #1.
	 */
	int ret;

	ret = temp_alarm_get_status(NULL);
	zassert_equal(ret, -EINVAL, "get_status with NULL must be -EINVAL, got %d", ret);
}

ZTEST(temp_alarm_invalid_input, test_init_without_device)
{
	/* TODO(l8-task2): init without a sensor device is rejected.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #2.
	 */
	int ret;

	ret = temp_alarm_init(NULL, 30);
	zassert_equal(ret, -ENODEV, "init with NULL must be -ENODEV, got %d", ret);
}

ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
	/* TODO(l8-task2): init rejects thresholds outside the allowed range.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #3.
	 */
	int ret;

	ret = temp_alarm_init(&fake_sensor_dev, -1);
	zassert_equal(ret, -EINVAL, "init with -1 must be -EINVAL, got %d", ret);
	
	ret = temp_alarm_init(&fake_sensor_dev, 101);
	zassert_equal(ret, -EINVAL, "init with 101 must be -EINVAL, got %d", ret);
}

ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
	/* TODO(l8-task2): set_threshold rejects values outside the allowed range,
	 * and keeps the old threshold.
	 * See TEST_SPEC.md "Suite temp_alarm_invalid_input" #4.
	 */
	int ret;

	ret = temp_alarm_set_threshold(-1);
	zassert_equal(ret, -EINVAL, "set_threshold with -1 must be -EINVAL, got %d", ret);
	
	ret = temp_alarm_set_threshold(101);
	zassert_equal(ret, -EINVAL, "set_threshold with 101 must be -EINVAL, got %d", ret);
}
