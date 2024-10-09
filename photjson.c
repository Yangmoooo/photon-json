#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "photjson.h"

#ifndef PHOT_PARSE_STACK_INIT_SIZE
#define PHOT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef PHOT_PARSE_STRINGIFY_INIT_SIZE
#define PHOT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

// 留着优化用
#define LIKELY(x) __builtin_expect(!!(x), 1)    // x 很可能为真
#define UNLIKELY(x) __builtin_expect(!!(x), 0)  // x 很可能为假

typedef struct {
    const char *json;
    char *stack;
    size_t size, top;
} phot_context;


static inline void expect(phot_context *c, char ch)
{
    assert(*c->json == ch);
    c->json++;
}

// digit 指单个的数字，number 指一个抽象的数
static inline bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }

static inline bool is_digit_1to9(char ch) { return ch >= '1' && ch <= '9'; }

// 虚假的 push，只分配了空间，还需手动把东西压进去
static void *phot_context_push(phot_context *c, size_t size)
{
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = PHOT_PARSE_STACK_INIT_SIZE;
        }
        while (c->top + size > c->size) {
            c->size += c->size >> 1;  // 将 c->size 增加到原来的 1.5 倍
        }
        c->stack = (char *)realloc(c->stack, c->size);
        assert(c->stack != NULL);
    }
    void *ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static inline void *phot_context_pop(phot_context *c, size_t size)
{
    assert(c->top >= size);
    c->top -= size;
    return c->stack + c->top;
}

static inline void phot_push_ch(phot_context *c, char ch) { *(char *)phot_context_push(c, sizeof(char)) = ch; }

static inline void phot_push_str(phot_context *c, const char *str, size_t len)
{
    memcpy(phot_context_push(c, len), str, len);
}

