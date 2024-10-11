#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "photjson.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format)                                                           \
    do {                                                                                                           \
        test_count++;                                                                                              \
        if (equality) {                                                                                            \
            test_pass++;                                                                                           \
        } else {                                                                                                   \
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual); \
            main_ret = 1;                                                                                          \
        }                                                                                                          \
    } while (0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_BOOL(expect, actual) \
    EXPECT_EQ_BASE((expect) == (actual), expect ? "true" : "false", actual ? "true" : "false", "%s")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_EQ_STR(expect, actual, alength) \
    EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength) == 0, expect, actual, "%s")
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#define EXPECT_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")


// 正确解析
static void test_parse_null(void)
{
    phot_elem e;
    phot_init(&e);
    phot_set_bool(&e, false);
    EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "null"));
    EXPECT_EQ_INT(PHOT_NULL, phot_get_type(&e));
    phot_free(&e);
}

#define TEST_BOOL(expect, json)                             \
    do {                                                    \
        phot_elem e;                                        \
        phot_init(&e);                                      \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, json)); \
        EXPECT_EQ_INT(PHOT_BOOL, phot_get_type(&e));        \
        EXPECT_EQ_BOOL(expect, phot_get_bool(&e));          \
        phot_free(&e);                                      \
    } while (0)

static void test_parse_bool(void)
{
    TEST_BOOL(true, "true");
    TEST_BOOL(false, "false");
}

#define TEST_NUM(expect, json)                              \
    do {                                                    \
        phot_elem e;                                        \
        phot_init(&e);                                      \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, json)); \
        EXPECT_EQ_INT(PHOT_NUM, phot_get_type(&e));         \
        EXPECT_EQ_DOUBLE(expect, phot_get_num(&e));         \
        phot_free(&e);                                      \
    } while (0)

