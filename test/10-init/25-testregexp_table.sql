-- Write the testcase corresponding to the test data
INSERT INTO `testcase`
(`execute_sql`, `evaluate_sql`, `description`)
VALUES
(
    'SELECT (
                    (SELECT GROUP_CONCAT(rowid) FROM REGEXP_TABLE)
        || ''/'' || (SELECT GROUP_CONCAT(a    ) FROM REGEXP_TABLE)
        || ''/'' || (SELECT GROUP_CONCAT(b    ) FROM REGEXP_TABLE)
    )',
    '`report_value` IS (''1,2,3,4,5,6,7,8,9''
        || ''/1001,1002,1003,1004,1005,1006,1007,1008,1009''
        || ''/2001,2002,2003,2004,2005,2006,2007,2008,2009''
    ) AND IFNULL(`exit_code` = 0, FALSE)',
    'Ensure that the REGEXP_TABLE issued from the template is working'
);
