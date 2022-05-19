# Sqlite3-pcre2: Regex Extension
This is sqlite3-pcre2, an extension for sqlite3 that uses libpcre2 to provide the REGEXP() function.

The original source code was written by Alexey Tourbin and can be found at:
[http://git.altlinux.org/people/at/packages/?p=sqlite3-pcre.git](http://git.altlinux.org/people/at/packages/?p=sqlite3-pcre.git)

## Build

### Prerequisite

#### Debian

```bash
sudo apt-get install \
    libsqlite3-dev \
    libpcre2-dev
```

### Installation
```bash
git clone https://github.com/christian-proust/sqlite3-pcre2
cd sqlite3-pcre2
make
make install
```

### Development

The tests and the documentation can be generated using:

```bash
make check
make doc
```

See [test](test) documentation.


## Usage

Below is a summary of the available functions. More details can be found
in the [SQL](doc/sql) documentation page.

### REGEXP instruction
```sql
-- Test if asdf starts with the letter A with the case insensitive flag
SELECT 'asdf' REGEXP '(?i)^A'
; -- => 1
-- Test if asdf starts with the letter A
SELECT 'asdf' REGEXP '(?i)^A'
; -- => 0
```

### `REGEXP_SUBSTR(subject, pattern)` function
```sql
-- Extract the part of a string after '@' and before '.'
SELECT REGEXP_SUBSTR('foo@bar.org', '@\K.*?\<=\.', '')
; -- => bar
```

### `REGEXP_REPLACE(subject, pattern, replacement)` function
```sql
-- Reorder first name and surname.
SELECT REGEXP_REPLACE(
    'My name is Bond, James Bond',
    '(\w+), (\w+) \1',
    '$2, $2 $1'
) AS reorder_name
; -- My name is James, James Bond
```

### `REGEXP_INSTR(subject, pattern)` function
```sql
-- Find the level of the header
SELECT REGEXP_INSTR(
    '### This is a header of third level',
    '#[^#]'
) AS header_level
; -- 3
```


## PCRE2 library

The documentation of the PCRE2 library can be found at: [http://pcre.org/](http://pcre.org/).

The regular expression syntax documentation can be found [https://perldoc.perl.org/perlre](here).


## Changelog

### 2022 Update

- Add functions `REGEXP_SUBSTR`, `REGEXP_REPLACE`, `REGEXP_INSTR`.


### 2021 UPDATE

- Rename the lib from pcre.so to pcre2.so.
- Use the libpcre2 instead of libpcre.
- Handle a NULL value for both pattern and subject.
- Handle a NULL character into a string.


### 2020 UPDATE

Updated to work with MacOS. Modified the Makefile to compile and install on MacOS, changed C source code to get rid of "no string" error. (credit to @santiagoyepez for a less hacky fix of explicitly checking for nulls)

### 2006-11-02 Initial version

The original source code was written by Alexey Tourbin and can be found at:
[http://git.altlinux.org/people/at/packages/?p=sqlite3-pcre.git](http://git.altlinux.org/people/at/packages/?p=sqlite3-pcre.git)
