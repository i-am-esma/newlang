#include "pch.h"

#ifdef UNITTEST
#include "parser.h"

#include <signal.h>

#include <warning_pop.h>
#include <warning_push.h>
#include <gtest/gtest.h>

#include <builtin.h>
#include <newlang.h>

using namespace newlang;

//To clarify a few things; it's not the compiler (Clang) itself that produces import libraries, but the linker, and the object file format plays a large role in the process.
//Adjusting which symbols are exported via __attribute__((visibility(default)))
//(when either marking other symbols as hidden with __attribute__((visibility(hidden)))
//, or setting the default with something like -fvisibility=hidden
//, works with both GCC and Clang when building ELF object files. COFF doesn't have a similar per-symbol visibility flag.
//When linking a DLL with MS link.exe, or LLVM's lld-link (which mimics link.exe's behaviour), only symbols either marked with __declspec(dllexport)
//, or listed in a def file that is passed to the linker, are exported.
//Within the MinGW ecosystem (which brings a bit more of unix-like behaviours), the default is to export all global symbols (with some amount of logic to avoid exporting things that belong to the mingw base libraries themselves) if no symbols are explicitly chosen to be exported.
//If linking with lld-link instead of MS link.exe (either by calling lld-link instead of link, if calling the linker directly, or by adding -fuse-ld=lld
//if invoking the linker via the clang-cl frontend), you can opt in to this behaviour by adding the lld specific option -lldmingw
//, which enables a number of MinGW-specific behaviours in lld.

int64_t var_long = 987654321;

int64_t func_export(int64_t arg_long, uint8_t arg_byte) {
    return arg_long + arg_byte;
}

TEST(Eval, Assign) {

    Context ctx(RunTime::Init());

    ObjPtr list = ctx.ExecStr("$");
    ASSERT_STREQ("$=(,)", list->toString().c_str());

    ObjPtr var1 = ctx.ExecStr("var1 ::= 123");
    ASSERT_TRUE(var1);
    ASSERT_TRUE(var1->is_arithmetic_type());
    ASSERT_TRUE(var1->is_integer());
    ASSERT_EQ(var1->m_var_type_current, ObjType::Char);
    ASSERT_EQ(var1->m_var_type_fixed, ObjType::None);
    ASSERT_STREQ("var1=123", var1->toString().c_str());
    ASSERT_FALSE(ctx.find("var1") == ctx.end());

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var1',)", list->toString().c_str());

    ASSERT_THROW(ctx.ExecStr("var1 ::= 123"), Interrupt);

    ASSERT_TRUE(ctx.ExecStr("var1 = 100:Char"));
    ASSERT_EQ(var1->m_var_type_current, ObjType::Char);
    ASSERT_EQ(var1->m_var_type_fixed, ObjType::None);
    ASSERT_STREQ("var1=100", var1->toString().c_str());

    ASSERT_TRUE(ctx.ExecStr("var1 = 999"));
    ASSERT_STREQ("var1=999", var1->toString().c_str());
    ASSERT_FALSE(ctx.find("var1") == ctx.end());

    ASSERT_TRUE(ctx.ExecStr("var1 = _"));
    ASSERT_EQ(var1->getType(), ObjType::None);
    ASSERT_STREQ("var1=_", var1->toString().c_str());
    ASSERT_FALSE(ctx.find("var1") == ctx.end());

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var1',)", list->toString().c_str());

    //    ASSERT_TRUE(ctx.ExecStr("var1 = "));
    //    ASSERT_TRUE(ctx.select("var1").complete());
    ctx.clear_();

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=(,)", list->toString().c_str());

    ObjPtr var_str = ctx.ExecStr("var_str := 'Строка'");
    ASSERT_TRUE(var_str);
    ASSERT_TRUE(var_str->is_string_type());
    ASSERT_EQ(var_str->m_var_type_current, ObjType::StrChar);
    //    ASSERT_EQ(var_str->m_var_type_fixed, ObjType::String);
    ASSERT_STREQ("var_str='Строка'", var_str->toString().c_str());
    ASSERT_FALSE(ctx.find("var_str") == ctx.end());

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str',)", list->toString().c_str());

    ObjPtr var_num = ctx.ExecStr("var_num := 123.456: Number");
    ASSERT_TRUE(var_num);
    ASSERT_TRUE(var_num->is_arithmetic_type());
    ASSERT_TRUE(var_num->is_tensor());
    //    ASSERT_EQ(var_num->m_var_type_current, ObjType::Double);
    //    ASSERT_EQ(var_num->m_var_type_fixed, ObjType::Number);
    ASSERT_STREQ("var_num=123.456", var_num->toString().c_str());

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str', 'var_num',)", list->toString().c_str());


    LLVMAddSymbol("var_long", &var_long);
    LLVMAddSymbol("func_export", (void *) &func_export);


    var_long = 987654321;
    ObjPtr var_export = ctx.ExecStr("var_export := :Pointer(\"var_long:Long\")");
    ASSERT_TRUE(var_export);
    ASSERT_TRUE(var_export->is_tensor()) << var_export;
    ASSERT_EQ(var_export->getType(), ObjType::Long);
    ASSERT_STREQ("var_export=987654321", var_export->toString().c_str());
    var_long = 123132132;
    ASSERT_STREQ("var_export=123132132", var_export->toString().c_str());
    var_export->SetValue_(Obj::CreateValue(59875, ObjType::None));
    ASSERT_EQ(59875, var_long);

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str', 'var_num', 'var_export',)", list->toString().c_str());

    ObjPtr func_export = ctx.ExecStr("func_export := :Pointer(\"func_export(arg1:Long, arg2:Char=100):Long\")");
    ASSERT_TRUE(func_export);
    ASSERT_TRUE(func_export->is_function()) << func_export;
    ASSERT_EQ(func_export->getType(), ObjType::NativeFunc);
    ASSERT_STREQ("func_export=func_export(arg1:Long, arg2:Char=100):Long{}", func_export->toString().c_str());

    ObjPtr result = func_export->Call(&ctx, Obj::Arg(200), Obj::Arg(10));
    ASSERT_TRUE(result);
    ASSERT_EQ(210, result->GetValueAsInteger());

    result = func_export->Call(&ctx, Obj::Arg(10), Obj::Arg(10));
    ASSERT_TRUE(result);
    ASSERT_EQ(20, result->GetValueAsInteger());

    result = func_export->Call(&ctx, Obj::Arg(10));
    ASSERT_TRUE(result);
    ASSERT_EQ(110, result->GetValueAsInteger());

    // Переполнение второго аргумента
    ASSERT_ANY_THROW(func_export->Call(&ctx, Obj::Arg(1000), Obj::Arg(1000)));

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str', 'var_num', 'var_export', 'func_export',)", list->toString().c_str());

    var_num.reset();
    func_export.reset();

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str', 'var_export',)", list->toString().c_str());

    // Функция возвращает словарь с именами объектов в текущем контексте
    ObjPtr func_eval = ctx.ExecStr("func_eval(arg1, arg2) := {$;}");
    ASSERT_TRUE(func_eval);
    ASSERT_TRUE(func_eval->is_function()) << func_eval;
    ASSERT_EQ(func_eval->getType(), ObjType::EVAL_FUNCTION) << toString(func_eval->getType());
    ASSERT_STREQ("func_eval=func_eval(arg1, arg2):={$;}", func_eval->toString().c_str());

    ObjPtr result_eval = func_eval->Call(&ctx, Obj::Arg(200), Obj::Arg(10));
    ASSERT_TRUE(result_eval);
    ASSERT_STREQ("$=('$0', 'arg1', 'arg2', 'var_str', 'var_export', 'func_eval',)", result_eval->toString().c_str());

    list = ctx.ExecStr("$");
    ASSERT_STREQ("$=('var_str', 'var_export', 'func_eval',)", list->toString().c_str());


    ObjPtr dict1 = ctx.ExecStr("(10, 2,  3,   4,   )");
    ASSERT_TRUE(dict1);
    ASSERT_EQ(ObjType::Dictionary, dict1->m_var_type_current) << toString(dict1->m_var_type_current);
    ASSERT_EQ(ObjType::None, dict1->m_var_type_fixed) << toString(dict1->m_var_type_fixed);
    ASSERT_EQ(4, dict1->size());
    ASSERT_STREQ("(10, 2, 3, 4,)", dict1->toString().c_str());

    ObjPtr dict2 = ctx.ExecStr("( (10, 2,  3,   4, (1,2,),   ), (10, 2,  3,   4,   ),)");
    ASSERT_TRUE(dict2);
    ASSERT_EQ(ObjType::Dictionary, dict2->m_var_type_current) << toString(dict2->m_var_type_current);
    ASSERT_EQ(ObjType::None, dict2->m_var_type_fixed) << toString(dict2->m_var_type_fixed);
    ASSERT_EQ(2, dict2->size());
    ASSERT_STREQ("((10, 2, 3, 4, (1, 2,),), (10, 2, 3, 4,),)", dict2->toString().c_str());

    ObjPtr tensor = ctx.ExecStr("[1,1,0,0,]");
    ASSERT_TRUE(tensor);
    ASSERT_EQ(ObjType::Bool, tensor->m_var_type_current) << toString(tensor->m_var_type_current);
    ASSERT_EQ(ObjType::None, tensor->m_var_type_fixed) << toString(tensor->m_var_type_fixed);
    ASSERT_EQ(1, tensor->m_value.dim());
    ASSERT_EQ(4, tensor->m_value.size(0));
    ASSERT_EQ(1, tensor->index_get({0})->GetValueAsInteger());
    ASSERT_EQ(1, tensor->index_get({1})->GetValueAsInteger());
    ASSERT_EQ(0, tensor->index_get({2})->GetValueAsInteger());
    ASSERT_EQ(0, tensor->index_get({3})->GetValueAsInteger());

    ASSERT_STREQ("[1, 1, 0, 0,]:Bool", tensor->GetValueAsString().c_str());

    ObjPtr tensor2 = ctx.ExecStr("[222,333,3333,]");
    ASSERT_TRUE(tensor2);
    ASSERT_STREQ("[222, 333, 3333,]:Short", tensor2->GetValueAsString().c_str());

    ObjPtr tensorf = ctx.ExecStr("[1.2, 0.22, 0.69,]");
    ASSERT_TRUE(tensorf);
    ASSERT_STREQ("[1.2, 0.22, 0.69,]:Double", tensorf->GetValueAsString().c_str());

    ObjPtr tensor_all = ctx.ExecStr("[ [1, 1, 0, 0,], [10, 10, 0.1, 0.2,], ]");
    ASSERT_TRUE(tensor_all);
    ASSERT_EQ(ObjType::Double, tensor_all->m_var_type_current) << toString(tensor_all->m_var_type_current);
    ASSERT_EQ(ObjType::None, tensor_all->m_var_type_fixed) << toString(tensor_all->m_var_type_fixed);
    ASSERT_EQ(2, tensor_all->m_value.dim());
    ASSERT_EQ(2, tensor_all->m_value.size(0));
    ASSERT_EQ(4, tensor_all->m_value.size(1));

    ASSERT_STREQ("1", tensor_all->index_get({0, 0})->GetValueAsString().c_str());
    ASSERT_STREQ("1", tensor_all->index_get({0, 1})->GetValueAsString().c_str());
    ASSERT_STREQ("0", tensor_all->index_get({0, 2})->GetValueAsString().c_str());
    ASSERT_STREQ("0", tensor_all->index_get({0, 3})->GetValueAsString().c_str());

    ASSERT_STREQ("10", tensor_all->index_get({1, 0})->GetValueAsString().c_str());
    ASSERT_STREQ("10", tensor_all->index_get({1, 1})->GetValueAsString().c_str());
    ASSERT_STREQ("0.1", tensor_all->index_get({1, 2})->GetValueAsString().c_str());
    ASSERT_STREQ("0.2", tensor_all->index_get({1, 3})->GetValueAsString().c_str());

    ASSERT_STREQ("[\n  [1, 1, 0, 0,], [10, 10, 0.1, 0.2,],\n]:Double", tensor_all->GetValueAsString().c_str());
}

