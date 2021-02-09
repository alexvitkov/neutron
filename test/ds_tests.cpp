#ifndef DS_TESTS_CPP
#define DS_TESTS_CPP

#include "../ds.h"
#include "../util.h"
#include <iostream>
using std::wcout;


struct Foo {
    enum {
        Zero,
        Constructed,
        Destroyed,
        Moved
    } state;

    Foo() : state(Constructed) { 
    }

    Foo(const Foo& other) {
        state = other.state;
    }

    ~Foo() { 
        state = Destroyed; 
    }

    Foo(Foo &&other) {
        state = other.state;
        other.state = Moved;
    }
};

bool test_arr_destructs_after_pop() {
    arr<Foo> the_array;
    Foo foo;
    Foo &foo_in_ds = the_array.push(foo);
    the_array.pop();

    return foo_in_ds.state == Foo::Moved;
}

bool test_arr_destructs_after_arr_destroyed() {
    char buffer[1000];

    new (buffer) arr<Foo>();
    arr<Foo> &the_array = *(arr<Foo>*)buffer;

    Foo foo;
    Foo &element = the_array.push(foo);

    the_array.~arr();
    return element.state == Foo::Destroyed;
}

void ok(bool is_ok) {
    if (is_ok) {
        wcout << "OK\n";
    } else {
        wcout << red << "FAILED\n" << resetstyle;
    }
}

void run_ds_tests() {
        wcout << "Test case test_arr_destructs_after_pop... ";
        ok(test_arr_destructs_after_pop());

        wcout << "Test case test_arr_destructs_after_arr_destroyed... ";
        ok(test_arr_destructs_after_arr_destroyed());
}


#endif // guard
