
%{ /*** C/C++ Declarations ***/

#include "pch.h"

#include <term.h>
#include <context.h>

#include "parser.h"
#include "lexer.h"

/* this "connects" the bison parser in the driver to the flex scanner class
 * object. it defines the yylex() function call to pull the next token from the
 * current lexer object of the driver context. */
#undef yylex
#define yylex driver.NewToken
    //driver.lexer->lex

#define YYDEBUG 1
    
%}

/*** yacc/bison Declarations ***/

%require "3.6"

/* add debug output code to generated parser. disable this for release
 * versions. */
%debug 

/* start symbol is named "start" */
%start start

/* write out a header file containing the token defines */
%defines

//%no-lines

/* use newer C++ skeleton file */
%skeleton "lalr1.cc"

/* namespace to enclose parser in */
%define api.prefix {newlang}

/* verbose error messages */
/* %define parse.error verbose */
%define parse.error detailed

/* set the parser's class identifier */

/* keep track of the current position within the input */
%locations
%initial-action
{
    // initialize the initial location object
    @$.begin.filename = @$.end.filename = &driver.streamname;
//    yydebug_ = 1;
};

/* The driver is passed by reference to the parser and to the scanner. This
 * provides a simple but effective pure interface, not relying on global
 * variables. */
%parse-param { class Parser& driver }


/*** BEGIN EXAMPLE - Change the example grammar's tokens below ***/
%define api.value.type {TermPtr}

/*
 * Проблема перегоузки функций разными типами аргументов специфична только для компилируемых языков со статической типизацией
 * Для языков с динамической типизацией, перегразука функций не требуется, т.к. типы аргументов могут быть произвольными
 * Но перегрузка фунций в языке со статической типизацие может еще использоваться и для замены одной реализациина функции 
 * на другую, если типы аргументов различаются, что эквивалентно замене (добавлению) новой функции для другого типа аргументов.
 * 
 * Как сделать замену одной реализации фунции на другую для языка с динамиеческой типизацией без перегрузки функций?
 * 1. Сохранять указатель на предыдущую функцию в новой реализации, тогда  нужны локальные статические переменные и/или деструкторы.
 * 2. Управлять именами функций средствами языка (не нужны локальные статические переменные и деструкторы, 
 * но нужна лексическая контструкция чтобы обращаться к предыдущей реализации (что эквавалентно досутпу к родительсокму классу),
 * а весь список функций можно вытащить итератором)
 * 
 * Связанный вопрос - пересечения имен у переменный и функций и их уникальность в одной области видимости.
 * Таготею к подходу в Эсперанто, где по струкутре слова можно понять часть речи и нет двойных смыслов
 * 
 * Для глобальных объектов - имена уникальны, но есть возможность добавлять несколько варинатов реализации одного и того же термина 
 * (новый термин заменяет старый, но в новом термине остается возможност вызывать предыдущий вариант реализации).
 * 
 * Для локальных объектов - имена <перекрываются>, т.е. объекты не "заменяются", а "перекрываются".
 * Это получается из-за того, что к локальным объектам кроме имени можно обратить по <индексу>, а к глобальным только по имени.
 * Локальный объект добаляется в начало списка, а его имя ищется последовательно (потом можно будет прикрутить хешмап)
 * 
 * 
 * все метод классов, кроме статических - вирутальные (могут быть переопределены в наследниках)
 * Класс с чистым вирутальным методом (None) создать можно, но вызвать метод нельзя (будет ошибка)
 * Интерфейсы ненужны, т.к. есть утиная типизация
 * 
 * Ссылка на статические поля класса
 * Ссылка на статические методы класса
 * Ссылка на поля класса
 * Ссылка на методы класса
 * Конструкторы
 * Дестурктор
 * 
 * Derived2::*Derived2_mfp
 * 
 * 
 * https://docs.microsoft.com/ru-ru/cpp/build/reference/decorated-names?view=msvc-160
 * Формат внутреннего имени C++
 * Внутреннее имя функции C++ содержит следующие сведения.
 * - Имя функции.
 * - Класс, членом которого является функция, если это функция-член. Это может быть класс, в который входит содержащий функцию класс, и т. д.
 * - Пространство имен, которой принадлежит функция, если она входит в пространство имен.
 * - Типы параметров функции.
 * - Соглашение о вызовах.
 * - Тип значения, возвращаемого функцией.
 * Имена функций и классов кодируются во внутреннем имени. Остальная часть внутреннего имени — это код, 
 * который имеет смысл только для компилятора и компоновщика. 
 * 
 * 
 *   :class.var_class (static) := "";
 *   :class.var_class (static) := 1; # публичное поле объекта
 *   :class._var_class (static) := 1; # защиенное поле объекта
 *   :class.__var_class (static) := 1; # приватное поле объекта
 *   :class.__var_class__ (static) := 1; # Системное 
 * 
 *   var := 1; # публичное поле объекта
 *   _var := 1; # защиенное поле объекта
 *   __var := 1; # приватное поле объекта
 *   __var__ := 1; # системное поле
 * 
 */


%token                  INTEGER		"Integer"
%token                  NUMBER		"Float"
%token                  COMPLEX		"Complex"
%token                  RATIONAL	"Rational"
%token           	STRCHAR		"StrChar"
%token           	STRWIDE		"StrWide"
%token           	TEMPLATE	"Template"
%token           	EVAL            "Eval"