TEST(Eval, Tensor) {

    Context ctx(RunTime::Init());

    ObjPtr tensor = ctx.ExecStr(":Tensor(1)");
    ASSERT_TRUE(tensor);
    ASSERT_EQ(ObjType::Tensor, tensor->m_var_type_fixed) << toString(tensor->m_var_type_fixed);
    ASSERT_EQ(ObjType::Bool, tensor->getType()) << toString(tensor->m_var_type_current);
    ASSERT_EQ(0, tensor->size());

    ASSERT_STREQ("1", tensor->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    tensor = ctx.ExecStr("(1,2,3,)");
    ASSERT_TRUE(tensor);
    ASSERT_STREQ("(1, 2, 3,)", tensor->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    tensor = ctx.ExecStr(":Tensor([1,2,3,])");
    ASSERT_TRUE(tensor);
    ASSERT_STREQ("[1, 2, 3,]:Char", tensor->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    tensor = ctx.ExecStr(":Int([1,])");
    ASSERT_TRUE(tensor);
    ASSERT_STREQ("[1,]:Int", tensor->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    ObjPtr tt = ctx.ExecStr(":Tensor[3]( (1,2,3,) )");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[1, 2, 3,]:Char", tt->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    tt = ctx.ExecStr(":Int((1,2,3,))");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[1, 2, 3,]:Int", tt->GetValueAsString().c_str()) << tensor->GetValueAsString().c_str();

    tt = ctx.ExecStr(":Int[2,3]((1,2,3,4,5,6,))");
    ASSERT_TRUE(tt);

    EXPECT_EQ(2, tt->m_value.dim());
    EXPECT_EQ(2, tt->m_value.size(0));
    EXPECT_EQ(3, tt->m_value.size(1));

    ASSERT_STREQ("[\n  [1, 2, 3,], [4, 5, 6,],\n]:Int", tt->GetValueAsString().c_str());

    ObjPtr str = ctx.ExecStr(":Tensor('first second')");
    ASSERT_TRUE(str);
    ASSERT_STREQ("[102, 105, 114, 115, 116, 32, 115, 101, 99, 111, 110, 100,]:Char", str->GetValueAsString().c_str());

    tt = ctx.ExecStr(":Tensor((first='first', space=32, second='second',))");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[102, 105, 114, 115, 116, 32, 115, 101, 99, 111, 110, 100,]:Char", tt->GetValueAsString().c_str());

    ASSERT_TRUE(str->op_equal(tt));

    tt = ctx.ExecStr(":Int[6,2](\"Тензор Int  \")");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[\n  [1058, 1077,], [1085, 1079,], [1086, 1088,], [32, 73,], "
            "[110, 116,], [32, 32,],\n]:Int",
            tt->GetValueAsString().c_str());

    tt = ctx.ExecStr(":Tensor(99)");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("99", tt->GetValueAsString().c_str());

    tt = ctx.ExecStr(":Double[10,2](0, ... )");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[\n  [0, 0,], [0, 0,], [0, 0,], [0, 0,], [0, 0,], [0, 0,], [0, "
            "0,], [0, 0,], [0, 0,], [0, 0,],\n]:Double",
            tt->GetValueAsString().c_str());

    ObjPtr rand = ctx.ExecStr("rand := :Pointer('rand():Int')");

    // Может быть раскрытие словаря, который возвращает вызов функции
    // и может быть многократный вызов одной и той функции
    // :Int[3,2]( ... rand() ... )
    tt = ctx.ExecStr(":Int[3,2]( ... rand() ... )");
    ASSERT_TRUE(tt);
    std::string rand_str = tt->GetValueAsString();
    ASSERT_TRUE(50 < tt->GetValueAsString().size()) << rand_str;

    tt = ctx.ExecStr(":Int[5,2]( 0..10 )");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[\n  [0, 1,], [2, 3,], [4, 5,], [6, 7,], [8, 9,],\n]:Int", tt->GetValueAsString().c_str());

    tt = ctx.ExecStr(":Double[5,2]( 0..10 )");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[\n  [0, 1,], [2, 3,], [4, 5,], [6, 7,], [8, 9,],\n]:Double", tt->GetValueAsString().c_str());

    tt = ctx.ExecStr("0..1..0.1");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("0..1..0.1", tt->toString().c_str());

    //    tt = ctx.ExecStr(":Dictionary( 0..0.99..0.1 )");
    //    ASSERT_TRUE(tt);
    //    ASSERT_STREQ("(0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,)", tt->GetValueAsString().c_str());

    tt = ctx.ExecStr(":Tensor( 0..0.99..0.1 )");
    ASSERT_TRUE(tt);
    ASSERT_STREQ("[0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9,]:Double", tt->GetValueAsString().c_str());
}

TEST(Eval, FuncSimple) {

    Context ctx(RunTime::Init());

    ObjPtr test_and = ctx.ExecStr("test_and(arg1, arg2) :&&= $arg1 == $arg2, $arg1");
    ASSERT_TRUE(test_and);

    //    EXPECT_FALSE((*test_and)(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE((*test_and)(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(123, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE((*test_and)(ctx, Obj::Arg(555, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_FALSE((*test_and)(ctx, Obj::Arg(0, "arg1"), Obj::Arg(0,
    //    "arg2"))->GetValueAsBoolean());

    ObjPtr test_or = ctx.ExecStr("test_or(arg1, arg2) :||= $arg1 == 555, $arg1");
    ASSERT_TRUE(test_or);

    //    EXPECT_TRUE(test_or->Call(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE(test_or->Call(ctx, Obj::Arg(555, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE(test_or->Call(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(123, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE(test_or->Call(ctx, Obj::Arg(555, "arg1"),
    //    Obj::Arg(0, "arg2"))->GetValueAsBoolean());
    //    EXPECT_FALSE(test_or->Call(ctx, Obj::Arg(0, "arg1"), Obj::Arg(0,
    //    "arg2"))->GetValueAsBoolean());

    ObjPtr test_xor = ctx.ExecStr("test_xor(arg1, arg2) :^^= $arg1 == $arg2, $arg1");
    ASSERT_TRUE(test_xor);

    //    EXPECT_TRUE(test_xor->Call(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_FALSE(test_xor->Call(ctx, Obj::Arg(555, "arg1"),
    //    Obj::Arg(555, "arg2"))->GetValueAsBoolean());
    //    EXPECT_FALSE(test_xor->Call(ctx, Obj::Arg(123, "arg1"),
    //    Obj::Arg(123, "arg2"))->GetValueAsBoolean());
    //    EXPECT_TRUE(test_xor->Call(ctx, Obj::Arg(0, "arg1"), Obj::Arg(0,
    //    "arg2"))->GetValueAsBoolean());
}

TEST(Eval, TypesNative) {

    Context::Reset();
    Context ctx(RunTime::Init());

    //    ASSERT_EQ(41, ctx.m_types.size());

    std::vector<std::wstring> types = ctx.SelectPredict(":");
    ASSERT_EQ(2, types.size());

    //    ASSERT_STREQ(":Bool", utf8_encode(types[0]).c_str());
    //    ASSERT_STREQ(":Char", utf8_encode(types[1]).c_str());

    types = ctx.SelectPredict(":", 5);
    ASSERT_EQ(7, types.size());

    ObjPtr file = ctx.ExecStr(":File ::= :Pointer;");
    ASSERT_TRUE(file);

    types = ctx.SelectPredict(":File");
    ASSERT_EQ(1, types.size());

    //    ObjPtr f_stdout = ctx.CreateNative("stdout:File");
    //    ASSERT_TRUE(f_stdout);
    //    ASSERT_TRUE(f_stdout->m_func_ptr); // 0x555555cf39c0
    //    <stdout@@GLIBC_2.2.5> fputs("TEST STDOUT", stdout); // 0x7fff6ec7e760
    //    <_IO_2_1_stdout_> fputs("TEST STDOUT", (FILE *)f_stdout->m_func_ptr);

    //    //stdout:File ::= :Pointer("stdout:File");
    //    ObjPtr f2_stdout = ctx.ExecStr("stdout:File ::= :Pointer('stdout:File')");
    //    ASSERT_TRUE(f2_stdout);
    //    ASSERT_TRUE(f2_stdout->m_func_ptr);
    //    ASSERT_EQ(f_stdout->m_func_ptr, f2_stdout->m_func_ptr);
    //    //    ASSERT_EQ(f_stdout->m_func_ptr, (void *)stdout);

    ObjPtr fopen = ctx.CreateNative("fopen(filename:StrChar, modes:StrChar):File");
    ASSERT_TRUE(fopen);
    ASSERT_TRUE(fopen->m_func_ptr);

    ObjPtr fopen2 = ctx.ExecStr("@fopen2 ::= :Pointer('fopen(filename:StrChar, modes:StrChar):File')");
    ASSERT_TRUE(fopen2);
    ASSERT_TRUE(fopen2->m_func_ptr);
    ASSERT_EQ(fopen->m_func_ptr, fopen2->m_func_ptr);
    ASSERT_TRUE(ctx.FindTerm("fopen2"));
    auto iter = ctx.m_global_terms.find("fopen2");
    ASSERT_NE(iter, ctx.m_global_terms.end());

    ObjPtr fopen3 = ctx.ExecStr("@fopen3(filename:String, modes:String):File ::= "
            ":Pointer('fopen(filename:StrChar, modes:StrChar):File')");
    ASSERT_TRUE(fopen3);
    ASSERT_TRUE(fopen3->m_func_ptr);
    ASSERT_EQ(fopen->m_func_ptr, fopen3->m_func_ptr);

    ObjPtr fclose = ctx.ExecStr("@fclose(stream:File):Int ::= :Pointer(\"fclose(stream:File):Int\")");
    ASSERT_TRUE(fclose);
    ASSERT_TRUE(fclose->m_func_ptr);

    ObjPtr fremove = ctx.ExecStr("@fremove(filename:String):Int ::= "
            ":Pointer(\"remove(filename:StrChar):Int\")");
    ASSERT_TRUE(fremove);
    ASSERT_TRUE(fremove->m_func_ptr);

    ObjPtr frename = ctx.ExecStr("@rename(old:String, new:String):Int ::= "
            ":Pointer('rename(old:StrChar, new:StrChar):Int')");
    ASSERT_TRUE(frename);
    ASSERT_TRUE(frename->m_func_ptr);

    ObjPtr fprintf = ctx.ExecStr("@fprintf(stream:File, format:FmtChar, ...):Int ::= "
            ":Pointer('fprintf(stream:File, format:FmtChar, ...):Int')");
    ASSERT_TRUE(fremove);
    ObjPtr fputc = ctx.ExecStr("@fputc(c:Int, stream:File):Int ::= "
            ":Pointer('fputc(c:Int, stream:File):Int')");
    ASSERT_TRUE(fremove);
    ObjPtr fputs = ctx.ExecStr("@fputs(s:String, stream:File):Int ::= "
            ":Pointer('fputs(s:StrChar, stream:File):Int')");
    ASSERT_TRUE(fputs);

    std::filesystem::create_directories("temp");
    ASSERT_TRUE(std::filesystem::is_directory("temp"));

    ObjPtr F = fopen->Call(&ctx, Obj::Arg("temp/ffile.temp"), Obj::Arg("w+"));
    ASSERT_TRUE(F);
    ASSERT_TRUE(F->GetValueAsInteger());
    ASSERT_TRUE(fputs->Call(&ctx, Obj::Arg("test fopen()\ntest fputs()\n"), Obj::Arg(F)));

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
    ASSERT_TRUE(fprintf->Call(&ctx, Obj::Arg(F), Obj::Arg("%s"), Obj::Arg(oss.str())));

    ASSERT_TRUE(fclose->Call(&ctx, Obj::Arg(F)));

    ObjPtr F2 = ctx.ExecStr("@F2 ::= fopen2('temp/ffile_eval.temp','w+')");
    ASSERT_TRUE(F2);
    ObjPtr F_res = ctx.ExecStr("fputs('test from eval !!!!!!!!!!!!!!!!!!!!\\n', F2)");
    ASSERT_TRUE(F_res);

    /*
     * Enum (перечисление) использование символьного названия поля вместо "магического" числа
     * 
     * :EnumStruct = :Enum(One:Int=1, Two=..., Three=10) - все поля имеют имена и автоматическую нумерацию + могут иметь тип
     * :EnumStruct.One
     * var = :EnumStruct(thread, gpu);
     * 
     * :TypeStruct = :Struct(One:Int=1, Two:Char=0, Three=10) - все типы поле определены
     * :Class(One:Int=1, Two=..., Three=10)
     * :Class3 = :Class1(One:Int=1, Two=..., Three=10), Class2(One:Int=1, Two=..., Three=10);
     * 
     * (One:Int=1, Two=_, Three=10,):Enum(thread, gpu) использовать тип значения в имени поля вместо общего типа ----- Enum(One, Two=2, Three=3):Int  ------
     * [[ One, Two=2, Three=3 ]]:Enum(thread, gpu)
     * 
     * :Seek::SET или @Seek::SET - статическое зачение у глобального типа
     * Seek.SET или $Seek.SET - статическое зачение у глобального типа
     */

    ObjPtr SEEK1 = ctx.ExecStr("@SEEK1 ::= :Enum(SET:Int=10, \"CUR\", END=20)");
    ASSERT_TRUE(SEEK1);

    ASSERT_EQ(3, SEEK1->size());
    ASSERT_TRUE((*SEEK1)[0].second);
    ASSERT_EQ(ObjType::Char, (*SEEK1)[0].second->getType()) << newlang::toString((*SEEK1)[0].second->getType());
    ASSERT_EQ(ObjType::Char, (*SEEK1)[0].second->m_var_type_fixed) << newlang::toString((*SEEK1)[0].second->m_var_type_fixed);
    ASSERT_EQ(10, (*SEEK1)[0].second->GetValueAsInteger());

    ASSERT_TRUE((*SEEK1)[1].second);
    ASSERT_EQ(ObjType::Char, (*SEEK1)[1].second->getType()) << newlang::toString((*SEEK1)[1].second->getType());
    ASSERT_EQ(ObjType::Char, (*SEEK1)[1].second->m_var_type_fixed) << newlang::toString((*SEEK1)[1].second->m_var_type_fixed);
    ASSERT_EQ(11, (*SEEK1)[1].second->GetValueAsInteger());

    ASSERT_TRUE((*SEEK1)[2].second);
    ASSERT_EQ(ObjType::Char, (*SEEK1)[2].second->getType()) << newlang::toString((*SEEK1)[2].second->getType());
    ASSERT_EQ(ObjType::Char, (*SEEK1)[2].second->m_var_type_fixed) << newlang::toString((*SEEK1)[2].second->m_var_type_fixed);
    ASSERT_EQ(20, (*SEEK1)[2].second->GetValueAsInteger());

    ASSERT_EQ(10, (*SEEK1)["SET"].second->GetValueAsInteger());
    ASSERT_EQ(11, (*SEEK1)["CUR"].second->GetValueAsInteger());
    ASSERT_EQ(20, (*SEEK1)["END"].second->GetValueAsInteger());

    ObjPtr SEEK2 = ctx.ExecStr("@SEEK2 ::= :Enum(SET=, CUR=, END=300)");
    ASSERT_TRUE(SEEK2);
    ASSERT_EQ(3, SEEK2->size());
    ASSERT_EQ(0, (*SEEK2)[0].second->GetValueAsInteger());
    ASSERT_EQ(ObjType::Bool, (*SEEK2)[0].second->getType());
    ASSERT_EQ(1, (*SEEK2)[1].second->GetValueAsInteger());
    ASSERT_EQ(ObjType::Bool, (*SEEK2)[1].second->getType());
    ASSERT_EQ(300, (*SEEK2)[2].second->GetValueAsInteger());
    ASSERT_EQ(ObjType::Short, (*SEEK2)[2].second->getType());
    ASSERT_EQ(0, (*SEEK2)["SET"].second->GetValueAsInteger());
    ASSERT_EQ(1, (*SEEK2)["CUR"].second->GetValueAsInteger());
    ASSERT_EQ(300, (*SEEK2)["END"].second->GetValueAsInteger());

    ObjPtr SEEK = ctx.ExecStr("@SEEK ::= :Enum(SET=0, CUR=1, END=2)");
    ASSERT_TRUE(SEEK);

    ASSERT_EQ(3, SEEK->size());
    ASSERT_EQ(0, (*SEEK)[0].second->GetValueAsInteger());
    ASSERT_EQ(1, (*SEEK)[1].second->GetValueAsInteger());
    ASSERT_EQ(2, (*SEEK)[2].second->GetValueAsInteger());
    ASSERT_EQ(0, (*SEEK)["SET"].second->GetValueAsInteger());
    ASSERT_EQ(1, (*SEEK)["CUR"].second->GetValueAsInteger());
    ASSERT_EQ(2, (*SEEK)["END"].second->GetValueAsInteger());

    F_res = ctx.ExecStr("@SEEK.SET");
    ASSERT_TRUE(F_res);
    ASSERT_EQ(0, F_res->GetValueAsInteger());
    F_res = ctx.ExecStr("@SEEK.CUR");
    ASSERT_TRUE(F_res);
    ASSERT_EQ(1, F_res->GetValueAsInteger());
    F_res = ctx.ExecStr("@SEEK.END");
    ASSERT_TRUE(F_res);
    ASSERT_EQ(2, F_res->GetValueAsInteger());

    ObjPtr seek = ctx.ExecStr("@fseek(stream:File, offset:Long, whence:Int):Int ::= "
            ":Pointer('fseek(stream:File, offset:Long, whence:Int):Int')");
    ASSERT_TRUE(seek);

    F_res = ctx.ExecStr("fseek(F2, 10, @SEEK.SET)");
    ASSERT_TRUE(F_res);
    ASSERT_EQ(0, F_res->GetValueAsInteger());

    F_res = ctx.ExecStr("fclose(F2)");
    ASSERT_TRUE(F_res);

    // extern size_t fread (void *__restrict __ptr, size_t __size,
    //		     size_t __n, FILE *__restrict __stream) __wur;
    //
    // extern size_t fwrite (const void *__restrict __ptr, size_t __size,
    //		      size_t __n, FILE *__restrict __s);
}

TEST(Eval, Fileio) {

    Context::Reset();
    Context ctx(RunTime::Init());

    ASSERT_NO_THROW(ctx.ExecFile("../examples/fileio.nlp", nullptr, false));

    ASSERT_TRUE(ctx.FindTerm("fopen"));
    ASSERT_TRUE(ctx.FindTerm("fputs"));
    ASSERT_TRUE(ctx.FindTerm("fclose"));

    std::filesystem::create_directories("temp");
    ASSERT_TRUE(std::filesystem::is_directory("temp"));

    ObjPtr file = ctx.ExecStr("file ::= fopen('temp/ffile_eval2.temp','w+')");
    ASSERT_TRUE(file);
    ObjPtr file_res = ctx.ExecStr("fputs('test 222 from eval !!!!!!!!!!!!!!!!!!!!\\n', file)");
    ASSERT_TRUE(file_res);
    file_res = ctx.ExecStr("fclose(file)");
    ASSERT_TRUE(file_res);

    //@todo try and catch segfault
    //    ASSERT_ANY_THROW(
    //            // Double free
    //            file_res = ctx.ExecStr("fclose(file)"););
    //            
    //    Context::Reset();
}

TEST(ExecStr, Funcs) {

    Context::Reset();
    Context ctx(RunTime::Init());

    EXPECT_TRUE(ctx.m_runtime->GetNativeAddr("printf"));

    ObjPtr p = ctx.ExecStr("printf := :Pointer('printf(format:FmtChar, ...):Int');");
    ASSERT_TRUE(p);
    ASSERT_TRUE(p->m_func_ptr);
    ASSERT_STREQ("printf=printf(format:FmtChar, ...):Int{}", p->toString().c_str());

    typedef int (* printf_type)(const char *, ...);

    printf_type ttt = (printf_type) p->m_func_ptr;
    ASSERT_EQ(7, (*ttt)("Test 1 "));
    ASSERT_EQ(18, (*ttt)("%s", "Test Variadic call"));

    ObjPtr res = p->Call(&ctx, Obj::Arg(Obj::CreateString("Test")));
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->is_integer());
    ASSERT_EQ(4, res->GetValueAsInteger());

    res = p->Call(&ctx, Obj::Arg(Obj::CreateString("%s")), Obj::Arg(Obj::CreateString("call direct")));
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->is_integer());
    ASSERT_EQ(11, res->GetValueAsInteger());

    res = p->Call(&ctx, Obj::Arg(Obj::CreateString("%s")), Obj::Arg(Obj::CreateString("Русские символы")));
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->is_integer());
    ASSERT_TRUE(15 <= res->GetValueAsInteger());

    ObjPtr hello = ctx.ExecStr("hello(str='') := {printf('%s', $str); $str;}");
    ASSERT_TRUE(hello);
    ASSERT_STREQ("hello=hello(str=''):={printf('%s', $str); $str;}", hello->toString().c_str());

    ObjPtr result = ctx.ExecStr("printf('%s%d\\n', 'Привет, мир!', 2);");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->GetValueAsInteger() >= 23);

    result = ctx.ExecStr("hello('Привет, мир2!\\n');");
    ASSERT_TRUE(result);
    ASSERT_STREQ("Привет, мир2!\n", result->GetValueAsString().c_str());
}