static void test_parse_num(void)
{
    TEST_NUM(0.0, "0");
    TEST_NUM(0.0, "-0");
    TEST_NUM(0.0, "-0.0");
    TEST_NUM(1.0, "1");
    TEST_NUM(-1.0, "-1");
    TEST_NUM(1.5, "1.5");
    TEST_NUM(-1.5, "-1.5");
    TEST_NUM(3.1416, "3.1416");
    TEST_NUM(1E10, "1E10");
    TEST_NUM(1e10, "1e10");
    TEST_NUM(1E+10, "1E+10");
    TEST_NUM(1E-10, "1E-10");
    TEST_NUM(-1E10, "-1E10");
    TEST_NUM(-1e10, "-1e10");
    TEST_NUM(-1E+10, "-1E+10");
    TEST_NUM(-1E-10, "-1E-10");
    TEST_NUM(1.234E+10, "1.234E+10");
    TEST_NUM(1.234E-10, "1.234E-10");
    TEST_NUM(0.0, "1e-10000");  // 必须下溢为 0

    TEST_NUM(1.0000000000000002, "1.0000000000000002");            // 大于 1 的最小精度
    TEST_NUM(4.9406564584124654e-324, "4.9406564584124654e-324");  // 牺牲精度后的最小次正规数
    TEST_NUM(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUM(2.2250738585072009e-308, "2.2250738585072009e-308");  // 最大次正规数
    TEST_NUM(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUM(2.2250738585072014e-308, "2.2250738585072014e-308");  // 最小正规数
    TEST_NUM(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUM(1.7976931348623157e308, "1.7976931348623157e308");  // 最大正规数
    TEST_NUM(-1.7976931348623157e308, "-1.7976931348623157e308");
}

#define TEST_STR(expect, json)                                         \
    do {                                                               \
        phot_elem e;                                                   \
        phot_init(&e);                                                 \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, json));            \
        EXPECT_EQ_INT(PHOT_STR, phot_get_type(&e));                    \
        EXPECT_EQ_STR(expect, phot_get_str(&e), phot_get_str_len(&e)); \
        phot_free(&e);                                                 \
    } while (0)

static void test_parse_str(void)
{
    TEST_STR("", "\"\"");
    TEST_STR("Hello", "\"Hello\"");
    TEST_STR("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STR("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STR("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STR("\x24", "\"\\u0024\"");                     // 美元符号 U+0024
    TEST_STR("\xC2\xA2", "\"\\u00A2\"");                 // 分号 U+00A2
    TEST_STR("\xE2\x82\xAC", "\"\\u20AC\"");             // 欧元符号 U+20AC
    TEST_STR("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  // 乐符 U+1D11E
    TEST_STR("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");
}

static void test_parse_arr(void)
{
    phot_elem e;

    phot_init(&e);
    EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "[ ]"));
    EXPECT_EQ_INT(PHOT_ARR, phot_get_type(&e));
    EXPECT_EQ_SIZE_T(0, phot_get_arr_len(&e));
    phot_free(&e);

    phot_init(&e);
    EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(PHOT_ARR, phot_get_type(&e));
    EXPECT_EQ_SIZE_T(5, phot_get_arr_len(&e));
    EXPECT_EQ_INT(PHOT_NULL, phot_get_type(phot_get_arr_elem(&e, 0)));
    EXPECT_EQ_INT(PHOT_BOOL, phot_get_type(phot_get_arr_elem(&e, 1)));
    EXPECT_EQ_INT(PHOT_BOOL, phot_get_type(phot_get_arr_elem(&e, 2)));
    EXPECT_EQ_INT(PHOT_NUM, phot_get_type(phot_get_arr_elem(&e, 3)));
    EXPECT_EQ_INT(PHOT_STR, phot_get_type(phot_get_arr_elem(&e, 4)));
    EXPECT_EQ_BOOL(false, phot_get_bool(phot_get_arr_elem(&e, 1)));
    EXPECT_EQ_BOOL(true, phot_get_bool(phot_get_arr_elem(&e, 2)));
    EXPECT_EQ_DOUBLE(123.0, phot_get_num(phot_get_arr_elem(&e, 3)));
    EXPECT_EQ_STR("abc", phot_get_str(phot_get_arr_elem(&e, 4)), phot_get_str_len(phot_get_arr_elem(&e, 4)));
    phot_free(&e);

    phot_init(&e);
    EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(PHOT_ARR, phot_get_type(&e));
    EXPECT_EQ_SIZE_T(4, phot_get_arr_len(&e));
    for (size_t i = 0; i < 4; i++) {
        phot_elem *ae1 = phot_get_arr_elem(&e, i);
        EXPECT_EQ_INT(PHOT_ARR, phot_get_type(ae1));
        EXPECT_EQ_SIZE_T(i, phot_get_arr_len(ae1));
        for (size_t j = 0; j < i; j++) {
            phot_elem *ae2 = phot_get_arr_elem(ae1, j);
            EXPECT_EQ_INT(PHOT_NUM, phot_get_type(ae2));
            EXPECT_EQ_DOUBLE((double)j, phot_get_num(ae2));
        }
    }
    phot_free(&e);
}

static void test_parse_obj(void)
{
    phot_elem e;

    phot_init(&e);
    EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "{ }"));
    EXPECT_EQ_INT(PHOT_OBJ, phot_get_type(&e));
    EXPECT_EQ_SIZE_T(0, phot_get_obj_len(&e));
    phot_free(&e);

    phot_init(&e);
    EXPECT_EQ_INT(PHOT_PARSE_OK,
                  phot_parse(&e,
                             "{\"n\" : null , \"f\" : false , \"t\" : true , \"i\" : 123 , \"s\" : \"abc\", \"a\" : [ "
                             "1, 2, 3 ], \"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 } }"));
    EXPECT_EQ_INT(PHOT_OBJ, phot_get_type(&e));
    EXPECT_EQ_SIZE_T(7, phot_get_obj_len(&e));
    EXPECT_EQ_STR("n", phot_get_obj_key(&e, 0), phot_get_obj_key_len(&e, 0));
    EXPECT_EQ_INT(PHOT_NULL, phot_get_type(phot_get_obj_value(&e, 0)));
    EXPECT_EQ_STR("f", phot_get_obj_key(&e, 1), phot_get_obj_key_len(&e, 1));
    EXPECT_EQ_BOOL(false, phot_get_bool(phot_get_obj_value(&e, 1)));
    EXPECT_EQ_STR("t", phot_get_obj_key(&e, 2), phot_get_obj_key_len(&e, 2));
    EXPECT_EQ_BOOL(true, phot_get_bool(phot_get_obj_value(&e, 2)));
    EXPECT_EQ_STR("i", phot_get_obj_key(&e, 3), phot_get_obj_key_len(&e, 3));
    EXPECT_EQ_DOUBLE(123.0, phot_get_num(phot_get_obj_value(&e, 3)));
    EXPECT_EQ_STR("s", phot_get_obj_key(&e, 4), phot_get_obj_key_len(&e, 4));
    EXPECT_EQ_STR("abc", phot_get_str(phot_get_obj_value(&e, 4)), phot_get_str_len(phot_get_obj_value(&e, 4)));
    EXPECT_EQ_STR("a", phot_get_obj_key(&e, 5), phot_get_obj_key_len(&e, 5));
    EXPECT_EQ_INT(PHOT_ARR, phot_get_type(phot_get_obj_value(&e, 5)));
    EXPECT_EQ_SIZE_T(3, phot_get_arr_len(phot_get_obj_value(&e, 5)));
    for (size_t i = 0; i < 3; i++) {
        phot_elem *ae = phot_get_arr_elem(phot_get_obj_value(&e, 5), i);
        EXPECT_EQ_INT(PHOT_NUM, phot_get_type(ae));
        EXPECT_EQ_DOUBLE(i + 1.0, phot_get_num(ae));
    }
    EXPECT_EQ_STR("o", phot_get_obj_key(&e, 6), phot_get_obj_key_len(&e, 6));
    {
        phot_elem *ov1 = phot_get_obj_value(&e, 6);
        EXPECT_EQ_INT(PHOT_OBJ, phot_get_type(ov1));
        for (size_t i = 0; i < 3; i++) {
            phot_elem *ov2 = phot_get_obj_value(ov1, i);
            EXPECT_EQ_BOOL(true, (char)('1' + i) == phot_get_obj_key(ov1, i)[0]);
            EXPECT_EQ_SIZE_T(1, phot_get_obj_key_len(ov1, i));
            EXPECT_EQ_INT(PHOT_NUM, phot_get_type(ov2));
            EXPECT_EQ_DOUBLE(i + 1.0, phot_get_num(ov2));
        }
    }
    phot_free(&e);
}

