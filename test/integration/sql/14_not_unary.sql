-- Simple NOT
SELECT id FROM test_sch.users WHERE NOT id = 1;
SELECT id FROM test_sch.users WHERE NOT id = 999;
SELECT id FROM test_sch.users WHERE NOT age > 25;

-- NOT with comparison
SELECT id FROM test_sch.users WHERE NOT id > 3;
SELECT id FROM test_sch.users WHERE NOT id < 3;
SELECT id FROM test_sch.users WHERE NOT id >= 3;
SELECT id FROM test_sch.users WHERE NOT id <= 3;
SELECT id FROM test_sch.users WHERE NOT id <> 1;

-- NOT NOT (double negation)
SELECT id FROM test_sch.users WHERE NOT NOT id = 1;
SELECT id FROM test_sch.users WHERE NOT NOT id = 999;
SELECT id FROM test_sch.users WHERE NOT NOT NOT id = 1;

-- NOT with LIKE
SELECT id FROM test_sch.users WHERE NOT name LIKE 'v%';
SELECT id FROM test_sch.users WHERE NOT (name LIKE 'v%');

-- NOT with IN
SELECT id FROM test_sch.users WHERE NOT id IN (1, 2);
SELECT id FROM test_sch.users WHERE NOT (id IN (1, 2));

-- NOT with BETWEEN
SELECT id FROM test_sch.users WHERE NOT id BETWEEN 2 AND 4;
SELECT id FROM test_sch.users WHERE NOT (id BETWEEN 2 AND 4);

-- NOT with parentheses
SELECT id FROM test_sch.users WHERE NOT (id = 1);
SELECT id FROM test_sch.users WHERE NOT (id = 1 AND age = 25);
SELECT id FROM test_sch.users WHERE NOT (id = 1 OR id = 2);