/*
 * scalar_int := 100:Int; # Тип скаляра во время компиляции
 * scalar_int := 100:Int(__device__="GPU"); # Тип скаляра во время компиляции
 * scalar_int := 100:Int(); ?????? # Тип скаляра во время компиляции
 * :type_int := :Int; # Синоним типа Int во время компиляции (тип не может быть изменен)
 * :type_int := :Int(); # Копия типа Int во время выполнения (тип может быть изменен после Mutable)
 * 
 * scalar_int := :Int(0); # Преобразование типа во время выполнения с автоматической размерностью (скаляр)
 * scalar_int := :Int[0](0); # Преобразование типа во время выполнения с указанием размерности (скаляр)
 * scalar_int := :Int[0]([0,]); # Преобразование типа во время выполнения с указанием размерности (скаляр)
 * 
 * tensor_int := :Int([0,]); # Преобразование типа во время выполнения с автоматической размерностью (тензор)
 * tensor_int := :Int[1](0); # Преобразование типа во время выполнения с указанием размерности (тензор)
 * tensor_int := :Int[...](0); # Преобразование типа во время выполнения с произвольной размернотью (тензор)
 * 
 * 
 * :синоним := :Int; # Неизменяемый ?????
 * :копия := :Int(); # Изменяемый ?????
 * 
 * Mutable(:синоним); # Ошибка
 * Mutable(:копия); # Норма
 * 
 * :Int(1); # Не меняет размерность, только тип !!!!
 * :Int[2, ...](100, 200); # Одномерный тензор Int произвольного размера
 * :Int([,], 100, 200); # Объединить аргументы, если их несколько и преобразовать их тип к Int
 * :Int[0](10000000); # Преобразовать тип к скаляру
 * :Int[2](100, 200); # Преобразовать аргументы в тензор указанной размерности и заданного типа
 * :Int[2,1](100, 200); # Преобразовать аргументы в тензор указанной размерности и заданного типа
 * :Int[1,2](100, 200); # Преобразовать аргументы в тензор указанной размерности и заданного типа
 * 
 */
