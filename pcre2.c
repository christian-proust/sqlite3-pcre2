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
 *    // Call sqlite3_result_error with string:
 *    // "missing closing parenthesis"
 */
void error_pcre2sqlite(sqlite3_context *ctx, int error_code) {
    PCRE2_UCHAR error_buffer[256];
    pcre2_get_error_message(error_code, error_buffer, sizeof(error_buffer));
    sqlite3_result_error(ctx, error_buffer, -1);
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
 *    error_pcre2sqlite_prefixed(
 *      ctx,
 *      PCRE2_ERROR_MISSING_CLOSING_PARENTHESIS,
 *      "Cannot compile pattern \"%s\" at offset %d"
 *      "ab(", 3
 *    );
 *    // Call sqlite3_result_error with string:
 *    // "Cannot compile pattern \"ab(\" at offset 3 (missing closing parenthesis)"
 *
 */
void error_pcre2sqlite_prefixed(sqlite3_context *ctx, int error_code, const char* fmt, ...) {
    // Get the error from PCRE
    PCRE2_UCHAR error_buffer[256];
    pcre2_get_error_message(error_code, error_buffer, sizeof(error_buffer));
    // initialize the sqlite3_str object
    sqlite3* db = sqlite3_context_db_handle(ctx);
    sqlite3_str* str = sqlite3_str_new(db);
    // Add the error message, as formatted with fmt and the variadic args
    va_list vargs;
    va_start(vargs, fmt);
    sqlite3_str_vappendf(str, fmt, vargs);
    va_end(vargs);
    // Append the message from PCRE
    sqlite3_str_append(str, " (", 2);
    sqlite3_str_appendall(str, error_buffer);
    sqlite3_str_appendchar(str, 1, ')');
    // Return the sqlite3_str as error to the user
    switch(sqlite3_str_errcode(str)) {
      case SQLITE_OK:
        sqlite3_result_error(ctx, sqlite3_str_value(str), sqlite3_str_length(str));
        break;
      case SQLITE_NOMEM:
        sqlite3_result_error_nomem(ctx);
        break;
      case SQLITE_TOOBIG:
        sqlite3_result_error_toobig(ctx);
        break;
      default:
        assert(0);
    }
    // destroy the sqlite3_str object
    sqlite3_free(sqlite3_str_finish(str));
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
pcre2_code* pcre2_compile_from_sqlite_cache(
        sqlite3_context *ctx,
        const char *pattern_str,
        int pattern_len) {
    /* simple LRU cache */
    int i;
    int found = 0;
    cache_entry *cache = sqlite3_user_data(ctx);

    assert(cache);

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
            error_pcre2sqlite_prefixed(
                ctx, error_code,
                "Cannot compile REGEXP pattern %Q at offset %lu",
                pattern_str, error_position
                // FIXME Cannot display pattern in case of NUL character
            );
            return NULL;
        }
        c.pattern_str = malloc(pattern_len);
        if (!c.pattern_str) {
            sqlite3_result_error_nomem(ctx);
            pcre2_code_free(c.pattern_code);
            return NULL;
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
    return cache[0].pattern_code;
}


static
void regexp(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
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

    pattern_code = pcre2_compile_from_sqlite_cache(
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

        assert(rc != 0);  // because we have not set match_data
        if(rc >= 0) {
          // Normal case because we have not set match_data
          sqlite3_result_int(ctx, 1);
        } else if(rc == PCRE2_ERROR_NOMATCH) {
          sqlite3_result_int(ctx, 0);
        } else { // (rc < 0 and the code is not one of the above)
            error_pcre2sqlite(ctx, rc);
            return;  // FIXME memory leak
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

    pattern_code = pcre2_compile_from_sqlite_cache(
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

    pattern_code = pcre2_compile_from_sqlite_cache(
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
                error_pcre2sqlite_prefixed(ctx, substitute_cnt,
                    "Cannot execute REGEXP_REPLACE(%Q, %Q, %Q) at character %d",
                    subject_str, pattern_str, replacement_str,
                    substitute_len);
            }
        } else if(substitute_cnt >= 0) {
            assert(substitute_len == 0);
            sqlite3_result_text(ctx, "", substitute_len, SQLITE_STATIC);
        } else {
            assert(substitute_cnt < 0);
            error_pcre2sqlite_prefixed(
                ctx, substitute_cnt,
                "Cannot execute REGEXP_REPLACE(%Q, %Q, %Q) at character %d",
                subject_str, pattern_str, replacement_str,
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
    sqlite3_create_function(db, "REGEXP_SUBSTR", 2, SQLITE_UTF8, cache, regexp_substr, NULL, NULL);
    sqlite3_create_function(db, "REGEXP_REPLACE", 3, SQLITE_UTF8, cache, regexp_replace, NULL, NULL);
    return 0;
}
