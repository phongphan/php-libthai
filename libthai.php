<?php
$c = th_brk_new(null);
try {
    $text = 'ไก่จิกเด็กneoไทย💟💟💟บน💟💟ปากโอ่่ง';
    //$text = '💟';
    echo "$text\n";
    $pos = th_brk_wc_find_breaks($c, $text);
    $words = th_brk_wc_split($c, $text);
    print_r($pos);
    print_r($words);
} finally {
    th_brk_delete($c);
}