TEST(Eval, Convert) {

    /*
     * - Встроеные функции преобразования простых типов данных
     * - Передача аргументов функци по ссылкам
     */

    RuntimePtr opts = RunTime::Init();
    Context ctx(opts);

    ObjPtr type_int = ctx.ExecStr(":Int");
    ASSERT_TRUE(type_int);
    ASSERT_EQ(ObjType::Type, type_int->getType()) << toString(type_int->m_var_type_current);
    ASSERT_EQ(ObjType::Int, type_int->m_var_type_fixed) << toString(type_int->m_var_type_fixed);
    ASSERT_EQ(0, type_int->size());


    ObjPtr type_dim = ctx.ExecStr(":Int[0]");
    ASSERT_TRUE(type_dim);
    ASSERT_EQ(ObjType::Type, type_dim->getType()) << toString(type_dim->m_var_type_current);
    ASSERT_EQ(ObjType::Int, type_dim->m_var_type_fixed) << toString(type_dim->m_var_type_fixed);

    ASSERT_TRUE(type_dim->m_dimensions);
    ASSERT_EQ(1, type_dim->m_dimensions->size());
    ASSERT_EQ(0, (*type_dim->m_dimensions)[0].second->GetValueAsInteger());

    ObjPtr type_ell = ctx.ExecStr(":Int[10, ...]");
    ASSERT_TRUE(type_ell);
    ASSERT_EQ(ObjType::Type, type_ell->getType());
    ASSERT_EQ(ObjType::Int, type_ell->m_var_type_fixed);

    ASSERT_TRUE(type_ell->m_dimensions);
    ASSERT_EQ(2, type_ell->m_dimensions->size());
    ASSERT_EQ(10, (*type_ell->m_dimensions)[0].second->GetValueAsInteger());
    ASSERT_EQ(ObjType::Ellipsis, (*type_ell->m_dimensions)[1].second->getType());



    ObjPtr obj_0 = ctx.ExecStr("0");
    ASSERT_TRUE(obj_0);
    ASSERT_EQ(at::ScalarType::Bool, obj_0->m_value.scalar_type());
    ASSERT_EQ(ObjType::Bool, obj_0->getType());
    ASSERT_EQ(ObjType::None, obj_0->m_var_type_fixed);
    ASSERT_STREQ("0", obj_0->GetValueAsString().c_str());

    ObjPtr obj_1 = ctx.ExecStr("1");
    ASSERT_TRUE(obj_1);
    ASSERT_EQ(at::ScalarType::Bool, obj_1->m_value.scalar_type());
    ASSERT_EQ(ObjType::Bool, obj_1->getType());
    ASSERT_EQ(ObjType::None, obj_1->m_var_type_fixed);
    ASSERT_STREQ("1", obj_1->GetValueAsString().c_str());

    ObjPtr obj_2 = ctx.ExecStr("2");
    ASSERT_TRUE(obj_2);
    ASSERT_EQ(at::ScalarType::Char, obj_2->m_value.scalar_type());
    ASSERT_EQ(ObjType::Char, obj_2->getType());
    ASSERT_EQ(ObjType::None, obj_2->m_var_type_fixed);
    ASSERT_STREQ("2", obj_2->GetValueAsString().c_str());

    ObjPtr obj_int = ctx.ExecStr(":Int(0)");
    ASSERT_TRUE(obj_int);
    ASSERT_TRUE(obj_int->is_tensor());
    ASSERT_TRUE(obj_int->is_scalar());
    ASSERT_EQ(at::ScalarType::Int, obj_int->m_value.scalar_type());
    ASSERT_EQ(ObjType::Int, obj_int->getType()) << toString(obj_int->m_var_type_current);
    ASSERT_EQ(ObjType::Int, obj_int->m_var_type_fixed) << toString(obj_int->m_var_type_fixed);

    ASSERT_TRUE(obj_int->m_var_is_init);
    ASSERT_STREQ("0", obj_int->GetValueAsString().c_str());
    ASSERT_EQ(0, obj_int->GetValueAsInteger());


    ObjPtr scalar = ctx.ExecStr(":Int[0](0)");
    ASSERT_TRUE(scalar);
    ASSERT_TRUE(scalar->is_tensor());
    ASSERT_TRUE(scalar->is_scalar());
    ASSERT_EQ(at::ScalarType::Int, scalar->m_value.scalar_type());
    ASSERT_EQ(ObjType::Int, scalar->getType()) << toString(scalar->m_var_type_current);
    ASSERT_EQ(ObjType::Int, scalar->m_var_type_fixed) << toString(scalar->m_var_type_fixed);

    ASSERT_TRUE(scalar->m_var_is_init);
    ASSERT_STREQ("0", scalar->GetValueAsString().c_str());
    ASSERT_EQ(0, scalar->GetValueAsInteger());


    ObjPtr ten = ctx.ExecStr("[0,]");
    ASSERT_TRUE(ten);
    ASSERT_TRUE(ten->is_tensor());
    ASSERT_FALSE(ten->is_scalar());

    ASSERT_TRUE(ten->m_var_is_init);
    ASSERT_STREQ("[0,]:Bool", ten->GetValueAsString().c_str());
    ASSERT_EQ(0, ten->GetValueAsInteger());



    ObjPtr obj_ten = ctx.ExecStr(":Int([0,])");
    ASSERT_TRUE(obj_ten);
    ASSERT_TRUE(obj_ten->is_tensor());
    ASSERT_FALSE(obj_ten->is_scalar());
    ASSERT_EQ(at::ScalarType::Int, obj_ten->m_value.scalar_type());
    ASSERT_EQ(ObjType::Int, obj_ten->getType()) << toString(obj_ten->m_var_type_current);
    ASSERT_EQ(ObjType::Int, obj_ten->m_var_type_fixed) << toString(obj_ten->m_var_type_fixed);

    ASSERT_TRUE(obj_ten->m_var_is_init);
    ASSERT_STREQ("[0,]:Int", obj_ten->GetValueAsString().c_str());
    ASSERT_EQ(0, obj_ten->GetValueAsInteger());


    ObjPtr obj_auto = ctx.ExecStr(":Int(0, 1, 2, 3)");
    ASSERT_TRUE(obj_auto);
    ASSERT_EQ(at::ScalarType::Int, obj_auto->m_value.scalar_type());
    ASSERT_EQ(ObjType::Int, obj_auto->getType()) << toString(obj_auto->m_var_type_current);
    ASSERT_EQ(ObjType::Int, obj_auto->m_var_type_fixed) << toString(obj_auto->m_var_type_fixed);

    ASSERT_TRUE(obj_auto->m_var_is_init);
    ASSERT_STREQ("[0, 1, 2, 3,]:Int", obj_auto->GetValueAsString().c_str());


    ObjPtr obj_float = ctx.ExecStr(":Float[2,2](3, 4, 1, 2)");
    ASSERT_TRUE(obj_float);
    ASSERT_EQ(ObjType::Float, obj_float->getType()) << toString(obj_float->m_var_type_current);
    ASSERT_EQ(ObjType::Float, obj_float->m_var_type_fixed) << toString(obj_float->m_var_type_fixed);

    ASSERT_TRUE(obj_float->m_var_is_init);
    ASSERT_STREQ("[\n  [3, 4,], [1, 2,],\n]:Float", obj_float->GetValueAsString().c_str());





    ObjPtr frac_0 = ctx.ExecStr("0\\1");
    ASSERT_TRUE(frac_0);
    ASSERT_EQ(ObjType::Fraction, frac_0->getType());
    ASSERT_EQ(ObjType::None, frac_0->m_var_type_fixed);
    ASSERT_STREQ("0\\1", frac_0->GetValueAsString().c_str());

    ObjPtr frac_1 = ctx.ExecStr("1\\1");
    ASSERT_TRUE(frac_1);
    ASSERT_EQ(ObjType::Fraction, frac_1->getType());
    ASSERT_EQ(ObjType::None, frac_1->m_var_type_fixed);
    ASSERT_STREQ("1\\1", frac_1->GetValueAsString().c_str());

    ObjPtr frac_2 = ctx.ExecStr("222_222_222_222222_222_222_222\\1_1_1_1");
    ASSERT_TRUE(frac_2);
    ASSERT_EQ(ObjType::Fraction, frac_2->getType());
    ASSERT_EQ(ObjType::None, frac_2->m_var_type_fixed);
    ASSERT_STREQ("222222222222222222222222\\1111", frac_2->GetValueAsString().c_str());

    ObjPtr obj_frac = ctx.ExecStr(":Fraction(0)");
    ASSERT_TRUE(obj_frac);
    ASSERT_FALSE(obj_frac->is_tensor());
    ASSERT_FALSE(obj_frac->is_scalar());
    ASSERT_EQ(ObjType::Fraction, obj_frac->getType()) << toString(obj_frac->m_var_type_current);

    ASSERT_TRUE(obj_frac->m_var_is_init);
    ASSERT_STREQ("0\\1", obj_frac->GetValueAsString().c_str());
    ASSERT_EQ(0, obj_frac->GetValueAsInteger());



    //    EXPECT_TRUE(dlsym(nullptr, "_ZN7newlang4CharEPKNS_7ContextERKNS_6ObjectE"));
    //    EXPECT_TRUE(dlsym(nullptr, "_ZN7newlang6Short_EPNS_7ContextERNS_6ObjectE"));
}