%token			SYS_ENV_STR
%token			SYS_ENV_INT
%token			NAME
%token			LOCAL
%token			MODULE
%token			NATIVE
%token			SYMBOL

%token			UNKNOWN		"Token ONLY UNKNOWN"
%token			OPERATOR_DIV
%token			OPERATOR_AND
%token			OPERATOR_PTR
%token			OPERATOR_ANGLE_EQ

%token			NEWLANG		"@@"
%token			PARENT		"$$"
%token			ARGS		"$*"
%token			ALIAS		"Alias"
%token			MACRO
%token			MACRO_DEF
%token			MACRO_BODY      "Macro body"
%token			MACRO_STR       "Macro str"
%token			MACRO_DEL       "Macro del"


%token			CREATE		"::="
%token			CREATE_OR_ASSIGN ":="
%token			APPEND		"[]="

%token			INT_PLUS
%token			INT_MINUS
%token			INT_REPEAT
%token			TRY_PLUS_BEGIN
%token			TRY_PLUS_END
%token			TRY_MINUS_BEGIN
%token			TRY_MINUS_END
%token			TRY_ALL_BEGIN
%token			TRY_ALL_END
%token                  MACRO_LEVEL_BEGIN
%token                  MACRO_LEVEL_END


%token			FOLLOW
%token			MATCHING
%token			REPEAT

%token			ARGUMENT
%token			MODULES


%token			RANGE           ".."
%token			ELLIPSIS        "..."
%token			NAMESPACE       "::"
%token			ASSIGN_MATH     "Arithmetic assign value"

%token			END         0	"end of file"
%token			COMMENT

%token                  SOURCE
%token			ITERATOR
%token			ITERATOR_QQ
%token			ELSE

%token			PUREFUNC
%token			OPERATOR
%token			DOC_BEFORE
%token			DOC_AFTER


/* Есть предупреждения, parser.y: предупреждение: shift/reduce conflict on token ';' [-Wcounterexamples]
  First example: assign_items '=' assign_expr • ';' doc_after "end of file"
  Second example: assign_items '=' assign_expr • ';' "end of file"
*/
//%expect 2

%% /*** Grammar Rules ***/


/* Незнаю, нужны ли теперь символы? Раньше планировалось с их помощью расширять синтаксис языковых конструкций, 
 * но это относится к парсеру и не может изменяться динамически в зависимости от наличия существующий объектов и определений.
 * Если относится к символам как к идентификаторам на других языках, то опять же это лучше делать на уровне лексера и парсера,
 * чтобы еще при обработке исходников вместо создания неопределнных последовательностьей возникала ошибка времени компиляции,
 * а не передача отдельных символов как не распознанных терминалов.
 */
symbols: SYMBOL
            {
                $$ = $1;
            }
        | symbols  SYMBOL
            {
                $$ = $1; 
                $$->AppendText($2->getText());
            }


ns:     NAME
            {
                $$ = $1;
            }
        | ns  NAMESPACE  NAME
            {
                $$ = $3;
                $$->m_text.insert(0, "::");
                $$->m_text.insert(0, $1->m_text);
            }
            
ns_name:  ns
            {
                $$ = $1;
            }
        | NAMESPACE
            {
                $$ = $1;
            }
        | NAMESPACE  ns
            {
                $$ = $2;
                $$->m_text.insert(0, "::");
            }

local:  '$'
            {
                $$ = $1;
                $$->SetTermID(TermID::LOCAL);
            }
        | LOCAL
            {
                $$ = $1;
            }



name:  ns_name
            {
                $$ = $1;
                $$->TestConst();
            }
        |  local
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
                $$->TestConst();
            }
        |  MODULE
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
                $$->TestConst();
            }
        |  MODULE   NAMESPACE   assign_name
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
                $$->Last()->Append($3);
                $$->TestConst();
            }
        |  NATIVE
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
                $$->TestConst();
            }
        |  PARENT  /* $$ - rval */
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
            }
        |  NEWLANG  /* @@ - rval */
            {
                $$ = $1;
                $$->SetTermID(TermID::NAME);
            }
        | MACRO
            {
                $$ = $1;
            }

/* Фиксированная размерность тензоров для использования в типах данных */
type_dim: INTEGER
        {
            $$ = $1;
        }
    |  ELLIPSIS
        {
            // Произвольное количество элементов
            $$ = $1; 
        }
/*    | TERM  '='  INTEGER
        { // torch поддерживает именованные диапазоны, но пока незнаю, нужны ли они?
            $$ = $3;
            $$->SetName($1->getText());
        } */

type_dims: type_dim
        {
            $$ = $1;
        }
    | type_dims  ','  type_dim
        {
            $$ = $1;
            $$->AppendList($3);
        }

type_class:  ':'  name
            {
                $$ = $2;
                $$->m_text.insert(0, ":");
            }

ptr: '&' 
        {
            $$ = $1;
        }
    | OPERATOR_AND
        {
            $$ = $1;
        }
    | OPERATOR_PTR
        {
            $$ = $1;
        }
    
    
