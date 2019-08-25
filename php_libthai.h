#ifndef PHP_LIBTHAI_H
#define PHP_LIBTHAI_H

extern zend_module_entry libthai_module_entry;
#define phpext_libthai_ptr &libthai_module_entry

#define PHP_LIBTHAI_VERSION "0.1.0"

#ifdef PHP_WIN32
#	define PHP_LIBTHAI_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_LIBTHAI_API __attribute__ ((visibility("default")))
#else
#	define PHP_LIBTHAI_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

/* Always refer to the globals in your function as LIBTHAI_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
   */
#define LIBTHAI_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(libthai, v)

#if defined(ZTS) && defined(COMPILE_DL_LIBTHAI)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif	/* PHP_LIBTHAI_H */

