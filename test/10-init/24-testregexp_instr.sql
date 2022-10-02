DROP TABLE IF EXISTS testregexp_instr;
CREATE TABLE testregexp_instr (
    `id` INTEGER PRIMARY KEY,
    `pattern` TEXT NOT NULL,  -- SQL representation of the pattern
    `subject` TEXT NOT NULL,  -- SQL representation of the subject
    `retval` TEXT,            -- SQL representation of the return value
    `failmessage` TEXT,  -- Pattern of the failmessage, function call shall crash
    `description` TEXT,
    CONSTRAINT retval_xor_failmessage CHECK (
        (`retval` IS NULL AND `failmessage` IS NOT NULL)
        OR (`retval` IS NOT NULL AND `failmessage` IS NULL)
    )
);


-- Insert testregexp_instr for checking the retval value based on the function input
INSERT INTO testregexp_instr
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
        '0 return value due to no match',
        '''a''', '0',
        '''pattern'''
    ),
    ( -- description, subject, retval, pattern
        'simple match',
        '''abcde''', '2',
        '''b.'''
    ),
    ( -- description, subject, retval, pattern
        'only first match is returned',
        '''1, 2, 3''', '1',
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
        'Blank subject shall return 0',
        '''''', '0',
        '''pattern'''
    ),
    ( -- description, subject, retval, pattern
        'Blank pattern shall return 1',
        '''abc''', '1',
        ''''''
    ),
    ( -- description, subject, retval, pattern
        'NUL character in subject',
        '''ab''||x''00''||''cde''', '3',
        '''.c'''
    ),
    ( -- description, subject, retval, pattern
        'NUL character in pattern',
        '''ab''||x''00''||''cde''', '3',
        'x''00''||''.'''
    ),
    ( -- description, subject, retval, pattern
        '\K reset the begining of the match',
        '''abcde''', '4',
        '''b.\Kd'''
    ),
    ( -- description, subject, retval, pattern
        'Multi-byte character string handling',
        '''abçde''', '4',
        '''d'''
    ),
    ( -- description, subject, retval, pattern
        'Multi-byte character blob handling',
        'CAST(''abçde'' AS BLOB)', '5',
        '''d'''
    ),
    ( -- description, subject, retval, pattern
        'No UTF8 characters',
        'x''FFFE''', '2',
        '''\xfe'''
    )
;


-- Insert testregexp_instr for checking the error message when the function crash
INSERT INTO testregexp_instr
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
DELETE FROM testcase WHERE src_table = 'testregexp_instr';
INSERT INTO `testcase`
(`src_table`, `src_id`, `execute_sql`, `evaluate_sql`, `description`)
SELECT
    'testregexp_instr' AS src_table,
    id AS src_id,
    (
        'SELECT REGEXP_INSTR('
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
            'REGEXP_INSTR('
                ||`subject`||', '
                ||`pattern`||')'
            || ' shall return '
            || `retval`
            || '.'
        ELSE
            'The evaluation of `'
            || 'REGEXP_INSTR('
                ||`subject`||', '
                ||`pattern`||')'
            || '` shall crash with failmessage '
            || '''' || `failmessage` || ''''
            || '.'
        END
    ) AS `description`
FROM testregexp_instr
;