type_name:  type_class
            {
                $$ = $1;
            }
        |  type_class   '['  type_dims   ']'
            {
                $$ = $1;
                Term::ListToVector($type_dims, $$->m_dims);
            }
        | ':'  ptr  NAME
            {
                // Для функций, возвращаюющих ссылки
                $$ = $3;
                $$->m_text.insert(0, ":");
                $$->MakeRef($ptr);
            }
        | ':'  ptr  NAME   '['  type_dims   ']'
            {
                // Для функций, возвращаюющих ссылки
                $$ = $3;
                $$->m_text.insert(0, ":");
                Term::ListToVector($type_dims, $$->m_dims);
                $$->MakeRef($ptr);
            }


type_call: type_name   call
            {
                $$ = $1;
                $$->SetArgs($2);
                $$->SetTermID(TermID::TYPE);
                $$->TestConst();
            }
        
type_item:  type_name
            {
                $$ = $1;
                $$->SetTermID(TermID::TYPE);
                $$->TestConst();
            }
        | type_call
            {
                $$ = $1;
            }
        | ':'  eval
            {
                // Если тип еще не определён и/или его ненужно проверять во время компиляции, то имя типа можно взять в кавычки.
                $$ = $2;
                $$->SetTermID(TermID::TYPENAME);
                $$->m_text.insert(0, ":");
            }

type_items:  type_item
            {
                $$ = $1;
            }
        | type_items   ','   type_item
            {
                $$ = $1;
                $$->AppendList($3);
            }

        
type_list:  ':'  '<'  type_items  '>'
            {
                $$ = $type_items;
            }
        


digits_literal: INTEGER
            {
                $$ = $1;
                $$->SetType(nullptr);
            }
         | NUMBER
            {
                $$ = $1;
                $$->SetType(nullptr);
            }
         | COMPLEX
            {
                $$ = $1;
                $$->SetType(nullptr);
            }
         | RATIONAL
            {
                $$ = $1;
                $$->SetType(nullptr);
            }
        | SYS_ENV_INT
            {
                $$ = Term::GetEnvTerm($1);
            }
        
digits:  digits_literal
            {
                $$ = $1;
            }
        | digits_literal  type_item
            {
                $$ = $1;
                $$->SetType($type_item);
            }

        
        
range_val:  rval_range
        {
            $$ = $1;  
        }
    | '('  arithmetic  ')'
        {
            $$ = $2;
        }
            
        
range: range_val  RANGE  range_val
        {
            $$ = $2;
            $$->push_back($1, "start");
            $$->push_back($3, "stop");
        }
    | range_val  RANGE  range_val  RANGE  range_val
        {
            $$ = $2;
            $$->push_back($1, "start");
            $$->push_back($3, "stop");
            $$->push_back($5, "step");
        }
        
        
        
        
strtype: STRWIDE
        {
            $$ = $1;
            $$->SetType(nullptr);
        }
    | STRCHAR
        {
            $$ = $1;
            $$->SetType(nullptr);
        }

string: strtype
        {
            $$ = $1;
        }
    | SYS_ENV_STR
        {
            $$ = Term::GetEnvTerm($1);
        }
    | strtype  call
        {
            $$ = $1;
            $$->SetArgs($2);
        }
   

doc_before: DOC_BEFORE 
            {
                $$ = $1;
            }    
        | doc_before  DOC_BEFORE 
            {
                $$ = $1;
                $$->AppendList($2);
            }    
    
doc_after: DOC_AFTER
            {
                $$ = $1;
            }    
        | doc_after  DOC_AFTER
            {
                $$ = $1;
                $$->AppendList($2);
            }    

        
arg_name: name 
        {
            $$ = $1;
            $$->TestConst();
        }
    | strtype 
        {
            $$ = $1;
        }
        
/* Допустимые <имена> объеков */
assign_name:  name
                {
                    $$ = $1;
                    $$->TestConst();
                }
            |  symbols
                {
                    $$ = $1;  
                    $$->TestConst();
                }
            |  ns  NAMESPACE  symbols
                {
                    $$ = $2;  
                    $$->m_text.insert(0, "::");
                    $$->m_text.insert(0, $ns->m_text);
                    $$->TestConst();
                }
            | ARGUMENT  /* $123 */
                {
                    $$ = $1;
                }
            
       