TEST(Eval, Macros) {

    Context::Reset();
    Context ctx(RunTime::Init());

    ASSERT_EQ(0, Context::m_macros.size());
    ObjPtr none = ctx.ExecStr("\\\\macro\\\\\\");

    ASSERT_EQ(1, Context::m_macros.size());
    none = ctx.ExecStr("\\\\macro2 2\\\\\\");
    ASSERT_EQ(2, Context::m_macros.size());

    none = ctx.ExecStr("\\\\macro3() 3\\\\\\");
    ASSERT_EQ(3, Context::m_macros.size());

    none = ctx.ExecStr("\\\\macro4(arg) \\$arg\\\\\\");
    ASSERT_EQ(4, Context::m_macros.size());


    ObjPtr result = ctx.ExecStr("\\macro");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_none_type());

    result = ctx.ExecStr("\\macro2");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(2, result->GetValueAsInteger());

    result = ctx.ExecStr("\\macro3()");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(3, result->GetValueAsInteger());

    result = ctx.ExecStr("\\macro4(999)");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(999, result->GetValueAsInteger());

    result = ctx.ExecStr("\\macro4(999);\\macro;\\macro4(42)");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(42, result->GetValueAsInteger());

}

TEST(Eval, MacroDSL) {

    Context::Reset();
    Context ctx(RunTime::Init());

    /*
     * Для следующего релиза:
     * - Макросы
     * - Синтаксис циклов к единому виду
     * - Обработка в циклах вовзврата, ошибок, break и continue
     * - Примеры программ в обычном виде
     * - Сборка nlc под Windows и выпуск в виде бинарника
     * 
     * - Расширение методов Obj за счет вызовов torch
     * - Примеры программ для тензорных вычислений
     * 
     */

    const char * dsl = ""
            "\\\\if(cond)       [\\$cond]-->\\\\\\"
            "\\\\elif(cond)     ,[\\$cond]-->\\\\\\"
            "\\\\else           ,[_]-->\\\\\\"
            ""
            "\\\\while(cond)    [\\$cond]<<-->>\\\\\\"
            "\\\\dowhile(cond)  <<-->>[\\$cond]\\\\\\"
            ""
            "\\\\break      --:Break--\\\\\\"
            "\\\\continue   --:Continue--\\\\\\"
            ""
            "\\\\return         --\\\\\\"
            "\\\\return(...)    --\\$*--\\\\\\"
            "\\\\error(...)    --:Error(\\$*)--\\\\\\"
            ""
            "\\\\true 1\\\\\\"
            "\\\\yes 1\\\\\\"
            "\\\\false 0\\\\\\"
            "\\\\no 0\\\\\\"
            ""
            "\\\\this $0\\\\\\"
            ""
            //            "\\\\iquery(...) \\$var?(\\$*)\\\\\\"
            //            "\\\\inext(var) \\$var!\\\\\\"
            //            "\\\\inext(var, count) \\$var!(\\$count)\\\\\\"
            //            "\\ireset(var) \\$var??\\\\\\"
            //            "\\iselect(var) \\$var?!\\\\\\"
            //            "\\iselect(var, ...) \\$var?!(\\$*)\\\\\\"
            "";

    ASSERT_EQ(0, Context::m_macros.size());
    ObjPtr none = ctx.ExecStr(dsl);
    ASSERT_TRUE(Context::m_macros.size() > 10);

    ObjPtr count = ctx.ExecStr("@count:=0;");
    ASSERT_TRUE(count);
    ASSERT_EQ(0, count->GetValueAsInteger());

    const char * run_raw = ""
            "count:=5;"
            "[count<10]<<-->>{{"
            "  [count>5]-->{"
            "    --100--;"
            "  }; "
            "  count+=1;"
            "}};"
            ;

    ObjPtr result = ctx.ExecStr(run_raw, nullptr, true);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(6, count->GetValueAsInteger());
    ASSERT_EQ(100, result->GetValueAsInteger());

    const char * run_macro = ""
            "count:=5;"
            "\\while(count<10){{"
            "  \\if(count>5){"
            "    \\return(42);"
            "  };"
            "  count+=1;"
            "}};"
            "";



    result = ctx.ExecStr(run_macro, nullptr, true);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_integer());
    ASSERT_EQ(6, count->GetValueAsInteger());
    ASSERT_EQ(42, result->GetValueAsInteger());
}

