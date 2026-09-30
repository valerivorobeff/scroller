-- LIKE + IN
SELECT id FROM test_sch.users WHERE name LIKE 'v%' AND id IN (1, 4);

-- BETWEEN + LIKE
SELECT id FROM test_sch.users 
WHERE id BETWEEN 1 AND 4 AND name LIKE 'v%';

-- Arithmetic + comparison + logical
SELECT id FROM test_sch.users 
WHERE id * 2 > 3 AND age - 5 < 30;

-- Everything combined
SELECT id FROM test_sch.users 
WHERE (id BETWEEN 1 AND 4) 
  AND (name LIKE 'v%' OR name LIKE 'p%') 
  AND age > 20 
  AND id + 10 < 20;

-- NOT + IN
SELECT id FROM test_sch.users WHERE id NOT IN (1, 2);

-- NOT + LIKE
SELECT id FROM test_sch.users WHERE name NOT LIKE 'v%';

-- NOT + BETWEEN
SELECT id FROM test_sch.users WHERE id NOT BETWEEN 2 AND 4;

