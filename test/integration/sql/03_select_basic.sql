-- Select all
SELECT id FROM test_sch.users;

-- Select specific columns
SELECT id, name FROM test_sch.users;

-- Select with no match
SELECT id FROM test_sch.users WHERE id = 999;

