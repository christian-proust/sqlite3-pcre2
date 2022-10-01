/*
 * Initially written by Alexey Tourbin <at@altlinux.org>.
 *
 * The author has dedicated the code to the public domain.  Anyone is free
 * to copy, modify, publish, use, compile, sell, or distribute the original
 * code, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <pcre2.h>
#include <sqlite3ext.h>

SQLITE_EXTENSION_INIT1

typedef struct {
    char *pattern_str;
    int pattern_len;
    pcre2_code *pattern_code;
} cache_entry;

#ifndef CACHE_SIZE
#define CACHE_SIZE 16
#endif


/**
 * Escape chars to ASCII representation as SQLite literal string.
 *
 * @param out_str: pointer to the allocate size of string.
 * @param out_len: allocate size of `out_str` buffer.
 * @param in_str: input string to escape.
 * @param in_len: length of in_string. When `in_len < 0`,
 *     it will be replaced by `strlen(in_str)`.
 * @return the literal string to be used. It does not necessary equals to
 *     `out_str` in some corner cases.
 *
 * In case `out_len` is not high enough for the whole string to be
 * escaped, the string is followed by "..." to emphase the missing
 * characters.
 *
 * `in_str` may contains binary character, ASCII characters, or a
 * combinaison of both. ASCII caracters are the ones between 0x20 and 0x7E.
 *
 * In case `in_str` is the null pointer, the functions returns `"NULL"`.
 *
 * Example
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * out_str = char[22];
 * escape_str_to_sql_literal(out_str, sizeof(out_str), "1é2", 4); // returns "'1234'..."
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * Truth table with `out_len` equals to 22
 * ========================================
 *
 * || `in_str`               || in_len || Return                    ||
 * | `"123"`                 |       0 | `"''"`                      |
 * | `"123"`                 |       2 | `"'12'"`                    |
 * | `"123"`                 |       3 | `"'123'"`                   |
 * | `"123"`                 |      -1 | `"'123'"`                   |
 * | `"1é2"`                 |       4 | `"'1'||x'c3a9'||'2'"`       |
 * | `"1234567890abcdefghi"` |      20 | `"'1234567890abcdef'..."`   |
 * | `"1é2é3é"`              |       9 | `"'1'||x'c3a9'||'2'..."`    |
 * | `"1\0""2"`              |       3 | `"'1'||x'00'||'2'"`         |
 * | `NULL`                  |       0 | `"NULL"`                    |
 * | `"1'2"`                 |      -1 | `"'1''2"`                   |
 */
static char* escape_str_to_sql_literal(
        char* out_str, size_t out_len,
        const char* in_str,  ssize_t in_len
) {
    if(in_str == NULL) {
        return "NULL";
    }
    if(in_len < 0) {
        in_len = strlen(in_str);
    }
    if(in_len == 0) {
        return "''";
    }
    if(out_len < 6) {
        return "''...";
    }
    size_t cursor = 0;
    // mode = 'x' for binary string and 'a' for ascii string and '\0' at the initialization
    char oldmode = '\0';
    bool has_broken = false;
    assert(in_len > 0);
    for(size_t idx = 0; idx < ((size_t)in_len); idx++) {
        char c = in_str[idx];
        char mode;
        if(c >= 0x20 && c <= 0x7E) {
            mode = 'a';
        } else {
            mode = 'x';
        }
        char buffer_str[20];
        size_t buffer_len;
        char* fmt;
        char* join_prefix;
        char* quote_prefix;
        char* escape_prefix;
        if(oldmode != mode) {
            if(oldmode == '\0') {
                join_prefix = "";
            } else {
                join_prefix = "'||";
            }
            if(mode == 'a') {
                quote_prefix = "'";
            } else {
                quote_prefix = "x'";
            }
        } else {
            join_prefix = "";
            quote_prefix = "";
        }
        oldmode = mode;
        if(c == '\'') {
            escape_prefix = "'";
        } else {
            escape_prefix = "";
        }
        if(mode == 'x') {
            // Note: Length modifier hh, available in glibc snprintf, is not
            // available for sqlite3_snprintf.
            // A mask needs to be applied to the char in order to ensure that
            // only the first byte is taken in account.
            fmt = "%s%s%s%02x";
        } else {
            fmt = "%s%s%s%c";
        }
        sqlite3_snprintf(
                sizeof(buffer_str), buffer_str, fmt,
                join_prefix, quote_prefix, escape_prefix, c & 0xff
        );
        buffer_len = strlen(buffer_str);
        assert(buffer_len < sizeof(buffer_str)-2);

        if(cursor + buffer_len + 5 > out_len) {
            has_broken = true;
            break;
        } else {
            strcpy(out_str + cursor, buffer_str);
            cursor += buffer_len;
        }
    }
    if(has_broken) {
        strcpy(out_str + cursor, "'...");
        cursor += 4;
    } else {
        strcpy(out_str + cursor, "'");
        cursor += 1;
    }
    return out_str;
}


