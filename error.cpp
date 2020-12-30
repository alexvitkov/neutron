#include "error.h"

void print(Context& global, Error& err) {

    switch (err.code) {


        default:
            printf("Fatal error %d\n", err.code);
    } 
}
