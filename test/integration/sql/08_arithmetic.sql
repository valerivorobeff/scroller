-- Addition
SELECT id FROM test_sch.users WHERE id + 1 = 2;
SELECT id FROM test_sch.users WHERE id + 5 = 10;

-- Subtraction
SELECT id FROM test_sch.users WHERE id - 1 = 0;
SELECT id FROM test_sch.users WHERE age - 5 = 20;

-- Multiplication
SELECT id FROM test_sch.users WHERE id * 2 = 4;
SELECT id FROM test_sch.users WHERE id * 3 = 9;

-- Division
SELECT id FROM test_sch.users WHERE id / 2 = 1;
SELECT id FROM test_sch.users WHERE id / 2 = 2;

-- Modulo
SELECT id FROM test_sch.users WHERE id % 2 = 0;
SELECT id FROM test_sch.users WHERE id % 2 = 1;
SELECT id FROM test_sch.users WHERE age % 5 = 0;

-- Parentheses
SELECT id FROM test_sch.users WHERE (id + 1) * 2 = 6;
SELECT id FROM test_sch.users WHERE id + 1 * 2 = 3;
SELECT id FROM test_sch.users WHERE (id + 1) * (id + 1) = 9;

-- Priority: * before +
SELECT id FROM test_sch.users WHERE id + 1 * 2 = 3;
SELECT id FROM test_sch.users WHERE (id + 1) * 2 = 6;

-- Column vs column arithmetic
SELECT id FROM test_sch.users WHERE id + id = 4;
SELECT id FROM test_sch.users WHERE age - id = 24;