/* Допустимые lvalue объекты */
lval:  assign_name
            {
                $$ = $1;
            }
        |  '&'  assign_name
            {
                $$ = $2;
                $$->MakeRef($1);
            }
        |  '&'  '&'  assign_name
            {
                $$ = $3;
                $$->MakeRef($1);
                $$->m_ref->m_text += "&";
            }
        /* Не могут быть ссылками*/
        |  assign_name  '['  args  ']'
            {   
                $$ = $1; 
                $2->SetTermID(TermID::INDEX);
                $2->SetArgs($args);
                $$->Last()->Append($2);
            }
        |  lval  '.'  NAME
            {
                $$ = $1; 
                $3->SetTermID(TermID::FIELD);
                $$->Last()->Append($3);
            }
        |  lval  '.'  NAME  call
            {
                $$ = $1; 
                $3->SetTermID(TermID::FIELD);
                $3->SetArgs($call);
                $$->Last()->Append($3);
            }
        |  lval  '.'  NAME  type_item
            {
                $$ = $1; 
                $3->SetTermID(TermID::FIELD);
                $3->SetType($type_item);
                $$->Last()->Append($3);
            }
        |  lval  '.'  NAME  call  type_item
            {
                $$ = $1; 
                $3->SetTermID(TermID::FIELD);
                $3->SetArgs($call);
                $3->SetType($type_item);
                $$->Last()->Append($3);
            }
        |  lval  '.'  NAME  call  type_list
            {
                $$ = $1; 
                $3->SetTermID(TermID::FIELD);
                $3->SetArgs($call);
                $$->Last()->Append($3);
                Term::ListToVector($type_list, $$->m_type_allowed);
            }
        |  type_item
            {   
                $$ = $type_item; 
            }
        |  type_item  type_item
            {   
                $$ = $1; 
                $$->SetType($2);
            }
        |  name  type_item
            {   
                $$ = $1; 
                $$->SetType($type_item);
            }
        |  name  call
            {   
                $$ = $name; 
                $$->SetArgs($call);
            }
        |  name  call  type_item
            {   
                $$ = $name; 
                $$->SetArgs($call);
                $$->SetType($type_item);
            }
        |  name  call  type_list
            {   
                $$ = $name; 
                $$->SetArgs($call);
                Term::ListToVector($type_list, $$->m_type_allowed);
            }

rval_name: lval
            {
                $$ = $1; 
            }
        | ARGS /* $* и @* - rval */
            {
                $$ = $1;
            }

        
rval_range: rval_name
            {
                $$ = $1;
            }
        | digits
            {
                $$ = $1;
            }
        |  string
            {
                $$ = $1;
            }
            
eval:  EVAL 
        {
            $$ = $1;
        }
    |  EVAL  call
        {
            $$ = $1;
            $$->SetArgs($call);
        }
    
rval_var:  rval_range
            {
                $$ = $1;
            }
        |  collection
            {
                $$ = $1;
            }
        |  range
            {
                $$ = $1;
            }
        |  eval 
            {   
                $$ = $1;
            }
        
        
        
rval:   rval_var
            {
                $$ = $1;
            }
        |  assign_arg
            {
                $$ = $1;
            }


iter:  '?'
            {
                $$=$1;
                $$->SetTermID(TermID::ITERATOR);
            }
        | '!'
            {
                $$=$1;
                $$->SetTermID(TermID::ITERATOR);
            }
        | ITERATOR  /* !! ?? */
            {
                $$=$1;
            }

iter_call:  iter  '('  args   ')'
            {
                $$ = $1;
                $$->SetArgs($args);
            }

        
iter_all:  ITERATOR_QQ  /* !?  ?! */
            {
                $$=$1;
                $$->SetTermID(TermID::ITERATOR);
            }
        | iter
            {
                $$=$1;
            }
        | iter_call
            {
                $$=$1;
            }

       

/*
 * Порядок аргументов проверяется не на уровне парсера, а при анализе объекта, поэтому 
 * в парсере именованные и не именованные аргуметы могут идти в любом порядке и в любом месте.
 * 
 * Но различаются аругменты с левой и правой стороны от оператора присвоения!
 * С левой стороны в скобках указывается прототип функции, где у каждого аргумента должно быть имя, может быть указан тип данных 
 * и признак ссылки, а последним оператором может быть многоточие (т.е. произвольное кол-во аргументов).
 * С правой стороны в скобках происходит вызов функции (для функции, которая возвращает ссылку, перед именем "&" не ставится),
 * а перед аргументами может стоять многоточие или два (т.е. операторы раскрытия словаря).
 * 
 * <Но все это анализирутся тоже после парсера на уровне компилятора/интерпретатора!>
 * 
 */
        
    

/* Аргументом может быть что угодно */
arg: arg_name  '='
        {  // Именованный аргумент
            $$ = $2;
            $$->m_name.swap($1->m_text);
            $$->SetTermID(TermID::EMPTY);
        }
    | name  type_item  '='
        { // Именованный аргумент
            $$ = $3;
            $$->SetType($type_item);
            $$->m_name.swap($1->m_text);
            $$->SetTermID(TermID::EMPTY);
        }
    | arg_name  '='  logical
        { // Именованный аргумент
            $$ = $3;
            $$->SetName($1->getText());
        }
    | name  type_item  '='  logical
        { // Именованный аргумент
            $$ = $4;
            $$->SetType($type_item);
            $$->SetName($1->getText());
        }
    | name  type_list  '='  logical
        { // Именованный аргумент
            $$ = $4;
            Term::ListToVector($type_list, $$->m_type_allowed);
            $$->SetName($1->getText());
        }
    | logical
        {
            // сюда попадают и именованные аргументы как операция присвоения значения
            $$ = $1;
        }
    |  ELLIPSIS
        {
            // Раскрыть элементы словаря в последовательность не именованных параметров
            $$ = $1; 
        }
    |  ELLIPSIS  logical
        {
            // Раскрыть элементы словаря в последовательность не именованных параметров
            $$ = $1; 
            $$->Append($2, Term::RIGHT);
        }
    |  ELLIPSIS  ELLIPSIS  logical
       {            
            // Раскрыть элементы словаря в последовательность ИМЕНОВАННЫХ параметров
            $$ = $2;
            $$->Append($1, Term::LEFT); 
            $$->Append($3, Term::RIGHT); 
        }
    |  ELLIPSIS  logical  ELLIPSIS
       {            
            // Заполнить данные значением
            $$ = $1;
            $$->SetTermID(TermID::FILLING);
            $$->Append($2, Term::RIGHT); 
        }
