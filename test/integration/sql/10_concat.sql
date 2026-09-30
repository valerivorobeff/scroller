-- Simple concatenation
SELECT id FROM test_sch.users WHERE name = 'vas' || 'ia';
SELECT id FROM test_sch.users WHERE name = 'vasia' || '';

-- Concatenation with column
SELECT id FROM test_sch.users WHERE name = name || '';
SELECT id FROM test_sch.users WHERE name || '!' = 'vasia!';

-- Multiple concatenations
SELECT id FROM test_sch.users WHERE name = 'va' || 'si' || 'a';

-- Concatenation with LIKE
SELECT id FROM test_sch.users WHERE name LIKE 'va' || '%';

