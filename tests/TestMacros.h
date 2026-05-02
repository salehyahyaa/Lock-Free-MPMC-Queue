#pragma once

/// Records one assertion with the source expression preserved for CSV / failures.
#define QUEUE_TEST_RECORD(recorder, cond, id, desc, expected, owner) \
    (recorder).record(!!(cond), (id), (desc), (expected), (owner), #cond)
