DROP TABLE IF EXISTS testregexp_replace;
CREATE TABLE testregexp_replace (
    `id` INTEGER PRIMARY KEY,
    `pattern` TEXT NOT NULL,  -- SQL representation of the pattern
    `replacement` TEXT NOT NULL,  -- SQL representation of the replacement
    `subject` TEXT NOT NULL,  -- SQL representation of the subject
    `substitute` TEXT,    -- SQL representation of the substitute value
    `failmessage` TEXT,  -- Pattern of the failmessage, when the match operation shall crash
    `description` TEXT,
    CONSTRAINT match_xor_failmessage CHECK (
        (`substitute` IS NULL AND `failmessage` IS NOT NULL)
        OR (`substitute` IS NOT NULL AND `failmessage` IS NULL)
    )
);


-- Insert testregexp_replace for checking the substitute value based on the function input
INSERT INTO testregexp_replace
(
    `description`,
    `subject`, `substitute`,
    `pattern`, `replacement`
)
VALUES
    -- ( -- description, subject, substitute, pattern, replacement
    --     'description',
    --     '''subject''', '''substitute''',
    --     '''pattern''', '''replacement'''
    -- ),
    ( -- description, subject, substitute, pattern, replacement
        '0 replacement due to no match',
        '''a''', '''a''',
        '''pattern''', '''replacement'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        '1 replacement',
        '''abcde''', '''a-x-de''',
        '''b.''', '''-x-'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        '3 replacement',
        '''1, 2, 3''', '''x, x, x''',
        '''\d''', '''x'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NULL subject shall return NULL',
        'NULL', 'NULL',
        '''b.''', '''x'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NULL pattern shall return NULL',
        '''subject''', 'NULL',
        'NULL', '''replacement'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NULL replacement shall return NULL',
        '''abcde''', 'NULL',
        '''b.''', 'NULL'
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Blank subject shall return blank',
        '''''', '''''',
        '''pattern''', '''replacement'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Blank pattern shall match all before and after each characters as in Python',
        -- Python behaviour is: re.sub('', '012', 'abc') => '012a012b012c012'
        '''abc''', '''012a012b012c012''',
        '''''', '''012'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Blank pattern shall match all the full string as in MariaDB',
        -- MariaDB behaviour is: REGEXP_REPLACE('abc', '', '012') => '012'
        '''abc''', '''012''',
        '''''', '''012'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Blank replacement shall erase matched items',
        '''abcde''', '''ade''',
        '''b.''', ''''''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Replacement at the begining of lines',
        '''a b''||x''0a''||''c d''', '''x b''||x''0a''||''x d''',
        '''(?m)^.''', '''x'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NUL character in subject',
        '''ab''||x''00''||''cde''', '''ab8de''',
        '''.c''', '''8'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NUL character in pattern',
        '''ab''||x''00''||''cde''', '''ab8de''',
        'x''00''||''.''', '''8'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'NUL character in replacement',
        '''abcde''', '''a''||x''00''||''de''',
        '''b.''', 'x''00'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Replacement with \0 reference shall replace with the whole match',
        '''abcde''', '''a"bc"de''',
        '''b.''', '''"$0"'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Replacement with ${1} reference shall replace with the first group',
        '''abcde''', '''aBcXde''',
        '''b(.)''', '''B${1}X'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Replacement with $3 reference shall replace the third group',
        '''abcde''', '''c''',
        '''(a)(b)(.)(d)(e)''', '''$3'''
    ),
    ( -- description, subject, substitute, pattern, replacement
        'Replacement with ${name} shall replace with group (?<name>)',
        '''abcde''', '''c''',
        '''(a)(b)(?<name>.)(d)(e)''', '''${name}'''
    )
;


-- Insert testregexp_replace for checking the error message when the function crash
INSERT INTO testregexp_replace
(
    `description`,
    `subject`,
    `pattern`, `replacement`,
    `failmessage`
)
VALUES
    -- (
    --     'description',
    --     '''subject''',
    --     '''pattern''',     '''replacement''',
    --     '%message%'
    -- ),
    (
        'Non-compilable regexp',
        '''abcde''',
        '''^(b''',     '''x''',
        '%Cannot compile REGEXP pattern%missing closing parenthesis%'
    ),
    (
        'Invalid reference to group \3 for pattern with only 2 group',
        '''abcde''',
        '''^(\w)(.)''',     '''$3 abcdef''',
        '%Cannot execute REGEXP_REPLACE%at character 2%unknown substring%'
    )
;


-- Write the testcase corresponding to the test data
DELETE FROM testcase WHERE src_table = 'testregexp_replace';
INSERT INTO `testcase`
(`src_table`, `src_id`, `execute_sql`, `evaluate_sql`, `description`)
SELECT
    'testregexp_replace' AS src_table,
    id AS src_id,
    (
        'SELECT REGEXP_REPLACE('
            ||`subject`||', '
            ||`pattern`||', '
            ||`replacement`||') AS `value`'
    ) AS `execute_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            '`report_value`'||' IS '||`substitute`
            || ' AND ' || 'IFNULL(`exit_code`=0, FALSE)'
        ELSE '`stderr` LIKE '''  || REPLACE(`failmessage`, '%', '%%') || ''''
        END
    ) AS `evaluate_sql`,
    (
        CASE WHEN `failmessage` IS NULL
        THEN
            'REGEXP_REPLACE('
                ||`subject`||', '
                ||`pattern`||', '
                ||`replacement`||')'
            || ' shall return '
            || `substitute`
            || '.'
        ELSE
            'The evaluation of `'
            || 'REGEXP_REPLACE('
                ||`subject`||', '
                ||`pattern`||', '
                ||`replacement`||')'
            || '` shall crash with failmessage '
            || '''' || `failmessage` || ''''
            || '.'
        END
    ) AS `description`
FROM testregexp_replace
;
