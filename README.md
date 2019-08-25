# PHP Libthai wrapper - php-libthai

PHP-libthai is a simple wrapper to the [libthai](https://github.com/tlwg/libthai) library.

## Usage

The extension assume the input is in `UTF-8`.

- `th_brk_new($path = null)` - Initialize thbrk instance with optional path to the custom dictionary. Returns `thbrk` resource.
- `th_brk_wc_find_breaks($thbrk, $text)` - Returns array of the break positions.
- `th_brk_wc_split($thbrk, $text)` - Returns array of words.
- `th_brk_delete($thbrk)` - [Optional] close `thbrk` resource.

## Example

``` php
<?php
$text = 'ไก่จิกเด็กตายบนปากโอ่ง';
$brk = th_brk_new();
print_r(th_brk_wc_find_breaks($brk, $text));
print_r(th_brk_wc_split($brk, $text));
```