/*    |  operator
       {            
            $$ = $1;
       }
    |  op_factor
       {            
            $$ = $1;
       } */

args: arg
            {
                $$ = $1;
            }
        | args  ','  arg
            {
                $$ = $1;
                $$->AppendList($3);
            }
        
        
call:  '('  ')'
            {   
                $$ = $1;
                $$->SetTermID(TermID::END);
            }
        | '('  args   ')'
            {
                $$ = $2;
            }
        
        
array: '['  args  ','  ']'
            {
                $$ = $1;
                $$->m_text.clear();
                $$->SetTermID(TermID::TENSOR);
                $$->SetArgs($args);
            }
        | '['  args  ','  ']'  type_item
            {
                $$ = $1;
                $$->m_text.clear();
                $$->SetTermID(TermID::TENSOR);
                $$->SetArgs($args);
                $$->SetType($type_item);
            }
        | '['  ','  ']'  type_item
            {
                // Не инициализированый тензор должен быть с конкретным типом 
                $$ = $1;
                $$->m_text.clear();
                $$->SetTermID(TermID::TENSOR);
                $$->SetType($type_item);
            }

            
dictionary: '('  ','  ')'
            {
                $$ = $1;
                $$->m_text.clear();
                $$->SetTermID(TermID::DICT);
            }
        | '('  args  ','  ')'
            {
                $$ = $1;
                $$->m_text.clear();
                $$->SetTermID(TermID::DICT);
                $$->SetArgs($2);
            }


class:  dictionary
            {
                $$ = $1;
            }
        | dictionary   type_class        
            {
                $$ = $1;
                $$->m_text = $type_class->m_text;
                $$->m_class = $type_class->m_text;
            }

collection: array 
            {
                $$ = $1;
            }
        | class
            {
                $$ = $1;
            }
class_props: assign_arg 
            {
                $$ = $1;
            }
        | class_props   ';'   assign_arg
            {
                $$ = $1;
                $$->AppendSequenceTerm($3);
            }

class_item:  type_item
            {
                $$ = $1;
            }
        | name  call
            {
                $$ = $1;
                $$->SetArgs($call);
            }

class_base: class_item
            {
                $$ = $1;
            }
        | class_base   ','   class_item
            {
                $$ = $1;
                $$->AppendList($3);
            }

        
class_def:  class_base  '{'  '}'
            {
                $$ = $2;
                Term::ListToVector($class_base, $$->m_base);
                $$->SetTermID(TermID::CLASS);
            }
        | class_base '{' class_props  ';'  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $class_props;
                Term::ListToVector($class_base, $$->m_base);
                $$->ConvertSequenceToBlock(TermID::CLASS);
                driver.MacroLevelEnd( $5 );
            }
        | class_base '{' doc_after  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $2;
                $$->SetTermID(TermID::CLASS);
                Term::ListToVector($class_base, $$->m_base);
                Term::ListToVector($doc_after, $$->m_docs);
                driver.MacroLevelEnd( $4 );
            }
        | class_base '{' doc_after  class_props  ';'  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $class_props;
                $$->ConvertSequenceToBlock(TermID::CLASS);
                Term::ListToVector($class_base, $$->m_base);
                Term::ListToVector($doc_after, $$->m_docs);
                driver.MacroLevelEnd( $6 );
            }
        
        
       
block_ns:  ns_name  '{'  '}'
            {
                $$ = $2; 
                $$->SetTermID(TermID::BLOCK);
                $$->m_class = $1->m_text;
            }
        | ns_name  '{'  sequence  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                $$->m_class = $1->m_text;
                driver.MacroLevelEnd( $4 );
            }
        | ns_name  '{'  sequence  separator  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                $$->m_class = $1->m_text;
                driver.MacroLevelEnd( $5 );
            }        
        | ns_name  '{' doc_after '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $2; 
                $$->SetTermID(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                $$->m_class = $1->m_text;
                driver.MacroLevelEnd( $4 );
            }
        | ns_name  '{'  doc_after  sequence  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                $$->m_class = $1->m_text;
                driver.MacroLevelEnd( $5 );
            }
        | ns_name  '{'  doc_after  sequence  separator  '}'
            {
                driver.MacroLevelBegin( $2 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                $$->m_class = $1->m_text;
                driver.MacroLevelEnd( $6 );
            }        

        
        
assign_op: CREATE_OR_ASSIGN /* := */
            {
                $$ = $1;
            }
        | CREATE /* ::= */
            {
                $$ = $1;
            }
        | APPEND /* []= */
            {
                $$ = $1;
            }
        | PUREFUNC /* ::- :- */
            {
                $$ = $1;
            }


assign_expr:  body_all
                {
                    $$ = $1;  
                }
            | ELLIPSIS  rval
                {
                    $$ = $1;  
                    $$->Append($rval, Term::RIGHT); 
                }
            | class_def
                {
                    $$ = $1;  
                }
            
            
assign_item:  lval
                {
                    $$ = $1;
                }
            |  ELLIPSIS
                {
                    $$ = $1;
                }

assign_items: assign_item
                {
                    $$ = $1;
                }
            |  assign_items  ','  assign_item
                {
                    $$ = $1;
                    $$->AppendList($3);
                }
/*
lval = rval;
lval, lval, lval = rval;
func() = rval;
*/
assign_arg:  lval  assign_op  assign_expr
            {
                $$ = $2;  
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
            }
        | lval  '='  assign_expr
            {
                $$ = $2;  
                $$->SetTermID(TermID::ASSIGN);
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
                Term::CheckSetEnv($$);
            }

assign_seq:  assign_items  assign_op  assign_expr
            {
                $$ = $2;  
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
            }
        | assign_items  '='  assign_expr
            {
                $$ = $2;  
                $$->SetTermID(TermID::ASSIGN);
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
            }
        
        
block:  '{'  '}'
            {
                $$ = $1; 
                $$->SetTermID(TermID::BLOCK);
            }
        | '{'  sequence  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                driver.MacroLevelEnd( $3 );
            }
        | '{'  sequence  separator  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                driver.MacroLevelEnd( $4 );
            }
        |  '{'  doc_after  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $1; 
                $$->SetTermID(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                driver.MacroLevelEnd( $3 );
            }
        | '{'  doc_after  sequence  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                driver.MacroLevelEnd( $4 );
            }
        | '{'  doc_after  sequence  separator  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $sequence; 
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                Term::ListToVector($doc_after, $$->m_docs);
                driver.MacroLevelEnd( $5 );
            }

     
        
