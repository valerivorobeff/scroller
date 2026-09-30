-- NOT LIKE
SELECT id FROM test_sch.users WHERE name NOT LIKE 'v%';
SELECT id FROM test_sch.users WHERE name NOT LIKE '%a%';
SELECT id FROM test_sch.users WHERE name NOT LIKE 'v_sia';
SELECT id FROM test_sch.users WHERE name NOT LIKE '%';
SELECT id FROM test_sch.users WHERE name NOT LIKE 'xyz%';

-- NOT IN
SELECT id FROM test_sch.users WHERE id NOT IN (1, 2);
SELECT id FROM test_sch.users WHERE id NOT IN (10, 20, 30);
SELECT id FROM test_sch.users WHERE id NOT IN (1, 2, 3, 4, 5);
SELECT id FROM test_sch.users WHERE name NOT IN ('vasia', 'petya');
SELECT id FROM test_sch.users WHERE name NOT IN ('xyz', 'abc');

-- NOT BETWEEN
SELECT id FROM test_sch.users WHERE id NOT BETWEEN 2 AND 4;
SELECT id FROM test_sch.users WHERE id NOT BETWEEN 10 AND 20;
SELECT id FROM test_sch.users WHERE id NOT BETWEEN 1 AND 5;
SELECT id FROM test_sch.users WHERE age NOT BETWEEN 25 AND 30;

-- NOT LIKE + AND
SELECT id FROM test_sch.users WHERE name NOT LIKE 'v%' AND age > 25;

-- NOT IN + OR
SELECT id FROM test_sch.users WHERE id NOT IN (1, 2) OR id = 5;

-- NOT BETWEEN + AND
SELECT id FROM test_sch.users WHERE id NOT BETWEEN 2 AND 4 AND age > 25;

