-- Write the testcase corresponding to the test data
INSERT INTO `testcase`
(`execute_sql`, `evaluate_sql`, `description`)
VALUES
(
    'SELECT (
                    (SELECT GROUP_CONCAT(rowid           ) FROM REGEXP_TABLE(4007, 5007))
        || ''/'' || (SELECT GROUP_CONCAT(`group_id`      ) FROM REGEXP_TABLE(4007, 5007))
        || ''/'' || (SELECT GROUP_CONCAT(`value`         ) FROM REGEXP_TABLE(4007, 5007))
        || ''/'' || (SELECT GROUP_CONCAT(`match_order`   ) FROM REGEXP_TABLE(4007, 5007))
        || ''/'' || (SELECT GROUP_CONCAT(`subject`       ) FROM REGEXP_TABLE(4007, 5007))
        || ''/'' || (SELECT GROUP_CONCAT(`pattern`       ) FROM REGEXP_TABLE(4007, 5007))
    )',
    '`report_value` IS (''7''
        || ''/1007''
        || ''/2007''
        || ''/3007''
        || ''/4007''
        || ''/5007''
    ) AND IFNULL(`exit_code` = 0, FALSE)',
    'Ensure that the REGEXP_TABLE issued from the template is working'
);
