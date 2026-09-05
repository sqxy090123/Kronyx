#include "kronyx/input.h"
#include <stdio.h>

int main(void) {
    printf("=== Input System Test ===\n");
    int assertions = 0, failures = 0;
    #define ASSERT(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while (0)

    ky_input_reset();

    /* empty queue */
    kyInputEvent ev;
    ASSERT(ky_input_poll(&ev) == 0, "poll on empty queue returns 0");

    /* simulate + poll order */
    ky_input_simulate_key(KY_KEY_A, 1);
    ky_input_simulate_key(KY_KEY_B, 1);
    ky_input_simulate_key(KY_KEY_A, 0);
    ASSERT(ky_input_poll(&ev) == 1 && ev.key == KY_KEY_A && ev.pressed == 1, "first event is A press");
    ASSERT(ky_input_poll(&ev) == 1 && ev.key == KY_KEY_B && ev.pressed == 1, "second event is B press");
    ASSERT(ky_input_poll(&ev) == 1 && ev.key == KY_KEY_A && ev.pressed == 0, "third event is A release");
    ASSERT(ky_input_poll(&ev) == 0, "queue empty after drain");

    /* reset clears */
    ky_input_simulate_key(KY_KEY_SPACE, 1);
    ky_input_reset();
    ASSERT(ky_input_poll(&ev) == 0, "reset empties the buffer");

    /* ring overflow drops oldest */
    for (int i = 0; i < 300; i++)
        ky_input_simulate_key((kyInputKey)(KY_KEY_A + (i % 26)), 1);
    /* should keep last 256 entries; first event seen here is from index 44 */
    int kept = 0;
    while (ky_input_poll(&ev)) {
        if (ev.key >= KY_KEY_A && ev.key <= KY_KEY_Z) kept++;
    }
    ASSERT(kept == 256, "ring buffer keeps 256 entries on overflow");

    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