body:  condition
            {
                $$ = $1;
            }
        |  block
            {
                $$ = $1;
            }
        |  block_ns
            {
                $$ = $1;
            }
        |  doc_before  block
            {
                $$ = $block;
                Term::ListToVector($doc_before, $$->m_docs);
            }
        |  doc_before  block_ns
            {
                $$ = $block_ns;
                Term::ListToVector($doc_before, $$->m_docs);
            }

body_all: body
            {
                $$ = $1;
            }
        |  try_catch
            {
                $$ = $1;
            }
        |  exit
            {
                $$ = $1;
            }

        
body_else: ',' else  FOLLOW  body_all
            {
                $$ = $4; 
            }


try_all: TRY_ALL_BEGIN  sequence  TRY_ALL_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_TRY);
                driver.MacroLevelEnd( $3 );
            }
        | TRY_ALL_BEGIN  sequence  separator  TRY_ALL_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_TRY);
                driver.MacroLevelEnd( $4 );
            }

try_plus: TRY_PLUS_BEGIN  sequence  TRY_PLUS_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_PLUS);
                driver.MacroLevelEnd( $3 );
            }
        | TRY_PLUS_BEGIN  sequence  separator  TRY_PLUS_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_PLUS);
                driver.MacroLevelEnd( $4 );
            }
        
try_minus: TRY_MINUS_BEGIN  sequence  TRY_MINUS_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_MINUS);
                driver.MacroLevelEnd( $3 );
            }
        | TRY_MINUS_BEGIN  sequence  separator  TRY_MINUS_END
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2; 
                $$->ConvertSequenceToBlock(TermID::BLOCK_MINUS);
                driver.MacroLevelEnd( $4 );
            }

try_catch:  try_plus 
            {
                $$ = $1;
            }
        | try_minus
            {
                $$ = $1;
            }
        | try_all
            {
                $$ = $1;
            }
        | try_all  type_name
            {
                $$ = $1;
                $$->m_type_allowed.push_back($type_name);
            }
        | try_all  type_list
            {
                $$ = $1;
                Term::ListToVector($type_list, $$->m_type_allowed);
            }
        
/* 
 * lvalue - объект в памяти, которому может быть присовено значение (может быть ссылкой и/или константой)
 * rvalue - объект, которому <НЕ> может быть присвоено значение (литерал, итератор, вызов функции)
 * Все lvalue могут быть преобразованы в rvalue. 
 * eval - rvalue или операция с rvalue. Возвращает результат выполнения <ОДНОЙ операции !!!!!!!>
 * 
 * Операции присвоения используют lvalue, многоточие или определение функций
 * Алгоритмы используют eval или блок кода (у matching)
 */
        
/*
 * <arithmetic> -> <arithmetic> + <addition> | <arithmetic> - <addition> | <addition>
 * <addition> -> <addition> * <factor> | <addition> / <factor> | <factor>
 * <factor> -> vars | ( <expr> )
 */

operator: OPERATOR
            {
                $$ = $1;
            }
        | '~'
            {
                $$ = $1;
                $$->SetTermID(TermID::OPERATOR);
            }
        | '>'
            {
                $$ = $1;
                $$->SetTermID(TermID::OPERATOR);
            }
        | '<'
            {
                $$ = $1;
                $$->SetTermID(TermID::OPERATOR);
            }
        |  OPERATOR_AND
            {
                $$ = $1;
                $$->SetTermID(TermID::OPERATOR);
            }
        |  OPERATOR_ANGLE_EQ
            {
                $$ = $1;
                $$->SetTermID(TermID::OPERATOR);
            }


