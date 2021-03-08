#ifndef NUMBER_H
#define NUMBER_H

#include "ds.h"
#include <iostream>

struct NumberData {
    arr<u8> digits;
    u16     base;
    bool    negative = false;
    // index of the digit before which the decimal point is
    i32     decimal_point = -1;

    void convert_base(u16 new_base);
    u8 divide(u16 divisor);
    void add(NumberData &other);
    bool is_zero();
};

// NOTE - this changes the base of the number to 256
template <typename T>
bool number_data_to_unsigned(NumberData *num, T *out) {
    num->convert_base(256);

    if (num->digits.size > sizeof(T) || num->negative)
        return false;

    T the_number = 0;
    if (sizeof(T) > 1) {
        for (i32 i = 0; i < num->digits.size; i++) {
            the_number *= 256;
            the_number += num->digits[i];
        }
    }
    the_number |= num->digits.last();

    *out = the_number;
    return true;
}


std::wostream& operator<< (std::wostream& o, NumberData* number);

#endif // guard