/**
 * Forward the error from PCRE to SQLite as is.
 *
 * @param ctx: SQLite context.
 * @param error_code: error code of PCRE.
 *
 * Example:
 *
 *    error_pcre2sqlite(
 *      ctx,
 *      PCRE2_ERROR_MISSING_CLOSING_PARENTHESIS
 *    );
 *    // Call
 *    // Call sqlite3_result_error with string:
 *    // "missing closing parenthesis"
 */
static void error_pcre2sqlite(sqlite3_context *ctx, int error_code) {
    PCRE2_UCHAR error_buffer[256];
    pcre2_get_error_message(error_code, error_buffer, sizeof(error_buffer));
    sqlite3_result_error(ctx, error_buffer, -1);
}


/**
 * See error_pcre2sqlite_prefixed variadic function.
 */
static int error_pcre2sqlite_prefixedv(int error_code, char **error_str, const char* fmt, va_list vargs) {
    PCRE2_UCHAR error_buffer[256];
    int error_len = pcre2_get_error_message(error_code, error_buffer, sizeof(error_buffer));
    if(error_len == PCRE2_ERROR_NOMEMORY) {
        // The error buffer is too small: the error message is truncated.
        // This is not an ideal behaviour, but acceptable in such condition.
        // It is better to continue than blocking here.
        error_len = sizeof(error_buffer);
    }
    char* substr = sqlite3_vmprintf(fmt, vargs);

    if(substr == NULL) {
        *error_str = NULL;
        return SQLITE_NOMEM;
    }
    sqlite3_uint64 substr_len = strlen(substr);

    *error_str = sqlite3_mprintf("%s (%s)", substr, error_buffer);

    sqlite3_free(substr);
    if(error_str == NULL) {
        return SQLITE_NOMEM;
    } else {
        return SQLITE_ERROR;
    }
}


/**
 * Create an error message from PCRE to SQLite and prefix it by a custom message.
 *
 * @param error_code: error code of PCRE.
 * @param str: output string from the function will be written here, or NULL if
 *    there is a dynamic memory allocation error.
 *    The `str` needs to be freed afterwards with `sqlite3_free()`.
 * @param fmt: format of the prefix of the error message.
 *    The format of fmt and the variadic arguments are the same as
 *    the ones for `snprintf`.
 * @return: SQLITE_NOMEM if a dymanic memory allocation fails
 *    or SQLITE_ERROR if the error message has successfully been generated.
 *
 * Example:
 *
 *    char* error_str;
 *    int error_code = error_pcre2sqlite_prefixed(
 *      PCRE2_ERROR_MISSING_CLOSING_PARENTHESIS,
 *      &error_str,
 *      "Cannot compile pattern \"%s\" at offset %d"
 *      "ab(", 3
 *    );
 *    switch(error_code) {
 *    case SQLITE_NOMEM:
 *        sqlite3_result_error_nomem(ctx);
 *        break;
 *    case SQLITE_ERROR:
 *        //             /-------- pre-formatted error ------------\  /------ PCRE2 error -------\
 *        // error_str = "Cannot compile pattern \"ab(\" at offset 3 (missing closing parenthesis)"
 *        sqlite3_result_error(ctx, error_str);
 *        sqlite3_free(error_str);
 *        break;
 *    default: assert(false);
 *    }
 *    // In case of memory alloc error:
 *    // - error_str = NULL
 *    // - error_code = SQLITE_NOMEM
 *    // otherwise:
 *    // - error_code = SQLITE_NOMEM
 *    // In both case, the following code will produce the expected outputs:
 *    sqlite3_result_error(ctx, error_str, -1);
 */