TEST(Eval, Iterator) {

    Context::Reset();
    Context ctx(RunTime::Init());

    ObjPtr dict = ctx.ExecStr("dict := ('1'=1, \"22\"=2, '333'=3, 4, \"555\"=5,)");
    ASSERT_TRUE(dict);
    ASSERT_EQ(5, dict->size());
    ASSERT_EQ(1, dict->at(0).second->GetValueAsInteger());
    ASSERT_EQ(2, dict->at(1).second->GetValueAsInteger());
    ASSERT_EQ(3, dict->at(2).second->GetValueAsInteger());
    ASSERT_EQ(4, dict->at(3).second->GetValueAsInteger());
    ASSERT_EQ(5, dict->at(4).second->GetValueAsInteger());
    ASSERT_STREQ("1", dict->at(0).first.c_str());
    ASSERT_STREQ("22", dict->at(1).first.c_str());
    ASSERT_STREQ("333", dict->at(2).first.c_str());
    ASSERT_STREQ("", dict->at(3).first.c_str());
    ASSERT_STREQ("555", dict->at(4).first.c_str());

    ObjPtr dict1 = ctx.ExecStr("dict?!");
    ASSERT_TRUE(dict1);
    ASSERT_EQ(5, dict1->size());
    ASSERT_EQ(1, dict1->at(0).second->GetValueAsInteger());
    ASSERT_EQ(2, dict1->at(1).second->GetValueAsInteger());
    ASSERT_EQ(3, dict1->at(2).second->GetValueAsInteger());
    ASSERT_EQ(4, dict1->at(3).second->GetValueAsInteger());
    ASSERT_EQ(5, dict1->at(4).second->GetValueAsInteger());
    ASSERT_STREQ("1", dict1->at(0).first.c_str());
    ASSERT_STREQ("22", dict1->at(1).first.c_str());
    ASSERT_STREQ("333", dict1->at(2).first.c_str());
    ASSERT_STREQ("", dict1->at(3).first.c_str());
    ASSERT_STREQ("555", dict1->at(4).first.c_str());


    ObjPtr iter = ctx.ExecStr("iter := dict?");

    ASSERT_TRUE(iter);
    ASSERT_EQ(ObjType::Iterator, iter->getType()) << toString(iter->getType());
    ASSERT_TRUE(iter->m_iterator);

    ASSERT_TRUE(iter->m_iterator->begin() != iter->m_iterator->end());
    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->begin());

    ObjPtr one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(1, dict->at(0).second->GetValueAsInteger());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(2, one->GetValueAsInteger());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(3, one->GetValueAsInteger());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(4, one->GetValueAsInteger());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(5, one->GetValueAsInteger());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(ObjType::IteratorEnd, one->getType()) << one << " " << toString(one->getType());

    one = ctx.ExecStr("iter!");
    ASSERT_TRUE(one);
    ASSERT_EQ(ObjType::IteratorEnd, one->getType()) << one << " " << toString(one->getType());


    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->end());
    ctx.ExecStr("iter??");
    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->begin());

    dict1 = ctx.ExecStr("iter!?(-3)");
    ASSERT_TRUE(dict1);
    ASSERT_EQ(3, dict1->size());
    ASSERT_EQ(1, dict1->at(0).second->GetValueAsInteger());
    ASSERT_EQ(2, dict1->at(1).second->GetValueAsInteger());
    ASSERT_EQ(3, dict1->at(2).second->GetValueAsInteger());

    ObjPtr dict2 = ctx.ExecStr("iter!?(-3)");
    ASSERT_TRUE(dict2);
    ASSERT_EQ(3, dict2->size());
    ASSERT_EQ(4, dict2->at(0).second->GetValueAsInteger());
    ASSERT_EQ(5, dict2->at(1).second->GetValueAsInteger());
    ASSERT_EQ(ObjType::IteratorEnd, dict2->at(2).second->getType());

    ObjPtr dict3 = ctx.ExecStr("iter!?(-3)");
    ASSERT_TRUE(dict3);
    ASSERT_EQ(3, dict1->size());
    ASSERT_EQ(ObjType::IteratorEnd, dict3->at(0).second->getType());
    ASSERT_EQ(ObjType::IteratorEnd, dict3->at(1).second->getType());
    ASSERT_EQ(ObjType::IteratorEnd, dict3->at(2).second->getType());



    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->end());
    ctx.ExecStr("iter??");
    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->begin());

    dict1 = ctx.ExecStr("iter!?(3)");
    ASSERT_TRUE(dict1);
    ASSERT_EQ(3, dict1->size());
    ASSERT_EQ(1, dict1->at(0).second->GetValueAsInteger());
    ASSERT_EQ(2, dict1->at(1).second->GetValueAsInteger());
    ASSERT_EQ(3, dict1->at(2).second->GetValueAsInteger());

    dict2 = ctx.ExecStr("iter!?(3)");
    ASSERT_TRUE(dict2);
    ASSERT_EQ(2, dict2->size());
    ASSERT_EQ(4, dict2->at(0).second->GetValueAsInteger());
    ASSERT_EQ(5, dict2->at(1).second->GetValueAsInteger());

    dict3 = ctx.ExecStr("iter!?(3)");
    ASSERT_TRUE(dict3);
    ASSERT_EQ(0, dict3->size());



    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->end());
    ctx.ExecStr("iter??");
    ASSERT_TRUE(*(iter->m_iterator) == iter->m_iterator->begin());

    ObjPtr flt_res = ctx.ExecStr("dict!?('')");
    ASSERT_TRUE(flt_res);
    ASSERT_EQ(1, flt_res->size());
    ASSERT_EQ(4, flt_res->at(0).second->GetValueAsInteger());


    ObjPtr flt1_res = ctx.ExecStr("dict!?('.',100)");
    ASSERT_TRUE(flt1_res);
    ASSERT_EQ(1, flt1_res->size());
    ASSERT_EQ(1, flt1_res->at(0).second->GetValueAsInteger());


    ObjPtr flt2_res = ctx.ExecStr("dict!?('..',100)");
    ASSERT_TRUE(flt2_res);
    ASSERT_EQ(1, flt2_res->size());
    ASSERT_EQ(2, flt2_res->at(0).second->GetValueAsInteger());

    ObjPtr flt3_res = ctx.ExecStr("dict!?('...',100)");
    ASSERT_TRUE(flt3_res);
    ASSERT_EQ(2, flt3_res->size());
    ASSERT_EQ(3, flt3_res->at(0).second->GetValueAsInteger());
    ASSERT_EQ(5, flt3_res->at(1).second->GetValueAsInteger());

}

