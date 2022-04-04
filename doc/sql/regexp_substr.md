# `REGEXP_SUBSTR` function

## Synopsis

```sql
REGEXP_SUBSTR(`subject`, `pattern`)
```

## Description

Return the portion of the `subject` matching the `pattern`.

Return `NULL` if the subject or the pattern is NULL.

## Parameters

* `subject`: subject of the test.
* `pattern`: regular expression that is tested against the subject.

## Example

```sql
-- Extract the part of a string after '@' and before '.'
SELECT REGEXP_SUBSTR('foo@bar.org', '@\K.*?\<=\.', '')
; -- => bar
```

## Compatibility with other DB engines

### MariaDB

This function also exists in MariaDB.