static int error_pcre2sqlite_prefixed(int error_code, char **error_str, const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    error_pcre2sqlite_prefixedv(error_code, error_str, fmt, vargs);
    va_end(vargs);
}


/**
 * Forward the error from PCRE to SQLite and prefix it by custom message.
 *
 * @param ctx: SQLite context.
 * @param error_code: error code of PCRE.
 * @param fmt: format of the prefix of the error message.
 *    The format of fmt and the variadic arguments are the same as
 *    the ones for `snprintf`.
 *
 * Example:
 *
 *    error_pcre2sqlite_ctx_prefixed(
 *      ctx,
 *      PCRE2_ERROR_MISSING_CLOSING_PARENTHESIS,
 *      "Cannot compile pattern \"%s\" at offset %d"
 *      "ab(", 3
 *    );
 *    // Call sqlite3_result_error with string:
 *    // "Cannot compile pattern \"ab(\" at offset 3 (missing closing parenthesis)"
 *
 */
static void error_pcre2sqlite_ctx_prefixed(sqlite3_context *ctx, int error_code, const char* fmt, ...) {
    char* error_str;
    va_list vargs;
    va_start(vargs, fmt);
    int ans = error_pcre2sqlite_prefixedv(error_code, &error_str, fmt, vargs);
    va_end(vargs);

    if(ans == SQLITE_NOMEM) {
        sqlite3_result_error_nomem(ctx);
    } else {
        assert(ans == SQLITE_ERROR);
        sqlite3_result_error(ctx, error_str, -1);
        sqlite3_free(error_str);
    }
}


/**
 * Get a `pcre2_code*` entry from the cache or create a new one.
 *
 * Try to find a cache value corresponding to the `pattern_str` and
 * `pattern_len` in the cache. If no entry is found, a new `pcre2_code*`
 * instance is created instead and put in the cache.
 *
 * If no error have been raised, the function return `SQLITE_OK` and the
 * function will set `*pattern_code`.
 *
 * If an error has been raised, the function returns an error code and
 * `*pattern_code` will be set to NULL.
 * - SQLITE_ERROR is returned if the pattern is invalid and `*error_str` is
 * set with a string that needs to be freed with sqlite3_free.
 * - SQLITE_NOMEM is returned in case of memory allocation error and `*error_str`
 * is set to NULL.
 */
