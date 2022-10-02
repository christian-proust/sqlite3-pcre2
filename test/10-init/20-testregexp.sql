DROP TABLE IF EXISTS testregexp;
CREATE TABLE testregexp (
    `id` INTEGER PRIMARY KEY,
    `pattern` TEXT NOT NULL,  -- SQL representation of the pattern
    `subject` TEXT NOT NULL,  -- SQL representation of the subject
    `match` TEXT,    -- SQL representation of the match value
    `failmessage` TEXT,  -- Pattern of the failmessage, when the match operation shall crash
    `description` TEXT,
    CONSTRAINT match_xor_failmessage CHECK (
        (`match` IS NULL AND `failmessage` IS NOT NULL)
        OR (`match` IS NOT NULL AND `failmessage` IS NULL)
    )
);


-- Insert testregexp for checking the match value based on the function input
INSERT INTO testregexp
(`pattern`, `subject`, `match`, `description`)
VALUES
    -- pattern      | `subject`     | match | `description`                    |
    ('''(?i)^A''',   '''asdf''',     1,      NULL),
    ('''(?i)^A''',   '''bsdf''',     0,      NULL),
    ('''^A''',       '''asdf''',     0,      NULL),
    ('NULL',         '''asdf''',     'NULL', 'NULL pattern shall return NULL'),
    ('''^A''',       'NULL',         'NULL', 'NULL subject shall return NULL'),
    ('''''',         '''asdf''',     1,      'Blank pattern shall match'),
    ('''^A''',       '''''',         0,      'Blank subject'),
    (
        '''B''',
        'x''00''||''B''',            1,      'NUL character in subject'
    ),
    (
        'x''00''||''B''',
        '''a''||x''00''||''B''',     1,      'NUL character in pattern'
    )
;


-- Insert testregexp for checking the error message when the function crash
INSERT INTO testregexp
(`pattern`, `subject`,
    `failmessage`,
    `description`
)
VALUES
    ('''^(A''',     '''asdf''',
        '%Cannot compile REGEXP pattern ''^(A''%missing closing parenthesis%',
        'Non-compilable regexp'
    ),
    ('''^(''||x''00''',     '''asdf''',
        '%Cannot compile REGEXP pattern ''^(''||x''00''%missing closing parenthesis%',
        'Error containing NUL character'
    ),
    ('''(1''''2''',     '''asdf''',
        '%Cannot compile REGEXP pattern ''(1''''2''%missing closing parenthesis%',
        'Error containing QUOTE character'
    ),
    (
        '''(' || (
                -- q is the table of all integer between 0 and 255
                WITH RECURSIVE q AS (
                    SELECT 0 AS n
                    UNION ALL
                    SELECT n+1 FROM q WHERE n < 255
                )
                SELECT GROUP_CONCAT(PRINTF('%x', n % 16), '') FROM q
                -- return 256 characters:
                -- '0123456789abcdef0123'...'4567890abcdef'
            ) || '''',
        '''asdf''',
        '%Cannot compile REGEXP pattern '
            || '''(0123456789abcdef0123%def012345678''...'
            || '%missing closing parenthesis%',
                                                             --  cdef012345678
        'Error containing a very long parameter'
    );


-- Write the testcase corresponding to the test data
DELETE FROM testcase WHERE src_table = 'testregexp';
INSERT INTO `testcase`
(`src_table`, `src_id`, `execute_sql`, `evaluate_sql`, `description`)
SELECT
    'testregexp' AS src_table,
    id AS src_id,
    (
        'SELECT '||`subject`||' REGEXP '||`pattern`||' AS `value`'
    ) AS `execute_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            '`report_value`'||' IS '||`match`
        ELSE '`stderr` LIKE '  || QUOTE(REPLACE(`failmessage`, '%', '%%'))
        END
    ) AS `evaluate_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            `pattern`
            || ' shall '
            || (CASE WHEN `match` THEN 'match' ELSE 'not match' END)
            || ' '
            || `match`
            || "."
        ELSE
            'The evaluation of `'
            || `subject` || ' REGEXP ' || `pattern`
            || '` shall crash with failmessage '
            || '''' || `failmessage` || ''''
            || '.'
        END
    ) AS `description`
FROM testregexp
;
