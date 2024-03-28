//
// Created by HAIRONG ZHU on 2024/3/28.
//

#ifndef ESP_TEST_URL_DECODER_H
#define ESP_TEST_URL_DECODER_H
#include <sys/param.h>
// Utility function to convert a single hex digit character to its integer value
unsigned char hexCharToVal(unsigned char c) {
    if ('0' <= c && c <= '9') return (unsigned char)(c - '0');
    if ('a' <= c && c <= 'f') return (unsigned char)(c - 'a' + 10);
    if ('A' <= c && c <= 'F') return (unsigned char)(c - 'A' + 10);
    return 0;
}

// URL decode a string in place
void urlDecode(char *str) {
    char *dest = str;
    while (*str) {
        if (*str == '%') {
            if (*(str + 1) && *(str + 2) && isxdigit(*(str + 1)) && isxdigit(*(str + 2))) {
                *dest = (char)(hexCharToVal(*(str + 1)) * 16 + hexCharToVal(*(str + 2)));
                str += 3;
            } else {
                // 如果百分号后不是两个有效的十六进制数字，则跳过
                str++;
            }
        } else if (*str == '+') {
            *dest = ' ';
            str++;
        } else {
            *dest = *str;
            str++;
        }
        dest++;
    }
    *dest = '\0'; // 确保解码后的字符串以空字符终止
}


#endif //ESP_TEST_URL_DECODER_H