static int pcre2_compile_from_sqlite_cache(
        cache_entry *cache,
        const char *pattern_str,
        int pattern_len,
        pcre2_code** pattern_code,
        char **error_str) {
    /* simple LRU cache */
    int i;
    int found = 0;

    assert(cache);
    *pattern_code = NULL;

    for (i = 0; i < CACHE_SIZE && cache[i].pattern_str; i++)
        if (
            pattern_len == cache[i].pattern_len
            && memcmp(pattern_str, cache[i].pattern_str, pattern_len) == 0
        ) {
            found = 1;
            break;
        }
    if (found) {
        if (i > 0) {
            cache_entry c = cache[i];
            memmove(cache + 1, cache, i * sizeof(cache_entry));
            cache[0] = c;
        }
    } else {
        cache_entry c;
        const char *err;
        int error_code;
        unsigned long error_position;
        c.pattern_code = pcre2_compile(
            pattern_str,           /* the pattern */
            pattern_len,           /* the length of the pattern */
            0,                     /* default options */
            &error_code,           /* for error number */
            &error_position,       /* for error offset */
            NULL);                 /* use default compile context */
        if (!c.pattern_code) {
            char literal_regex[256];
            return error_pcre2sqlite_prefixed(
                error_code, error_str,
                "Cannot compile REGEXP pattern %s at offset %lu",
                escape_str_to_sql_literal(
                    literal_regex, sizeof(literal_regex),
                    pattern_str, pattern_len
                ),
                error_position
            );
        }
        c.pattern_str = malloc(pattern_len);
        if (!c.pattern_str) {
            SQLITE_NOMEM;
        }
        memcpy(c.pattern_str, pattern_str, pattern_len);
        c.pattern_len = pattern_len;
        i = CACHE_SIZE - 1;
        if (cache[i].pattern_str) {
            free(cache[i].pattern_str);
            assert(cache[i].pattern_code);
            pcre2_code_free(cache[i].pattern_code);
        }
        memmove(cache + 1, cache, i * sizeof(cache_entry));
        cache[0] = c;
    }
    *pattern_code = cache[0].pattern_code;
    *error_str = NULL;
    return SQLITE_OK;
}


/**
 * Get a pcre2_code entry from the cache or create a new one.
 *
 * Try to find a cache value corresponding to the pattern_str and pattern_len
 * in the cache. If no entry is found, a new pcre2_code instance is created
 * instead and put in the cache.
 *
 * If an error has been raised, the function return NULL.
 *
 * Raises an error in the following case:
 * * the pattern is invalid.
 * * there are no memory available to build the regular expression.
 */
static pcre2_code* pcre2_compile_from_sqlite_ctx_cache(
        sqlite3_context *ctx,
        const char *pattern_str,
        int pattern_len) {
    cache_entry *cache = sqlite3_user_data(ctx);
    pcre2_code* pattern_code;
    char *error_str;
    int error_code = pcre2_compile_from_sqlite_cache(
        cache, pattern_str, pattern_len,
        &pattern_code, &error_str
    );
    if(error_code == SQLITE_OK) {
        return pattern_code;
    } else {
        if(error_code == SQLITE_NOMEM) {
            sqlite3_result_error_nomem(ctx);
        } else {
            sqlite3_result_error(ctx, error_str, -1);
        }
        return NULL;
    }
}


/**
 * Compute the number of character up to position n of byte array.
 *
 * @param bytes: bytes from which we are counting character.
 * @param n: index of byte up to which we want to count characters.
 *
 * Example
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * // "1é2" in UTF-8 equals
 * // {
 * //     0x31,         // 1
 * //     0xC3, 0xA9,   // é
 * //     0x32,         // 2
 * // }
 * utf8_char_cnt("1é2", 0); // return 0
 * utf8_char_cnt("1é2", 1); // return 1
 * utf8_char_cnt("1é2", 2); // return 1, even if invalid
 * utf8_char_cnt("1é2", 3); // return 2
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
static size_t utf8_char_cnt(const char * bytes, const size_t n) {
    size_t continuation_byte_cnt = 0;
    for(size_t idx = 0; idx < n; idx++) {
        if ((bytes[idx] &  0xC0) == 0x80) {
          continuation_byte_cnt++;
        }
    }
    return n - continuation_byte_cnt;
}


static void regexp(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    const char *pattern_str, *subject_str;
    int pattern_len, subject_len;
    pcre2_code *pattern_code;

    assert(argc == 2);    // TODO check argc = 1 and argc = 3
    /* check null */
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        return;
    }

    /* extract parameters values */
    pattern_str = (const char *) sqlite3_value_text(argv[0]);
    if (!pattern_str) {
        sqlite3_result_error(ctx, "no pattern", -1);
        return;
    }
    pattern_len = sqlite3_value_bytes(argv[0]);

    subject_str = (const char *) sqlite3_value_text(argv[1]);
    if (!subject_str) {
        sqlite3_result_error(ctx, "no subject", -1);
        return;
    }
    subject_len = sqlite3_value_bytes(argv[1]);

    pattern_code = pcre2_compile_from_sqlite_ctx_cache(
        ctx, pattern_str, pattern_len
    );
    if(pattern_code == NULL) {
        return;
    }

    {
        int rc;
        pcre2_match_data *match_data;
        assert(pattern_code);

        match_data = pcre2_match_data_create_from_pattern(pattern_code, NULL);
        rc = pcre2_match(
          pattern_code,         /* the compiled pattern */
          subject_str,          /* the subject string */
          subject_len,          /* the length of the subject */
          0,                    /* start at offset 0 in the subject */
          0,                    /* default options */
          match_data,           /* block for storing the result */
          NULL);                /* use default match context */

        assert(rc != 0);
        if(rc >= 0) {
            sqlite3_result_int(ctx, 1);
        } else if(rc == PCRE2_ERROR_NOMATCH) {
            sqlite3_result_int(ctx, 0);
        } else { // (rc < 0 and the code is not one of the above)
            error_pcre2sqlite(ctx, rc);
        }
        pcre2_match_data_free(match_data);
        return;
    }
}


