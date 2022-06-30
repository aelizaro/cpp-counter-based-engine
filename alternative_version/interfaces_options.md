# Ways to to set generator's state

## State representation:

### Original interpretation

| n * w Counter |             |            |            |            | Keys (aka Seed)   |            |            |
| ---------- | ----------- |----------- |----------- |----------- |----------- |----------- |----------- |
| counter[0] | ...         | counter[c - 1] | ...        | counter[n - 1] | key[0]     | ...        | key[n/2 - 1]     |

```cpp
// in philox_engine:
void set_counter(std::initializer_list<result_type> counter);

// in user's code:
philox4x32 engine;
// set n counters (or less, the rest would be set to 0)
engine.set_counter({n, 0, idx, 0});
```

### John's interpretation

| c * w Counter |             |            | Seed (Keys + (n-c) counters) |            |          |            |            |
| ---------- | ----------- |----------- |----------- |----------- |----------- |----------- |----------- |
| counter[0] | ...         | counter[c-1] | ...        | counter[n-1] | key[0]     | ...        | key[n/2 - 1]     |

```cpp
// in counter_based_engine:
void seed(std::initializer_list<result_type> s);

// in user's code:
philox4x32 engine;
// set seed from counter[c] to the end of state
engine.seed({idx, 0, key1, key2});
```

### Alternatives

| c * w Counter |             |            | Subsequence indexes (n-c counters) |            | Keys (aka Seed) |            |            |
| ---------- | ----------- |----------- |----------- |----------- |----------- |----------- |----------- |
| counter[0] | ...         | counter[c - 1] | ...        | counter[n - 1] | key[0]     | ...        | key[n/2 - 1]     |

```cpp
// in philox_engine:
void set_counter(std::initializer_list<result_type> counter);

void set_index(std::initializer_list<result_type> idx);

// to make it one-line (could also be with seed_seq)
void set_state(std::initializer_list<result_type> counter, std::initializer_list<result_type> idx, result_type seed);

// in user's code:
philox4x32_rc<10, 2> engine(global_seed);
// set c counters (or less, the rest would be set to 0)
engine.set_counter({n, 0});
// set n-c counters (or less, the rest would be set to 0)
engine.set_idx({0, idx});

// alt:
engine.set_state({n}, {0, idx}, global_seed);
```

**_TODO_**: check if set_index may be turn off with enable_if in case n = c