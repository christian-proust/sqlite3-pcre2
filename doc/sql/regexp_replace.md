# `REGEXP_REPLACE` function

## Synopsis

```sql
REGEXP_REPLACE(`subject`, `pattern`, `replacement`)
```

## Description

Return a string image of `subject` where each matched portion following
`pattern` is replaced by `replacement`.

The function is similar of the function `pcre2_substitute`.

Return `NULL` if the subject, the pattern, or the replacement are NULL.

## Parameters

* `subject`: subject of the test.
* `pattern`: regular expression that is tested against the subject.
* `replacement`: pattern of replacement that shall replace each occurrence of
  matched substring.

## Example

```sql
-- Reorder first name and surname.
SELECT REGEXP_REPLACE(
    'My name is Bond, James Bond',
    '(\w+), (\w+) \1',
    '$2, $2 $1'
) AS reorder_name
; -- My name is James, James Bond
```

## Compatibility with other DB engines

### MariaDB

This function also exists in MariaDB, but the `replacement` pattern behave
differently:
* In MariaDB, only the reference to group number are recognized with the
  backslash: `\0` to `\9`. They are written in PCRE2 library with `$0` to `$9`.
* The PCRE2 library recognize the backslash and the dollar symbol as special
  character. MariaDB only recognize the pattern above.

Besides, the regexp `pattern` has a different behaviour when it is the blank
string:
* In MariaDB, the return value will be a blank value.
* Here, each characters of the `subject` will be preceded and followed by the
  `replacement`. This behaviour corresponds to the one of Python, amongst
  other.