//TEST(Eval, Brother) {
//    /*
//     * 
//     * :Human := :Class(пол:Пол=_, родители = (,));
//     *  #!./dist/Debug/GNU-Linux/nlc --exec
//     * :Sex := :Enum(male, female); 
//     * :Human := :Class(sex:Sex=, parent=(,));
//     * @Tom := :Human(Sex.male);
//     * @Janna := :Human(Sex.female);
//     * @Jake := :Human(Sex.male, (Tom, Janna,));
//     * @Tim := :Human(Sex.male, parent=(Tom,));
//     *
//     * Brother(h1, h2) := $h1 != $h2, $h1.sex==male, $h1.parent * $h2.parent;
//     * printf := :Native("printf(format:FmtChar, ...):Int"); 
//     * 
//     * h1 := $?;
//     * [ h1 ] <<-->> {
//     *      h2 := $?;
//     *      [ h2 ] <<-->> {
//     *          [ Brother(h1!, h2!) ] --> { 
//     *              printf("%s brother %s", ""(h1), `h2`);
//     *          }
//     *      }
//     * }
//     * 
//     * h1 := \iterator($);
//     * \while( h1 ) {
//     *      h2 := \iterator($);
//     *      \while( h2 } {
//     *          \if( Brother(h1!, h2!) ) { 
//     *              result []= "$1 brother $2"(h1, h2);
//     *          }
//     *      }
//     * }
//     * --result--;
//     * 
//     * >>> ("Tim brother Jake", "Jake brother Tim",)
//     * 
//     */
//
//    Context::Reset();
//    Context ctx(RunTime::Init());
//
//    ObjPtr Sex = ctx.ExecStr(":Sex := :Enum(male=1, 'female')");
//    ASSERT_TRUE(Sex);
//    ASSERT_TRUE(Sex->isConst());
//    ASSERT_EQ(2, Sex->size());
//    ASSERT_STREQ("male", Sex->at(0).first.c_str());
//    ASSERT_EQ(1, Sex->at(0).second->GetValueAsInteger());
//    ASSERT_STREQ("female", Sex->at(1).first.c_str());
//    ASSERT_EQ(2, Sex->at(1).second->GetValueAsInteger());
//
//    ObjPtr Human = ctx.ExecStr(":Human := :Class(sex:Sex=, parent=(,))");
//
//    ASSERT_TRUE(Human);
//    ASSERT_EQ(2, Human->size());
//    ASSERT_STREQ("sex", Human->at(0).first.c_str());
//    ASSERT_TRUE(Human->at(0).second->is_none_type());
//    ASSERT_STREQ("parent", Human->at(1).first.c_str());
//    ASSERT_TRUE(Human->at(1).second->is_dictionary_type());
//    ASSERT_EQ(0, Human->at(1).second->size());
//
//
//    ObjPtr Tom = ctx.ExecStr("Tom := :Human(:Sex.male)");
//    ASSERT_TRUE(Tom);
//    ASSERT_EQ(2, Tom->size());
//    ASSERT_STREQ("sex", Tom->at(0).first.c_str());
////    ASSERT_TRUE(Tom->at(0).second->is_integer());
//    ASSERT_EQ(1, Tom->at(0).second->GetValueAsInteger());
//    ASSERT_STREQ("parent", Tom->at(1).first.c_str());
//    ASSERT_TRUE(Tom->at(1).second->is_dictionary_type());
//    ASSERT_EQ(0, Tom->at(1).second->size());
//
//    ObjPtr Janna = ctx.ExecStr("Janna := :Human(:Sex.female)");
//    ASSERT_TRUE(Janna);
//    ASSERT_EQ(2, Janna->size());
//    ASSERT_STREQ("sex", Janna->at(0).first.c_str());
//    ASSERT_TRUE(Janna->at(0).second->is_integer());
//    ASSERT_EQ(2, Janna->at(0).second->GetValueAsInteger());
//    ASSERT_STREQ("parent", Janna->at(1).first.c_str());
//    ASSERT_TRUE(Janna->at(1).second->is_dictionary_type());
//    ASSERT_EQ(0, Janna->at(1).second->size());
//
//
//    ObjPtr Jake = ctx.ExecStr("Jake := :Human(:Sex.male, (Tom, Janna,))");
//    ASSERT_TRUE(Jake);
//    ASSERT_EQ(2, Jake->size());
//    ASSERT_STREQ("sex", Jake->at(0).first.c_str());
//    ASSERT_TRUE(Jake->at(0).second->is_integer());
//    ASSERT_EQ(1, Jake->at(0).second->GetValueAsInteger());
//    ASSERT_STREQ("parent", Jake->at(1).first.c_str());
//    ASSERT_TRUE(Jake->at(1).second->is_dictionary_type());
//    ASSERT_EQ(2, Jake->at(1).second->size());
//
//    ObjPtr Tim = ctx.ExecStr("Tim := :Human(:Sex.male, (Tom,))");
//    ASSERT_TRUE(Tim);
//    ASSERT_EQ(2, Tim->size());
//    ASSERT_STREQ("sex", Tim->at(0).first.c_str());
//    ASSERT_TRUE(Tim->at(0).second->is_integer());
//    ASSERT_EQ(1, Tim->at(0).second->GetValueAsInteger());
//    ASSERT_STREQ("parent", Tim->at(1).first.c_str());
//    ASSERT_TRUE(Tim->at(1).second->is_dictionary_type());
//    ASSERT_EQ(1, Tim->at(1).second->size());
//
//
//
//
//}

class OpEvalTest : public ::testing::Test {
protected:
    Context m_ctx;
    ObjPtr m_result;
    std::string m_string;

    OpEvalTest() : m_ctx(RunTime::Init()) {
    }

    const char *Test(std::string eval, Obj *vars) {
        eval += ";";
        m_result = m_ctx.ExecStr(eval, vars);
        if(m_result) {
            m_string = m_result->GetValueAsString();
            return m_string.c_str();
        }
        std::cout << "Fail parsing: '" << eval << "'\n";
        ADD_FAILURE();

        return nullptr;
    }

    const char *Test(const char *eval) {
        Obj vars;

        return Test(eval, &vars);
    }
};

TEST_F(OpEvalTest, Ops) {
    ASSERT_STREQ("10", Test("10"));
    ASSERT_STREQ("32", Test("10+22"));
    ASSERT_STREQ("5.1", Test("1.1+4"));
    ASSERT_STREQ("5.5", Test("1+4.5"));

    ASSERT_STREQ("10\\1", Test("10\\1"));
    ASSERT_STREQ("32\\1", Test("10\\1+22\\1"));
    ASSERT_STREQ("5\\1", Test("4\\5 + 42\\10"));
    ASSERT_STREQ("11\\1", Test("10\\1 + 1"));
    ASSERT_STREQ("4\\3", Test("1\\3 + 1"));

    ASSERT_STREQ("-12", Test("10 - 22"));
    ASSERT_STREQ("-2.9", Test("1.1 - 4"));
    ASSERT_STREQ("-3.5", Test("1 - 4.5"));
    ASSERT_STREQ("-17\\5", Test("4\\5 - 42\\10"));
    ASSERT_STREQ("-9\\10", Test("1\\10 - 1"));
    ASSERT_STREQ("-2\\3", Test("1\\3 - 1"));

    ASSERT_STREQ("66", Test("2 * 33"));
    ASSERT_STREQ("-5.5", Test("1.1 * -5"));
    ASSERT_STREQ("180", Test("10 * 18"));
    ASSERT_STREQ("66\\1", Test("2\\1 * 66\\2"));
    ASSERT_STREQ("-15\\1", Test("3\\1 * -5"));
    ASSERT_STREQ("9\\5", Test("18\\100 * 10"));

    ASSERT_STREQ("5", Test("10/2"));
    ASSERT_STREQ("5.05", Test("10.1 / 2"));
    ASSERT_STREQ("0.1", Test("1 / 10"));
    ASSERT_STREQ("0.1", Test("1.0 / 10"));

    ASSERT_STREQ("4\\3", Test("12\\3 / 3"));
    ASSERT_STREQ("1\\1", Test("5\\10 / 1\\2"));
    ASSERT_STREQ("1\\100", Test("1\\10 / 10"));

    ASSERT_STREQ("5", Test("10//2"));
    ASSERT_STREQ("5", Test("10.0 // 2"));
    ASSERT_STREQ("0", Test("1 // 10"));
    ASSERT_STREQ("-3", Test("-25 // 10"));
    ASSERT_STREQ("-3", Test("-30 // 10"));
    ASSERT_STREQ("-4", Test("-31 // 10"));

    //    ASSERT_STREQ("100", Test("2*20+10*5"));

    //    ASSERT_STREQ("1", Test("1 ** 1"));
    //    ASSERT_STREQ("4", Test("2 ** 2"));
    //    ASSERT_STREQ("-8.0", Test("-2.0 ** 3"));

    ASSERT_STREQ("", Test("\"\""));
    ASSERT_STREQ(" ", Test("\" \""));
    ASSERT_STREQ("строка", Test("\"\"++\"строка\" "));
    ASSERT_STREQ("строка 222", Test("\"строка \" ++ \"222\" "));
    ASSERT_STREQ("строка строка строка ", Test("\"строка \" ** 3 "));

    ASSERT_STREQ("100", Test("var1:=100"));
    ObjPtr var1 = m_result;
    ASSERT_TRUE(var1);
    ASSERT_STREQ("$=('var1',)", Test("$"));
    ASSERT_STREQ("100", Test("var1"));

    ObjPtr vars = Obj::CreateDict(Obj::Arg(var1, "var1"));

    ASSERT_ANY_THROW(Test("$var1"));
    ASSERT_NO_THROW(Test("$var1", vars.get()));
    ASSERT_STREQ("100", Test("$var1", vars.get()));

    ASSERT_STREQ("20", Test("var2:=9+11"));
    ObjPtr var2 = m_result;
    ASSERT_TRUE(var2);
    ASSERT_STREQ("$=('var1', 'var2',)", Test("$"));
    ASSERT_STREQ("20", Test("var2"));

    ASSERT_ANY_THROW(Test("$var2"));
    ASSERT_ANY_THROW(Test("$var2", vars.get()));
    vars->push_back(Obj::Arg(var2, "var2"));

    ASSERT_NO_THROW(Test("$var2", vars.get()));
    ASSERT_STREQ("20", Test("$var2", vars.get()));

    ASSERT_STREQ("100", Test("var1"));
    ASSERT_STREQ("120", Test("var1+=var2"));
    ASSERT_STREQ("$=('var1', 'var2',)", Test("$"));

    ASSERT_ANY_THROW(Test("$var1"));
    ASSERT_NO_THROW(Test("$var1", vars.get()));
    ASSERT_STREQ("120", Test("$var1", vars.get()));

    vars->clear_();
    m_result.reset();
    var1.reset();
    ASSERT_STREQ("$=('var2',)", Test("$"));
    var2.reset();
    ASSERT_STREQ("$=(,)", Test("$"));
}