static void phot_parse_whitespace(phot_context *c)
{
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

static int phot_parse_null(phot_context *c, phot_elem *e)
{
    if (strncmp(c->json, "null", 4) != 0) return PHOT_PARSE_INVALID_VALUE;
    c->json += 4;
    e->type = PHOT_NULL;
    return PHOT_PARSE_OK;
}

static int phot_parse_bool(phot_context *c, phot_elem *e)
{
    const char *p = c->json;
    bool value;
    if (*p == 't') {
        if (strncmp(p, "true", 4) != 0) return PHOT_PARSE_INVALID_VALUE;
        value = true;
        c->json += 4;
    } else if (*p == 'f') {
        if (strncmp(p, "false", 5) != 0) return PHOT_PARSE_INVALID_VALUE;
        value = false;
        c->json += 5;
    } else {
        return PHOT_PARSE_INVALID_VALUE;
    }
    e->boolean = value;
    e->type = PHOT_BOOL;
    return PHOT_PARSE_OK;
}

static int phot_parse_num(phot_context *c, phot_elem *e)
{
    const char *p = c->json;
    if (*p == '-') {
        p++;
    }
    if (*p == '0') {
        p++;
    } else {
        if (!is_digit_1to9(*p)) return PHOT_PARSE_INVALID_VALUE;
        for (p++; is_digit(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!is_digit(*p)) return PHOT_PARSE_INVALID_VALUE;
        for (p++; is_digit(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') {
            p++;
        }
        if (!is_digit(*p)) return PHOT_PARSE_INVALID_VALUE;
        for (p++; is_digit(*p); p++);
    }
    // 至此说明数字格式正确
    errno = 0;
    e->num = strtod(c->json, NULL);
    if (errno == ERANGE && (e->num == HUGE_VAL || e->num == -HUGE_VAL)) return PHOT_PARSE_NUM_TOO_BIG;
    c->json = p;
    e->type = PHOT_NUM;
    return PHOT_PARSE_OK;
}

static const char *phot_parse_hex4(const char *p, uint32_t *u)
{
    *u = 0;
    for (int i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if (ch >= '0' && ch <= '9') {
            *u |= ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
            *u |= ch - ('A' - 10);
        } else if (ch >= 'a' && ch <= 'f') {
            *u |= ch - ('a' - 10);
        } else
            return NULL;
    }
    return p;
}

static void phot_encode_utf8(phot_context *c, uint32_t u)
{
    if (u <= 0x7F) {
        phot_push_ch(c, u & 0xFF);
    } else if (u <= 0x7FF) {
        phot_push_ch(c, 0xC0 | ((u >> 6) & 0xFF));
        phot_push_ch(c, 0x80 | (u & 0x3F));
    } else if (u <= 0xFFFF) {
        phot_push_ch(c, 0xE0 | ((u >> 12) & 0xFF));
        phot_push_ch(c, 0x80 | ((u >> 6) & 0x3F));
        phot_push_ch(c, 0x80 | (u & 0x3F));
    } else {
        assert(u <= 0x10FFFF);
        phot_push_ch(c, 0xF0 | ((u >> 18) & 0xFF));
        phot_push_ch(c, 0x80 | ((u >> 12) & 0x3F));
        phot_push_ch(c, 0x80 | ((u >> 6) & 0x3F));
        phot_push_ch(c, 0x80 | (u & 0x3F));
    }
}

#define STR_ERROR(ret)        \
    do {                      \
        c->top = initial_top; \
        return ret;           \
    } while (0)

static int phot_parse_str_raw(phot_context *c, char **str, size_t *len)
{
    const size_t initial_top = c->top;
    expect(c, '"');
    const char *const start = c->json;
    const char *p = c->json;

    // 快速扫描无需转义的部分
    while (*p != '"' && *p != '\\' && *p != '\0' && (unsigned char)*p >= 0x20) {
        p++;
    }
    ptrdiff_t prelen = p - start;
    // 若整个字符串都不需要特殊处理
    if (*p == '"') {
        *len = prelen;
        *str = (char *)start;
        c->json = p + 1;
        return PHOT_PARSE_OK;
    }
    // 若存在需要特殊处理的字符
    if (prelen > 0) {
        // 将无需处理的部分先保存到栈里
        memcpy(phot_context_push(c, prelen), start, prelen);
    }
    while (1) {
        uint32_t u;
        char ch = *p++;
        switch (ch) {
            case '"':
                *len = c->top - initial_top;
                *str = (char *)phot_context_pop(c, *len);
                c->json = p;
                return PHOT_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '"':
                        phot_push_ch(c, '"');
                        break;
                    case '\\':
                        phot_push_ch(c, '\\');
                        break;
                    case '/':
                        phot_push_ch(c, '/');
                        break;
                    case 'b':
                        phot_push_ch(c, '\b');
                        break;
                    case 'f':
                        phot_push_ch(c, '\f');
                        break;
                    case 'n':
                        phot_push_ch(c, '\n');
                        break;
                    case 'r':
                        phot_push_ch(c, '\r');
                        break;
                    case 't':
                        phot_push_ch(c, '\t');
                        break;
                    case 'u':
                        if ((p = phot_parse_hex4(p, &u)) == NULL) {
                            STR_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX);
                        }
                        if (u >= 0xD800 && u <= 0xDBFF) {  // 处理代理对
                            if (*p++ != '\\' || *p++ != 'u') {
                                STR_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            uint32_t u2;
                            if ((p = phot_parse_hex4(p, &u2)) == NULL) {
                                STR_ERROR(PHOT_PARSE_INVALID_UNICODE_HEX);
                            }
                            if (u2 < 0xDC00 || u2 > 0xDFFF) {
                                STR_ERROR(PHOT_PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        phot_encode_utf8(c, u);
                        break;
                    default:
                        STR_ERROR(PHOT_PARSE_INVALID_STR_ESCAPE);
                }
                break;
            case '\0':
                STR_ERROR(PHOT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20) {
                    STR_ERROR(PHOT_PARSE_INVALID_STR_CHAR);
                }
                phot_push_ch(c, ch);
        }
    }
}

static int phot_parse_str(phot_context *c, phot_elem *e)
{
    char *str;
    size_t len;
    int ret = phot_parse_str_raw(c, &str, &len);
    if (ret == PHOT_PARSE_OK) {
        phot_set_str(e, str, len);
    }
    return ret;
}

static int phot_parse_value(phot_context *c, phot_elem *e);

static int phot_parse_arr(phot_context *c, phot_elem *e)
{
    expect(c, '[');
    phot_parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        phot_set_arr(e, 0);
        return PHOT_PARSE_OK;
    }
    int ret;
    size_t len = 0;
    while (1) {
        phot_elem elem;
        phot_init(&elem);
        if ((ret = phot_parse_value(c, &elem)) != PHOT_PARSE_OK) {
            break;
        }
        memcpy(phot_context_push(c, sizeof(phot_elem)), &elem, sizeof(phot_elem));
        len++;
        phot_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            phot_parse_whitespace(c);
        } else if (*c->json == ']') {
            c->json++;
            phot_set_arr(e, len);
            memcpy(e->arr, phot_context_pop(c, len * sizeof(phot_elem)), len * sizeof(phot_elem));
            e->alen = len;
            return PHOT_PARSE_OK;
        } else {
            ret = PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    // 清理 context 上的临时栈
    for (size_t i = 0; i < len; i++) {
        phot_free((phot_elem *)phot_context_pop(c, sizeof(phot_elem)));
    }
    return ret;
}

static int phot_parse_obj(phot_context *c, phot_elem *e)
{
    expect(c, '{');
    phot_parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        phot_set_obj(e, 0);
        return PHOT_PARSE_OK;
    }
    int ret;
    size_t len = 0;
    phot_member m;
    m.key = NULL;
    while (1) {
        char *str;
        phot_init(&m.value);
        // 解析成员键
        if (*c->json != '"') {
            ret = PHOT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = phot_parse_str_raw(c, &str, &m.klen)) != PHOT_PARSE_OK) {
            break;
        }
        memcpy(m.key = (char *)malloc(m.klen + 1), str, m.klen);
        m.key[m.klen] = '\0';
        // 解析冒号及前后空白
        phot_parse_whitespace(c);
        if (*c->json != ':') {
            ret = PHOT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        phot_parse_whitespace(c);
        // 解析成员值
        if ((ret = phot_parse_value(c, &m.value)) != PHOT_PARSE_OK) {
            break;
        }
        memcpy(phot_context_push(c, sizeof(phot_member)), &m, sizeof(phot_member));
        len++;
        m.key = NULL;  // 此时 m.key 的所有权已经转移到 context 栈上
        // 解析下一个成员或结束
        phot_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            phot_parse_whitespace(c);
        } else if (*c->json == '}') {
            c->json++;
            phot_set_obj(e, len);
            memcpy(e->obj, phot_context_pop(c, len * sizeof(phot_member)), len * sizeof(phot_member));
            e->olen = len;
            return PHOT_PARSE_OK;
        } else {
            ret = PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    // 将 context 里的成员出栈并释放
    free(m.key);
    for (size_t i = 0; i < len; i++) {
        phot_member *member = (phot_member *)phot_context_pop(c, sizeof(phot_member));
        free(member->key);
        phot_free(&member->value);
    }
    e->type = PHOT_NULL;
    return ret;
}

static int phot_parse_value(phot_context *c, phot_elem *e)
{
    switch (*c->json) {
        case '"':
            return phot_parse_str(c, e);
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '-':
            return phot_parse_num(c, e);
        case '[':
            return phot_parse_arr(c, e);
        case '{':
            return phot_parse_obj(c, e);
        case 't':
        case 'f':
            return phot_parse_bool(c, e);
        case 'n':
            return phot_parse_null(c, e);
        case '\0':
            return PHOT_PARSE_EXPECT_VALUE;
        default:
            return PHOT_PARSE_INVALID_VALUE;
    }
}

int phot_parse(phot_elem *e, const char *json)
{
    assert(e != NULL);
    int ret;
    phot_context c;
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    phot_init(e);
    phot_parse_whitespace(&c);
    if ((ret = phot_parse_value(&c, e)) == PHOT_PARSE_OK) {
        phot_parse_whitespace(&c);
        if (*c.json != '\0') {
            e->type = PHOT_NULL;
            ret = PHOT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

static void phot_stringify_str(phot_context *c, const char *str, size_t len)
{
    static const char hex_digits[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };
    assert(str != NULL);
    size_t size = len * 6 + 2;  // 每个字符最多占 6 个字符和 2 个引号
    char *head = phot_context_push(c, size);
    char *p = head;
    *p++ = '"';
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)str[i];
        switch (ch) {
            case '\"':
                *p++ = '\\';
                *p++ = '\"';
                break;
            case '\\':
                *p++ = '\\';
                *p++ = '\\';
                break;
            case '\b':
                *p++ = '\\';
                *p++ = 'b';
                break;
            case '\f':
                *p++ = '\\';
                *p++ = 'f';
                break;
            case '\n':
                *p++ = '\\';
                *p++ = 'n';
                break;
            case '\r':
                *p++ = '\\';
                *p++ = 'r';
                break;
            case '\t':
                *p++ = '\\';
                *p++ = 't';
                break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\';
                    *p++ = 'u';
                    *p++ = '0';
                    *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 0xF];
                } else {
                    *p++ = ch;
                }
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

static void phot_stringify_value(phot_context *c, const phot_elem *e)
{
    switch (e->type) {
        case PHOT_NULL:
            phot_push_str(c, "null", 4);
            break;
        case PHOT_BOOL:
            phot_push_str(c, e->boolean ? "true" : "false", e->boolean ? 4 : 5);
            break;
        case PHOT_NUM:
            // 将数字转换为字符串放入栈中，sprintf 返回的是写入的字符数
            c->top -= 32 - sprintf(phot_context_push(c, 32), "%.17g", e->num);
            break;
        case PHOT_STR:
            phot_stringify_str(c, e->str, e->slen);
            break;
        case PHOT_ARR:
            phot_push_ch(c, '[');
            for (size_t i = 0; i < e->alen; i++) {
                if (i > 0) {
                    phot_push_ch(c, ',');
                }
                phot_stringify_value(c, &e->arr[i]);
            }
            phot_push_ch(c, ']');
            break;
        case PHOT_OBJ:
            phot_push_ch(c, '{');
            for (size_t i = 0; i < e->olen; i++) {
                if (i > 0) {
                    phot_push_ch(c, ',');
                }
                phot_stringify_str(c, e->obj[i].key, e->obj[i].klen);
                phot_push_ch(c, ':');
                phot_stringify_value(c, &e->obj[i].value);
            }
            phot_push_ch(c, '}');
            break;
        default:
            assert(0 && "invalid type");
    }
}

char *phot_stringify(const phot_elem *e, size_t *len)
{
    assert(e != NULL);
    phot_context c;
    c.size = PHOT_PARSE_STRINGIFY_INIT_SIZE;
    c.stack = (char *)malloc(c.size);
    c.top = 0;
    phot_stringify_value(&c, e);
    if (len != NULL) {
        *len = c.top;
    }
    phot_push_ch(&c, '\0');
    return c.stack;
}

void phot_copy(phot_elem *dst, const phot_elem *src)
{
    assert(dst != NULL && src != NULL && dst != src);
    phot_free(dst);
    switch (src->type) {
        case PHOT_STR:
            phot_set_str(dst, src->str, src->slen);
            break;
        case PHOT_ARR:
            phot_set_arr(dst, src->alen);
            for (size_t i = 0; i < src->alen; i++) {
                phot_copy(&dst->arr[i], &src->arr[i]);
            }
            dst->alen = src->alen;
            break;
        case PHOT_OBJ:
            phot_set_obj(dst, src->olen);
            for (size_t i = 0; i < src->olen; i++) {
                phot_elem *value = phot_set_obj_value(dst, src->obj[i].key, src->obj[i].klen);
                phot_copy(value, &src->obj[i].value);
            }
            break;
        default:
            memcpy(dst, src, sizeof(phot_elem));
    }
}

void phot_move(phot_elem *dst, phot_elem *src)
{
    assert(dst != NULL && src != NULL && dst != src);
    phot_free(dst);
    memcpy(dst, src, sizeof(phot_elem));
    phot_init(src);
}

void phot_swap(phot_elem *lhs, phot_elem *rhs)
{
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        phot_elem tmp;
        memcpy(&tmp, lhs, sizeof(phot_elem));
        memcpy(lhs, rhs, sizeof(phot_elem));
        memcpy(rhs, &tmp, sizeof(phot_elem));
    }
}

void phot_free(phot_elem *e)
{
    assert(e != NULL);
    switch (e->type) {
        case PHOT_STR:
            free(e->str);
            break;
        case PHOT_ARR:
            for (size_t i = 0; i < e->alen; i++) {
                phot_free(&e->arr[i]);
            }
            free(e->arr);
            break;
        case PHOT_OBJ:
            for (size_t i = 0; i < e->olen; i++) {
                free(e->obj[i].key);
                phot_free(&e->obj[i].value);
            }
            free(e->obj);
            break;
        default:
            break;
    }
    e->type = PHOT_NULL;
}

phot_type phot_get_type(const phot_elem *e)
{
    assert(e != NULL);
    return e->type;
}

int phot_is_equal(const phot_elem *lhs, const phot_elem *rhs)
{
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type) return 0;
    switch (lhs->type) {
        case PHOT_NUM:
            return lhs->num == rhs->num;
        case PHOT_STR:
            return lhs->slen == rhs->slen && memcmp(lhs->str, rhs->str, lhs->slen) == 0;
        case PHOT_ARR:
            if (lhs->alen != rhs->alen) return 0;
            for (size_t i = 0; i < lhs->alen; i++) {
                if (!phot_is_equal(&lhs->arr[i], &rhs->arr[i])) return 0;
            }
            return 1;
        case PHOT_OBJ:
            if (lhs->olen != rhs->olen) return 0;
            for (size_t i = 0; i < lhs->olen; i++) {
                phot_elem *value = phot_find_obj_value(lhs, rhs->obj[i].key, rhs->obj[i].klen);
                if (value == NULL || !phot_is_equal(value, &rhs->obj[i].value)) return 0;
            }
            return 1;
        case PHOT_BOOL:
            return lhs->boolean == rhs->boolean;
        default:
            return 1;
    }
}

void phot_set_bool(phot_elem *e, bool boolean)
{
    phot_free(e);
    e->boolean = boolean;
    e->type = PHOT_BOOL;
}

bool phot_get_bool(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_BOOL);
    return e->boolean;
}

void phot_set_num(phot_elem *e, double num)
{
    phot_free(e);
    e->num = num;
    e->type = PHOT_NUM;
}

double phot_get_num(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_NUM);
    return e->num;
}

void phot_set_str(phot_elem *e, const char *str, size_t len)
{
    // 元素 e 不能为空，字符串 str 可以为空但长度 len 必须为 0
    assert(e != NULL && (str != NULL || len == 0));
    phot_free(e);
    e->str = (char *)malloc(len + 1);
    assert(e->str != NULL);
    memcpy(e->str, str, len);
    e->str[len] = '\0';
    e->slen = len;
    e->type = PHOT_STR;
}

const char *phot_get_str(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_STR);
    return e->str;
}

size_t phot_get_str_len(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_STR);
    return e->slen;
}

void phot_set_arr(phot_elem *e, size_t cap)
{
    assert(e != NULL);
    phot_free(e);
    e->arr = cap > 0 ? (phot_elem *)malloc(cap * sizeof(phot_elem)) : NULL;
    e->alen = 0;
    e->acap = cap;
    e->type = PHOT_ARR;
}

size_t phot_get_arr_len(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR);
    return e->alen;
}