// 错误解析
#define TEST_ERROR(error, json)                      \
    do {                                             \
        phot_elem e;                                 \
        phot_init(&e);                               \
        e.type = PHOT_BOOL;                          \
        EXPECT_EQ_INT(error, phot_parse(&e, json));  \
        EXPECT_EQ_INT(PHOT_NULL, phot_get_type(&e)); \
        phot_free(&e);                               \
    } while (0)

static void test_parse_expect_value(void)
{
    TEST_ERROR(PHOT_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(PHOT_PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value(void)
{
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "?");

    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, ".123");  // at least one digit before '.'
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "1.");    // at least one digit after '.'
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "nan");

    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "[1,]");
    TEST_ERROR(PHOT_PARSE_INVALID_VALUE, "[\"a\", nul]");
}

static void test_parse_root_not_singular(void)
{
    TEST_ERROR(PHOT_PARSE_ROOT_NOT_SINGULAR, "null x");

    TEST_ERROR(PHOT_PARSE_ROOT_NOT_SINGULAR, "0123");  // after zero should be '.' or nothing
    TEST_ERROR(PHOT_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(PHOT_PARSE_ROOT_NOT_SINGULAR, "0x123");
}

static void test_parse_num_too_big(void)
{
    TEST_ERROR(PHOT_PARSE_NUM_TOO_BIG, "1e309");
    TEST_ERROR(PHOT_PARSE_NUM_TOO_BIG, "-1e309");
}

static void test_parse_missing_quotation_mark(void)
{
    TEST_ERROR(PHOT_PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(PHOT_PARSE_MISS_QUOTATION_MARK, "\"abc");
}

static void test_parse_invalid_str_escape(void)
{
    TEST_ERROR(PHOT_PARSE_INVALID_STR_ESCAPE, "\"\\v\"");
    TEST_ERROR(PHOT_PARSE_INVALID_STR_ESCAPE, "\"\\'\"");
    TEST_ERROR(PHOT_PARSE_INVALID_STR_ESCAPE, "\"\\0\"");
    TEST_ERROR(PHOT_PARSE_INVALID_STR_ESCAPE, "\"\\x12\"");
}

static void test_parse_invalid_str_char(void)
{
    TEST_ERROR(PHOT_PARSE_INVALID_STR_CHAR, "\"\x01\"");
    TEST_ERROR(PHOT_PARSE_INVALID_STR_CHAR, "\"\x1F\"");
}

static void test_parse_invalid_unicode_hex(void)
{
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u00/0\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
}

static void test_parse_invalid_unicode_surrogate(void)
{
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_miss_comma_or_square_bracket(void)
{
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1}");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1 2");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[[]");
}

static void test_parse_miss_key(void)
{
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{1:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(PHOT_PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon(void)
{
    TEST_ERROR(PHOT_PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(PHOT_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket(void)
{
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse(void)
{
    test_parse_null();
    test_parse_bool();
    test_parse_num();
    test_parse_str();
    test_parse_arr();
    test_parse_obj();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_num_too_big();
    test_parse_missing_quotation_mark();
    test_parse_invalid_str_escape();
    test_parse_invalid_str_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();
    test_parse_miss_comma_or_square_bracket();
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();
}

#define TEST_ROUNDTRIP(json)                                \
    do {                                                    \
        phot_elem e;                                        \
        phot_init(&e);                                      \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, json)); \
        size_t len;                                         \
        char *json2 = phot_stringify(&e, &len);             \
        EXPECT_EQ_STR(json, json2, len);                    \
        phot_free(&e);                                      \
        free(json2);                                        \
    } while (0)

static void test_stringify_num(void)
{
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");
    TEST_ROUNDTRIP("1.0000000000000002");       // 大于 1 的最小精度
    TEST_ROUNDTRIP("4.9406564584124654e-324");  // 最小次正规数
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  // 最大次正规数
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  // 最小正规数
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  // 最大正规数
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_str(void)
{
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

static void test_stringify_arr(void)
{
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_obj(void)
{
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP(
        "{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify(void)
{
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");
    test_stringify_num();
    test_stringify_str();
    test_stringify_arr();
    test_stringify_obj();
}

#define TEST_EQUAL(json1, json2, equality)                    \
    do {                                                      \
        phot_elem e1, e2;                                     \
        phot_init(&e1);                                       \
        phot_init(&e2);                                       \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e1, json1)); \
        EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e2, json2)); \
        EXPECT_EQ_INT(equality, phot_is_equal(&e1, &e2));     \
        phot_free(&e1);                                       \
        phot_free(&e2);                                       \
    } while (0)

static void test_equal(void)
{
    TEST_EQUAL("null", "null", true);
    TEST_EQUAL("null", "0", false);
    TEST_EQUAL("true", "true", true);
    TEST_EQUAL("true", "false", false);
    TEST_EQUAL("false", "false", true);
    TEST_EQUAL("123", "123", true);
    TEST_EQUAL("123", "456", false);
    TEST_EQUAL("\"abc\"", "\"abc\"", true);
    TEST_EQUAL("\"abc\"", "\"abcd\"", false);
    TEST_EQUAL("[]", "[]", true);
    TEST_EQUAL("[]", "null", false);
    TEST_EQUAL("[1,2,3]", "[1,2,3]", true);
    TEST_EQUAL("[1,2,3]", "[1,2,3,4]", false);
    TEST_EQUAL("[[]]", "[[]]", true);
    TEST_EQUAL("{}", "{}", true);
    TEST_EQUAL("{}", "null", false);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2}", true);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"b\":2,\"a\":1}", true);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":3}", false);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2,\"c\":3}", false);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":{}}}}", true);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":[]}}}", false);
}

static void test_copy(void)
{
    phot_elem e1, e2;
    phot_init(&e1);
    phot_parse(&e1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    phot_init(&e2);
    phot_copy(&e2, &e1);
    EXPECT_TRUE(phot_is_equal(&e2, &e1));
    phot_free(&e1);
    phot_free(&e2);
}

static void test_move(void)
{
    phot_elem e1, e2, e3;
    phot_init(&e1);
    phot_init(&e2);
    phot_init(&e3);
    phot_parse(&e1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    phot_copy(&e2, &e1);
    phot_move(&e3, &e2);
    EXPECT_EQ_INT(PHOT_NULL, phot_get_type(&e2));
    EXPECT_TRUE(phot_is_equal(&e3, &e1));
    phot_free(&e1);
    phot_free(&e2);
    phot_free(&e3);
}

static void test_swap(void)
{
    phot_elem e1, e2;
    phot_init(&e1);
    phot_init(&e2);
    phot_set_str(&e1, "Hello", 5);
    phot_set_str(&e2, "World!", 6);
    phot_swap(&e1, &e2);
    EXPECT_EQ_STR("World!", phot_get_str(&e1), phot_get_str_len(&e1));
    EXPECT_EQ_STR("Hello", phot_get_str(&e2), phot_get_str_len(&e2));
    phot_free(&e1);
    phot_free(&e2);
}

static void test_file(void)
{
    phot_elem *e1 = phot_read_from_file("test.in.json");
    phot_write_to_file(e1, "test.out.json");
    phot_elem *e2 = phot_read_from_file("test.out.json");
    EXPECT_TRUE(phot_is_equal(e1, e2));
    phot_free(e1);
    phot_free(e2);
}

static void test_access_null(void)
{
    phot_elem e;
    phot_init(&e);
    phot_set_str(&e, "a", 1);
    phot_set_null(&e);
    EXPECT_EQ_INT(PHOT_NULL, phot_get_type(&e));
    phot_free(&e);
}

static void test_access_bool(void)
{
    phot_elem e;
    phot_init(&e);
    phot_set_str(&e, "a", 1);
    phot_set_bool(&e, true);
    EXPECT_EQ_BOOL(true, phot_get_bool(&e));
    phot_set_bool(&e, false);
    EXPECT_EQ_BOOL(false, phot_get_bool(&e));
    phot_free(&e);
}

static void test_access_num(void)
{
    phot_elem e;
    phot_init(&e);
    phot_set_str(&e, "a", 1);
    phot_set_num(&e, 123.45);
    EXPECT_EQ_DOUBLE(123.45, phot_get_num(&e));
    phot_free(&e);
}

static void test_access_str(void)
{
    phot_elem e;
    phot_init(&e);
    phot_set_str(&e, "", 0);
    EXPECT_EQ_STR("", phot_get_str(&e), phot_get_str_len(&e));
    phot_set_str(&e, "Hello", 5);
    EXPECT_EQ_STR("Hello", phot_get_str(&e), phot_get_str_len(&e));
    phot_free(&e);
}

static void test_access_arr(void)
{
    phot_elem a, e;
    size_t i, j;

    phot_init(&a);

    for (j = 0; j <= 5; j += 5) {
        phot_set_arr(&a, j);
        EXPECT_EQ_SIZE_T(0, phot_get_arr_len(&a));
        EXPECT_EQ_SIZE_T(j, phot_get_arr_cap(&a));
        for (i = 0; i < 10; i++) {
            phot_init(&e);
            phot_set_num(&e, i);
            phot_move(phot_push_arr(&a), &e);
            phot_free(&e);
        }

        EXPECT_EQ_SIZE_T(10, phot_get_arr_len(&a));
        for (i = 0; i < 10; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));
    }

    phot_pop_arr(&a);
    EXPECT_EQ_SIZE_T(9, phot_get_arr_len(&a));
    for (i = 0; i < 9; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));

    phot_erase_arr(&a, 4, 0);
    EXPECT_EQ_SIZE_T(9, phot_get_arr_len(&a));
    for (i = 0; i < 9; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));

    phot_erase_arr(&a, 8, 1);
    EXPECT_EQ_SIZE_T(8, phot_get_arr_len(&a));
    for (i = 0; i < 8; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));

    phot_erase_arr(&a, 0, 2);
    EXPECT_EQ_SIZE_T(6, phot_get_arr_len(&a));
    for (i = 0; i < 6; i++) EXPECT_EQ_DOUBLE((double)i + 2, phot_get_num(phot_get_arr_elem(&a, i)));

    for (i = 0; i < 2; i++) {
        phot_init(&e);
        phot_set_num(&e, i);
        phot_move(phot_insert_arr(&a, i), &e);
        phot_free(&e);
    }

    EXPECT_EQ_SIZE_T(8, phot_get_arr_len(&a));
    for (i = 0; i < 8; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));

    EXPECT_TRUE(phot_get_arr_cap(&a) > 8);
    phot_shrink_arr(&a);
    EXPECT_EQ_SIZE_T(8, phot_get_arr_cap(&a));
    EXPECT_EQ_SIZE_T(8, phot_get_arr_len(&a));
    for (i = 0; i < 8; i++) EXPECT_EQ_DOUBLE((double)i, phot_get_num(phot_get_arr_elem(&a, i)));

    phot_set_str(&e, "Hello", 5);
    phot_move(phot_push_arr(&a), &e);  // 测试元素是否正确释放
    phot_free(&e);

    i = phot_get_arr_cap(&a);
    phot_clear_arr(&a);
    EXPECT_EQ_SIZE_T(0, phot_get_arr_len(&a));
    EXPECT_EQ_SIZE_T(i, phot_get_arr_cap(&a));  // 测试是否只清空了 size，而没有缩容
    phot_shrink_arr(&a);
    EXPECT_EQ_SIZE_T(0, phot_get_arr_cap(&a));

    phot_free(&a);
}

