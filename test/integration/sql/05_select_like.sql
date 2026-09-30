-- Exact match
SELECT id FROM test_sch.users WHERE name LIKE 'vasia';

-- Percent at end
SELECT id FROM test_sch.users WHERE name LIKE 'vas%';

-- Percent at start
SELECT id FROM test_sch.users WHERE name LIKE '%sia';

-- Percent on both sides
SELECT id FROM test_sch.users WHERE name LIKE '%as%';

-- Percent in middle
SELECT id FROM test_sch.users WHERE name LIKE 'va%ia';

-- Underscore - single char
SELECT id FROM test_sch.users WHERE name LIKE 'v_sia';
SELECT id FROM test_sch.users WHERE name LIKE '_asia';
SELECT id FROM test_sch.users WHERE name LIKE 'vasi_';

-- Multiple underscores
SELECT id FROM test_sch.users WHERE name LIKE '_____';
SELECT id FROM test_sch.users WHERE name LIKE '__t__';

-- Percent and underscore combined
SELECT id FROM test_sch.users WHERE name LIKE 'v_%';
SELECT id FROM test_sch.users WHERE name LIKE '%_a';
SELECT id FROM test_sch.users WHERE name LIKE 'v%s_a';

-- Percent matches everything
SELECT id FROM test_sch.users WHERE name LIKE '%';

-- No match
SELECT id FROM test_sch.users WHERE name LIKE 'xyz%';
SELECT id FROM test_sch.users WHERE name LIKE '%xyz%';

-- Empty pattern (no match)
SELECT id FROM test_sch.users WHERE name LIKE '';

-- LIKE with AND
SELECT id FROM test_sch.users WHERE name LIKE 'v%' AND age > 25;

-- LIKE with OR
SELECT id FROM test_sch.users WHERE name LIKE 'v%' OR name LIKE 'p%';

