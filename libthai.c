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

#include <iconv.h>
#include <stdint.h>

/* sharing globals is *evil* */
static int le_thbrk_context = FAILURE;

static const char* input_charset = "UTF-8";
static const char* internal_charset = "UTF-16LE";

static zend_string* thwchar_substr(thwchar_t* input, int start, int end)
{
    int count = end - start;
    int buffer_len = count * 2 + 1;

    char buffer[buffer_len];
    memset(buffer, 0, buffer_len * sizeof(char));

    for (int i = 0; i < buffer_len - 1; i += 2) {
        buffer[i] = input[start+i/2];
        buffer[i+1] = input[start+i/2] >> 8;
    }
    zend_string* out = NULL;
    php_iconv_string(buffer, (size_t)buffer_len, &out, input_charset, internal_charset);

    return out;
}

static void thbrk_context_dtor(zend_resource* rsrc)
{
    if (rsrc->ptr != NULL) {
        th_brk_delete((ThBrk*)rsrc->ptr);
    }
}

/* {{{ proto string th_brk_new(string arg)
   Return a pointer to th_brk struct */
PHP_FUNCTION(th_brk_new)
{
    char* path = NULL;
    size_t path_len;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &path_len) == FAILURE) {
        return;
    }

    ThBrk* brk = th_brk_new(path[0] == '\0' ? NULL : path);
    if (brk == NULL) {
        // FIXME: Set error state for inquiry
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
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rS", &res, &input) == FAILURE) {
        RETURN_FALSE;
    }
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
        thwchar[i/2] = (ZSTR_VAL(ustr)[i+1] << 8) + ZSTR_VAL(ustr)[i];
    }

    int pos[ustr_len];
    memset(pos, 0, ustr_len * sizeof(int));

    int numcut = th_brk_wc_find_breaks(brk, thwchar, pos, ustr_len);
    array_init(return_value);
    for (int i = 0; i < numcut; i++) {
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
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "rS", &res, &input) == FAILURE) {
        RETURN_FALSE;
    }
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
        thwchar[i/2] = (ZSTR_VAL(ustr)[i+1] << 8) + ZSTR_VAL(ustr)[i];
    }

    int pos[ustr_len];
    memset(pos, 0, ustr_len * sizeof(int));

    int numcut = th_brk_wc_find_breaks(brk, thwchar, pos, ustr_len);
    pos[numcut] = ustr_len; // Add last

    array_init(return_value);
    int prev = 0;
    for (int i = 0; i < numcut + 1; i++) {
        zend_string* out = thwchar_substr(thwchar, prev, pos[i]);
        add_index_stringl(return_value, i, ZSTR_VAL(out), ZSTR_LEN(out));
        zend_string_free(out);
        prev = pos[i];
    }

    zend_string_free(ustr);
    efree(thwchar);
}
/* }}} */

/* {{{ proto void th_brk_delete(thbrk) */
PHP_FUNCTION(th_brk_delete)
{
    zval* res;
    if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &res) == FAILURE) {
        return;
    }

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

