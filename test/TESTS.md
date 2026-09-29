# Testing Guide

This document describes the test infrastructure of the scroller project.

<!-- Use \mainpage to make this the main page in Doxygen -->

<!-- \mainpage Testing Guide -->

## Overview

The project uses a **three-level** test hierarchy:

```
                    make test
                        │
        ┌───────────────┼───────────────┐
        │               │               │
   unit-test      functional-test  integration-test
                                        │
                                ┌───────┴───────┐
                                │               │
                          smoke-test       sql-test
```

| Level | Purpose | Implementation | Location |
|-------|---------|----------------|----------|
| **Unit** | Test individual functions/modules | C code with `TEST()` macros | `lib/core/test/` |
| **Functional** | Test each utility separately | Bash scripts + expected output | `test/functional/` |
| **Integration** | Test interaction between utilities | Bash + SQL + expected | `test/integration/` |

## Directory Layout

```
test/
├── TESTS.md                       # This file
├── functional/                    # Functional tests
│   ├── run.sh                     # Test runner
│   ├── update_expected.sh         # Update expected/ from results/
│   ├── cases/                     # Test cases (bash scripts)
│   │   ├── scrc_help.sh
│   │   ├── scrc_no_args.sh
│   │   └── ...
│   ├── expected/                  # Expected outputs (git tracked)
│   │   ├── scrc_help.out
│   │   └── ...
│   └── results/                   # Actual outputs (git ignored)
│
└── integration/                   # Integration tests
    ├── run.sh                     # SQL test runner
    ├── update_expected.sh         # Update expected/ from results/
    ├── smoke/                     # Legacy smoke tests
    │   ├── integration_test.sh    # Test cliend-server basics
    ├── sql/                       # SQL test cases
    │   ├── 01_create.sql
    │   ├── 02_insert.sql
    │   └── ...
    ├── expected/                  # Expected outputs (git tracked)
    │   ├── 01_create.out
    │   └── ...
    └── results/                   # Actual outputs (git ignored)
```

## Quick Start

```bash
# Run everything
make test

# Run specific level
make unit-test
make functional-test
make integration-test

# Run SQL tests only
make sql-test
```

## Unit Tests

### Purpose

Test **individual functions** and **modules** in isolation.

### Location

`lib/core/test/test_*.c`

### Framework

Custom `TEST()` / `TEST_END()` macros (see `lib/core/test/test_*.c`).

### Running

```bash
make unit-test
```

### Adding a New Unit Test

1. Create `lib/core/test/test_mymodule.c`
2. Use `TEST()` / `TEST_END()` macros
3. Add to `TEST_SRCS_core` in `Makefile` (auto-detected via `wildcard`)

### Example

```c
#include "quin.h"
#include "mymodule.h"

TEST(mymodule)

    TEST_SUITE(basic)

        TEST_CASE(foo) {
            TEST_CHECK(foo(1, 2) == 3);
        }

    TEST_SUITE_END()

TEST_END()
```

## Functional Tests

### Purpose

Test **each utility separately** with various **arguments** and check **output** and **exit code**.

### Location

`test/functional/`

### Framework

Bash scripts + expected output comparison.

### Running

```bash
make functional-test
```

### Adding a New Functional Test

1. Create `test/functional/cases/<name>.sh`:

```bash
#!/bin/bash
# Test: scrc --help
"$BIN_DIR/scrc" --help
```

2. Run `make functional-test` — the `expected/<name>.out` will be created automatically.

3. Review the result:

```bash
cat test/functional/expected/<name>.out
```

4. If correct — commit:

```bash
git add test/functional/expected/<name>.out
```

### How It Works

`run.sh`:

1. Runs each `cases/*.sh`
2. Captures **stdout + stderr** + **exit code**
3. Compares with `expected/*.out`
4. On mismatch — shows diff

### Exit Code Handling

`run.sh` records the **exit code** of the test case and includes it in the output:

```
exit: 0
```

This allows tests to check both **output** and **exit code**.

### Updating Expected Output

```bash
# Update all expected files
make functional-update

# Update one test
make functional-update-one TEST=scrc_help

# Update list of tests
make functional-update-list TESTS="scrc_help scrc_no_args"

# List available tests
make functional-list
```

## Integration Tests

### Purpose

Test **interaction between utilities**:
- `scr_init` — database initialization
- `scroller` — server
- `scrc` — client

### Location

`test/integration/`

### Two Sub-types

#### Smoke Test

**Location**: `test/integration/integration_test.sh`

**Purpose**: Quick check that the whole system starts and basic commands work.

**Running**:

```bash
make smoke-test
```

Uses `expect` to interact with the client.

#### SQL Tests

**Location**: `test/integration/sql/`

**Purpose**: Test **SQL logic** — queries, WHERE, LIKE, IN, BETWEEN, arithmetic, logical operators.

**Running**:

```bash
make sql-test
```

### Adding a New SQL Test

1. Create `test/integration/sql/<name>.sql`:

```sql
CREATE USER test_user;
CREATE CATALOG test_cat;
CREATE SCHEMA test_sch;
CREATE TABLE test_sch.users (id int, name char(16));

INSERT INTO test_sch.users (id, name) VALUES (1, 'vasia');

SELECT id FROM test_sch.users WHERE name LIKE '%sia';
```

2. Run `make sql-test` — `expected/<name>.out` created automatically.

3. Review and commit:

```bash
cat test/integration/expected/<name>.out
git add test/integration/expected/<name>.out
```

### How It Works

`run.sh`:

1. Creates test cluster in `/tmp/scroller/sql_test_<pid>`
2. Runs `scr_init` to initialize
3. Starts `scroller` server
4. For each `sql/*.sql`:
   - Runs through `scrc` client
   - Captures output
   - Compares with `expected/*.out`
5. Kills server, cleans up

### Updating Expected Output

```bash
# Update all expected files
make sql-update

# Update one test
make sql-update-one TEST=05_select_like

# Update list of tests
make sql-update-list TESTS="05_select_like 06_select_in"

# List available tests
make sql-list
```

## Common Workflows

### Adding a New Feature

```bash
# 1. Write code
vim lib/core/src/mymodule.c

# 2. Write unit tests
vim lib/core/test/test_mymodule.c

# 3. Run unit tests
make unit-test

# 4. Add integration test (if feature is user-visible)
vim test/integration/sql/10_my_feature.sql

# 5. Run and create expected
make sql-test

# 6. Review and commit
git add test/integration/expected/10_my_feature.out
git commit -m "Add my_feature"
```

### Fixing a Bug

```bash
# 1. Reproduce in a test
vim test/functional/cases/scrc_bug.sh

# 2. Run — should FAIL
make functional-test

# 3. Fix the code
vim src/scrc/main.c

# 4. Run — should PASS
make functional-test
```

### Debugging a Failed Test

```bash
# 1. Run with verbose output
make sql-test

# 2. Look at the diff
diff test/integration/expected/05_select_like.out \
     test/integration/results/05_select_like.out

# 3. Look at the full result
cat test/integration/results/05_select_like.out

# 4. Check server log (if any)
cat /tmp/scroller/sql_test_*/scroller.log
```

## Make Targets Reference

| Target | Description |
|--------|-------------|
| `make test` | Run **all** tests (unit + functional + integration) |
| `make unit-test` | Run unit tests only |
| `make functional-test` | Run functional tests only |
| `make integration-test` | Run integration tests (smoke + SQL) |
| `make smoke-test` | Run smoke test only |
| `make sql-test` | Run SQL tests only |
| `make functional-update` | Update all functional expected outputs |
| `make functional-update-one` | Update one functional test (`TEST=<name>`) |
| `make functional-update-list` | Update list of functional tests (`TESTS="..."`) |
| `make functional-list` | List available functional tests |
| `make sql-update` | Update all SQL expected outputs |
| `make sql-update-one` | Update one SQL test (`TEST=<name>`) |
| `make sql-update-list` | Update list of SQL tests (`TESTS="..."`) |
| `make sql-list` | List available SQL tests |
| `make help` | Show all available targets |

## Best Practices

### Do

- ✅ **Review** diffs before `make *-update`
- ✅ **Commit** `expected/` changes **separately** from code changes
- ✅ **Test edge cases**: empty input, wrong types, boundaries
- ✅ **One test case** = **one behaviour**
- ✅ **Meaningful names**: `scrc_help`, `sql_05_like`

### Don't

- ❌ **Don't** commit `results/` (in `.gitignore`)
- ❌ **Don't** use `|| true` in test cases (hides exit code)
- ❌ **Don't** run `make *-update` blindly
- ❌ **Don't** test multiple behaviours in one case
- ❌ **Don't** rely on test order

## CI Integration

```yaml
# .github/workflows/test.yml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install deps
        run: sudo apt-get install -y flex bison expect netcat
      - name: Build
        run: make all
      - name: Run all tests
        run: make test
```

## Troubleshooting

### `make functional-test` fails with "NO EXPECTED"

**Cause**: Test case was added but `expected/*.out` doesn't exist.

**Solution**: Review `results/*.out`, then:

```bash
make functional-update-one TEST=<name>
```

### `make sql-test` fails with "client error"

**Cause**: `scrc` couldn't connect to `scroller`.

**Solution**:

1. Check that binaries are built: `make all`
2. Check port: `netstat -tlnp | grep 8081`
3. Check `strace` output

### Server doesn't start

**Cause**: Port 8081 already in use, or `scr_init` failed.

**Solution**:

```bash
# Check port
lsof -i :8081

# Check cluster dir
ls -la /tmp/scroller/

# Manual test
./build/debug/bin/scr_init /tmp/test_cluster
./build/debug/bin/scroller /tmp/test_cluster
```

### Diff is empty but test failed

**Cause**: Probably exit code mismatch (for functional tests).

**Solution**:

```bash
# Check exit code in result file
tail -1 test/functional/results/<name>.out
# Should show "exit: N"
```

## Related Documents

- [README.md](../README.md) — project overview

