-- Multiple commands on one line
SELECT id FROM test_sch.users; SELECT id FROM test_sch.users;

-- Multiple commands with different content
SELECT id FROM test_sch.users WHERE id = 1; SELECT id FROM test_sch.users WHERE id = 2;

-- Multiple commands with comments
SELECT id FROM test_sch.users; -- first query
SELECT id FROM test_sch.users WHERE id = 1; -- second query

-- Complex multi-line queries
SELECT id FROM test_sch.users 
WHERE id = 1;

SELECT id FROM test_sch.users 
WHERE id = 1 
  AND age > 25;

SELECT id FROM test_sch.users 
WHERE id = 1 
  OR id = 2 
  OR id = 3;