size_t phot_get_arr_cap(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR);
    return e->acap;
}

void phot_reserve_arr(phot_elem *e, size_t cap)
{
    assert(e != NULL && e->type == PHOT_ARR);
    if (cap > e->acap) {
        e->arr = (phot_elem *)realloc(e->arr, cap * sizeof(phot_elem));
        e->acap = cap;
    }
}

void phot_shrink_arr(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR);
    if (e->alen < e->acap) {
        e->arr = (phot_elem *)realloc(e->arr, e->alen * sizeof(phot_elem));
        e->acap = e->alen;
    }
}

void phot_clear_arr(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR);
    phot_erase_arr(e, 0, e->alen);
}

phot_elem *phot_get_arr_elem(const phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_ARR);
    assert(index < e->alen);
    return &e->arr[index];
}

// 同样是虚假的 push，返回指向新元素的指针，还需手动写入元素和长度
// 目的是保持灵活性并减少开销，后同
phot_elem *phot_push_arr(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR);
    if (e->alen == e->acap) {
        phot_reserve_arr(e, e->acap == 0 ? 1 : e->acap * 2);
    }
    phot_init(&e->arr[e->alen]);
    return &e->arr[e->alen++];
}

void phot_pop_arr(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_ARR && e->alen > 0);
    phot_free(&e->arr[--e->alen]);
}