static void test_access_obj(void)
{
    phot_elem o, v, *pv;
    size_t i, j, index;

    phot_init(&o);

    for (j = 0; j <= 5; j += 5) {
        phot_set_obj(&o, j);
        EXPECT_EQ_SIZE_T(0, phot_get_obj_len(&o));
        EXPECT_EQ_SIZE_T(j, phot_get_obj_cap(&o));
        for (i = 0; i < 10; i++) {
            char key[2] = "a";
            key[0] += i;
            phot_init(&v);
            phot_set_num(&v, i);
            phot_move(phot_set_obj_value(&o, key, 1), &v);
            phot_free(&v);
        }
        EXPECT_EQ_SIZE_T(10, phot_get_obj_len(&o));
        for (i = 0; i < 10; i++) {
            char key[] = "a";
            key[0] += i;
            index = phot_find_obj_index(&o, key, 1);
            EXPECT_TRUE(index != PHOT_KEY_NOT_EXIST);
            pv = phot_get_obj_value(&o, index);
            EXPECT_EQ_DOUBLE((double)i, phot_get_num(pv));
        }
    }

    index = phot_find_obj_index(&o, "j", 1);
    EXPECT_TRUE(index != PHOT_KEY_NOT_EXIST);
    phot_remove_obj_value(&o, index);
    index = phot_find_obj_index(&o, "j", 1);
    EXPECT_TRUE(index == PHOT_KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(9, phot_get_obj_len(&o));

    index = phot_find_obj_index(&o, "a", 1);
    EXPECT_TRUE(index != PHOT_KEY_NOT_EXIST);
    phot_remove_obj_value(&o, index);
    index = phot_find_obj_index(&o, "a", 1);
    EXPECT_TRUE(index == PHOT_KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(8, phot_get_obj_len(&o));

    EXPECT_TRUE(phot_get_obj_cap(&o) > 8);
    phot_shrink_obj(&o);
    EXPECT_EQ_SIZE_T(8, phot_get_obj_cap(&o));
    EXPECT_EQ_SIZE_T(8, phot_get_obj_len(&o));
    for (i = 0; i < 8; i++) {
        char key[] = "a";
        key[0] += i + 1;
        EXPECT_EQ_DOUBLE((double)i + 1, phot_get_num(phot_get_obj_value(&o, phot_find_obj_index(&o, key, 1))));
    }

    phot_set_str(&v, "Hello", 5);
    phot_move(phot_set_obj_value(&o, "World", 5), &v);
    phot_free(&v);

    pv = phot_find_obj_value(&o, "World", 5);
    EXPECT_TRUE(pv != NULL);
    EXPECT_EQ_STR("Hello", phot_get_str(pv), phot_get_str_len(pv));

    i = phot_get_obj_cap(&o);
    phot_clear_obj(&o);
    EXPECT_EQ_SIZE_T(0, phot_get_obj_len(&o));
    EXPECT_EQ_SIZE_T(i, phot_get_obj_cap(&o));
    phot_shrink_obj(&o);
    EXPECT_EQ_SIZE_T(0, phot_get_obj_cap(&o));

    phot_free(&o);
}

static void test_access(void)
{
    test_access_null();
    test_access_bool();
    test_access_num();
    test_access_str();
    test_access_arr();
    test_access_obj();
}

int main(void)
{
    test_parse();
    test_stringify();
    test_equal();
    test_copy();
    test_move();
    test_swap();
    test_file();
    test_access();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
