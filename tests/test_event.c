#include "kronyx/event.h"
#include <stdio.h>
#include <string.h>

static int g_hit_a = 0;
static int g_hit_b = 0;
static int g_miss = 0;

static void on_hit_a(const char *name, const void *data, void *user) {
    g_hit_a++;
    (void)name; (void)data; (void)user;
}

static void on_hit_b(const char *name, const void *data, void *user) {
    g_hit_b++;
    (void)name; (void)data; (void)user;
}

static void on_miss(const char *name, const void *data, void *user) {
    g_miss++;
    (void)name; (void)data; (void)user;
}

int main(void) {
    printf("=== Event System Test ===\n");
    int assertions = 0, failures = 0;
    #define ASSERT(cond, msg) do { assertions++; if (!(cond)) { failures++; printf("FAIL: %s\n", msg); } else { printf("PASS: %s\n", msg); } } while (0)

    g_hit_a = 0; g_hit_b = 0; g_miss = 0;

    ASSERT(ky_event_register("hit", on_hit_a, NULL) > 0, "register hit listener a");
    ASSERT(ky_event_register("hit", on_hit_b, NULL) > 0, "register hit listener b");
    ASSERT(ky_event_register("miss", on_miss, NULL) > 0, "register miss listener");
    ASSERT(ky_event_register(NULL, on_hit_a, NULL) < 0, "null name rejected");
    ASSERT(ky_event_register("bad", NULL, NULL) < 0, "null fn rejected");
    ASSERT(g_hit_a == 0 && g_hit_b == 0 && g_miss == 0, "no callbacks before trigger");

    ky_event_trigger("hit", NULL);
    ASSERT(g_hit_a == 1 && g_hit_b == 1, "both hit listeners fire");
    ASSERT(g_miss == 0, "miss listener untouched by hit");

    g_hit_a = 0; g_hit_b = 0; g_miss = 0;
    ky_event_trigger("miss", NULL);
    ASSERT(g_miss == 1 && g_hit_a == 0 && g_hit_b == 0, "miss only fires miss listener");

    g_miss = 0;
    ky_event_trigger("noop", NULL);
    ASSERT(g_miss == 0, "unregistered event fires no listeners");

    /* cumulative across multiple triggers */
    g_hit_a = 0; g_hit_b = 0;
    ky_event_trigger("hit", NULL);
    ky_event_trigger("hit", NULL);
    ASSERT(g_hit_a == 2 && g_hit_b == 2, "cumulative hits accumulate correctly");

    /* pass payload pointer is forwarded to callback */
    int payload = 42;
    g_hit_a = 0;
    ky_event_trigger("hit", &payload);
    ASSERT(g_hit_a == 1, "event with payload still dispatches");

    printf("\n=== %d assertions, %d failures ===\n", assertions, failures);
    return failures == 0 ? 0 : 1;
}
