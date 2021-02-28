#include "default_casts.h"


bool number_literal_to_u64 (CastJob *self) {
    AST_NumberLiteral *nl = (AST_NumberLiteral*)self->source;
    u64 num;
    if (!number_data_to_unsigned(&nl->number_data, &num)) {
        self->set_error_flag();
        return false;
    }
    AST_SmallNumber *sn = self->global.alloc<AST_SmallNumber>(&t_u64);
    sn->u64_val = num;
    *self->result = sn;
    return true;
}


bool CastJob::run(Message *msg) {
    return run_fn(this);
}

std::wstring CastJob::get_name() {
    return L"CastJob";
}