arithmetic:  arithmetic '+' addition
                { 
                    $$ = $2;
                    $$->SetTermID(TermID::OPERATOR);
                    $$->Append($1, Term::LEFT);                    
                    $$->Append($3, Term::RIGHT); 
                }
            | arithmetic '-'  addition
                { 
                    $$ = $2;
                    $$->SetTermID(TermID::OPERATOR);
                    $$->Append($1, Term::LEFT);                    
                    $$->Append($3, Term::RIGHT); 
                }
            |  addition   digits
                {
                    if($digits->m_text[0] != '-') {
                        NL_PARSER($digits, "Missing operator!");
                    }
                    //@todo location
                    $$ = Term::Create(token::OPERATOR, TermID::OPERATOR, $2->m_text.c_str(), 1, & @$);
                    $$->Append($1, Term::LEFT); 
                    $2->m_text = $2->m_text.substr(1);
                    $$->Append($2, Term::RIGHT); 
                }
            | addition
                { 
                    $$ = $1; 
                }


op_factor: '*'
            {
                $$ = $1;
            }
        |  '/'
            {
                $$ = $1;
            }
        |  OPERATOR_DIV
            {
                $$ = $1;
            }
        |  '%'
            {
                $$ = $1;
            }
        
addition:  addition  op_factor  factor
                { 
                    if($1->getTermID() == TermID::INTEGER && $op_factor->m_text.compare("/")==0 && $3->m_text.compare("1")==0) {
                        NL_PARSER($op_factor, "Do not use division by one (e.g. 123/1), "
                                "as this operation does not make sense, but it is easy to "
                                "confuse it with the notation of a rational literal (123\\1).");
                    }
    
                    $$ = $op_factor;
                    $$->SetTermID(TermID::OPERATOR);
                    $$->Append($1, Term::LEFT); 
                    $$->Append($3, Term::RIGHT); 
                }
        |  factor
                { 
                    $$ = $1; 
                }    

factor:   rval_var
            {
                $$ = $1; 
            }
        | '-'  factor
            { 
                $$ = Term::Create(token::OPERATOR, TermID::OPERATOR, "-", 1,  & @$);
                $$->Append($2, Term::RIGHT); 
            }
        | '('  arithmetic  ')'
            {
                $$ = $2; 
            }

       
   
        
condition: SOURCE
            {
                $$=$1;
            }
        | logical
            {
                $$ = $1;
            }

        
logical:  arithmetic
            {
                    $$ = $1;
            }
        |  logical  operator  arithmetic
            {
                $$ = $2;
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
            }
        |  arithmetic  iter_all
            {
                $$ = $2;
                $$->Last()->Append($1, Term::LEFT); 
            }
        |  logical  operator  arithmetic   iter_all
            {
                $$ = $2;
                $$->Append($1, Term::LEFT); 
                $iter_all->Last()->Append($arithmetic, Term::LEFT); 
                $$->Append($iter_all, Term::RIGHT); 
            }
        
        
else:   ELSE
            {
                 $$ = Term::Create(token::SYMBOL, TermID::NONE, "_", 1, & @$);
            }
        |  '['  '_'  ']' 
            {
                 $$ = Term::Create(token::SYMBOL, TermID::NONE, "_", 1, & @$);
            }
            

match_cond: '['   condition   ']' 
            {
                $$ = $condition;
            }
        |  else
            {
                $$ = $else;
            }

if_then:  match_cond  FOLLOW  body_all
            {
                $$=$2;
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT); 
            }

follow: if_then
            {
                $$ = $1; 
                $$->AppendFollow($1);
            }
        | follow  ','  if_then
            {
                $$ = $1; 
                $$->AppendFollow($3);
            }
        
   
repeat: body_all  REPEAT  match_cond
            {
                $$=$2;
                $$->SetTermID(TermID::DOWHILE);
                $$->Append($body_all, Term::LEFT); 
                $$->Append($match_cond, Term::RIGHT); 
            }
        | match_cond  REPEAT  body_all
            {
                $$=$2;
                $$->SetTermID(TermID::WHILE);
                $$->Append($match_cond, Term::LEFT); 
                $$->Append($body_all, Term::RIGHT); 
            }
        | match_cond  REPEAT  body_all  body_else
            {
                $$=$2;
                $$->SetTermID(TermID::WHILE);
                $$->Append($match_cond, Term::LEFT); 
                $$->Append($body_all, Term::RIGHT); 
                $$->AppendFollow($body_else); 
            }

matches:  rval_name
            {
                $$=$1;
            }
        |  matches  ','  rval_var
            {
                $$ = $1;
                $$->AppendFollow($3);
            }        
        
match_item: '[' matches  ']' FOLLOW  body
            {
                $$=$4;
                $$->Append($2, Term::LEFT); 
                $$->Append($5, Term::RIGHT); 
            }

match_items:  match_item
            {
                $$ = $1;
            }
        | match_items  ';'  match_item
            {
                $$ = $1;
                $$->AppendSequenceTerm($match_item);
            }
        
match_body:  '{'  match_items  '}'
            {
                driver.MacroLevelBegin( $1 );
                $$ = $2;
                $$->ConvertSequenceToBlock(TermID::BLOCK);
                driver.MacroLevelEnd( $3 );
            }



match:  match_cond   MATCHING  match_body
            {
                $$=$2;
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT);
            }
        |  body_all  MATCHING  match_body
            {
                $$=$2;
                $$->Append($1, Term::LEFT); 
                $$->Append($3, Term::RIGHT);
            }

