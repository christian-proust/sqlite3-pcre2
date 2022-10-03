-- Write the testcase corresponding to the test data
INSERT INTO `testcase`
(`execute_sql`, `evaluate_sql`, `description`)
VALUES
(
    'SELECT (
                    (SELECT GROUP_CONCAT(rowid           ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
        || ''/'' || (SELECT GROUP_CONCAT(`group_id`      ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
        || ''/'' || (SELECT GROUP_CONCAT(`value`         ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
        || ''/'' || (SELECT GROUP_CONCAT(`match_order`   ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
        || ''/'' || (SELECT GROUP_CONCAT(`subject`       ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
        || ''/'' || (SELECT GROUP_CONCAT(`pattern`       ) FROM REGEXP_TABLE(''abac'', ''a(.)?''))
    )',
    '`report_value` IS (''1,2,3,4''
        || ''/0,1,0,1''
        || ''/ab,b,ac,c''
        || ''/1,1,2,2''
        || ''/abac,abac,abac,abac''
        || ''/a(.)?,a(.)?,a(.)?,a(.)?''
    ) AND IFNULL(`exit_code` = 0, FALSE)',
    'Basic test case for REGEXP_TABLE'
);
