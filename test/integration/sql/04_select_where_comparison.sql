-- Equality
SELECT id FROM test_sch.users WHERE id = 1;
SELECT id FROM test_sch.users WHERE name = 'vasia';

-- Not equal
SELECT id FROM test_sch.users WHERE id <> 1;
SELECT id FROM test_sch.users WHERE id != 1;

-- Less than
SELECT id FROM test_sch.users WHERE id < 3;
SELECT id FROM test_sch.users WHERE age < 28;

-- Less or equal
SELECT id FROM test_sch.users WHERE id <= 3;

-- Greater than
SELECT id FROM test_sch.users WHERE id > 3;

-- Greater or equal
SELECT id FROM test_sch.users WHERE id >= 3;

-- Column vs column
SELECT id FROM test_sch.users WHERE id = age;

-- Column with arithmetic
SELECT id FROM test_sch.users WHERE id + 10 = age;

-- Constant arithmetic
SELECT id FROM test_sch.users WHERE id = 1 + 2;

