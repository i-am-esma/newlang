#include "contrib/logger/logger.h"
#include "pch.h"

#include <core/context.h>
#include <core/newlang.h>
#include <core/term.h>
#include <core/types.h>
#include <filesystem>

using namespace newlang;

/*
 Идентификатором объектов является его имя, причем у сессионных и глобальных
объектов оно присваивается автоматически даже для переменных литералов. Имена
локальных объектов перекрывают сессионные и глобальные, а сессионный объкт
перекрывает глобальный. Дубли локальных объектов не допускаются, т.к. имена
локальных переменных должны быть уникальные на уровне языка реализации. Дубли
сессионных объектов для текущей сессии не допускаются, но можно перекрывать
сессионные объекты более высокого уровня. Дубли глобальных объектов допускаются
без ограничений, а при создании дубля выводится предупреждение.

При наличии переменных с одним именем, но разным временем жизни, приоритет
доступпа будет следующим. обращение по имени объекта - доступ только к
локальному объекту (разрешение имени происходит во время компиляции) или будет
ошибка если объект отсутствует. обращение как к сессионному обекту (префикс $) -
если есть локальный объект, вернется локальный, иначе вернется сессионный или
будет ошибка если объект отсутствует. обращение как к глобальному обекту
(префикс @) - если есть локальный, вернется локальный, иначе вернется
сессионный, иначе вернется глобальный или будет ошибка если объект отсутствует.

Можно всегда указывать глобальный идентификатор объекта, то доступ будет в
зависимости от наличия локальных или сессионных переменных/объектов.

$.obj прямое обращения к сессионному объекту при наличии локального
@.obj прямое обращения к глобальному объекту при наличии локального или
сессионного

 */

std::map<std::string, Context::EvalFunction> Context::m_ops;
std::map<std::string, Context::EvalFunction> Context::m_builtin_calls;
std::map<std::string, ObjPtr> Context::m_types;
std::map<std::string, Context::FuncItem> Context::m_funcs;