phot_elem *phot_insert_arr(phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_ARR && index <= e->alen);
    if (e->alen == e->acap) {
        phot_reserve_arr(e, e->acap == 0 ? 1 : e->acap * 2);
    }
    memmove(&e->arr[index + 1], &e->arr[index], (e->alen - index) * sizeof(phot_elem));
    phot_init(&e->arr[index]);
    e->alen++;
    return &e->arr[index];
}

void phot_erase_arr(phot_elem *e, size_t index, size_t count)
{
    assert(e != NULL && e->type == PHOT_ARR && index + count <= e->alen);
    for (size_t i = index; i < index + count; i++) {
        phot_free(&e->arr[i]);
    }
    memmove(&e->arr[index], &e->arr[index + count], (e->alen - index - count) * sizeof(phot_elem));
    for (size_t i = e->alen - count; i < e->alen; i++) {
        phot_init(&e->arr[i]);
    }
    e->alen -= count;
}

void phot_set_obj(phot_elem *e, size_t cap)
{
    assert(e != NULL);
    phot_free(e);
    e->obj = cap > 0 ? (phot_member *)malloc(cap * sizeof(phot_member)) : NULL;
    e->olen = 0;
    e->ocap = cap;
    e->type = PHOT_OBJ;
}

size_t phot_get_obj_len(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    return e->olen;
}

