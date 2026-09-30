-- Simple grouping
SELECT id FROM test_sch.users WHERE (id = 1);
SELECT id FROM test_sch.users WHERE ((id = 1));
SELECT id FROM test_sch.users WHERE (((id = 1)));

-- Arithmetic grouping
SELECT id FROM test_sch.users WHERE id + 1 = 2;
SELECT id FROM test_sch.users WHERE (id + 1) = 2;
SELECT id FROM test_sch.users WHERE id + 1 * 2 = 3;
SELECT id FROM test_sch.users WHERE (id + 1) * 2 = 4;

-- Comparison grouping
SELECT id FROM test_sch.users WHERE (id = 1) OR (id = 2);
SELECT id FROM test_sch.users WHERE (id = 1 OR id = 2);
SELECT id FROM test_sch.users WHERE (id = 1) AND (age > 25);
SELECT id FROM test_sch.users WHERE (id = 1 AND age > 25);

-- Mixed grouping
SELECT id FROM test_sch.users WHERE (id + 1) * 2 = 4;
SELECT id FROM test_sch.users WHERE (id = 1 AND age > 25) OR id = 5;
SELECT id FROM test_sch.users WHERE id = 1 AND (age > 25 OR id = 5);

-- Nested parentheses
SELECT id FROM test_sch.users WHERE ((id = 1 OR id = 2) AND age = 30);
SELECT id FROM test_sch.users WHERE (((id = 1 OR id = 2) AND age = 30));
SELECT id FROM test_sch.users 
WHERE ((id = 1 OR id = 2) AND (age = 30 OR name LIKE 'v%'));

-- Parentheses with NOT
SELECT id FROM test_sch.users WHERE NOT (id = 1);
SELECT id FROM test_sch.users WHERE NOT (id = 1 OR id = 2);
SELECT id FROM test_sch.users WHERE NOT (id = 1 AND age > 25);
SELECT id FROM test_sch.users WHERE NOT ((id = 1 OR id = 2) AND age = 30);

-- Parentheses with arithmetic
SELECT id FROM test_sch.users WHERE (id + 1) * 2 = 4;
SELECT id FROM test_sch.users WHERE ((id * 2) + (age / 5)) = 10;
SELECT id FROM test_sch.users WHERE (id + 1) * (id + 1) = 4;

