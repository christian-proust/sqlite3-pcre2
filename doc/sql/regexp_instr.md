# `REGEXP_INSTR` function

## Synopsis

```sql
REGEXP_INSTR(`subject`, `pattern`)
```

## Description

Return the first index from the `subject` where the `pattern` is matching.

Return 0 if the subject does not match the pattern.

Return NULL if the subject or the pattern is NULL.

## Parameters

* `subject`: subject of the test.
* `pattern`: regular expression that is tested against the subject.

## Example

```sql
-- Find the level of the header
SELECT REGEXP_INSTR(
    '### This is a header of third level',
    '[^#]'
) -1 AS reorder_name
; -- 3
```

## Compatibility with other DB engines

### MariaDB

This function also exists in MariaDB.
