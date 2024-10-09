#ifndef PHOTJSON_H_
#define PHOTJSON_H_

#include <stddef.h>

#define PHOT_KEY_NOT_EXIST ((size_t) - 1)

typedef enum { PHOT_NULL, PHOT_BOOL, PHOT_NUM, PHOT_STR, PHOT_ARR, PHOT_OBJ } phot_type;
typedef struct phot_elem phot_elem;
typedef struct phot_member phot_member;

struct phot_elem {
    union {
        bool boolean;
        double num;
        struct {
            char *str;    // 字符串
            size_t slen;  // 长度
        };
        struct {
            phot_elem *arr;  // 数组
            size_t acap;     // 容量
            size_t alen;     // 元素个数
        };  // 数组里保存的叫元素
        struct {
            phot_member *obj;  // 对象
            size_t ocap;       // 容量
            size_t olen;       // 成员个数
        };  // 对象里保存的叫成员
    };
    phot_type type;
};

struct phot_member {
    char *key;
    phot_elem value;
    size_t klen;
};  // 成员本身是键值对

// enum 会自动声明为连续的常量，故在 C 中常用这种方式来声明一组常量
enum {
    PHOT_PARSE_OK = 0,
    PHOT_PARSE_EXPECT_VALUE,  // 预期值缺失
    PHOT_PARSE_INVALID_VALUE,
    PHOT_PARSE_ROOT_NOT_SINGULAR,
    PHOT_PARSE_NUM_TOO_BIG,
    PHOT_PARSE_MISS_QUOTATION_MARK,
    PHOT_PARSE_INVALID_STR_ESCAPE,
    PHOT_PARSE_INVALID_STR_CHAR,
    PHOT_PARSE_INVALID_UNICODE_HEX,
    PHOT_PARSE_INVALID_UNICODE_SURROGATE,
    PHOT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    PHOT_PARSE_MISS_KEY,
    PHOT_PARSE_MISS_COLON,
    PHOT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,
};


#define phot_init(e) (e)->type = PHOT_NULL

int phot_parse(phot_elem *e, const char *json);
char *phot_stringify(const phot_elem *e, size_t *length);

void phot_copy(phot_elem *dst, const phot_elem *src);
void phot_move(phot_elem *dst, phot_elem *src);
void phot_swap(phot_elem *lhs, phot_elem *rhs);

void phot_free(phot_elem *e);

phot_type phot_get_type(const phot_elem *e);
int phot_is_equal(const phot_elem *lhs, const phot_elem *rhs);

#define phot_set_null(e) phot_free(e)

void phot_set_bool(phot_elem *e, bool boolean);
bool phot_get_bool(const phot_elem *e);

void phot_set_num(phot_elem *e, double num);
double phot_get_num(const phot_elem *e);

void phot_set_str(phot_elem *e, const char *str, size_t len);
const char *phot_get_str(const phot_elem *e);
size_t phot_get_str_len(const phot_elem *e);

void phot_set_arr(phot_elem *e, size_t cap);
size_t phot_get_arr_len(const phot_elem *e);
size_t phot_get_arr_cap(const phot_elem *e);
void phot_reserve_arr(phot_elem *e, size_t cap);
void phot_shrink_arr(phot_elem *e);
void phot_clear_arr(phot_elem *e);
phot_elem *phot_get_arr_elem(const phot_elem *e, size_t index);
phot_elem *phot_push_arr(phot_elem *e);
void phot_pop_arr(phot_elem *e);
phot_elem *phot_insert_arr(phot_elem *e, size_t index);
void phot_erase_arr(phot_elem *e, size_t index, size_t count);

void phot_set_obj(phot_elem *e, size_t cap);
size_t phot_get_obj_len(const phot_elem *e);
size_t phot_get_obj_cap(const phot_elem *e);
void phot_reserve_obj(phot_elem *e, size_t cap);
void phot_shrink_obj(phot_elem *e);
void phot_clear_obj(phot_elem *e);
const char *phot_get_obj_key(const phot_elem *e, size_t index);
size_t phot_get_obj_key_len(const phot_elem *e, size_t index);
phot_elem *phot_get_obj_value(const phot_elem *e, size_t index);
size_t phot_find_obj_index(const phot_elem *e, const char *key, size_t klen);
phot_elem *phot_find_obj_value(const phot_elem *e, const char *key, size_t klen);
phot_elem *phot_set_obj_value(phot_elem *e, const char *key, size_t klen);
void phot_remove_obj_value(phot_elem *e, size_t index);

#endif  // PHOTJSON_H_
