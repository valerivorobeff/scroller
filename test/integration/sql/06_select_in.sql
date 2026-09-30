-- Single value
SELECT id FROM test_sch.users WHERE id IN (1);

-- Multiple values
SELECT id FROM test_sch.users WHERE id IN (1, 3, 5);

-- All values
SELECT id FROM test_sch.users WHERE id IN (1, 2, 3, 4, 5);

-- No match
SELECT id FROM test_sch.users WHERE id IN (10, 20, 30);

-- With strings
SELECT id FROM test_sch.users WHERE name IN ('vasia', 'petya');
SELECT id FROM test_sch.users WHERE name IN ('vasia', 'xyz');

-- IN with AND
SELECT id FROM test_sch.users WHERE id IN (1, 2) AND age > 25;

-- IN with OR
SELECT id FROM test_sch.users WHERE id IN (1, 2) OR id = 5;