size_t phot_get_obj_cap(const phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    return e->ocap;
}

void phot_reserve_obj(phot_elem *e, size_t cap)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    if (cap > e->ocap) {
        e->obj = (phot_member *)realloc(e->obj, cap * sizeof(phot_member));
        e->ocap = cap;
    }
}

void phot_shrink_obj(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    if (e->olen < e->ocap) {
        e->obj = (phot_member *)realloc(e->obj, e->olen * sizeof(phot_member));
        e->ocap = e->olen;
    }
}

void phot_clear_obj(phot_elem *e)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    for (size_t i = 0; i < e->olen; i++) {
        free(e->obj[i].key);
        phot_free(&e->obj[i].value);
    }
    e->olen = 0;
}

const char *phot_get_obj_key(const phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    assert(index < e->olen);
    return e->obj[index].key;
}

size_t phot_get_obj_key_len(const phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    assert(index < e->olen);
    return e->obj[index].klen;
}

phot_elem *phot_get_obj_value(const phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_OBJ);
    assert(index < e->olen);
    return &e->obj[index].value;
}

size_t phot_find_obj_index(const phot_elem *e, const char *key, size_t klen)
{
    assert(e != NULL && e->type == PHOT_OBJ && key != NULL);
    for (size_t i = 0; i < e->olen; i++) {
        if (e->obj[i].klen == klen && memcmp(e->obj[i].key, key, klen) == 0) {
            return i;
        }
    }
    return PHOT_KEY_NOT_EXIST;
}