interrupt: INT_PLUS 
            {
                $$ = $1;
            }
        | INT_MINUS
            {
                $$ = $1;
            }
        
        
exit:  interrupt
        {
            $$ = $1;
        }
    |  interrupt   rval   interrupt
        {
            $$ = $1;
            $$->Append($2, Term::RIGHT); 
        }
    |  INT_REPEAT   rval   INT_REPEAT
        {
            $$ = $1;
            $$->Append($2, Term::RIGHT); 
        }


alias_item: NAME
            {
                $$ = $1; 
            }
        | SYMBOL
            {
                $$ = $1; 
            }
//        | OPERATOR
//            {
//                $$ = $1; 
//            }
//        | OPERATOR_DIV
//            {
//                $$ = $1; 
//            }
//        | OPERATOR_AND
//            {
//                $$ = $1; 
//            }
//        | OPERATOR_PTR
//            {
//                $$ = $1; 
//            }
//        | OPERATOR_ANGLE_EQ
//            {
//                $$ = $1; 
//            }
//        | ITERATOR
//            {
//                $$ = $1; 
//            }
//        | ITERATOR_QQ
//            {
//                $$ = $1; 
//            }

        
alias_name: alias_item
            {
                $$ = $1->Clone(); 
                $$->AppendFollow($1);
            }
        | alias_item  call
            {
                $1->SetArgs($call);
                $$ = $1->Clone();
                $$->AppendFollow($1);
            }

macro_name: alias_name
            {
                $$ = $1; 
            }
        | macro_name  alias_name
            {
                $$ = $1; 
                $$->AppendFollow($2);
            }

/*
 * Последовательность токенов в теле макроса может быть совершенно произвольной, например,
 * при использовании нескольких макросов при записи одной синтаксической конструкции (match), из-за чего 
 * возможные последовательности символов в теле макроса нельзя описать в парсере без конфликтов грамматики.
 * 
 * Поэтому макросы формируются непосредственно в лексере и в анализатор грамматики передаются одним корректным объектом.
 * 
 * Первый MACRO_DEF запрещает раскрытие макросов
 * Второй MACRO_DEF начинает формировать последовательность терминов для тела макроса.
 * Третий MACRO_DEF создает MACRO_BODY, передает его в анализатор и разрешает раскрытие макросов.
 * 
 * Макросы двух видов, первый AST с парсером терминов, второй как строка (С/C++ препроцессор).
 */        
    
macros:  /* MACRO_DEF   macro_name   MACRO_DEF     seq_item   MACRO_DEF
            {
                $$ = $2; 
                $$->SetTermID(TermID::ALIAS);
                $$->Append($4, Term::RIGHT);
                driver.MacroTerm($$);
            }
        |  */ MACRO_DEF   macro_name   MACRO_BODY
            {
                $$ = $2; 
                $$->SetTermID(TermID::MACRO_DEF);
                $$->Append($3, Term::RIGHT);
                driver.MacroTerm($$);
            }
        |  MACRO_DEF   macro_name   MACRO_STR
            {
                $$ = $2; 
                $$->SetTermID(TermID::MACRO_STR);
                $$->Append($3, Term::RIGHT);
                driver.MacroTerm($$);
            }
        |  MACRO_DEF   macro_name   MACRO_DEL
            {
                $$ = $2; 
                $$->SetTermID(TermID::MACRO_DEL);
                driver.MacroTerm($$);
            }
        
    
/*  expression - одна операция или результат <ОДНОГО выражения без завершающей точки с запятой !!!!!> */
seq_item: assign_seq
            {
                $$ = $1;
            }
        | doc_before assign_seq
            {
                $$ = $assign_seq;
                Term::ListToVector($doc_before, $$->m_docs);
            }
        | follow
            {
                $$ = $1; 
            }
        | match
            {
                $$ = $1; 
            }
        | repeat
            {
                $$ = $1; 
            }
        |  macros
            {
                $$ = $1;
            }
        | body_all
            {
                $$ = $1;
            }
        | try_catch  body_else
            {
                $$ = $1; 
                $$->AppendFollow($body_else); 
            }
        
sequence:  seq_item
            {
                $$ = $1;  
            }
        | seq_item  doc_after
            {
                $$ = $1;  
                Term::ListToVector($doc_after, $$->m_docs);
            }
        | sequence  separator  seq_item
            {
                $$ = $1;  
                // Несколько команд подряд
                $$->AppendSequenceTerm($seq_item);
            }
        | sequence  separator  doc_after  seq_item
            {
                $$ = $1;  
                Term::ListToVector($doc_after, $$->m_docs);
                // Несколько команд подряд
                $$->AppendSequenceTerm($seq_item);
            }

separator: ';' | separator  ';'        
        

ast:    END
        | separator
        | sequence
            {
                driver.AstAddTerm($1);
            }
        | sequence separator
            {
                driver.AstAddTerm($1);
            }
        | sequence separator  doc_after
            {
                Term::ListToVector($doc_after, $1->m_docs);
                driver.AstAddTerm($1);
            }
/*        | comment     Комменатарии не добавляются в AST, т.к. в парсере они не нужны, а
                        их потенциальное использование - документирование кода, решается 
                        элементами DOC_BEFORE и DOC_AFTER
*/

start	:   ast

%% /*** Additional Code ***/
