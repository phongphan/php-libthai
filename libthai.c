#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/iconv/php_iconv.h"
#include "php_libthai.h"

#include <thai/thbrk.h>
#include <thai/thwbrk.h>

#include <stdint.h>
#include <stdbool.h>

static int le_thbrk_context = FAILURE;

static const char* input_charset = "UTF-8";
static const char* internal_charset = "UTF-16LE";

static zend_string* thwchar_substr_to_utf8(thwchar_t* input, int start, int end)
{
    int count = end - start;
    int buffer_len = count * 2 + 1;

    unsigned char buffer[buffer_len];
    memset(buffer, 0, buffer_len * sizeof(char));

    for (int i = 0; i < buffer_len - 1; i += 2) {
        buffer[i] = input[start+i/2];
        buffer[i+1] = input[start+i/2] >> 8;
    }
    zend_string* out = NULL;
    php_iconv_string((const char*)buffer, (size_t)buffer_len, &out, input_charset, internal_charset);

    return out;
}

static void thbrk_context_dtor(zend_resource* rsrc)
{
    if (rsrc->ptr != NULL) {
        th_brk_delete((ThBrk*)rsrc->ptr);
    }
}

static bool is_high_surrogate(thwchar_t w)
{
    return w >= 0xd800 && w <= 0xdbff;
}

static bool is_low_surrogate(thwchar_t w)
{
    return w >= 0xdc00 && w <= 0xdcff;
}

static int find_breaks(ThBrk* brk, thwchar_t* str, int** pos_ptr, int len)
{
    thwchar_t subwchar[len + 1];
    memset(subwchar, 0, (len + 1) * sizeof(thwchar_t));
    memmove(subwchar, str, len * sizeof(thwchar_t));

    // libthai always insert marker at the end of it. we also considered the last.
    return th_brk_wc_find_breaks(brk, subwchar, *pos_ptr, len) + 1;
}

static void fix_pos_order(int* pos_ptr, bool remove_last)
{
    int* pos = pos_ptr;
    int base = 0; int prev = 0;
    do {
        int val = *pos;
        if (val == 0) {
            if (remove_last) {
                *(pos-1) = 0;
            }
            break;
        }
        if (val < prev) {
            base = *(pos-1);
        }
        prev = val;
        *pos = base + val;
    } while (*(pos++) > 0);
}

/* {{{ proto string th_brk_new(string arg)
   Return a pointer to th_brk struct */
PHP_FUNCTION(th_brk_new)
{
    char* path = NULL;
    size_t path_len;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    ThBrk* brk = th_brk_new((path == NULL || path[0] == '\0') ? NULL : path);
    if (brk == NULL) {
        php_error_docref(NULL, E_ERROR, "Cannot initialize thbrk!");
        RETURN_FALSE;
    }
    RETURN_RES(zend_register_resource(brk, le_thbrk_context));
}
/* }}} */

/* {{{ proto string th_brk_wc_find_breaks(long brk, string arg)
   Return an array of the break position */
PHP_FUNCTION(th_brk_wc_find_breaks)
{
    zval* res;
    zend_string* input = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_RESOURCE(res)
        Z_PARAM_STR(input)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    ThBrk* brk = (ThBrk*)Z_RES_P(res)->ptr;

    zend_string* ustr = NULL;
    php_iconv_err_t err = php_iconv_string(ZSTR_VAL(input), (size_t)ZSTR_LEN(input), &ustr, internal_charset, input_charset);
    if (err != PHP_ICONV_ERR_SUCCESS || ustr == NULL) {
        zend_string_free(ustr);
        RETURN_FALSE;
    }

    int ustr_len = ZSTR_LEN(ustr) / 2;
    thwchar_t* thwchar = (thwchar_t*)emalloc(ustr_len * sizeof(thwchar_t));
    for (int i = 0; i < ZSTR_LEN(ustr); i += 2) {
        thwchar[i/2] = ((ZSTR_VAL(ustr)[i+1] & 0xFF) << 8) + (ZSTR_VAL(ustr)[i] & 0xFF);
    }

    int pos[ustr_len];
    memset(pos, 0, ustr_len * sizeof(int));
    int* pos_ptr = &pos[0];

    int offset = 0;
    for (int i = 0; i < ustr_len; i++) {
        if (is_high_surrogate(thwchar[i]) && !(i > 0 && is_low_surrogate(thwchar[i-1]))) {
            int numcut = find_breaks(brk, thwchar + offset, &pos_ptr, i - offset);
            pos_ptr += numcut;
        }
        else if (is_low_surrogate(thwchar[i])) {
            *pos_ptr = *(pos_ptr - 1) + 1;
            pos_ptr++;
		    offset = i + 1;
        }
        else if (i == ustr_len - 1) {
            find_breaks(brk, thwchar + offset, &pos_ptr, ustr_len - offset);
        }
    }

    // Fixed the index
    fix_pos_order(&pos[0], true);

    array_init(return_value);
    for (int i = 0; pos[i] != 0; i++) {
        add_index_long(return_value, i, pos[i]);
    }

    zend_string_free(ustr);
    efree(thwchar);
}
/* }}} */