phot_elem *phot_find_obj_value(const phot_elem *e, const char *key, size_t klen)
{
    assert(e != NULL && e->type == PHOT_OBJ && key != NULL);
    size_t index = phot_find_obj_index(e, key, klen);
    return index == PHOT_KEY_NOT_EXIST ? NULL : &e->obj[index].value;
}

phot_elem *phot_set_obj_value(phot_elem *e, const char *key, size_t klen)
{
    assert(e != NULL && e->type == PHOT_OBJ && key != NULL);
    size_t index = phot_find_obj_index(e, key, klen);
    if (index == PHOT_KEY_NOT_EXIST) {
        if (e->olen == e->ocap) {
            phot_reserve_obj(e, e->ocap == 0 ? 1 : e->ocap * 2);
        }
        index = e->olen++;
        e->obj[index].key = (char *)malloc(klen + 1);
        memcpy(e->obj[index].key, key, klen);
        e->obj[index].key[klen] = '\0';
        e->obj[index].klen = klen;
        phot_init(&e->obj[index].value);
    }
    return &e->obj[index].value;
}

void phot_remove_obj_value(phot_elem *e, size_t index)
{
    assert(e != NULL && e->type == PHOT_OBJ && index < e->olen);
    free(e->obj[index].key);
    phot_free(&e->obj[index].value);
    if (index < e->olen - 1) {
        memmove(&e->obj[index], &e->obj[index + 1], (e->olen - index - 1) * sizeof(phot_member));
    }
    e->olen--;
}
