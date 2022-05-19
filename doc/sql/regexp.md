# REGEXP operator and function

## Synopsis

```sql
`subject` REGEXP `pattern`
REGEXP(`pattern`, `subject`)
```

## Description

Return `1` if the subject match the pattern.
Return `0` if the subject does not match the pattern.
Return `NULL` if the subject or the pattern are NULL.

## Parameters

* `subject`: subject of the test.
* `pattern`: regular expression that is tested against the subject.

## Example

```sql
-- Test if asdf starts with the letter A with the case insensitive flag
SELECT 'asdf' REGEXP '(?i)^A'
; -- => 1
-- Test if asdf starts with the letter A
SELECT 'asdf' REGEXP '(?-i)^A'
; -- => 0
```

## Compatibility with other DB engines

### MariaDB

MariaDB has also the operator `RLIKE` that is an alias of `REGEXP`.
The function `REGEXP('pattern', 'subject')` is does not exist, only the
operator `'subject' REGEXP 'pattern'`.