static void regexp_instr(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    const char *pattern_str, *subject_str;
    int pattern_len, subject_len;
    bool subject_is_blob;
    pcre2_code *pattern_code;

    assert(argc == 2);    // TODO check argc = 1 and argc = 3
    /* check null */
    if (sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL) {
        return;
    }

    /* extract parameters values */
    subject_is_blob = sqlite3_value_type(argv[0]) == SQLITE_BLOB;
    if (subject_is_blob) {
        subject_str = (const char *) sqlite3_value_blob(argv[0]);
    } else {
        subject_str = (const char *) sqlite3_value_text(argv[0]);
    }
    if (!subject_str) {
        sqlite3_result_error(ctx, "no subject", -1);
        return;
    }
    subject_len = sqlite3_value_bytes(argv[0]);

    pattern_str = (const char *) sqlite3_value_text(argv[1]);
    if (!pattern_str) {
        sqlite3_result_error(ctx, "no pattern", -1);
        return;
    }
    pattern_len = sqlite3_value_bytes(argv[1]);

    pattern_code = pcre2_compile_from_sqlite_ctx_cache(
        ctx, pattern_str, pattern_len
    );
    if(pattern_code == NULL) {
        return;
    }

    {
        int rc;
        pcre2_match_data *match_data;
        assert(pattern_code);

        match_data = pcre2_match_data_create_from_pattern(pattern_code, NULL);
        rc = pcre2_match(
          pattern_code,         /* the compiled pattern */
          subject_str,          /* the subject string */
          subject_len,          /* the length of the subject */
          0,                    /* start at offset 0 in the subject */
          0,                    /* default options */
          match_data,           /* block for storing the result */
          NULL);                /* use default match context */

        assert(rc != 0);
        if(rc >= 0) {
            PCRE2_SIZE ans;
            PCRE2_SIZE* ovector = pcre2_get_ovector_pointer(match_data);
            uint32_t ovector_count = pcre2_get_ovector_count(match_data);
            assert(ovector_count > 0);
            ans = ovector[0];
            assert(ans < ((uint64_t)subject_len));
            if (!subject_is_blob) {
                ans = utf8_char_cnt(subject_str, ans);
            }
            assert(ans < INT64_MAX - 1);
            ans++;
            sqlite3_result_int64(ctx, ans);
        } else if(rc == PCRE2_ERROR_NOMATCH) {
            sqlite3_result_int(ctx, 0);
        } else { // (rc < 0 and the code is not one of the above)
            error_pcre2sqlite(ctx, rc);
        }
        pcre2_match_data_free(match_data);
        return;
    }
}


