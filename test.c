#include <stdio.h>
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

// static void test_parse_arr(void)
// {
//     phot_elem e;
//     phot_init(&e);
//     EXPECT_EQ_INT(PHOT_PARSE_OK, phot_parse(&e, "[ ]"));
//     EXPECT_EQ_INT(PHOT_ARR, phot_get_type(&e));
//     EXPECT_EQ_SIZE_T(0, phot_get_arr_size(&e));
//     phot_free(&e);
// }

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

static void test_parse(void)
{
    test_parse_null();
    test_parse_bool();
    test_parse_num();
    test_parse_str();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_num_too_big();
    test_parse_missing_quotation_mark();
    test_parse_invalid_str_escape();
    test_parse_invalid_str_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();
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

static void test_access(void)
{
    test_access_null();
    test_access_bool();
    test_access_num();
    test_access_str();
}

int main(void)
{
    test_parse();
    test_access();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}
