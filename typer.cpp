#include "typer.h"

std::vector<Type> defined_types;

// TODO pointers to Type should be permanent
// vector is a really bad fit for this
void init_typer() {
	defined_types.reserve(10000);
	defined_types.push_back({ .kind = PRIMITIVE_UNSIGNED, .size = 1 }); // u8
	defined_types.push_back({ .kind = PRIMITIVE_UNSIGNED, .size = 8 }); // u64
	defined_types.push_back({ .kind = PRIMITIVE_FLOAT, .size = 8 });    // f64
}
