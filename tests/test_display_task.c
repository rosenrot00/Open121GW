#include "display_task.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
    display_task_init(100u);
    assert(display_task_update_due(100u));
    assert(display_task_bargraph_update_due(100u));

    display_task_schedule_after(100u, 250u);
    assert(!display_task_update_due(349u));
    assert(display_task_update_due(350u));
    assert(display_task_update_due(351u));

    display_task_schedule_after(351u, 250u);
    assert(!display_task_update_due(600u));
    display_task_request_now();
    assert(display_task_update_due(600u));

    display_task_schedule_bargraph_after(100u, 50u);
    assert(!display_task_bargraph_update_due(149u));
    assert(display_task_bargraph_update_due(150u));

    display_task_schedule_after(0xFFFFFFF0u, 32u);
    assert(!display_task_update_due(0u));
    assert(display_task_update_due(16u));

    display_task_schedule_bargraph_after(500u, 0u);
    assert(display_task_bargraph_update_due(500u));
    return 0;
}