Context::Context(RuntimePtr global) {
    m_runtime = global;

    if(Context::m_funcs.empty()) {
        Context::m_funcs["min"] = CreateBuiltin("min(arg, ...)", (void *) &min, ObjType::TRANSPARENT);
        Context::m_funcs["мин"] = Context::m_funcs["min"];

        Context::m_funcs["max"] = CreateBuiltin("max(arg, ...)", (void *) &max, ObjType::TRANSPARENT);
        Context::m_funcs["макс"] = Context::m_funcs["max"];

        Context::m_funcs["import"] =
                CreateBuiltin("import(arg, module='', lazzy=0)", (void *) &import, ObjType::FUNCTION);

        Context::m_funcs["eval"] = CreateBuiltin("eval(string:String)", (void *) &eval, ObjType::FUNCTION);
        Context::m_funcs["exec"] = CreateBuiltin("exec(filename:String)", (void *) &exec, ObjType::FUNCTION);


#define REGISTER_TYPES(name) \
    ASSERT(Context::m_funcs.find(#name) == Context::m_funcs.end()); \
    Context::m_funcs[#name] = CreateBuiltin(#name "(...): " #name, (void *)& newlang:: name, ObjType::TRANSPARENT);
        //    Context::m_funcs[#name "_"] = CreateBuiltin(#name "_(...): " #name, (void *)& newlang:: name##_, ObjType::FUNCTION);
        NL_BUILTIN_CAST_TYPE(REGISTER_TYPES)
#undef REGISTER_TYPES

    }

    if(Context::m_types.empty()) {
#define REGISTER_TYPES(name)                                                                                     \
    ASSERT(Context::m_types.find(#name) == Context::m_types.end());                                                    \
    Context::m_types[#name] = CreateSimpleType(ObjType::name);

        NL_BUILTIN_CAST_TYPE(REGISTER_TYPES)

#undef REGISTER_TYPES

        VERIFY(CreateTypeName(":Format", ":String"));

        //        VERIFY(CreateTypeName(":Class", ":Dictionary"));
        VERIFY(CreateTypeName(":Enum", Object::CreateFunc("Enum(...)", &CreateEnum, ObjType::TRANSPARENT)));
        VERIFY(CreateTypeName(":Struct", ":Class"));

        VERIFY(CreateTypeName(":Interruption", ":Class"));

        VERIFY(CreateTypeName(":Break", ":Interruption"));
        VERIFY(CreateTypeName(":Error", ":Interruption"));

        VERIFY(CreateTypeName(":Parser", ":Error"));
        VERIFY(CreateTypeName(":RunTime", ":Error"));
        VERIFY(CreateTypeName(":Signal", ":Error"));

        VERIFY(CreateTypeName(":SIGABRT", ":Signal"));
        VERIFY(CreateTypeName(":SIGCHLD", ":Signal"));
        VERIFY(CreateTypeName(":SIGINT", ":Signal"));
        VERIFY(CreateTypeName(":SIGQUIT", ":Signal"));
        VERIFY(CreateTypeName(":SIGUSR1", ":Signal"));
        VERIFY(CreateTypeName(":SIGUSR2", ":Signal"));
        VERIFY(CreateTypeName(":SIGTSTP", ":Signal"));
    }

    if(Context::m_builtin_calls.empty()) {
#define REGISTER_FUNC(name, func)                                                                                      \
    ASSERT(Context::m_builtin_calls.find(name) == Context::m_builtin_calls.end());                                     \
    Context::m_builtin_calls[name] = &Context::func_##func;

        NL_BUILTIN(REGISTER_FUNC);

#undef REGISTER_FUNC
    }

    if(Context::m_ops.empty()) {
#define REGISTER_OP(op, func)                                                                                          \
    ASSERT(Context::m_ops.find(op) == Context::m_ops.end());                                                           \
    Context::m_ops[op] = &Context::op_##func;

        NL_OPS(REGISTER_OP);

#undef REGISTER_OP
    }
}

//    std::string func_dump(prototype);
//    func_dump += " := {}";
//
//    TermPtr proto;
//    Parser parser(proto);
//    parser.Parse(func_dump);
//
//    ObjPtr obj = Object::CreateFunc(ctx, proto->Left(), type,
//    proto->Left()->getName().empty() ? proto->Left()->getText() :
//    proto->Left()->getName()); obj->m_function = func;
//
//    return RegisterFunc(obj, true);

ObjPtr Context::CreateBuiltin(const char *prototype, void *func, ObjType type) {
    ASSERT(prototype);
    ASSERT(func);

    std::string func_dump(prototype);
    func_dump += " := {};";

    TermPtr proto = Parser::ParseString(func_dump);
    ObjPtr obj =
            Object::CreateFunc(this, proto->Left(), type,
            proto->Left()->getName().empty() ? proto->Left()->getText() : proto->Left()->getName());
    obj->m_func_ptr = func;

    return obj;
}

inline ObjType newlang::typeFromString(const std::string type, Context *ctx, bool *has_error) {

    if(ctx) {
        return ctx->BaseTypeFromString(type, has_error);
    }

    std::string search;
    if(isType(type)) {
        search = type.substr(1);
    } else {
        search = type;
    }

#define DEFINE_CASE(name)                                                                                        \
    else if (search.compare(#name) == 0) {                                                                             \
        return ObjType:: name;                                                                                          \
    }

    if(type.compare("") == 0) {
        return ObjType::None;
    } else if(type.compare("_") == 0) {
        return ObjType::None;
    }
    NL_BUILTIN_CAST_TYPE(DEFINE_CASE)

#undef DEFINE_CASE

    if(has_error) {
        *has_error = true;

        return ObjType::None;
    }
    LOG_RUNTIME("Undefined type name '%s'!", type.c_str());
}

ObjPtr Context::RegisterObject(ObjPtr var) {
    if(!var || var->getName().empty()) {
        LOG_RUNTIME("Empty object name %s", var ? var->toString().c_str() : "");
    }
    ASSERT(var->m_namespace.empty());
    //    std::string full_name = var->m_namespace;
    //    if(!full_name.empty()) {
    //        full_name += "::";
    //    }
    //    full_name += var->getName();

    if(isGlobal(var->getName())) {
        var->getName() = var->getName().substr(1);
        m_global_terms.push_back(var, var->getName());
    }
    if(isLocal(var->getName())) {
        var->getName() = var->getName().substr(1);
    }
    push_back(var, var->getName());

    //    if(!var->is_function()) {
    //    full_name += var->getName();
    //        LOG_RUNTIME("Object '%s' not function!", var->toString().c_str());
    //    }
    //    push_back(var, full_name);

    return var;
}

void newlang::NewLangSignalHandler(int signal) {

    throw abort_exception("Signal SIGABRT received");
}

//#include "StdCapture.h"

ObjPtr Context::Eval(Context *ctx, TermPtr term, Object *args) {

    //    StdCapture Capture;
    //
    //    Capture.BeginCapture();

    auto previous_handler = signal(SIGABRT, &NewLangSignalHandler);
    try {

        switch(term->m_id) {
            case TermID::END:
                return eval_END(ctx, term, args);

#define DEFINE_CASE(name)                                                                                              \
    case TermID::name:                                                                                                 \
        return eval_##name(ctx, term, args);

                NL_TERMS(DEFINE_CASE)

#undef DEFINE_CASE

            default:
                return eval_UNKNOWN(ctx, term, args);
        }
    } catch (abort_exception &err) {

        signal(SIGABRT, previous_handler);
        //        Capture.EndCapture();
        NL_EVAL(term, "Exception eval '%s' (%s)", term->toString().c_str(), err.what());
    }
}

ObjPtr Context::eval_END(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->m_text.compare("") == 0);
    LOG_RUNTIME("eval_END: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_UNKNOWN(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->m_text.compare("") == 0);
    LOG_RUNTIME("eval_UNKNOWN: %s", term->toString().c_str());

    return nullptr;
}

/*
 *
 *
 */

ObjPtr Context::eval_BLOCK(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->getTermID() == TermID::BLOCK);
    ObjPtr obj = Object::CreateType(ObjType::BLOCK);
    obj->m_block_source = term;
    obj->m_var_is_init = true;

    return obj;
}

ObjPtr Context::eval_BLOCK_TRY(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->getTermID() == TermID::BLOCK_TRY);
    ObjPtr obj = Object::CreateType(ObjType::BLOCK_TRY);
    obj->m_block_source = term;
    obj->m_var_is_init = true;

    return obj;
}

ObjPtr Context::eval_CALL_BLOCK(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->getTermID() == TermID::CALL_BLOCK);

    return CallBlock(ctx, term, args);
}

ObjPtr Context::eval_CALL_TRY(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->getTermID() == TermID::CALL_TRY);

    return CallBlockTry(ctx, term, args);
}

ObjPtr Context::eval_ITERATOR_QQ(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("eval_ITERATOR_QQ: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_MACRO(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("eval_MACRO: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_MACRO_BODY(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("eval_MACRO_BODY: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_PARENT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("eval_PARENT: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_NEWLANG(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("eval_NEWLANG: %s", term->toString().c_str());

    return nullptr;
}

ObjPtr Context::eval_TYPE(Context *ctx, const TermPtr &term, Object *local_vars) {
    return CreateRVal(ctx, term, local_vars);
}

ObjPtr Context::eval_TYPE_CALL(Context *ctx, const TermPtr &term, Object *local_vars) {
    return CreateRVal(ctx, term, local_vars);
}

ObjPtr Context::eval_TYPENAME(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPENAME Not implemented!");

    return nullptr;
}

inline ObjPtr Context::eval_INTEGER(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_NUMBER(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_COMPLEX(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_CURRENCY(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_FRACTION(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_STRWIDE(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_STRCHAR(Context *ctx, const TermPtr &term, Object *args) {
    return CreateRVal(ctx, term, args);
}

// inline ObjPtr Context::eval_STRVAR(Context *ctx, const TermPtr &term, Object
// &args) {
//     return CreateRVal(ctx, term, args);
// }

inline ObjPtr Context::eval_TENSOR(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

inline ObjPtr Context::eval_NONE(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->m_id == TermID::NONE);

    return Object::CreateNone();
}

inline ObjPtr Context::eval_EMPTY(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("EMPTY Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_TERM(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->m_id == TermID::TERM);

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_CALL(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && term->m_id == TermID::CALL);
    return CreateRVal(ctx, term, args);
}

ObjPtr Context::func_NOT_SUPPORT(Context *ctx, const TermPtr &term, Object *args) {
    NL_PARSER(term, "Function or operator '%s' is not supported in interpreter mode!", term->m_text.c_str());

    return nullptr;
}

ObjPtr Context::eval_TEMPLATE(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TEMPLATE Not implemented!");
    return nullptr;
}

ObjPtr Context::eval_COMMENT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("COMMENT Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_SYMBOL(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("SYMBOL Not implemented!");
    return nullptr;
}

ObjPtr Context::eval_NAMESPACE(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("NAMESPACE Not implemented!");
    return nullptr;
}

/*
 *
 *
 */

ObjPtr Context::ExpandAssign(Context *ctx, TermPtr lvar, TermPtr rval, Object *args, CreateMode mode) {
    LOG_RUNTIME("ExpandAssign Not implemented!");

    return nullptr;
}

ObjPtr Context::ExpandCreate(Context *ctx, TermPtr lvar, TermPtr rval, Object *args) {
    LOG_RUNTIME("ExpandCreate Not implemented!");
    return nullptr;
}

ObjPtr Context::CREATE_OR_ASSIGN(Context *ctx, const TermPtr &term, Object *local_vars, CreateMode mode) {
    // Присвоить значение можно как одному термину, так и сразу нескольким при
    // раскрытии словаря: var1, var2, _ = ... func(); // Первый и второй элементы
    // словаря записывается в var1 и var2, а остальные элементы возвращаемого
    // словаря игнорируются (если они есть) var1, var2 = ... func(); // Если
    // функция вернула словрь с двумя элементами, то их значения записываются в
    // var1 и var2.
    //   Если в словаре было больше двух элементов, то первый записывается в var1,
    //   а оставшиеся в var2. !!!!!!!!!!!!!
    // var1, ..., var2 = ... func(); // Первый элемент словаря записывается в
    // var1, а последний в var2.

    ASSERT(term && (term->getTermID() == TermID::ASSIGN || term->getTermID() == TermID::CREATE ||
            term->getTermID() == TermID::CREATE_OR_ASSIGN));
    ASSERT(term->Left());

    auto found = ctx->select(term->Left()->m_text);
    if(!term->Right() && mode == CreateMode::ASSIGN_ONLY) {
        if(!found.complete()) {
            ctx->erase(found);
            return Object::Yes();
        }
        return Object::No();
    }

    if(term->Left()->Right() || term->Right()->getTermID() == TermID::ELLIPSIS) {
        return ExpandAssign(ctx, term->Left(), term->Right(), local_vars, mode);
    }
    ObjPtr rval = Eval(ctx, term->Right(), local_vars);
    if(!rval) {
        NL_PARSER(term->Right(), "Object is missing or expression is not evaluated!");
    }

    if(isType(term->Left()->m_text)) {
        return ctx->CreateTypeName(term->Left(), rval);
    }

    ObjPtr lval = nullptr;
    if(found.complete()) {
        if(mode == CreateMode::ASSIGN_ONLY) {
            NL_PARSER(term->Left(), "Object '%s' not found!", term->Left()->m_text.c_str());
        }
    } else {
        lval = found.data().second.lock();
    }

    if(lval && mode == CreateMode::CREATE_ONLY) {
        NL_PARSER(term->Left(), "Object '%s' already exist!", term->Left()->m_text.c_str());
    }

    if(!lval) {
        lval = CreateLVal(ctx, term->Left(), local_vars);
        if(!lval) {
            NL_PARSER(term->Left(), "Fail create lvalue object!");
        }
        ctx->RegisterObject(lval);
    }

    if(lval->m_var_type_current == ObjType::FUNCTION && (rval->m_var_type_current == ObjType::BLOCK || rval->m_var_type_current == ObjType::BLOCK_TRY)) {
        lval->m_var_type_current = ObjType::EVAL_FUNCTION;
    }

    lval->SetValue_(rval);
    return lval;
}

ObjPtr Context::eval_ASSIGN(Context *ctx, const TermPtr &term, Object *local_vars) {
    return CREATE_OR_ASSIGN(ctx, term, local_vars, CreateMode::ASSIGN_ONLY);
}

ObjPtr Context::eval_CREATE(Context *ctx, const TermPtr &term, Object *local_vars) {
    return CREATE_OR_ASSIGN(ctx, term, local_vars, CreateMode::CREATE_ONLY);
}

ObjPtr Context::eval_CREATE_OR_ASSIGN(Context *ctx, const TermPtr &term, Object *args) {
    return CREATE_OR_ASSIGN(ctx, term, args, CreateMode::CREATE_OR_ASSIGN);
}

ObjPtr Context::eval_APPEND(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("APPEND Not implemented!");
    return nullptr;
}

ObjPtr Context::eval_FUNCTION(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term && (term->getTermID() == TermID::FUNCTION || term->getTermID() == TermID::TRANSPARENT ||
            term->getTermID() == TermID::SIMPLE_AND || term->getTermID() == TermID::SIMPLE_OR ||
            term->getTermID() == TermID::SIMPLE_XOR));
    ASSERT(term->Left());
    ASSERT(ctx);

    auto found = ctx->select(term->Left()->m_text);
    if(!term->Right()) {
        if(!found.complete()) {
            ctx->erase(found);
            return Object::Yes();
        }
        return Object::No();
    }

    ObjPtr lval = CreateLVal(ctx, term->Left(), args);
    if(!lval) {
        NL_PARSER(term->Left(), "Fail create lvalue object!");
    }

    if(term->Right()->getTermID() == TermID::CALL) {
        lval->SetValue_(CreateRVal(ctx, term->Right()));
    } else {
        if(term->getTermID() == TermID::FUNCTION) {
            lval->m_var_type_current = ObjType::EVAL_FUNCTION;
        } else if(term->getTermID() == TermID::TRANSPARENT) {
            lval->m_var_type_current = ObjType::EVAL_TRANSP;
        } else if(term->getTermID() == TermID::SIMPLE_AND) {
            lval->m_var_type_current = ObjType::EVAL_AND;
        } else if(term->getTermID() == TermID::SIMPLE_OR) {
            lval->m_var_type_current = ObjType::EVAL_OR;
        } else if(term->getTermID() == TermID::SIMPLE_XOR) {
            lval->m_var_type_current = ObjType::EVAL_XOR;
        } else {

            LOG_RUNTIME("Create function '%s' not implemented!", term->toString().c_str());
        }
        lval->m_var_type_fixed = lval->m_var_type_current;
        lval->m_var_is_init = true;
        lval->m_block_source = term->Right();
    }

    return ctx->RegisterObject(lval);
}

ObjPtr Context::eval_SIMPLE_AND(Context *ctx, const TermPtr &term, Object *args) {

    return eval_FUNCTION(ctx, term, args);
}

ObjPtr Context::eval_SIMPLE_OR(Context *ctx, const TermPtr &term, Object *args) {

    return eval_FUNCTION(ctx, term, args);
}

ObjPtr Context::eval_SIMPLE_XOR(Context *ctx, const TermPtr &term, Object *args) {

    return eval_FUNCTION(ctx, term, args);
}

ObjPtr Context::eval_TRANSPARENT(Context *ctx, const TermPtr &term, Object *args) {

    return eval_FUNCTION(ctx, term, args);
}

/*
 *
 *
 */
ObjPtr Context::eval_ITERATOR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("ITERATOR Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_FOLLOW(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("FOLLOW Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_MATCHING(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("MATCHING Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_RANGE(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_ELLIPSIS(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("ELLIPSIS Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_ARGUMENT(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_ARGS(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_EXIT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("EXIT Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_ERROR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("ERROR Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_WHILE(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("EXCEPTION Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_UNTIL(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("EXCEPTION Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_INDEX(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("INDEX Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_FIELD(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("FIELD Not implemented!");

    return nullptr;
}

ObjPtr Context::eval_CLASS(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_DICT(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_SOURCE(Context *ctx, const TermPtr &term, Object *args) {
    NL_PARSER(term, "Inclusion on the implementation language is not supported "
            "in interpreter mode!");

    return nullptr;
}

ObjPtr Context::eval_TENSOR_BEGIN(Context *ctx, const TermPtr &term, Object *args) {

    return CreateRVal(ctx, term, args);
}

ObjPtr Context::eval_TENSOR_END(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(false);

    return nullptr;
}

/*
 *
 *
 */

ObjPtr Context::eval_OPERATOR(Context *ctx, const TermPtr &term, Object *args) {
    if(Context::m_ops.find(term->m_text) == Context::m_ops.end()) {

        LOG_RUNTIME("Eval op '%s' not exist!", term->m_text.c_str());
    }
    return (*Context::m_ops[term->m_text])(ctx, term, args);
}

/*
 *
 *
 */

ObjPtr Context::op_EQUAL(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_equal(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_ACCURATE(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_accurate(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_NE(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator!=(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_LT(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator<(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_GT(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator>(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_LE(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator<=(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

ObjPtr Context::op_GE(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator>=(Eval(ctx, term->Right(), args)) ? Object::Yes() : Object::No();
}

/*
 *
 *
 */

ObjPtr Context::op_AND(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_bit_and(Eval(ctx, term->Right(), args), false);
}

ObjPtr Context::op_OR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("OR Not implemented!");

    return nullptr;
}

ObjPtr Context::op_XOR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("XOR Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_AND(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_AND Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_OR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_OR Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_XOR(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_XOR Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_AND_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_AND_ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_OR_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_OR_ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_BIT_XOR_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("BIT_XOR_ Not implemented!");

    return nullptr;
}

/*
 *
 *
 */
ObjPtr Context::op_PLUS(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Right());
    if(term->Left()) {

        return Eval(ctx, term->Left(), args)->operator+(Eval(ctx, term->Right(), args));
    }
    return Eval(ctx, term->Left(), args)->operator+();
}

ObjPtr Context::op_MINUS(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Right());
    if(term->Left()) {

        return Eval(ctx, term->Left(), args)->operator-(Eval(ctx, term->Right(), args));
    }
    return Eval(ctx, term->Left(), args)->operator-();
}

ObjPtr Context::op_DIV(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator/(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_DIV_CEIL(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_div_ceil(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_MUL(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator*(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_REM(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator%(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_POW(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_pow(Eval(ctx, term->Right(), args));
}

/*
 *
 *
 */
ObjPtr Context::op_PLUS_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator+=(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_MINUS_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator-=(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_DIV_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator/=(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_DIV_CEIL_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_div_ceil_(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_MUL_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator*=(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_REM_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->operator%=(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_POW_(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_pow_(Eval(ctx, term->Right(), args));
}

ObjPtr Context::op_CONCAT(Context *ctx, const TermPtr &term, Object *args) {
    ASSERT(term);
    ASSERT(term->Left());
    ASSERT(term->Right());

    return Eval(ctx, term->Left(), args)->op_concat_(Eval(ctx, term->Right(), args), ConcatMode::Append);
}

ObjPtr Context::op_TYPE_EQ(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_EQ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_TYPE_EQ2(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_EQ2 Not implemented!");

    return nullptr;
}

ObjPtr Context::op_TYPE_EQ3(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_EQ3 Not implemented!");

    return nullptr;
}

ObjPtr Context::op_TYPE_NE(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_NE Not implemented!");

    return nullptr;
}

ObjPtr Context::op_TYPE_NE2(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_NE2 Not implemented!");

    return nullptr;
}

ObjPtr Context::op_TYPE_NE3(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("TYPE_NE3 Not implemented!");

    return nullptr;
}

/*
 *
 */
ObjPtr Context::op_RSHIFT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("RSHIFT Not implemented!");

    return nullptr;
}

ObjPtr Context::op_LSHIFT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("LSHIFT Not implemented!");

    return nullptr;
}

ObjPtr Context::op_RSHIFT_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("RSHIFT_ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_LSHIFT_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("LSHIFT_ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_RRSHIFT(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("RRSHIFT Not implemented!");

    return nullptr;
}

ObjPtr Context::op_RRSHIFT_(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("RRSHIFT_ Not implemented!");

    return nullptr;
}

ObjPtr Context::op_SPACESHIP(Context *ctx, const TermPtr &term, Object *args) {
    LOG_RUNTIME("SPACESHIP Not implemented!");

    return nullptr;
}

/*
 *
 *
 */

ObjPtr Context::CallBlock(Context *ctx, const TermPtr &block, Object *local_vars) {
    ObjPtr result = Object::CreateNone();
    ASSERT(block);
    if(!block->m_block.empty()) {
        for (size_t i = 0; i < block->m_block.size(); i++) {
            result = Eval(ctx, block->m_block[i], local_vars);
        }
    } else {
        result = Eval(ctx, block, local_vars);
    }
    return result;
}

ObjPtr Context::CallBlockTry(Context *ctx, const TermPtr &block, Object *local_vars) {
    ObjPtr result = Object::CreateNone();
    try {
        result = CallBlock(ctx, block, local_vars);

        // newlang_exception;
        //
        // parser_exception;
        //
        // abort_exception;
    } catch (std::exception &error) {

        result = Object::CreateType(ObjType::Error, nullptr, ObjType::Error);
        result->m_str = error.what();
    }
    return result;
}

ObjPtr Context::EvalBlockAND(Context *ctx, const TermPtr &block, Object *local_vars) {
    ObjPtr result = nullptr;
    if(block->GetTokenID() == TermID::BLOCK) {
        for (size_t i = 0; i < block->m_block.size(); i++) {
            result = Eval(ctx, block->m_block[i], local_vars);
            if(!result || !result->GetValueAsBoolean()) {
                return Object::No();
            }
        }
    } else {
        result = Eval(ctx, block, local_vars);
    }
    if(!result || !result->GetValueAsBoolean()) {

        return Object::No();
    }
    return Object::Yes();
}

ObjPtr Context::EvalBlockOR(Context *ctx, const TermPtr &block, Object *local_vars) {
    ObjPtr result = nullptr;
    if(block->GetTokenID() == TermID::BLOCK) {
        for (size_t i = 0; i < block->m_block.size(); i++) {
            result = Eval(ctx, block->m_block[i], local_vars);
            if(result && result->GetValueAsBoolean()) {
                return Object::Yes();
            }
        }
    } else {
        result = Eval(ctx, block, local_vars);
    }
    if(result && result->GetValueAsBoolean()) {

        return Object::Yes();
    }
    return Object::No();
}

ObjPtr Context::EvalBlockXOR(Context *ctx, const TermPtr &block, Object *local_vars) {
    ObjPtr result;
    size_t xor_counter = 0;
    if(block->GetTokenID() == TermID::BLOCK) {
        for (size_t i = 0; i < block->m_block.size(); i++) {
            result = Eval(ctx, block->m_block[i], local_vars);
            if(result && result->GetValueAsBoolean()) {
                xor_counter++;
            }
        }
    } else {
        result = Eval(ctx, block, local_vars);
        if(result && result->GetValueAsBoolean()) {

            xor_counter++;
        }
    }
    // Результат равен 0, если нет операндов, равных 1, либо их чётное количество.
    return (xor_counter & 1) ? Object::Yes() : Object::No();
}

ObjPtr Context::CreateNative(const char *proto, const char *module, bool lazzy, const char *mangle_name, ffi_abi abi) {
    TermPtr term;
    try {
        // Термин или термин + тип парсятся без ошибок
        term = Parser::ParseString(proto);
    } catch (std::exception &e) {
        try {
            std::string func(proto);
            func += ":={}";
            term = Parser::ParseString(func)->Left();
        } catch (std::exception &e) {

            LOG_RUNTIME("Fail parsing prototype '%s'!", e.what());
        }
    }
    return CreateNative(term, module, lazzy, mangle_name, abi);
}

ObjPtr Context::CreateNative(TermPtr proto, const char *module, bool lazzy, const char *mangle_name, ffi_abi abi) {

    NL_CHECK(proto, "Fail prototype native function!");
    NL_CHECK((module == nullptr || (module && *module == '\0')) || m_runtime,
            "You cannot load a module '%s' without access to the runtime context!", module);

    ObjPtr result;
    ObjType type;
    if(proto->GetTokenID() == TermID::TERM) {
        if(proto->m_type_name.empty()) {
            LOG_RUNTIME("Cannot create native variable without specifying the type!");
        }

        type = typeFromString(proto->m_type_name, this);
        switch(type) {
            case ObjType::Bool:
                //            case ObjType::Byte:
            case ObjType::Char:
            case ObjType::Short:
            case ObjType::Int:
            case ObjType::Long:
            case ObjType::Float:
            case ObjType::Double:
            case ObjType::Pointer:
                break;
            default:
                LOG_RUNTIME("Creating a variable with type '%s' is not supported!", proto->m_type_name.c_str());
        }
    } else if(proto->GetTokenID() == TermID::CALL) {
        type = ObjType::NativeFunc;
    }

    result = Object::CreateType(type);
    result->m_var_type_fixed = type; // Тип определен и не может измениться в дальнейшем

    *const_cast<TermPtr *> (&result->m_func_proto) = proto;
    result->m_func_abi = abi;

    if(mangle_name) {
        result->m_func_mangle_name = mangle_name;
    }
    if(module) {
        result->m_module_name = module;
    }
    if(lazzy) {
        result->m_func_ptr = nullptr;
    } else {
        result->m_func_ptr = m_runtime->GetProcAddress(
                result->m_func_mangle_name.empty() ? proto->m_text.c_str() : result->m_func_mangle_name.c_str(), module);
        if(result->is_function() || type == ObjType::Pointer) {
            NL_CHECK(result->m_func_ptr, "Error getting address '%s' from '%s'!", proto->toString().c_str(), module);
        } else if(result->m_func_ptr && result->is_tensor()) {
            result->m_value = torch::from_blob(result->m_func_ptr, {
            }, toTorchType(type));
        } else {

            LOG_RUNTIME("Fail CreateNative for object %s", result->toString().c_str());
        }
    }
    return result;
}

void *RunTime::GetProcAddress(const char *name, const char *module) {
    if(module && module[0]) {
        if(m_modules.find(module) == m_modules.end()) {
            LoadModule(module, false, nullptr);
        }
        if(m_modules.find(module) == m_modules.end()) {
            LOG_WARNING("Fail load module '%s'!", module);

            return nullptr;
        }
        return dlsym(m_modules[module]->GetHandle(), name);
    }
    return dlsym(nullptr, name);
}

ObjPtr Context::FindSessionTerm(const char *name, bool current_only) {
    auto found = select(MakeName(name));
    while(!found.complete()) {
        ObjPtr obj = found.data().second.lock();
        if(obj) {

            return obj;
        }
        erase(found);
        found++;
    }
    return nullptr;
}

/*
 * обращение по имени - доступ только к локальному объекту (разрешение имени во
 * время компиляции). обращение как к сессионному обекту - если есть локальный,
 * будет локальный (разрешение имени во время компиляции), иначе вернется
 * сессионный или будет ошибка если объект отсутствует. обращение как к
 * глобальному обекту - если есть локальный, будет локальный (разрешение имени
 * во время компиляции), иначе вернется сессионный, иначе вернется глобальный
 * или будет ошибка если объект отсутствует. Можно всегда общащсять как к
 * глобальному объекту, а доступ будет в зависимости от наличия локальных или
 * сессионных переменных/объектов.
 */
ObjPtr Context::FindTerm(const char *name) {
    ObjPtr result = FindSessionTerm(name);
    if(!result && isType(name)) {
        return GetTypeFromString(name);
    }

    if(!result) {
        return GetObject(name);
    }

    if(result || isLocalAny(name) || isLocal(name)) {

        return result;
    }
    return FindGlobalTerm(name);
}

ObjPtr Context::GetTerm(const char *name, bool is_ref) {

    return FindTerm(name);
}

std::string newlang::GetFileExt(const char *str) {
    std::string filename(str);
    std::string::size_type idx = filename.rfind('.');
    if(idx != std::string::npos) {

        return filename.substr(idx);
    }
    return std::string("");
}

std::string newlang::AddDefaultFileExt(const char *str, const char *ext_default) {
    std::string filename(str);
    std::string file_ext = GetFileExt(str);
    if(file_ext.empty() && !filename.empty() && filename.compare(".") != 0) {

        filename.append(ext_default);
    }
    return filename;
}

std::string newlang::ReplaceFileExt(const char *str, const char *ext_old, const char *ext_new) {
    std::string filename(str);
    std::string file_ext = GetFileExt(str);
    if(file_ext.compare(ext_old) == 0) {
        filename = filename.substr(0, filename.length() - file_ext.length());
    }
    file_ext = GetFileExt(filename.c_str());
    if(file_ext.compare(".") != 0 && file_ext.compare(ext_new) != 0 && !filename.empty() &&
            filename.compare(".") != 0) {

        filename.append(ext_new);
    }
    return filename;
}

std::string newlang::ReadFile(const char *fileName) {
    std::ifstream f(fileName);
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();

    return ss.str();
}

ObjPtr Context::CreateLVal(Context *ctx, TermPtr term, Object *args) {

    ASSERT(ctx);
    ASSERT(term);
    ASSERT(!term->m_text.empty());

    if(!ctx->select(term->m_text).complete()) {
        // Объект должен отсутствовать
        NL_PARSER(term, "Object '%s' already exists!", term->m_text.c_str());
    }

    ObjPtr result = Object::CreateNone();
    //    result->m_ctx = ctx;
    result->m_var_name = term->m_text;

    *const_cast<TermPtr *> (&result->m_func_proto) = term;

    TermPtr type = term->GetType();
    if(term->IsFunction() || term->getTermID() == TermID::CALL) {

        result->m_var_type_current = ObjType::FUNCTION;
        result->m_var_type_fixed = result->m_var_type_current;
        *const_cast<TermPtr *> (&result->m_func_proto) = term;
        result->m_var_is_init = false;
    } else if(type) {
        result->m_var_type_current = typeFromString(type->getText().c_str(), ctx);
        result->m_var_type_fixed = result->m_var_type_current;
        if(result->is_tensor()) {
            std::vector<int64_t> dims;
            if(type->m_dims.size()) {
                for (size_t i = 0; i < type->m_dims.size(); i++) {
                    NL_CHECK(type->m_dims[i]->getName().empty(), "Dimension named not supported!");
                    ObjPtr temp = CreateRVal(ctx, type->m_dims[i]);
                    if(!temp) {
                        NL_PARSER(type, "Term not found!");
                    }
                    if(!temp->is_integer()) {

                        NL_PARSER(type, "Term type not integer!");
                    }
                    dims.push_back(temp->GetValueAsInteger());
                }
            }
            result->m_value = torch::empty(dims, toTorchType(result->m_var_type_current));
            result->m_var_is_init = false;
        }
    }

    return result;
}

ObjPtr Context::CreateRVal(Context *ctx, const char *source, Object *local_vars) {
    TermPtr ast;
    Parser parser(ast);
    parser.Parse(source);

    return CreateRVal(ctx, ast, local_vars);
}

void Context::ItemTensorEval_(torch::Tensor &tensor, c10::IntArrayRef shape, std::vector<Index> &ind, const int64_t pos,
        ObjPtr &obj, ObjPtr &args) {
    ASSERT(pos < ind.size());
    if(pos + 1 < ind.size()) {
        for (ind[pos] = 0; ind[pos].integer() < shape[pos]; ind[pos] = ind[pos].integer() + 1) {
            ItemTensorEval_(tensor, shape, ind, pos + 1, obj, args);
        }
    } else {

        at::Scalar value;
        ObjType type = fromTorchType(tensor.scalar_type());

        for (ind[pos] = 0; ind[pos].integer() < shape[pos]; ind[pos] = ind[pos].integer() + 1) {

            switch(type) {
                case ObjType::Char:
                case ObjType::Short:
                case ObjType::Int:
                case ObjType::Long:
                    value = at::Scalar(obj->Call(this)->GetValueAsInteger()); // args
                    tensor.index_put_(ind, value);
                    break;
                case ObjType::Float:
                case ObjType::Double:
                    value = at::Scalar(obj->Call(this)->GetValueAsNumber()); // args
                    tensor.index_put_(ind, value);

                    break;
                default:
                    ASSERT(!"Not implemented!");
            }
        }
    }
}

void Context::ItemTensorEval(torch::Tensor &self, ObjPtr obj, ObjPtr args) {
    if(self.dim() == 0) {

        signed char *ptr_char = nullptr;
        short *ptr_short = nullptr;
        int *ptr_int = nullptr;
        long *ptr_long = nullptr;
        float *ptr_float = nullptr;
        double *ptr_double = nullptr;

        switch(fromTorchType(self.scalar_type())) {
            case ObjType::Char:
                ptr_char = self.data_ptr<signed char>();
                ASSERT(ptr_char);
                *ptr_char = obj->Call(this)->GetValueAsInteger(); //, args)->GetValueAsInteger();
                return;
            case ObjType::Short:
                ptr_short = self.data_ptr<short>();
                ASSERT(ptr_short);
                *ptr_short = obj->Call(this)->GetValueAsInteger(); // args
                return;
            case ObjType::Int:
                ptr_int = self.data_ptr<int>();
                ASSERT(ptr_int);
                *ptr_int = obj->Call(this)->GetValueAsInteger(); // args
                return;
            case ObjType::Long:
                ptr_long = self.data_ptr<long>();
                ASSERT(ptr_long);
                *ptr_long = obj->Call(this)->GetValueAsInteger(); // args
                return;
            case ObjType::Float:
                ptr_float = self.data_ptr<float>();
                ASSERT(ptr_float);
                *ptr_float = obj->Call(this)->GetValueAsNumber(); // args
                return;
            case ObjType::Double:
                ptr_double = self.data_ptr<double>();
                ASSERT(ptr_double);
                *ptr_double = obj->Call(this)->GetValueAsNumber(); // args
                return;
        }

        ASSERT(!"Not implemented!");
    } else {

        c10::IntArrayRef shape = self.sizes(); // Кол-во эментов в каждом измерении
        std::vector<Index> ind(shape.size(),
                0); // Счетчик обхода всех эелемнтов тензора
        ItemTensorEval_(self, shape, ind, 0, obj, args);
    }
}

std::vector<int64_t> GetTensorShape(Context *ctx, TermPtr type, Object *local_vars) {
    std::vector<int64_t> result(type->size());
    for (int i = 0; i < type->size(); i++) {
        ObjPtr temp = ctx->CreateRVal(ctx, type->at(i).second, local_vars);
        if(temp->is_integer() || temp->is_bool_type()) {
            result[i] = temp->GetValueAsInteger();
        } else {
            NL_PARSER(type->at(i).second, "Measurement dimension can be an integer only!");
        }
        if(result[i] <= 0) {

            NL_PARSER(type->at(i).second, "Dimension size can be greater than zero!");
        }
    }
    return result;
}

std::vector<Index> Context::MakeIndex(Context *ctx, TermPtr term, Object *local_vars) {

    // `at::indexing::TensorIndex` is used for converting C++ tensor indices such
    // as
    // `{None, "...", Ellipsis, 0, true, Slice(1, None, 2), torch::tensor({1,
    // 2})}` into its equivalent `std::vector<TensorIndex>`, so that further
    // tensor indexing operations can be performed using the supplied indices.
    //
    // There is one-to-one correspondence between Python and C++ tensor index
    // types: Python                  | C++
    // -----------------------------------------------------
    // `None`                  | `at::indexing::None`
    // `Ellipsis`              | `at::indexing::Ellipsis`
    // `...`                   | `"..."`
    // `123`                   | `123`
    // `True` / `False`        | `true` / `false`
    // `:`                     | `Slice()` / `Slice(None, None)`
    // `::`                    | `Slice()` / `Slice(None, None, None)`
    // `1:`                    | `Slice(1, None)`
    // `1::`                   | `Slice(1, None, None)`
    // `:3`                    | `Slice(None, 3)`
    // `:3:`                   | `Slice(None, 3, None)`
    // `::2`                   | `Slice(None, None, 2)`
    // `1:3`                   | `Slice(1, 3)`
    // `1::2`                  | `Slice(1, None, 2)`
    // `:3:2`                  | `Slice(None, 3, 2)`
    // `1:3:2`                 | `Slice(1, 3, 2)`
    // `torch.tensor([1, 2])`) | `torch::tensor({1, 2})`

    std::vector<Index> result;

    if(!term->size()) {
        NL_PARSER(term, "Index not found!");
    }
    for (int i = 0; i < term->size(); i++) {
        if(!term->name(i).empty() || (term->at(i).second && term->at(i).second->IsString())) {
            NL_PARSER(term, "Named index not support '%d'!", i);
        }
        if(!term->at(i).second) {
            NL_PARSER(term, "Empty index '%d'!", i);
        }

        if(term->at(i).second->getTermID() == TermID::ELLIPSIS) {
            result.push_back(Index("..."));
        } else {

            ObjPtr temp = ctx->CreateRVal(ctx, term->at(i).second, local_vars);

            if(temp->is_none_type()) {

                result.push_back(Index(at::indexing::None));
            } else if(temp->is_integer() || temp->is_bool_type()) {

                if(temp->is_scalar()) {
                    result.push_back(Index(temp->GetValueAsInteger()));
                } else if(temp->m_value.dim() == 1) {
                    result.push_back(Index(temp->m_value));
                } else {
                    NL_PARSER(term->at(i).second, "Extra dimensions index not support '%d'!", i);
                }
            } else if(temp->is_range()) {

                int64_t start = temp->at("start")->GetValueAsInteger();
                int64_t stop = temp->at("stop")->GetValueAsInteger();
                int64_t step = temp->at("step")->GetValueAsInteger();

                result.push_back(Index(at::indexing::Slice(start, stop, step)));
            } else {

                NL_PARSER(term->at(i).second, "Fail tensor index '%d'!", i);
            }
        }
    }
    return result;
}

ObjPtr Context::CreateRVal(Context *ctx, TermPtr term, Object *local_vars) {

    if(!term) {
        ASSERT(term);
    }
    ASSERT(local_vars);

    ObjPtr result = nullptr;
    ObjPtr temp = nullptr;
    ObjPtr args = nullptr;
    ObjPtr value = nullptr;
    TermPtr field = nullptr;
    std::string full_name;

    result = Object::CreateNone();
    result->m_is_reference = term->m_is_ref;

    char *ptr;
    int64_t val_int;
    int64_t count;
    double val_dbl;
    ObjType type;
    bool has_error;
    std::vector<int64_t> sizes;
    at::Scalar torch_scalar;
    switch(term->getTermID()) {
        case TermID::INTEGER:
            val_int = parseInteger(term->getText().c_str());
            NL_TYPECHECK(term, newlang::toString(typeFromLimit(val_int)),
                    term->m_type_name); // Соответстствует ли тип значению?
            result->m_var_type_current = typeFromLimit(val_int);
            if(term->GetType()) {
                result->m_var_type_fixed = typeFromString(term->m_type_name, ctx);
                result->m_var_type_current = result->m_var_type_fixed;
            }
            result->m_value = torch::scalar_tensor(val_int, toTorchType(result->m_var_type_current));
            result->m_var_is_init = true;
            return result;
        case TermID::NUMBER:
            val_dbl = parseDouble(term->getText().c_str());
            NL_TYPECHECK(term, newlang::toString(typeFromLimit(val_dbl)),
                    term->m_type_name); // Соответстствует ли тип значению?
            result->m_var_type_current = typeFromLimit(val_dbl);
            if(term->GetType()) {
                result->m_var_type_fixed = typeFromString(term->m_type_name, ctx);
                result->m_var_type_current = result->m_var_type_fixed;
            }
            result->m_value = torch::scalar_tensor(val_dbl, toTorchType(result->m_var_type_current));
            result->m_var_is_init = true;
            return result;
        case TermID::STRWIDE:
            result->m_var_type_current = ObjType::StrWide;
            result->m_wstr = utf8_decode(term->getText());
            result->m_var_is_init = true;
            return result;

        case TermID::STRCHAR:
            result->m_var_type_current = ObjType::StrChar;
            result->m_str = term->getText();
            result->m_var_is_init = true;
            return result;

            /*        case TermID::FIELD:
                        if(module && module->HasFunc(term->GetFullName().c_str())) {
                            // Если поле является функцией и она загружена
                            result = Object::CreateType(Object::Type::FUNCTION,
               term->GetFullName().c_str()); result->m_module = module;
                            result->m_is_const = term->m_is_const;
                            result->m_is_ref = term->m_is_ref;
                            return result;
                        }
                        if(!result) {
                            LOG_RUNTIME("Term '%s' not found!",
               term->toString().c_str());
                        }
                        return result;
             */

            //@todo Что делать с пустыми значениями? Это None ???
        case TermID::EMPTY:
            result->m_var_type_current = ObjType::None;
            result->m_var_is_init = false;
            return result;

        case TermID::TERM:
            //            if(term->GetFullName().empty()) {
            //                return Object::CreateNone();
            //            }
            if(term->GetType()) {

                result->m_var_type_current = typeFromString(term->GetType()->m_text, ctx);
                result->m_var_type_fixed = result->m_var_type_current;
                result->m_var_is_init = false; // Нельзя считать значение

                // Check BuildInType
                has_error = false;
                typeFromString(term->GetType()->m_text, nullptr, &has_error);
                if(has_error) {
                    result->m_var_type_name = term->GetType()->m_text;
                }

                return result;
            }
            if(term->m_text.compare("_") == 0) {
                result->m_var_type_current = ObjType::None;
                result->m_var_is_init = false;
                return result;
            } else if(term->m_text.compare("$") == 0) {
                result->m_var_type_current = ObjType::Dictionary;
                result->m_var_name = "$";
                size_t pos = 0;
                while(ctx && pos < ctx->size()) {
                    if(ctx->at(pos).second.lock()) {
                        result->push_back(Object::CreateString(ctx->at(pos).first));
                        //                        result->push_back(Object::Arg(ctx->at(pos).first));
                        pos++;
                    } else {
                        ctx->erase(pos);
                    }
                }
                result->m_var_is_init = true;
                return result;
            } else if(term->m_text.compare("@") == 0) {
            } else if(term->m_text.compare("%") == 0) {
            }

            if(isLocal(term->m_text.c_str())) {
                full_name = MakeName(term->m_text);
                return local_vars->at(full_name);
            } else {
                result = ctx->GetTerm(term->GetFullName().c_str(), term->isRef());

                // Типы данных обрабатываются тут, а не в вызовах функций (TermID::CALL)

                if(term->size()) {
                    Object args(ctx, term, true, local_vars);
                    result = result->Call(ctx, &args);
                    //                    11111111111111111111111
                }

            }
            if(!result) {
                // Делать ислкючение или возвращать объект "ошибка" ?????
                LOG_RUNTIME("Term '%s' not found!", term->GetFullName().c_str());
            }
            result->m_var_is_init = true;

            field = term->m_right;
            if(field && field->getTermID() == TermID::FIELD) {
                while(field) {
                    result = result->at(field->getText());
                    field = field->m_right;
                    ASSERT(!field); // Нужно выполнять, а не просто получать значение поля
                }
            } else if(field && field->getTermID() == TermID::INDEX) {
                while(field) {
                    result = result->index_get(MakeIndex(ctx, field, local_vars));
                    field = field->m_right;
                    ASSERT(!field); // Нужно выполнять, а не просто получать значение поля
                }
            } else if(field) {
                LOG_RUNTIME("Not implemented! %s", field->toString().c_str());
            }

            return result;

        case TermID::TYPE:
        case TermID::TYPE_CALL:

            result = ctx->GetTerm(term->GetFullName().c_str(), term->isRef());

//            result->m_var_type_current = ObjType::Type;
            ASSERT(result->m_var_type_fixed == typeFromString(term->GetFullName(), ctx, nullptr));

            // Размерность, если указана
            result->m_type = Object::CreateType(result->m_var_type_current, nullptr, result->m_var_type_current);
            for (size_t i = 0; i < term->m_dims.size(); i++) {
                result->m_type->push_back(CreateRVal(ctx, term->m_dims[i], local_vars));
            }

//            if(result->m_type && result->m_type->size() == 1
//                    && (*result->m_type)[0]->toIndex().is_boolean() // 0 и 1 - логические значения
//                    && (*result->m_type)[0]->GetValueAsInteger() == 0) {
//                result->m_value = torch::scalar_tensor(0, toTorchType(result->m_var_type_fixed));
//            } else if(result->m_type) {
//                std::vector<int64_t> dims;
//                bool fixed = true;
//                for (size_t i = 0; i < result->m_type->size(); i++) {
//                    Index ind = (*result->m_type)[i]->toIndex();
//                    if(ind.is_integer()) {
//                        dims.push_back(ind.integer());
//                    } else {
//                        fixed = false;
//                        break;
//                    }
//                }
//                if(fixed) {
//                    result->m_value = torch::empty(dims, toTorchType(result->m_var_type_fixed));
//                } else {
//                    result->m_value = torch::empty({0}, toTorchType(result->m_var_type_fixed));
//                }
//            }
//            result->m_var_is_init = true;
//            //            result->m_var_type_fixed = result->m_var_type_current;
//            result->MakeConst();
//
//            if(term->getTermID() == TermID::TYPE) {
//                return result;
//            }


            args = Object::CreateDict();
            for (size_t i = 0; i < term->size(); i++) {
                if(term->name(i).empty()) {
                    args->push_back(CreateRVal(ctx, (*term)[i], local_vars));
                } else {
                    args->push_back(CreateRVal(ctx, (*term)[i], local_vars), term->name(i).c_str());
                }
            }
            result = result->Call(ctx, args.get());
//            result->m_var_type_fixed = result->m_var_type_current;
//            result->m_var_is_init = true;

            return result;



        case TermID::CALL:

            temp = ctx->GetTerm(term->GetFullName().c_str(), term->isRef());
            //            temp = CreateRVal(ctx, term, local_vars);
            if(!temp) {
                ASSERT(temp);
            }

            args = Object::CreateDict();
            for (size_t i = 0; i < term->size(); i++) {
                if(term->name(i).empty()) {
                    args->push_back(CreateRVal(ctx, (*term)[i], local_vars));
                } else {
                    args->push_back(CreateRVal(ctx, (*term)[i], local_vars), term->name(i).c_str());
                }
            }

            result = temp->Call(ctx, args.get());
            //                return ctx->CallByName(term, local_vars);
            return result;

        case TermID::TENSOR_BEGIN:

            return ctx->Comprehensions(ctx, term, local_vars);

            //            // Сколько элементов должно быть в тензоре
            //            count = -1;
            //            if(term->GetType()) {
            //                for (int i = 0; i < term->GetType()->m_dims.size();
            //                i++) {
            //                    temp = CreateRVal(ctx,
            //                    term->GetType()->m_dims[i], local_vars);
            //                    // Размеры тензора указываются целыми числами
            //                    больше нуля if(!temp->is_scalar() ||
            //                    !temp->is_integer()) {
            //                        NL_PARSER(term->GetType()->m_dims[i], "Dim
            //                        size supported as integer scalar only!");
            //                    }
            //                    if(temp->GetValueAsInteger() <= 0) {
            //                        NL_PARSER(term->GetType()->m_dims[i], "Dim
            //                        size failed!");
            //                    }
            //                    // Общее количество эелентов во всех измерениях
            //                    тензора if(count == -1) {
            //                        count = temp->GetValueAsInteger();
            //                    } else {
            //                        count *= temp->GetValueAsInteger();
            //                    }
            //                }
            //            }
            //
            //            result->m_var_type_current = ObjType::Dictionary;
            //
            //            //  Наполнить элементами
            //            for (int i = 0; i < term->size(); i++) {
            //
            //                ASSERT((*term)[i]);
            //
            //                if((*term)[i]->getTermID() == TermID::ELLIPSIS) {
            //
            //                    /* 123, ... ->  123, 123, 123, 123, 123, 123 ...
            //                     * [1,2,3,], ... -> [1,2,3,], [1,2,3,], ...
            //                     * ... [1,2,3,] ->  [1,2,3,],
            //                     * rand(), ... ->  0.123, 0.123, 0.123, ...
            //                     дублирует последний элемент
            //                     * ... rand()  ->  0.123, 0.987, 0.567, ...
            //                     раскрывает словарь или выполняет последний
            //                     элемент
            //                     * ... ... dict ->  name1=val1, name2=val2,
            //                     name3=val3 ... ????????????????????
            //                     */
            //                    ASSERT(!(*term)[i]->Left());
            //                    ASSERT(!(*term)[i]->Right());
            //
            //                    if(!temp) {
            //                        NL_PARSER((*term)[i], "No previous value!");
            //                    }
            //                    if(count < 0) {
            //                        NL_PARSER(term, "Tensor type and dimension
            //                        not set!");
            //                    }
            //
            //                    // Продублировать последенее значение до конца
            //                    размерности while(count) {
            //                        result->push_back(temp->Clone());
            //                        count -= 1;
            //                    }
            //
            //                    result->m_var_is_init = true;
            //                    return result->toType_(term->GetType());
            //                }
            //

            //                temp = CreateRVal(ctx, (*term)[i], local_vars);
            //                ASSERT(temp);
            //
            //
            //                if(temp->is_scalar()) {
            //
            //                    result->push_back(temp->Clone());
            //                    count -= 1;
            //
            //                } else if(temp->is_range()) {
            //
            //                    value = (*temp)["start"]->Clone();
            //                    while((*value) < (*temp)["stop"]) {
            //                        result->push_back(value->Clone());
            //                        count -= 1;
            //                        (*value) += (*temp)["step"];
            //                    }
            //
            //                } else if((*term)[i]->GetTokenID() ==
            //                TermID::TENSOR) {
            //
            //                    temp = CreateRVal(ctx, (*term)[i], local_vars);
            //                    result->op_concat_(*temp.get());
            //
            //
            //                } else {
            //                    LOG_RUNTIME("Not implemented %s!",
            //                    (*term)[i]->toString().c_str());
            //
            //                }
            //            }
            //
            //            result->m_var_is_init = true;
            //            return result->toType_(term->GetType());

            //        case TermID::CLASS:
            //            if(!term->m_class_name.empty()) {
            //                // Создается перечисление, если тип у имени класса один из арифметических
            //                // типов
            //
            //                // Check BuildInType
            //                has_error = false;
            //                type = typeFromString(term->m_class_name, nullptr, &has_error);
            //                if(has_error) {
            //                    NL_PARSER(term, "Typename '%s' not base type!", term->m_class_name.c_str());
            //                }
            //                if(!isIntegralType(type, true)) {
            //                    NL_PARSER(term, "Enum must be intergal type, not '%s'!", term->m_class_name.c_str());
            //                }
            //                result->m_var_type_current = ObjType::Dictionary;
            //                val_int = 0;
            //                for (size_t i = 0; i < term->size(); i++) {
            //                    if(term->name(i).empty()) {
            //                        NL_PARSER(term, "The enum element must have a name!");
            //                    }
            //
            //                    if((*term)[i]->m_text.empty()) {
            //                        temp = Object::CreateValue(val_int, type);
            //                    } else {
            //                        temp = CreateRVal(ctx, (*term)[i], local_vars);
            //                        val_int = temp->GetValueAsInteger();
            //                        NL_TYPECHECK(term, newlang::toString(typeFromLimit(val_int)),
            //                                term->m_class_name); // Соответстствует ли тип значению?
            //                    }
            //                    temp->m_var_type_fixed = type;
            //                    result->push_back(temp, term->name(i).c_str());
            //                    val_int += 1;
            //                }
            //                return result;
            //            }


        case TermID::TENSOR:
        case TermID::DICT:
            result->m_var_type_current = ObjType::Dictionary;
            for (size_t i = 0; i < term->size(); i++) {
                if(term->name(i).empty()) {
                    result->push_back(CreateRVal(ctx, (*term)[i], local_vars));
                } else {
                    result->push_back(CreateRVal(ctx, (*term)[i], local_vars), term->name(i).c_str());
                }
            }
            if(term->getTermID() == TermID::TENSOR) { // || term->getTermID() == TermID::TENSOR_BEGIN) {
                type = getSummaryTensorType(result, typeFromString(term->m_type_name, ctx));

                //                std::vector<int64_t> sizes =
                //                getTensorSizes(result.get());

                result->m_value = ConvertToTensor(result.get(), toTorchType(type));
                result->Variable::clear_();
                //                result->m_value = result->m_value.reshape(sizes);
                result->m_var_type_current = type;

                //                result->

                //                if(!canCastLimit(type,
                //                typeFromString(term->m_type_name))) {
                //                    LOG_RUNTIME("Can`t cast type '%s' to '%s'!", type,
                //                    newlang::toString(typeFromString(term->m_type_name)));
                //                }
                // Обычное конвертирование словаря в тензор выполняется без контроля
                // размерности, а создание тензора из литерала должно выполняться с
                // контролем размеров тензора
                //                if(result->size() == 0) {
                //                LOG_RUNTIME("Can`t create empty tensor!");
                //                }
                //                std::vector<int64_t> dims;
                //                calcTensorDims(result, dims);

                //                dims
                //                sizes = result->
                //
                //
                //
                //                        result->toType_(type);
            }
            result->m_var_is_init = true;
            return result;

        case TermID::ARGUMENT:

            val_int = IndexArg(term);
            if(val_int < local_vars->size()) {
                return local_vars->at(val_int).second;
            }
            LOG_RUNTIME("Argument '%s' not exist!", term->toString().c_str());

        case TermID::ELLIPSIS:
            result->m_var_type_current = ObjType::Ellipsis;
            result->m_var_type_fixed = ObjType::None;
            result->m_var_is_init = true;
            return result;

        case TermID::RANGE:

            for (size_t i = 0; i < term->size(); i++) {
                ASSERT(!term->name(i).empty());
                result->push_back(CreateRVal(ctx, (*term)[i], local_vars), term->name(i).c_str());
            }

            if(result->size() == 2) {
                result->push_back(Object::CreateValue(1, ObjType::None), "step");
            }

            result->m_var_type_current = ObjType::Range;
            result->m_var_type_fixed = ObjType::Range;
            result->m_var_is_init = true;

            return result;

            //        case TermID::FUNCTION:
            //        case TermID::TRANSPARENT:
            //            ASSERT(ctx);
            //
            //            if(term->getTermID() == TermID::FUNCTION) {
            //                result->m_var_type_current = ObjType::EVAL_FUNCTION;
            //            } else {
            //                result->m_var_type_current = ObjType::EVAL_TRANSP;
            //            }
            //            result->m_var_name = term->Left()
            //            result->m_func_source = term->Right();
            //            result->m_var_is_init = true;
            //            ctx->RegisterObject();
            //            return result;
    }
    LOG_RUNTIME("Fail create type %s from '%s'", newlang::toString(term->getTermID()), term->toString().c_str());

    return nullptr;
}

ObjPtr Context::Comprehensions(Context *ctx, Object *type, Object *args) {

    ASSERT(type->m_var_type_current == ObjType::Type);
    ASSERT(args);

    ObjPtr result;
    if(args->empty()) {
        result = Object::CreateType(type->m_var_type_fixed, nullptr, type->m_var_type_fixed);
        return result;
    }

    result = Object::CreateDict();
    int64_t count = -1; // Сколько элементов должно быть в тензоре
    if(type->m_type->size()) { // Указан ли размер создаваемого тензора?
        for (int64_t i = 0; i < type->m_type->size(); i++) {
            if((*type->m_type)[i]->GetValueAsInteger() <= 0) {
                LOG_RUNTIME("Dimensio size %ld at index %ld failed!", (*type->m_type)[i]->GetValueAsInteger(), i);
            }
            // Общее количество элементов во всех измерениях для тензора
            if(count == -1) {
                count = (*type->m_type)[i]->GetValueAsInteger();
            } else {
                count *= (*type->m_type)[i]->GetValueAsInteger();
            }
        }
    }

    ObjPtr prev_value = nullptr;
    //  Наполнить элементами
    for (int i = 0; i < args->size(); i++) {

        ASSERT((*args)[i]);
        /* 123, ... ->  123, 123, 123, 123, 123, 123 ...
         * [1,2,3,], ... -> [1,2,3,], [1,2,3,], ...
         * ... [1,2,3,] ->  [1,2,3,],
         * rand(), ... ->  0.123, 0.123, 0.123, ...  дублирует последний
         элемент
         * ... rand()  ->  0.123, 0.987, 0.567, ...  раскрывает словарь или
         выполняет последний элемент
         * ... ... dict ->  name1=val1, name2=val2, name3=val3 ...
         ????????????????????
         */

        bool call_by_item = false;

        if((*args)[i]->getType() == ObjType::Ellipsis) {

            if(i + 1 != args->size()) {
                LOG_RUNTIME("Ellipsis at index %d support as final element only!", i);
            }

            if(count < 0) {
                LOG_RUNTIME("Tensor dimension not set for ellipsis data!");
            }

            ObjPtr func = nullptr;

            //            if((*term)[i]->Left() && (*term)[i]->Right()) {
            //
            //                ASSERT((*term)[i]->Left()->getTermID() == TermID::ELLIPSIS);
            //
            //                // Раскрыть словарь c именами элементов
            //                prev_value = Comprehensions(ctx, (*term)[i], local_vars);
            //                if(!prev_value->is_dictionary_type()) {
            //                    LOG_RUNTIME("Expand value not dictionary type %s!", (*term)[i]->toString().c_str());
            //                }
            //                for (int i = 0; i < prev_value->size(); i++) {
            //                    result->push_back(prev_value->at(i).second->Clone(), prev_value->at(i).first);
            //                    count -= 1;
            //                }
            //
            //            } else if((*term)[i]->Right()) {
            //
            //                // Функция выполняется для каждого элемента
            //                ASSERT(!(*term)[i]->Left());
            //
            //                prev_value = CreateRVal(ctx, (*term)[i]->Right(), local_vars);
            //                ASSERT(prev_value);
            //
            //                call_by_item = prev_value->is_function();
            //                if(call_by_item) {
            //                    func = prev_value;
            //                }
            //            } else {
            //
            //                ASSERT(!(*term)[i]->Left());
            //                ASSERT(!(*term)[i]->Right());

            // Продублировать последенее значение до конца размерности
            if(!prev_value) {
                LOG_RUNTIME("No previous value for ellipsis index %d!", i);
            }
            //            }

            while(count > 0) {
                if(call_by_item) {
                    ASSERT(func);
                    prev_value = func->Call(this, ctx->at(0).second.lock().get());
                }
                result->push_back(prev_value->Clone());
                count -= 1;
            }

            result->m_var_is_init = true;
            return result->toType_(type);
        }

        prev_value = (*args)[i];
        ASSERT(prev_value);

        if(prev_value->is_range()) {

            ObjPtr temp = Object::CreateDict();
            ConvertRangeToDict(prev_value.get(), *temp.get());

            if(count >= 0 && temp->size() > count) {
                LOG_RUNTIME("Wrong range size to fill at pos %d!", i);
            }
            prev_value.swap(temp);
        }

        count -= ConcatData(result.get(), *prev_value.get(), ConcatMode::Append);
    }

    result->m_var_is_init = true;
    return result->toType_(type);
}

ObjPtr Context::Comprehensions(Context *ctx, TermPtr term, Object *local_vars) {

    ObjPtr result = Object::CreateDict();
    int64_t count = -1; // Сколько элементов должно быть в тензоре

    ObjPtr temp = nullptr;
    if(term->GetType()) { // Указант ли тип создаваемого объекта
        for (int i = 0; i < term->GetType()->m_dims.size(); i++) {
            temp = CreateRVal(ctx, term->GetType()->m_dims[i], local_vars);
            // Размеры тензора указываются целыми числами больше нуля
            if(!temp->is_scalar() || !temp->is_integer()) {
                NL_PARSER(term->GetType()->m_dims[i], "Dim size supported as integer scalar only!");
            }
            if(temp->GetValueAsInteger() <= 0) {
                NL_PARSER(term->GetType()->m_dims[i], "Dim size failed!");
            }
            // Общее количество элементов во всех измерениях для тензора
            if(count == -1) {
                count = temp->GetValueAsInteger();
            } else {
                count *= temp->GetValueAsInteger();
            }
        }
    }

    ObjPtr prev_value = nullptr;
    //  Наполнить элементами
    for (int i = 0; i < term->size(); i++) {

        ASSERT((*term)[i]);
        /* 123, ... ->  123, 123, 123, 123, 123, 123 ...
         * [1,2,3,], ... -> [1,2,3,], [1,2,3,], ...
         * ... [1,2,3,] ->  [1,2,3,],
         * rand(), ... ->  0.123, 0.123, 0.123, ...  дублирует последний
         элемент
         * ... rand()  ->  0.123, 0.987, 0.567, ...  раскрывает словарь или
         выполняет последний элемент
         * ... ... dict ->  name1=val1, name2=val2, name3=val3 ...
         ????????????????????
         */

        bool call_by_item = false;

        if((*term)[i]->getTermID() == TermID::ELLIPSIS) {

            if(i + 1 != term->size()) {
                NL_PARSER((*term)[i], "Ellipsis support as final element only!");
            }

            if(count < 0) {
                NL_PARSER(term, "Tensor type and dimension not set!");
            }

            ObjPtr func = nullptr;

            if((*term)[i]->Left() && (*term)[i]->Right()) {

                ASSERT((*term)[i]->Left()->getTermID() == TermID::ELLIPSIS);

                // Раскрыть словарь c именами элементов
                prev_value = Comprehensions(ctx, (*term)[i], local_vars);
                if(!prev_value->is_dictionary_type()) {
                    LOG_RUNTIME("Expand value not dictionary type %s!", (*term)[i]->toString().c_str());
                }
                for (int i = 0; i < prev_value->size(); i++) {
                    result->push_back(prev_value->at(i).second->Clone(), prev_value->at(i).first);
                    count -= 1;
                }

            } else if((*term)[i]->Right()) {

                // Функция выполняется для каждого элемента
                ASSERT(!(*term)[i]->Left());

                prev_value = CreateRVal(ctx, (*term)[i]->Right(), local_vars);
                ASSERT(prev_value);

                call_by_item = prev_value->is_function();
                if(call_by_item) {
                    func = prev_value;
                }
            } else {

                ASSERT(!(*term)[i]->Left());
                ASSERT(!(*term)[i]->Right());

                // Продублировать последенее значение до конца размерности
                if(!prev_value) {
                    NL_PARSER((*term)[i], "No previous value!");
                }
            }

            while(count > 0) {
                if(call_by_item) {
                    ASSERT(func);
                    prev_value = func->Call(this, local_vars);
                }
                result->push_back(prev_value->Clone());
                count -= 1;
            }

            result->m_var_is_init = true;
            if(term->GetType()) {
                return result->toType_(ctx, term->GetType(), local_vars);
            } else {
                return result;
            }
        }

        prev_value = CreateRVal(this, (*term)[i], local_vars);
        ASSERT(prev_value);

        if(prev_value->is_range()) {

            ObjPtr temp = Object::CreateDict();
            ConvertRangeToDict(prev_value.get(), *temp.get());

            if(count >= 0 && temp->size() > count) {
                NL_PARSER((*term)[i], "Wrong range size to fill!");
            }
            prev_value.swap(temp);
        }

        count -= ConcatData(result.get(), *prev_value.get(), ConcatMode::Append);

        //        else if (prev_value->is_scalar()) {
        //
        ////            result->push_back(prev_value->Clone());
        //            count -= ConcatData(result.get(), *prev_value.get(), ConcatMode::Append);
        //
        //        } else if (prev_value->is_tensor()) {
        //
        //            // ConcatData(result.get(), *temp.get(), ConcatMode::Append);
        //            // count -= temp->size();
        //
        ////            temp = CreateRVal(this, (*term)[i], local_vars);
        ////            ASSERT(temp);
        //            result->op_concat_(*prev_value.get());
        //            count -= prev_value->size();
        //
        //        } else {
        //            LOG_RUNTIME("Not implemented %s!", (*term)[i]->toString().c_str());
        //        }
    }

    result->m_var_is_init = true;
    return result->toType_(ctx, term->GetType(), local_vars);
    // return result;
}


//ObjPtr Context::Comprehensions(Context *ctx, TermPtr term, Object *local_vars) {
//
//    ObjPtr result = Object::CreateDict();
//    int64_t count = -1; // Сколько элементов должно быть в тензоре
//
//    ObjPtr temp = nullptr;
//    if(term->GetType()) { // Указант ли тип создаваемого объекта
//        for (int i = 0; i < term->GetType()->m_dims.size(); i++) {
//            temp = CreateRVal(ctx, term->GetType()->m_dims[i], local_vars);
//            // Размеры тензора указываются целыми числами больше нуля
//            if(!temp->is_scalar() || !temp->is_integer()) {
//                NL_PARSER(term->GetType()->m_dims[i], "Dim size supported as integer scalar only!");
//            }
//            if(temp->GetValueAsInteger() <= 0) {
//                NL_PARSER(term->GetType()->m_dims[i], "Dim size failed!");
//            }
//            // Общее количество элементов во всех измерениях для тензора
//            if(count == -1) {
//                count = temp->GetValueAsInteger();
//            } else {
//                count *= temp->GetValueAsInteger();
//            }
//        }
//    }
//
//    ObjPtr prev_value = nullptr;
//    //  Наполнить элементами
//    for (int i = 0; i < term->size(); i++) {
//
//        ASSERT((*term)[i]);
//        /* 123, ... ->  123, 123, 123, 123, 123, 123 ...
//         * [1,2,3,], ... -> [1,2,3,], [1,2,3,], ...
//         * ... [1,2,3,] ->  [1,2,3,],
//         * rand(), ... ->  0.123, 0.123, 0.123, ...  дублирует последний
//         элемент
//         * ... rand()  ->  0.123, 0.987, 0.567, ...  раскрывает словарь или
//         выполняет последний элемент
//         * ... ... dict ->  name1=val1, name2=val2, name3=val3 ...
//         ????????????????????
//         */
//
//        bool call_by_item = false;
//
//        if((*term)[i]->getTermID() == TermID::ELLIPSIS) {
//
//            if(i + 1 != term->size()) {
//                NL_PARSER((*term)[i], "Ellipsis support as final element only!");
//            }
//
//            if(count < 0) {
//                NL_PARSER(term, "Tensor type and dimension not set!");
//            }
//
//            ObjPtr func = nullptr;
//
//            if((*term)[i]->Left() && (*term)[i]->Right()) {
//
//                ASSERT((*term)[i]->Left()->getTermID() == TermID::ELLIPSIS);
//
//                // Раскрыть словарь c именами элементов
//                prev_value = Comprehensions(ctx, (*term)[i], local_vars);
//                if(!prev_value->is_dictionary_type()) {
//                    LOG_RUNTIME("Expand value not dictionary type %s!", (*term)[i]->toString().c_str());
//                }
//                for (int i = 0; i < prev_value->size(); i++) {
//                    result->push_back(prev_value->at(i).second->Clone(), prev_value->at(i).first);
//                    count -= 1;
//                }
//
//            } else if((*term)[i]->Right()) {
//
//                // Функция выполняется для каждого элемента
//                ASSERT(!(*term)[i]->Left());
//
//                prev_value = CreateRVal(ctx, (*term)[i]->Right(), local_vars);
//                ASSERT(prev_value);
//
//                call_by_item = prev_value->is_function();
//                if(call_by_item) {
//                    func = prev_value;
//                }
//            } else {
//
//                ASSERT(!(*term)[i]->Left());
//                ASSERT(!(*term)[i]->Right());
//
//                // Продублировать последенее значение до конца размерности
//                if(!prev_value) {
//                    NL_PARSER((*term)[i], "No previous value!");
//                }
//            }
//
//            while(count > 0) {
//                if(call_by_item) {
//                    ASSERT(func);
//                    prev_value = func->Call(this, local_vars);
//                }
//                result->push_back(prev_value->Clone());
//                count -= 1;
//            }
//
//            result->m_var_is_init = true;
//            if(term->GetType()) {
//                return result->toType_(ctx, term->GetType(), local_vars);
//            } else {
//                return result;
//            }
//        }
//
//        prev_value = CreateRVal(this, (*term)[i], local_vars);
//        ASSERT(prev_value);
//
//        if(prev_value->is_range()) {
//
//            ObjPtr temp = Object::CreateDict();
//            ConvertRangeToDict(prev_value.get(), *temp.get());
//
//            if(count >= 0 && temp->size() > count) {
//                NL_PARSER((*term)[i], "Wrong range size to fill!");
//            }
//            prev_value.swap(temp);
//        }
//
//        count -= ConcatData(result.get(), *prev_value.get(), ConcatMode::Append);
//
//        //        else if (prev_value->is_scalar()) {
//        //
//        ////            result->push_back(prev_value->Clone());
//        //            count -= ConcatData(result.get(), *prev_value.get(), ConcatMode::Append);
//        //
//        //        } else if (prev_value->is_tensor()) {
//        //
//        //            // ConcatData(result.get(), *temp.get(), ConcatMode::Append);
//        //            // count -= temp->size();
//        //
//        ////            temp = CreateRVal(this, (*term)[i], local_vars);
//        ////            ASSERT(temp);
//        //            result->op_concat_(*prev_value.get());
//        //            count -= prev_value->size();
//        //
//        //        } else {
//        //            LOG_RUNTIME("Not implemented %s!", (*term)[i]->toString().c_str());
//        //        }
//    }
//
//    result->m_var_is_init = true;
//    return result->toType_(ctx, term->GetType(), local_vars);
//    // return result;
//}