static void regexp_substr(
        sqlite3_context *ctx,
        int argc,
        sqlite3_value **argv) {
    const char *pattern_str, *subject_str;
    int pattern_len, subject_len;
    pcre2_code *pattern_code;

    assert(argc == 2);      // TODO check argc == 1 and argc = 3
    /* check NULL parameter */
    for(int idx = 0; idx < 2; idx++) {
      if(sqlite3_value_type(argv[idx]) == SQLITE_NULL) {
        return;
      }
    }

    /* extract parameter values */
    subject_len = sqlite3_value_bytes(argv[0]);
    subject_str = (const char *) sqlite3_value_text(argv[0]);
    if (!subject_str) {
        sqlite3_result_error(ctx, "no subject", -1);
        return;
    }

    pattern_str = (const char *) sqlite3_value_text(argv[1]);
    if (!pattern_str) {
        sqlite3_result_error(ctx, "no pattern", -1);
        return;
    }
    pattern_len = sqlite3_value_bytes(argv[1]);

    pattern_code = pcre2_compile_from_sqlite_ctx_cache(
        ctx, pattern_str, pattern_len
    );
    if(pattern_code == NULL) {
        return;
    }

    {
        int rc;
        pcre2_match_data *match_data;

        match_data = pcre2_match_data_create_from_pattern(pattern_code, NULL);
        rc = pcre2_match(
          pattern_code,         /* the compiled pattern */
          subject_str,          /* the subject string */
          subject_len,          /* the length of the subject */
          0,                    /* start at offset 0 in the subject */
          0,                    /* default options */
          match_data,           /* block for storing the result */
          NULL);                /* use default match context */

        if(rc >= 0) {
            size_t result_len;
            unsigned char * result_str;
            rc = pcre2_substring_get_bynumber(match_data, 0, &result_str, &result_len);
            if(rc == 0) {
                sqlite3_result_text(ctx, result_str, result_len, (void (*)(void *))pcre2_substring_free);
            } else {
                error_pcre2sqlite(ctx, rc);
            }
        } else if(rc == PCRE2_ERROR_NOMATCH) {
            sqlite3_result_text(ctx, "", 0, SQLITE_STATIC);
        } else { // (rc < 0 and the code is not one of the above)
            error_pcre2sqlite(ctx, rc);
        }
        pcre2_match_data_free(match_data);
        return;
    }
}
static void regexp_replace(
        sqlite3_context *ctx,
        int argc,
        sqlite3_value **argv) {
    const char *pattern_str, *subject_str, *replacement_str;
    int pattern_len, subject_len, replacement_len;
    pcre2_code *pattern_code;

    assert(argc == 3);      // TODO check argc == 2 and argc = 4
    /* check NULL parameter */
    for(int idx = 0; idx < 3; idx++) {
      if(sqlite3_value_type(argv[idx]) == SQLITE_NULL) {
        return;
      }
    }

    /* extract parameter values */
    pattern_str = (const char *) sqlite3_value_text(argv[1]);
    if (!pattern_str) {
        sqlite3_result_error(ctx, "no pattern", -1);
        return;
    }
    pattern_len = sqlite3_value_bytes(argv[1]);

    subject_str = (const char *) sqlite3_value_text(argv[0]);
    if (!subject_str) {
        sqlite3_result_error(ctx, "no subject", -1);
        return;
    }
    subject_len = sqlite3_value_bytes(argv[0]);

    replacement_str = (const char *) sqlite3_value_text(argv[2]);
    if (!replacement_str) {
        sqlite3_result_error(ctx, "no replacement", -1);
        return;
    }
    replacement_len = sqlite3_value_bytes(argv[2]);

    pattern_code = pcre2_compile_from_sqlite_ctx_cache(
        ctx, pattern_str, pattern_len
    );
    if(pattern_code == NULL) {
        return;
    }

    {
        int substitute_cnt;
        size_t substitute_len = 0;
        char *substitute_str = "";
        substitute_cnt = pcre2_substitute(
            pattern_code,       // const pcre2_code *code,
            subject_str,        // PCRE2_SPTR subject,
            subject_len,        // PCRE2_SIZE length,
            0,                  // PCRE2_SIZE startoffset,
            (
                PCRE2_SUBSTITUTE_GLOBAL
                | PCRE2_SUBSTITUTE_EXTENDED
                | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH
            ),                  // uint32_t options,
            NULL,               // pcre2_match_data *match_data,
            NULL,               // pcre2_match_context *mcontext,
            replacement_str,    // PCRE2_SPTR replacement,
            replacement_len,    // PCRE2_SIZE rlength,
            substitute_str,     // PCRE2_UCHAR *outputbuffer,
            &substitute_len     // PCRE2_SIZE *outlengthptr
        );
        if(substitute_cnt == PCRE2_ERROR_NOMEMORY) {
            assert(substitute_len > 0);
            substitute_str = sqlite3_malloc(substitute_len);
            substitute_cnt = pcre2_substitute(
                pattern_code,       // const pcre2_code *code,
                subject_str,        // PCRE2_SPTR subject,
                subject_len,        // PCRE2_SIZE length,
                0,                  // PCRE2_SIZE startoffset,
                (
                   PCRE2_SUBSTITUTE_GLOBAL
                   | PCRE2_SUBSTITUTE_EXTENDED
                ),                  // uint32_t options,
                NULL,               // pcre2_match_data *match_data,
                NULL,               // pcre2_match_context *mcontext,
                replacement_str,    // PCRE2_SPTR replacement,
                replacement_len,    // PCRE2_SIZE rlength,
                substitute_str,     // PCRE2_UCHAR *outputbuffer,
                &substitute_len     // PCRE2_SIZE *outlengthptr
            );

            if(substitute_cnt >= 0) {
                sqlite3_result_text(ctx, substitute_str, substitute_len, sqlite3_free);
            } else {
                char subject_literal[256], pattern_literal[256], replacement_literal[256];
                error_pcre2sqlite_ctx_prefixed(ctx, substitute_cnt,
                    "Cannot execute REGEXP_REPLACE(%s, %s, %s) at character %d",
                    escape_str_to_sql_literal(    subject_literal, sizeof(    subject_literal),     subject_str,     subject_len),
                    escape_str_to_sql_literal(    pattern_literal, sizeof(    pattern_literal),     pattern_str,     pattern_len),
                    escape_str_to_sql_literal(replacement_literal, sizeof(replacement_literal), replacement_str, replacement_len),
                    substitute_len);
            }
        } else if(substitute_cnt >= 0) {
            assert(substitute_len == 0);
            sqlite3_result_text(ctx, "", substitute_len, SQLITE_STATIC);
        } else {
            assert(substitute_cnt < 0);
            char subject_literal[256], pattern_literal[256], replacement_literal[256];
            error_pcre2sqlite_ctx_prefixed(
                ctx, substitute_cnt,
                "Cannot execute REGEXP_REPLACE(%s, %s, %s) at character %d",
                escape_str_to_sql_literal(    subject_literal, sizeof(    subject_literal),     subject_str,     subject_len),
                escape_str_to_sql_literal(    pattern_literal, sizeof(    pattern_literal),     pattern_str,     pattern_len),
                escape_str_to_sql_literal(replacement_literal, sizeof(replacement_literal), replacement_str, replacement_len),
                substitute_len);
        }
    }
}


int sqlite3_extension_init(sqlite3 *db, char **err, const sqlite3_api_routines *api) {
    SQLITE_EXTENSION_INIT2(api)
    cache_entry *cache = calloc(CACHE_SIZE, sizeof(cache_entry));
    if (!cache) {
        *err = "calloc: ENOMEM";
        return 1;
    }
    sqlite3_create_function(db, "REGEXP", 2, SQLITE_UTF8, cache, regexp, NULL, NULL);
    sqlite3_create_function(db, "REGEXP_INSTR", 2, SQLITE_UTF8, cache, regexp_instr, NULL, NULL);
    sqlite3_create_function(db, "REGEXP_SUBSTR", 2, SQLITE_UTF8, cache, regexp_substr, NULL, NULL);
    sqlite3_create_function(db, "REGEXP_REPLACE", 3, SQLITE_UTF8, cache, regexp_replace, NULL, NULL);
    return 0;
}
