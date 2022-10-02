DROP TABLE IF EXISTS testregexp_substr;
CREATE TABLE testregexp_substr (
    `id` INTEGER PRIMARY KEY,
    `pattern` TEXT NOT NULL,  -- SQL representation of the pattern
    `subject` TEXT NOT NULL,  -- SQL representation of the subject
    `retval` TEXT,    -- SQL representation of the return value
    `failmessage` TEXT,  -- Pattern of the failmessage, when the match operation shall crash
    `description` TEXT,
    CONSTRAINT retval_xor_failmessage CHECK (
        (`retval` IS NULL AND `failmessage` IS NOT NULL)
        OR (`retval` IS NOT NULL AND `failmessage` IS NULL)
    )
);


-- Insert testregexp_substr for checking the retval value based on the function input
INSERT INTO testregexp_substr
(
    `description`,
    `subject`, `retval`,
    `pattern` 
)
VALUES
    -- ( -- description, subject, retval, pattern
    --     'description',
    --     '''subject''', '''retval''',
    --     '''pattern'''
    -- ),
    ( -- description, subject, retval, pattern
        'blank return value due to no match',
        '''a''', '''''',
        '''pattern'''
    ),
    ( -- description, subject, retval, pattern
        'simple match',
        '''abcde''', '''bc''',
        '''b.'''
    ),
    ( -- description, subject, retval, pattern
        'only first match is returned',
        '''1, 2, 3''', '''1''',
        '''\d'''
    ),
    ( -- description, subject, retval, pattern
        'NULL subject shall return NULL',
        'NULL', 'NULL',
        '''b.'''
    ),
    ( -- description, subject, retval, pattern
        'NULL pattern shall return NULL',
        '''subject''', 'NULL',
        'NULL'
    ),
    ( -- description, subject, retval, pattern
        'Blank subject shall return blank',
        '''''', '''''',
        '''pattern'''
    ),
    ( -- description, subject, retval, pattern
        'Blank pattern shall return blank',
        '''abc''', '''''',
        ''''''
    ),
    ( -- description, subject, retval, pattern
        'NUL character in subject',
        '''ab''||x''00''||''cde''', 'x''00''||''c''',
        '''.c'''
    ),
    ( -- description, subject, retval, pattern
        'NUL character in pattern',
        '''ab''||x''00''||''cde''', 'x''00''||''c''',
        'x''00''||''.'''
    ),
    ( -- description, subject, retval, pattern
        'Usage of \K instruction for look-behind',
        '''abcdef''', '''cd''',
        '''ab\K..'''
    ),
    ( -- description, subject, retval, pattern
        'Usage of (?=) for look-ahead',
        '''abcdef''', '''cd''',
        '''..(?=ef)'''
    )
;


-- Insert testregexp_substr for checking the error message when the function crash
INSERT INTO testregexp_substr
(
    `description`,
    `subject`,
    `pattern`,
    `failmessage`
)
VALUES
    -- (
    --     'description',
    --     '''subject''',
    --     '''pattern''',
    --     '%message%'
    -- ),
    (
        'Non-compilable regexp',
        '''abcde''',
        '''^(b''',
        '%Cannot compile REGEXP pattern%missing closing parenthesis%'
    )
;


-- Write the testcase corresponding to the test data
DELETE FROM testcase WHERE src_table = 'testregexp_substr';
INSERT INTO `testcase`
(`src_table`, `src_id`, `execute_sql`, `evaluate_sql`, `description`)
SELECT
    'testregexp_substr' AS src_table,
    id AS src_id,
    (
        'SELECT REGEXP_SUBSTR('
            ||`subject`||', '
            ||`pattern`||') AS `value`'
    ) AS `execute_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            '`report_value`'||' IS '||`retval`
            || ' AND ' || 'IFNULL(`exit_code`=0, FALSE)'
        ELSE '`stderr` LIKE '  || QUOTE(REPLACE(`failmessage`, '%', '%%'))
        END
    ) AS `evaluate_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            'REGEXP_SUBSTR('
                ||`subject`||', '
                ||`pattern`||')'
            || ' shall return '
            || `retval`
            || '.'
        ELSE
            'The evaluation of `'
            || 'REGEXP_SUBSTR('
                ||`subject`||', '
                ||`pattern`||')'
            || '` shall crash with failmessage '
            || '''' || `failmessage` || ''''
            || '.'
        END
    ) AS `description`
FROM testregexp_substr
;
