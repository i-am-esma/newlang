#include "pch.h"

#ifdef UNITTEST

#include <warning_push.h>
#include <gtest/gtest.h>
#include <warning_pop.h>

#include <term.h>
#include <lexer.h>
#include <parser.h>
#include <nlc.h>


using namespace newlang;

class Lexer : public ::testing::Test {
protected:

    std::vector<TermPtr> tokens;

    void SetUp() {
    }

    void TearDown() {
    }

    int64_t TokenParse(const char *str) {
        std::istringstream strstr(str);

        Scanner lexer(&strstr);
        tokens.clear();
        TermPtr tok;
        parser::location_type loc;
        while(lexer.lex(&tok, &loc) != parser::token::END) {
            tokens.push_back(tok);
        }
        return tokens.size();
    }

    int Count(TermID token_id) {
        int result = 0;
        for (size_t i = 0; i < tokens.size(); i++) {
            if(tokens[i]->GetTokenID() == token_id) {
                result++;
            }
        }
        return result;
    }

    std::string Dump() {
        std::string result;
        for (int i = 0; i < tokens.size(); i++) {
            result += tokens[i]->m_text;
            result += ":";
            result += toString(tokens[i]->m_id);
            result += " ";
        }
        return result;
    }

};

TEST_F(Lexer, Word) {
    ASSERT_EQ(1, TokenParse("alpha  "));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_STREQ("alpha", tokens[0]->getText().c_str());


    ASSERT_EQ(2, TokenParse("буквы    ещёЁ_99"));
    EXPECT_EQ(2, Count(TermID::NAME));
    EXPECT_STREQ("буквы", tokens[0]->getText().c_str());
    EXPECT_STREQ("ещёЁ_99", tokens[1]->getText().c_str());

    ASSERT_EQ(3, TokenParse("one two \t three"));
    EXPECT_EQ(3, Count(TermID::NAME));

    EXPECT_STREQ("one", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    EXPECT_STREQ("two", tokens[1]->getText().c_str()) << tokens[1]->getText().c_str();
    EXPECT_STREQ("three", tokens[2]->getText().c_str()) << tokens[2]->getText().c_str();
}

//TEST_F(Lexer, StrvarEmpty) {
//    ASSERT_EQ(0, Parse(""));
//    ASSERT_EQ(1, Parse("``"));
//    EXPECT_EQ(1, Count(TermID::STRVAR));
//    EXPECT_STREQ("", tokens[0]->getText().c_str()) << tokens[0]->getText();
//}
//
//TEST_F(Lexer, StrvarSimple) {
//    ASSERT_EQ(1, Parse("` `"));
//    EXPECT_EQ(1, Count(TermID::STRVAR));
//    EXPECT_STREQ(" ", tokens[0]->getText().c_str()) << tokens[0]->getText();
//}
//
//TEST_F(Lexer, FullStrvar) {
//    ASSERT_EQ(1, Parse("` ${123} \t \xFF\r\\\"`"));
//    EXPECT_EQ(1, Count(TermID::STRVAR));
//    EXPECT_STREQ(" ${123} \t \xFF\r\"", tokens[0]->getText().c_str()) << tokens[0]->getText();
//}

TEST_F(Lexer, Template) {
    ASSERT_EQ(1, TokenParse("\"\"\"\"\"\""));
    EXPECT_EQ(1, Count(TermID::TEMPLATE));
    EXPECT_STREQ("", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, DISABLED_Template2) {
    ASSERT_EQ(1, TokenParse("\"\"\" ${123} \n \"\"\""));
    EXPECT_EQ(1, Count(TermID::TEMPLATE));
    EXPECT_STREQ(" ${123} \n ", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, DISABLED_Template3) {
    ASSERT_EQ(1, TokenParse("''' ${123} \n\t '''"));
    EXPECT_EQ(1, Count(TermID::TEMPLATE));
    EXPECT_STREQ(" ${123} \n\t ", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, StringEmpty) {
    ASSERT_EQ(0, TokenParse(""));
    ASSERT_EQ(1, TokenParse("''"));
    EXPECT_EQ(1, Count(TermID::STRCHAR));
    EXPECT_STREQ("", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, StringEmpty2) {
    ASSERT_EQ(0, TokenParse(""));
    ASSERT_EQ(1, TokenParse("\"\""));
    EXPECT_EQ(1, Count(TermID::STRWIDE));
    EXPECT_STREQ("", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, StringSimple) {
    ASSERT_EQ(1, TokenParse("' '"));
    EXPECT_EQ(1, Count(TermID::STRCHAR));
    EXPECT_STREQ(" ", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, StringSimple2) {
    ASSERT_EQ(1, TokenParse("\" \""));
    EXPECT_EQ(1, Count(TermID::STRWIDE));
    EXPECT_STREQ(" ", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, FullString) {
    ASSERT_EQ(1, TokenParse("'  \t \xFF\r\\''"));
    EXPECT_EQ(1, Count(TermID::STRCHAR));
    EXPECT_STREQ("  \t \xFF\r'", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, FullString2) {
    ASSERT_EQ(1, TokenParse("\"  \t \xFF\r\\\"\""));
    EXPECT_EQ(1, Count(TermID::STRWIDE));
    EXPECT_STREQ("  \t \xFF\r\"", tokens[0]->getText().c_str()) << tokens[0]->getText();
}

TEST_F(Lexer, Integer) {
    ASSERT_EQ(1, TokenParse("123456"));
    EXPECT_EQ(1, Count(TermID::INTEGER)) << newlang::toString(tokens[0]->GetTokenID());

    EXPECT_STREQ("123456", tokens[0]->getText().c_str());

    ASSERT_EQ(3, TokenParse("123456**123"));
    EXPECT_EQ(1, Count(TermID::OPERATOR));
    EXPECT_EQ(2, Count(TermID::INTEGER));

    EXPECT_STREQ("123456", tokens[0]->getText().c_str()) << tokens[0]->getText();
    EXPECT_STREQ("**", tokens[1]->getText().c_str()) << tokens[1]->getText();
    EXPECT_STREQ("123", tokens[2]->getText().c_str()) << tokens[2]->getText();
}

TEST_F(Lexer, Float) {
    ASSERT_EQ(1, TokenParse("1.e10"));
    EXPECT_EQ(1, Count(TermID::NUMBER));
    EXPECT_STREQ("1.e10", tokens[0]->getText().c_str());
}

//TEST_F(Lexer, Complex0) {
//    if(1 != Parse("-1j-0.2")) {
//        for (auto elem : tokens) {
//            std::cout << newlang::toString(elem->m_id) << " " << elem->m_text << "\n";
//        }
//    }
//    ASSERT_EQ(1, tokens.size());
//    EXPECT_EQ(1, Count(TermID::COMPLEX)) << newlang::toString(tokens[0]->m_id);
//    EXPECT_STREQ("-1j-0.2", tokens[0]->getText().c_str());
//}
//
//TEST_F(Lexer, Complex1) {
//    if(1 != Parse("1.333+0.e10j")) {
//        for (auto elem : tokens) {
//            std::cout << newlang::toString(elem->m_id) << " " << elem->m_text << "\n";
//        }
//    }
//    ASSERT_EQ(1, tokens.size());
//    EXPECT_EQ(1, Count(TermID::COMPLEX));
//    EXPECT_STREQ("1.333+0.e10j", tokens[0]->getText().c_str());
//}

TEST_F(Lexer, Term) {

    if(1 != TokenParse("$alpha  ")) {
        for (auto elem : tokens) {
            std::cout << newlang::toString(elem->m_id) << " " << elem->m_text << "\n";
        }

    }
    ASSERT_EQ(1, tokens.size());
    EXPECT_EQ(1, Count(TermID::LOCAL));
    EXPECT_STREQ("$alpha", tokens[0]->getText().c_str());


    ASSERT_EQ(2, TokenParse("буквы    ещёЁ_99"));
    EXPECT_EQ(2, Count(TermID::NAME));
    EXPECT_STREQ("буквы", tokens[0]->getText().c_str());
    EXPECT_STREQ("ещёЁ_99", tokens[1]->getText().c_str());

    ASSERT_EQ(3, TokenParse("one @two \t $three"));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::MODULE));
    EXPECT_EQ(1, Count(TermID::LOCAL));

    EXPECT_STREQ("one", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    EXPECT_STREQ("@two", tokens[1]->getText().c_str()) << tokens[1]->getText().c_str();
    EXPECT_STREQ("$three", tokens[2]->getText().c_str()) << tokens[2]->getText().c_str();
}

TEST_F(Lexer, AssignEq) {
    ASSERT_EQ(3, TokenParse("token=ssssssss"));
    EXPECT_EQ(2, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::SYMBOL));

    EXPECT_STREQ("token", tokens[0]->getText().c_str()) << tokens[0]->getText();
    EXPECT_STREQ("ssssssss", tokens[2]->getText().c_str()) << tokens[2]->getText();

    ASSERT_EQ(3, TokenParse("token:=\"ssssssss\""));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::CREATE_OR_ASSIGN));
    EXPECT_EQ(1, Count(TermID::STRWIDE));

    EXPECT_STREQ("token", tokens[0]->getText().c_str()) << tokens[0]->getText();
    EXPECT_STREQ("ssssssss", tokens[2]->getText().c_str()) << tokens[2]->getText();

    ASSERT_EQ(3, TokenParse("    token   \t  ::=   'ssssssss'       "));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::CREATE));
    EXPECT_EQ(1, Count(TermID::STRCHAR));

    EXPECT_STREQ("token", tokens[0]->getText().c_str()) << tokens[0]->getText();
    EXPECT_STREQ("ssssssss", tokens[2]->getText().c_str()) << tokens[2]->getText();
}

TEST_F(Lexer, CodeInner) {
    ASSERT_EQ(3, TokenParse("{%if(){%}   {%}else{%}   {%} %}"));
    EXPECT_EQ(3, Count(TermID::SOURCE));
    EXPECT_STREQ("if(){", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    EXPECT_STREQ("}else{", tokens[1]->getText().c_str()) << tokens[1]->getText().c_str();
    EXPECT_STREQ("} ", tokens[2]->getText().c_str()) << tokens[2]->getText().c_str();

    ASSERT_EQ(5, TokenParse("{ {%if(){%}   {%}else{%}   {%} %} }"));
    EXPECT_EQ(2, Count(TermID::SYMBOL));
    EXPECT_EQ(3, Count(TermID::SOURCE));
    EXPECT_STREQ("{", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    EXPECT_STREQ("if(){", tokens[1]->getText().c_str()) << tokens[1]->getText().c_str();
    EXPECT_STREQ("}else{", tokens[2]->getText().c_str()) << tokens[2]->getText().c_str();
    EXPECT_STREQ("} ", tokens[3]->getText().c_str()) << tokens[3]->getText().c_str();
    EXPECT_STREQ("}", tokens[4]->getText().c_str()) << tokens[4]->getText().c_str();
}

TEST_F(Lexer, Code) {
    ASSERT_EQ(2, TokenParse("{            }"));
    EXPECT_EQ(2, Count(TermID::SYMBOL));
    EXPECT_STREQ("{", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    EXPECT_STREQ("}", tokens[1]->getText().c_str()) << tokens[1]->getText().c_str();

    ASSERT_EQ(4, TokenParse("{ { } }"));
    EXPECT_EQ(4, Count(TermID::SYMBOL));
}

TEST_F(Lexer, CodeSource) {
    ASSERT_EQ(1, TokenParse("{%%}"));
    EXPECT_EQ(1, Count(TermID::SOURCE));
    EXPECT_STREQ("", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("{% % %}"));
    ASSERT_EQ(1, Count(TermID::SOURCE));
    ASSERT_STREQ(" % ", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
}

TEST_F(Lexer, Assign) {
    ASSERT_EQ(5, TokenParse(":= :- ::= ::- ="));
    EXPECT_EQ(1, Count(TermID::CREATE));
    EXPECT_EQ(1, Count(TermID::CREATE_OR_ASSIGN));
    EXPECT_EQ(2, Count(TermID::PUREFUNC));
}

TEST_F(Lexer, Function) {
    ASSERT_EQ(1, TokenParse("@"));
    EXPECT_EQ(1, Count(TermID::SYMBOL)) << toString(tokens[0]->getTermID());
    EXPECT_STREQ("@", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("@name"));
    EXPECT_EQ(1, Count(TermID::MODULE)) << toString(tokens[0]->getTermID());
    EXPECT_STREQ("@name", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("$"));
    EXPECT_EQ(1, Count(TermID::LOCAL)) << toString(tokens[0]->getTermID());
    EXPECT_STREQ("$", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("$name"));
    EXPECT_EQ(1, Count(TermID::LOCAL)) << toString(tokens[0]->getTermID());
    EXPECT_STREQ("$name", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("%native"));
    EXPECT_EQ(1, Count(TermID::NATIVE)) << toString(tokens[0]->getTermID());
    EXPECT_STREQ("%native", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("@name"));
    EXPECT_EQ(1, Count(TermID::MODULE));
    EXPECT_STREQ("@name", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();

    ASSERT_EQ(1, TokenParse("@функция_alpha_ёЁ"));
    EXPECT_EQ(1, Count(TermID::MODULE));
    EXPECT_STREQ("@функция_alpha_ёЁ", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
}

TEST_F(Lexer, Sentence) {
    ASSERT_EQ(2, TokenParse("token."));
    EXPECT_EQ(1, Count(TermID::NAME));
    ASSERT_EQ(2, TokenParse("token;"));
    EXPECT_EQ(1, Count(TermID::NAME));
}

TEST_F(Lexer, Comment) {
    ASSERT_EQ(0, TokenParse("# lskdafj ldsjf ldkjfa l;sdj fl;k"));
    ASSERT_EQ(0, TokenParse("#! lskdafj ldsjf ldkjfa l;sdj fl;k\n          "));

    ASSERT_EQ(1, TokenParse("/***** lskdafj\n\n\n\n ldsjf ldkjfa l;sdj fl;k*****/    "));
    EXPECT_EQ(1, Count(TermID::DOC_BEFORE));

    ASSERT_EQ(1, TokenParse("/// lskdafj"));
    EXPECT_EQ(1, Count(TermID::DOC_BEFORE));
    ASSERT_EQ(1, TokenParse("/// lskdafj\n"));
    EXPECT_EQ(1, Count(TermID::DOC_BEFORE));
    ASSERT_EQ(1, TokenParse("///< lskdafj"));
    EXPECT_EQ(1, Count(TermID::DOC_AFTER));
    ASSERT_EQ(1, TokenParse("///< lskdafj\n"));
    EXPECT_EQ(1, Count(TermID::DOC_AFTER));
}

TEST_F(Lexer, Comment2) {
    ASSERT_EQ(2, TokenParse("#!22\n#1\nterm;\n"));
    //    EXPECT_EQ(2, Count(TermID::COMMENT));
    EXPECT_EQ(1, Count(TermID::SYMBOL));
    EXPECT_EQ(1, Count(TermID::NAME));

    ASSERT_EQ(1, TokenParse("\n\n/* \n\n*/\n\n  term"));
    //    EXPECT_EQ(1, Count(TermID::COMMENT));
    EXPECT_EQ(1, Count(TermID::NAME));
    //    EXPECT_STREQ(" \n\n", tokens[0]->getText().c_str()) << tokens[0]->getText().c_str();
    //    EXPECT_EQ(5, tokens[0]->m_line);
    //    EXPECT_EQ(1, tokens[0]->m_col);
    EXPECT_STREQ("term", tokens[0]->getText().c_str()) << tokens[1]->getText().c_str();
    EXPECT_EQ(7, tokens[0]->m_line);
    EXPECT_EQ(3, tokens[0]->m_col);
}

TEST_F(Lexer, Paren) {
    ASSERT_EQ(3, TokenParse("@name()"));
    EXPECT_EQ(1, Count(TermID::MODULE));
    EXPECT_EQ(2, Count(TermID::SYMBOL));


    ASSERT_EQ(3, TokenParse("%функция_alpha_ёЁ ()"));
    EXPECT_EQ(1, Count(TermID::NATIVE));
    EXPECT_EQ(2, Count(TermID::SYMBOL));

}

TEST_F(Lexer, Module) {
    ASSERT_EQ(1, TokenParse("@name"));
    EXPECT_EQ(1, Count(TermID::MODULE));

    ASSERT_EQ(1, TokenParse("@dir.module"));
    EXPECT_EQ(1, Count(TermID::MODULE));

    ASSERT_EQ(1, TokenParse("@dir.dir.module"));
    EXPECT_EQ(1, Count(TermID::MODULE));
}

TEST_F(Lexer, Arg) {
    ASSERT_EQ(7, TokenParse("term(name=value);"));
    EXPECT_EQ(3, Count(TermID::NAME));
    EXPECT_EQ(4, Count(TermID::SYMBOL));
}

TEST_F(Lexer, Args) {
    ASSERT_EQ(10, TokenParse("$0 $1 $22 $333 $4sss $sss1 -- ++ $*   ")) << Dump();
    EXPECT_EQ(5, Count(TermID::ARGUMENT));
    EXPECT_EQ(1, Count(TermID::ARGS));
    EXPECT_EQ(1, Count(TermID::INT_PLUS));
    EXPECT_EQ(1, Count(TermID::INT_MINUS));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::LOCAL));
}

TEST_F(Lexer, UTF8) {
    ASSERT_EQ(7, TokenParse("термин(имя=значение);"));
    EXPECT_EQ(3, Count(TermID::NAME));
    EXPECT_EQ(4, Count(TermID::SYMBOL));
}

TEST_F(Lexer, ELLIPSIS) {
    ASSERT_EQ(2, TokenParse("... ..."));
    EXPECT_EQ(2, Count(TermID::ELLIPSIS));
}

TEST_F(Lexer, Alias) {
    ASSERT_EQ(5, TokenParse("+>:<-")) << Dump();
    EXPECT_EQ(5, Count(TermID::SYMBOL)) << Dump();

    ASSERT_EQ(5, TokenParse("\\\\alias\\\\ALIAS\\\\")) << Dump();
    EXPECT_EQ(3, Count(TermID::MACRO_DEF)) << Dump();
    EXPECT_EQ(2, Count(TermID::NAME)) << Dump();

    ASSERT_EQ(7, TokenParse("/** Comment */\\\\   alias2   \\\\      ALIAS2\\\\///< Комментарий")) << Dump();
    EXPECT_EQ(1, Count(TermID::DOC_BEFORE));
    EXPECT_EQ(1, Count(TermID::DOC_AFTER));
    EXPECT_EQ(2, Count(TermID::NAME));
    EXPECT_EQ(1, tokens[0]->m_line);
    EXPECT_EQ(1, tokens[0]->m_col);
    EXPECT_EQ(1, tokens[1]->m_line);
    EXPECT_EQ(15, tokens[1]->m_col);

    ASSERT_EQ(2, TokenParse("/** Русские символы */name")) << Dump();
    EXPECT_EQ(1, Count(TermID::DOC_BEFORE));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, tokens[0]->m_line);
    EXPECT_EQ(1, tokens[0]->m_col);
    EXPECT_EQ(1, tokens[1]->m_line);
    EXPECT_EQ(23 + 14, tokens[1]->m_col);
}

TEST_F(Lexer, Macro) {
    ASSERT_EQ(1, TokenParse("\\$1")) << Dump();
    ASSERT_EQ(1, TokenParse("\\$*")) << Dump();
    ASSERT_EQ(1, TokenParse("\\$args")) << Dump();

    ASSERT_EQ(7, TokenParse("\\\\macro       \\\\123 ... 456\\\\")) << Dump();
    EXPECT_EQ(3, Count(TermID::MACRO_DEF)) << Dump();
    
    ASSERT_EQ(3, TokenParse("\\\\macro       \\\\\\123 ... 456\\\\\\"));
    EXPECT_EQ(1, Count(TermID::MACRO_DEF));
    EXPECT_EQ(1, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::MACRO_STR));
    EXPECT_STREQ("macro", tokens[1]->m_text.c_str());
    EXPECT_STREQ("123 ... 456", tokens[2]->m_text.c_str());
    EXPECT_EQ(1, tokens[0]->m_line);
    EXPECT_EQ(1, tokens[0]->m_col);
    EXPECT_EQ(1, tokens[2]->m_line);
    EXPECT_EQ(15, tokens[2]->m_col);

    ASSERT_EQ(6, TokenParse("\\\\macro (name)      \\\\\\123 \n \n ... 456\\\\\\ # Комментарий"));
    EXPECT_EQ(1, Count(TermID::MACRO_DEF));
    EXPECT_EQ(2, Count(TermID::SYMBOL));
    EXPECT_EQ(2, Count(TermID::NAME));
    EXPECT_EQ(1, Count(TermID::MACRO_STR));
    EXPECT_STREQ("macro", tokens[1]->m_text.c_str());
    EXPECT_STREQ("123 \n \n ... 456", tokens[5]->m_text.c_str());
    EXPECT_EQ(1, tokens[0]->m_line);
    EXPECT_EQ(1, tokens[0]->m_col);
    EXPECT_EQ(3, tokens[5]->m_line);
    EXPECT_EQ(1, tokens[5]->m_col);
}

#endif // UNITTEST