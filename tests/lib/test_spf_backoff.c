// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * spf_backoff (IETF SPF delay) state machine tests
 */
#include <zebra.h>

#include "spf_backoff.h"
#include "frrevent.h"

static struct event_loop *master;
static struct event *t_stop;
static int fail;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL: line %d: %s\n", __LINE__, #cond);        \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static void stop_cb(struct event *event)
{
}

/* run the event loop until `msec` have elapsed */
static void run_for(long msec)
{
	struct event ev;

	event_add_timer_msec(master, stop_cb, NULL, msec, &t_stop);
	while (t_stop && event_fetch(master, &ev)) {
		bool is_stop = (ev.func == stop_cb);

		event_call(&ev);
		if (is_stop)
			break;
	}
}

/* delays: init 10, short 20, long 30; timetolearn 100ms, holddown 300ms */
static void test_accessors(void)
{
	struct spf_backoff *b = spf_backoff_new(master, "acc", 10, 20, 30, 300,
						100);

	CHECK(spf_backoff_init_delay(b) == 10);
	CHECK(spf_backoff_short_delay(b) == 20);
	CHECK(spf_backoff_long_delay(b) == 30);
	CHECK(spf_backoff_holddown(b) == 300);
	CHECK(spf_backoff_timetolearn(b) == 100);
	spf_backoff_free(b);
	spf_backoff_free(NULL);
}

static void test_state_machine(void)
{
	struct spf_backoff *b = spf_backoff_new(master, "sm", 10, 20, 30, 300,
						100);

	/* QUIET -> SHORT_WAIT */
	CHECK(spf_backoff_schedule(b) == 10);
	CHECK(spf_backoff_schedule(b) == 20);

	/* timetolearn elapses -> LONG_WAIT; holddown was restarted */
	run_for(150);
	CHECK(spf_backoff_schedule(b) == 30);
	CHECK(spf_backoff_schedule(b) == 30);

	/* holddown elapses without further events -> QUIET */
	run_for(450);
	CHECK(spf_backoff_schedule(b) == 10);

	/* freeing with timers pending must cancel them cleanly */
	spf_backoff_free(b);
}

static void test_holddown_before_learn(void)
{
	/* holddown shorter than timetolearn: back to QUIET, never LONG */
	struct spf_backoff *b = spf_backoff_new(master, "hd", 1, 2, 3, 50, 500);

	CHECK(spf_backoff_schedule(b) == 1);
	CHECK(spf_backoff_schedule(b) == 2);
	run_for(120);
	CHECK(spf_backoff_schedule(b) == 1);
	spf_backoff_free(b);
}

int main(int argc, char **argv)
{
	master = event_master_create(NULL);

	test_accessors();
	test_state_machine();
	test_holddown_before_learn();

	event_master_free(master);
	return fail;
}
