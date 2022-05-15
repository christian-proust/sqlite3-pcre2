# `REGEXP_SUBSTR` function

## Synopsis

```sql
REGEXP_SUBSTR(`subject`, `pattern`)
```

## Description

Return the position of the `pattern` into the `subject`.

Return `NULL` if the subject or the pattern is NULL.

In case the subject is a `BLOB`, the position is given in bytes.

In case the subject is a `TEXT`, the position is given in character.

## Parameters

* `subject`: subject of the test.
* `pattern`: regular expression that is tested against the subject.

## Example

```sql
-- Find the level of the header
SELECT REGEXP_INSTR(
    '### This is a header of third level',
    '#[^#]'
) AS header_level
; -- 3
```

## Compatibility with other DB engines

### MariaDB

This function also exists in MariaDB.
