-- NOT has higher priority than AND
SELECT id FROM test_sch.users WHERE NOT id = 1 AND age > 25;
SELECT id FROM test_sch.users WHERE (NOT id = 1) AND age > 25;
SELECT id FROM test_sch.users WHERE NOT (id = 1 AND age > 25);

-- NOT has higher priority than OR
SELECT id FROM test_sch.users WHERE NOT id = 1 OR id = 2;
SELECT id FROM test_sch.users WHERE (NOT id = 1) OR id = 2;
SELECT id FROM test_sch.users WHERE NOT (id = 1 OR id = 2);

-- NOT + AND + OR
SELECT id FROM test_sch.users WHERE NOT id = 1 AND age > 25 OR id = 5;
SELECT id FROM test_sch.users WHERE NOT (id = 1 AND age > 25) OR id = 5;
SELECT id FROM test_sch.users WHERE NOT id = 1 AND (age > 25 OR id = 5);

-- AND has higher priority than OR
SELECT id FROM test_sch.users WHERE id = 1 OR id = 2 AND age = 30;
SELECT id FROM test_sch.users WHERE id = 1 OR (id = 2 AND age = 30);
SELECT id FROM test_sch.users WHERE (id = 1 OR id = 2) AND age = 30;

-- Complex priority
SELECT id FROM test_sch.users 
WHERE NOT id = 1 
  AND age > 25 
  OR id = 2 
  AND name LIKE 'p%';

SELECT id FROM test_sch.users 
WHERE NOT (id = 1 AND age > 25)
  OR (id = 2 AND name LIKE 'p%');

-- NOT + NOT + AND
SELECT id FROM test_sch.users WHERE NOT NOT id = 1 AND age > 25;
SELECT id FROM test_sch.users WHERE NOT (NOT id = 1 AND age > 25);