TEST(EvalOp, InstanceName) {

    /*
     * Проверка имени типа «~» — немного похож на оператор instanceof в Java. Левым оператором должен быть проверяемый объект, 
     * а правым оператором — тип, строка литерал или объект строкового типа в котором содержится символьное назначение типа. 
     * Результатом операции будет истина, если правый операнд содержит название типа проверяемого объекта или 
     * он присутствует в иерархии наследования у правого операнда.
     * 
     * var ~ :class; 
     * var ~ "class";
     * name := "TypeName"; # Строка с именем типа
     * var ~ name; 
     * 
     * Утиная типизация «~~» — приблизительный аналог функции isinstance() в Python, который для простых 
     * типов сравнивает совместимость типа левого операнда по отношению к правому, а для словарей и классов 
     * в левом операнде проверяется наличие всех имен полей, присутствующих у правого операнда. т. е.
     * 
     * (field1=«value», field2=2, field3=«33»,) ~~ (); # Истина (т. е. левый операнд словарь)
     * (field1=«value», field2=2, field3=«33»,) ~~ (field1=_); # Тоже истина (т. к. поле field1 присутствует у левого операнда)
     * 
     * Строгая утиная типизация «~~~» — для простых типов сравнивается идентичности типов без учета совместимости, 
     * а для составных типов происходит строгое сравнение всех свойств. 
     * Для данной операции, пустой тип совместим только с другим пустим типом!
     */

    RuntimePtr opts = RunTime::Init();
    Context ctx(opts);

    /*
     * Реализация системы типов сделана следующим образом:
     * Каждый объект содержит одно из перечисления ObjType + символьное наименование класса
     * 
     *      
     */

    ObjPtr obj_bool = Obj::CreateBool(true);
    ObjPtr obj_char = Obj::CreateValue(20); // ObjType::Char
    ObjPtr obj_short = Obj::CreateValue(300); // ObjType::Short
    ObjPtr obj_int = Obj::CreateValue(100000); // ObjType::Int
    ObjPtr obj_long = Obj::CreateValue(999999999999); // ObjType::Long
    ObjPtr str_char = Obj::CreateString("Байтовая строка");
    ObjPtr str_wide = Obj::CreateString(L"Широкая строка");

    ObjPtr obj_none = Obj::CreateNone();
    ObjPtr obj_dict = Obj::CreateDict();
    ObjPtr obj_range = Obj::CreateRange(0, 20, 2);
    ObjPtr obj_class1 = Obj::CreateClass(":Class1");
    ObjPtr obj_class2 = Obj::CreateClass(":Class2");
    obj_class2->m_class_parents.push_back(obj_class1);

    ASSERT_TRUE(obj_bool->op_class_test(":Bool", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Char", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_bool->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_bool->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_bool->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_bool->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_bool->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_bool->op_class_test(":None", &ctx));

    ASSERT_FALSE(obj_char->op_class_test(":Bool", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Char", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_char->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_char->op_class_test(":None", &ctx));

    ASSERT_FALSE(obj_short->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_short->op_class_test(":Char", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_short->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_short->op_class_test(":None", &ctx));

    ASSERT_FALSE(obj_int->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_int->op_class_test(":Char", &ctx));
    ASSERT_FALSE(obj_int->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_int->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_int->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_int->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_int->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_int->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_int->op_class_test(":None", &ctx));


    ASSERT_FALSE(obj_long->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_long->op_class_test(":Char", &ctx));
    ASSERT_FALSE(obj_long->op_class_test(":Short", &ctx));
    ASSERT_EQ(obj_long->m_var_type_current, ObjType::Long);
    ASSERT_FALSE(obj_long->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_long->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_long->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_long->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_long->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_long->op_class_test(":None", &ctx));

    ASSERT_FALSE(obj_bool->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_short->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_int->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_long->op_class_test(":Class", &ctx));
    ASSERT_FALSE(str_char->op_class_test(":Class", &ctx));
    ASSERT_FALSE(str_wide->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_range->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_dict->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_none->op_class_test(":Class", &ctx));

    ASSERT_TRUE(str_char->op_class_test(":String", &ctx));
    ASSERT_TRUE(str_wide->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_range->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_dict->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_none->op_class_test(":String", &ctx));

    ASSERT_TRUE(str_char->op_class_test(":StrChar", &ctx));
    ASSERT_FALSE(str_wide->op_class_test(":StrChar", &ctx));
    ASSERT_TRUE(obj_range->op_class_test(":Other", &ctx));
    ASSERT_FALSE(obj_dict->op_class_test(":Other", &ctx));
    ASSERT_FALSE(obj_none->op_class_test(":Other", &ctx));

    ASSERT_FALSE(str_char->op_class_test(":StrWide", &ctx));
    ASSERT_TRUE(str_wide->op_class_test(":StrWide", &ctx));
    ASSERT_TRUE(obj_range->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_dict->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_none->op_class_test(":Range", &ctx));


    ASSERT_TRUE(obj_class1->op_class_test(":Class", &ctx));
    ASSERT_TRUE(obj_class1->op_class_test(":Class1", &ctx));
    ASSERT_FALSE(obj_class1->op_class_test(":Class2", &ctx));
    ASSERT_TRUE(obj_class2->op_class_test(":Class", &ctx));
    ASSERT_TRUE(obj_class2->op_class_test(":Class1", &ctx));
    ASSERT_TRUE(obj_class2->op_class_test(":Class2", &ctx));



    // [,]:Bool ~ :Int
    ObjPtr obj_empty_bool = ctx.ExecStr("[,]:Bool");
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Bool", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Char", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_empty_bool->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_empty_bool->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_empty_bool->op_class_test(":StrChar", &ctx));
    ASSERT_FALSE(obj_empty_bool->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_empty_bool->op_class_test(":None", &ctx));


    // [,]:Int ~ :Bool
    ObjPtr obj_empty_int = ctx.ExecStr("[,]:Int");
    ASSERT_FALSE(obj_empty_int->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":Char", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":Short", &ctx));
    ASSERT_TRUE(obj_empty_int->op_class_test(":Int", &ctx));
    ASSERT_TRUE(obj_empty_int->op_class_test(":Long", &ctx));
    ASSERT_TRUE(obj_empty_int->op_class_test(":Tensor", &ctx));
    ASSERT_TRUE(obj_empty_int->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_empty_int->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":StrChar", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_empty_int->op_class_test(":None", &ctx));


    // (,):Class ~ :Class
    ObjPtr obj_class = ctx.ExecStr("(,):Class");
    ASSERT_FALSE(obj_class->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":Char", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":Tensor", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_class->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":StrChar", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":None", &ctx));

    ASSERT_TRUE(obj_class->op_class_test(":Class", &ctx));
    ASSERT_FALSE(obj_class->op_class_test(":ClassOther", &ctx));

    ObjPtr obj_class3 = ctx.ExecStr("(,):Class2");
    obj_class3->m_class_parents.push_back(obj_class);
    ASSERT_FALSE(obj_class3->op_class_test(":Bool", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":Char", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":Tensor", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":Arithmetic", &ctx));
    ASSERT_TRUE(obj_class3->op_class_test(":Any", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":Range", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":StrChar", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":String", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":None", &ctx));

    ASSERT_TRUE(obj_class3->op_class_test(":Class2", &ctx));
    ASSERT_FALSE(obj_class3->op_class_test(":ClassOther", &ctx));




    ObjPtr res = ctx.ExecStr(":Int ~ :Int");
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->is_bool_type());
    ASSERT_TRUE(res->GetValueAsBoolean());

    res = ctx.ExecStr("1 ~ :Bool");
    ASSERT_TRUE(res);
    ASSERT_TRUE(res->is_bool_type());
    ASSERT_TRUE(res->GetValueAsBoolean());

    ASSERT_NO_THROW(ctx.ExecStr("1.0 ~ :FAIL_TYPE"));

    std::map<const char *, bool> test_name = {
        {"0 ~ :Int", true},
        {"1 ~ :Int", true},
        {"1 ~ :Bool", true},
        {"1 ~ :Integer", true},
        {"1 ~ :Number", true},
        {"1 ~ :Double", true},
        {"10 ~ :Bool", false},
        {"10 !~ :Bool", true},
        {"1 ~ :Float", true},
        {"1.0 ~ :Double", true},
        {"1.0 ~ :Integer", false},
        {"1.0 ~ :FAIL_TYPE", false},

        {"'' ~ :StrChar", true},
        {"\"\" ~ :StrWide", true},
        {"'' ~ :String", true},
        {"\"\" ~ :String", true},
        {"\"\" ~ :Int", false},
        {"\"\" ~ :None", false},

        {":Int ~ :Int", true},
        {":Int ~ :Bool", false},

        {"[,]:Int ~ :Bool", false},
        {"[,]:Bool ~ :Int", true},

        {"(,):Class ~ :Class", true},
        {"(,):ClassNameNotFound ~ :Class", false},
        {"(,):ClassNameNotFound !~ :Class", true},
        {"(,):ClassNameNotFound ~ :ClassNameNotFound", true},
        {"(,):ClassNameNotFound !~ :ClassNameNotFound", false},
    };

    for (auto &elem : test_name) {
        res.reset();
        ASSERT_NO_THROW(res = ctx.ExecStr(elem.first, nullptr, false)) << elem.first;
        EXPECT_TRUE(res) << elem.first;
        if(res) {
            EXPECT_TRUE(res->is_bool_type()) << elem.first;
            if(elem.second) {
                EXPECT_TRUE(res->GetValueAsBoolean()) << elem.first;
            } else {
                EXPECT_FALSE(res->GetValueAsBoolean()) << elem.first;
            }
        }
    }

}

#endif // UNITTEST