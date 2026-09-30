-- Empty result from comparison
SELECT id FROM test_sch.users WHERE id = 999;

-- Empty result from LIKE
SELECT id FROM test_sch.users WHERE name LIKE 'zzz%';

-- Empty result from IN
SELECT id FROM test_sch.users WHERE id IN (100, 200);

-- Empty result from BETWEEN
SELECT id FROM test_sch.users WHERE id BETWEEN 100 AND 200;

-- Empty result from AND
SELECT id FROM test_sch.users WHERE id = 1 AND id = 2;

-- Empty result from complex
SELECT id FROM test_sch.users
WHERE (id = 1 AND age = 30) OR (id = 5 AND age = 25);