/* {{{ proto string th_brk_wc_split(long brk, string arg)
   Return an array of the splitted string */
PHP_FUNCTION(th_brk_wc_split)
{
    zval* res;
    zend_string* input = NULL;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_RESOURCE(res)
        Z_PARAM_STR(input)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    ThBrk* brk = Z_RES_P(res)->ptr;

    zend_string* ustr = NULL;
    php_iconv_err_t err = php_iconv_string(ZSTR_VAL(input), (size_t)ZSTR_LEN(input), &ustr, internal_charset, input_charset);
    if (err != PHP_ICONV_ERR_SUCCESS || ustr == NULL) {
        zend_string_free(ustr);
        RETURN_FALSE;
    }

    int ustr_len = ZSTR_LEN(ustr) / 2;
    thwchar_t* thwchar = (thwchar_t*)emalloc(ustr_len * sizeof(thwchar_t));
    for (int i = 0; i < ZSTR_LEN(ustr); i += 2) {
        thwchar[i/2] = ((ZSTR_VAL(ustr)[i+1] & 0xFF) << 8) + (ZSTR_VAL(ustr)[i] & 0xFF);
    }

    int pos[ustr_len];
    memset(pos, 0, ustr_len * sizeof(int));
    int* pos_ptr = &pos[0];

    array_init(return_value);

    int idx = 0; int offset = 0; thwchar_t* wptr = thwchar;
    for (int i = 0; i < ustr_len; i++) {
        if (is_high_surrogate(thwchar[i]) && !(i > 0 && is_low_surrogate(thwchar[i-1]))) {
            int numcut = find_breaks(brk, wptr, &pos_ptr, i - offset);
            int prev = 0;
            for (int j = 0; j < numcut; j++) {
                int next = *(pos_ptr + j); int size = next - prev;

                zend_string* out = thwchar_substr_to_utf8(wptr, 0, size);
                add_index_stringl(return_value, idx++, ZSTR_VAL(out), ZSTR_LEN(out));
                zend_string_free(out);

                wptr += size; prev = next;
            }
        }
        else if (is_low_surrogate(thwchar[i])) {
            zend_string* out = thwchar_substr_to_utf8(wptr, 0, 2);
            add_index_stringl(return_value, idx++, ZSTR_VAL(out), ZSTR_LEN(out));
            zend_string_free(out);
            wptr += 2;
		    offset = i + 1;
        }
        else if (i == ustr_len - 1) {
            int numcut = find_breaks(brk, wptr, &pos_ptr, ustr_len - offset);
            int prev = 0;
            for (int j = 0; j < numcut; j++) {
                int next = *(pos_ptr + j); int size = next - prev;

                zend_string* out = thwchar_substr_to_utf8(wptr, 0, size);
                add_index_stringl(return_value, idx++, ZSTR_VAL(out), ZSTR_LEN(out));
                zend_string_free(out);

                wptr += size; prev = next;
            }
        }
    }

    zend_string_free(ustr);
    efree(thwchar);
}
/* }}} */

/* {{{ proto void th_brk_delete(thbrk) */
PHP_FUNCTION(th_brk_delete)
{
    zval* res;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_RESOURCE(res)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    ThBrk* brk = Z_RES_P(res)->ptr;
    if (brk != NULL) {
        th_brk_delete(brk);
        Z_RES_P(res)->ptr = NULL;
    }
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
*/
PHP_MINIT_FUNCTION(libthai)
{
    le_thbrk_context = zend_register_list_destructors_ex(thbrk_context_dtor, NULL, "thbrk-context", module_number);
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
*/
PHP_MSHUTDOWN_FUNCTION(libthai)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
*/
PHP_MINFO_FUNCTION(libthai)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "libthai support", "enabled");
    php_info_print_table_end();
}
/* }}} */

/* {{{ libthai_functions[]
 *
 * Every user visible function must have an entry in libthai_functions[].
 */
const zend_function_entry libthai_functions[] = {
    PHP_FE(th_brk_new, NULL)
    PHP_FE(th_brk_delete, NULL)
    PHP_FE(th_brk_wc_find_breaks, NULL)
    PHP_FE(th_brk_wc_split, NULL)
    PHP_FE_END	/* Must be the last line in libthai_functions[] */
};
/* }}} */

/* {{{ libthai_module_entry
*/
zend_module_entry libthai_module_entry = {
    STANDARD_MODULE_HEADER,
    "libthai",
    libthai_functions,
    PHP_MINIT(libthai),
    PHP_MSHUTDOWN(libthai),
    NULL,
    NULL,
    PHP_MINFO(libthai),
    PHP_LIBTHAI_VERSION,
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_LIBTHAI
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(libthai)
#endif

