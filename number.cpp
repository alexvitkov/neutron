#include "common.h"
#include "util.h"
#include "number.h"
#include <algorithm>

bool NumberData::is_zero() {
    return (digits.size == 1 && digits[0] == 0) || digits.size == 0;
}

u8 NumberData::divide(u16 divisor) {
    int acc = 0;
    int di_new = 0;

    for (int di = 0; di < digits.size; ) {
        bool f = true;

        acc *= base;
        acc += digits[di++];

        for ( ; acc < divisor && di < digits.size; di ++) {
            acc *= base;
            acc += digits[di];
            if (di_new)
                digits[di_new++] = 0;
        }

        int digit = acc / divisor;
        digits[di_new++] = digit;
        acc %= divisor;
    }

    digits.size = di_new;
    return acc;
}

std::wostream& operator<< (std::wostream& o, NumberData* number) {
    switch (number->base) {
        case 0:  o << "0b"; break;
        case 8:  o << "0o"; break;
        case 16: o << "0x"; break;
        case 10: break;
        default: o << "0#" << number->base << "#"; break;
    }

    if (number->base <= 16) {
        for (u32 i = 0; i < number->digits.size; i++) {
            u8 digit = number->digits[i];
            if (digit < 10)
                o << (char)(digit + '0');
            else
                o << (char)('a' + digit - 10);
        }
    } else {
        for (u32 i = 0; i < number->digits.size; i++) {
            u8 digit = number->digits[i];
            o << digit << "#";
        }
    }
    return o;
}

void NumberData::convert_base(u16 new_base) {
    if (new_base == base)
        return;

    arr<u8> new_digits;

    while (!is_zero()) {
        new_digits.push(divide(new_base));
    }

    digits = std::move(new_digits);
    std::reverse(digits.begin(), digits.end());
    base = new_base;

}
