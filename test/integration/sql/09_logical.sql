-- Simple AND
SELECT id FROM test_sch.users WHERE id = 1 AND name = 'vasia';
SELECT id FROM test_sch.users WHERE id = 1 AND name = 'petya';
SELECT id FROM test_sch.users WHERE age > 25 AND age < 30;

-- Simple OR
SELECT id FROM test_sch.users WHERE id = 1 OR id = 2;
SELECT id FROM test_sch.users WHERE id = 1 OR id = 999;
SELECT id FROM test_sch.users WHERE age > 30 OR age < 25;

-- NOT
SELECT id FROM test_sch.users WHERE NOT id = 1;
SELECT id FROM test_sch.users WHERE NOT age > 25;

-- AND + OR priority (AND higher)
SELECT id FROM test_sch.users WHERE id = 1 OR id = 2 AND age = 30;
SELECT id FROM test_sch.users WHERE id = 1 OR (id = 2 AND age = 30);
SELECT id FROM test_sch.users WHERE (id = 1 OR id = 2) AND age = 30;

-- Multiple AND
SELECT id FROM test_sch.users WHERE id > 1 AND id < 5 AND age > 25;

-- Multiple OR
SELECT id FROM test_sch.users WHERE id = 1 OR id = 3 OR id = 5;

-- Parentheses
SELECT id FROM test_sch.users WHERE (id = 1 OR id = 2) AND age = 30;
SELECT id FROM test_sch.users WHERE id = 1 OR (id = 2 AND age = 30);

-- NOT with AND
SELECT id FROM test_sch.users WHERE NOT (id = 1 AND age = 25);

-- Complex expression
SELECT id FROM test_sch.users
WHERE (id = 1 OR id = 2) AND (age > 25 OR name LIKE 'v%');

