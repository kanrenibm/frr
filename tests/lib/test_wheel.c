// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * lib/wheel timer wheel tests: slot placement, empty-slot skipping and removal
 */

#include <zebra.h>

#include "frrevent.h"
#include "linklist.h"
#include "wheel.h"

#define SLOTS 4
#define PERIOD_MSEC 40

struct item {
	unsigned int key;
	const char *name;
};

static int fail;
static struct event_loop *master;
static struct timer_wheel *wheel;

static const struct item *ran[16];
static unsigned int nran;
static unsigned int stop_after;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			fail = 1;                                              \
		}                                                              \
	} while (0)

static unsigned int item_key(const void *data)
{
	return ((const struct item *)data)->key;
}

static void item_run(void *data)
{
	if (nran < array_size(ran))
		ran[nran] = data;
	nran++;
}

static void run_until(unsigned int count)
{
	struct event ev;

	nran = 0;
	stop_after = count;
	while (nran < stop_after && event_fetch(master, &ev))
		event_call(&ev);
}

int main(void)
{
	struct item a = {1, "a"}, b = {3, "b"}, c = {5, "c"}, d = {2, "d"};

	master = event_master_create(NULL);
	wheel = wheel_init(master, PERIOD_MSEC, SLOTS, item_key, item_run,
			   "test");

	/* init state */
	CHECK(wheel->slots == SLOTS);
	CHECK(wheel->period == PERIOD_MSEC);
	CHECK(wheel->nexttime == PERIOD_MSEC / SLOTS);
	CHECK(wheel->curr_slot == 0);
	CHECK(wheel->timer != NULL);
	for (int i = 0; i < SLOTS; i++)
		CHECK(list_isempty(wheel->wheel_slot_lists[i]));

	/* placement: key modulo slots, 5 wraps to slot 1 */
	CHECK(wheel_add_item(wheel, &a) == 0);
	CHECK(wheel_add_item(wheel, &b) == 0);
	CHECK(wheel_add_item(wheel, &c) == 0);
	CHECK(listcount(wheel->wheel_slot_lists[1]) == 2);
	CHECK(listcount(wheel->wheel_slot_lists[3]) == 1);
	CHECK(listcount(wheel->wheel_slot_lists[0]) == 0);
	CHECK(listcount(wheel->wheel_slot_lists[2]) == 0);

	/* removal: only the matching item leaves its slot */
	CHECK(wheel_remove_item(wheel, &c) == 0);
	CHECK(listcount(wheel->wheel_slot_lists[1]) == 1);
	CHECK(listcount(wheel->wheel_slot_lists[3]) == 1);
	/* removing an item that is not present is harmless */
	CHECK(wheel_remove_item(wheel, &c) == 0);
	CHECK(listcount(wheel->wheel_slot_lists[1]) == 1);

	/*
	 * The first tick lands on (empty) slot 0, after which the wheel skips
	 * the empty slots and only visits slots 1 and 3, alternately.
	 */
	run_until(4);
	CHECK(nran >= 4);
	CHECK(ran[0] == &a);
	CHECK(ran[1] == &b);
	CHECK(ran[2] == &a);
	CHECK(ran[3] == &b);
	CHECK(wheel->curr_slot >= 3);

	/* remove one slot's item: the other keeps firing at full-period pace */
	CHECK(wheel_remove_item(wheel, &a) == 0);
	CHECK(list_isempty(wheel->wheel_slot_lists[1]));
	run_until(3);
	CHECK(nran >= 3);
	for (unsigned int i = 0; i < 3; i++)
		CHECK(ran[i] == &b);
	/* a single occupied slot is revisited once per full rotation */
	CHECK(wheel->slots_to_skip == SLOTS);

	/* adding to an empty slot is picked up on a later tick */
	CHECK(wheel_add_item(wheel, &d) == 0);
	CHECK(listcount(wheel->wheel_slot_lists[2]) == 1);
	run_until(1);
	CHECK(nran >= 1);

	wheel_delete(wheel);
	event_master_free(master);

	printf("%s\n", fail ? "FAILED" : "OK");
	return fail;
}
