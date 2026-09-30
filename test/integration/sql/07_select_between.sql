-- Basic between
SELECT id FROM test_sch.users WHERE id BETWEEN 2 AND 4;

-- Single value range
SELECT id FROM test_sch.users WHERE id BETWEEN 3 AND 3;

-- Boundary values included
SELECT id FROM test_sch.users WHERE id BETWEEN 1 AND 5;

-- No match
SELECT id FROM test_sch.users WHERE id BETWEEN 10 AND 20;

-- Between with age
SELECT id FROM test_sch.users WHERE age BETWEEN 25 AND 30;

-- Between reversed (no match)
SELECT id FROM test_sch.users WHERE id BETWEEN 5 AND 1;

-- Between with AND
SELECT id FROM test_sch.users WHERE id BETWEEN 1 AND 4 AND age > 25;

-- Between with OR
SELECT id FROM test_sch.users WHERE id BETWEEN 1 AND 2 OR id BETWEEN 4 AND 5;

