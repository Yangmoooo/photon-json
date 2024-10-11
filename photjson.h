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

/**
 * @brief 初始化元素，即将其类型设为 PHOT_NULL
 * @param e 待初始化的元素
 */
#define phot_init(e) (e)->type = PHOT_NULL

/**
 * @brief 将 JSON 文本解析为元素
 * @param e 待解析的元素
 * @param json JSON 文本
 * @return 解析出的枚举值
 */
int phot_parse(phot_elem *e, const char *json);
/**
 * @brief 将元素序列化为 JSON 文本
 * @param e 待序列化的元素
 * @param length JSON 文本的长度
 * @return JSON 文本
 */
char *phot_stringify(const phot_elem *e, size_t *length);
/**
 * @brief 将 JSON 文件读取为元素
 * @param filename 文件名
 * @return 读取到的元素
 */
phot_elem *phot_read_from_file(const char *filename);
/**
 * @brief 将元素保存至 JSON 文件
 * @param e 待写入的元素
 * @param filename 文件名
 * @return 写入结果
 */
int phot_write_to_file(const phot_elem *e, const char *filename);

/**
 * @brief 复制元素，即深拷贝
 * @param dst 目标元素
 * @param src 源元素
 */
void phot_copy(phot_elem *dst, const phot_elem *src);
/**
 * @brief 移动元素，与浅拷贝不同，src 会被置为 NULL
 * @param dst 目标元素
 * @param src 源元素
 */
void phot_move(phot_elem *dst, phot_elem *src);
/**
 * @brief 交换两个元素的内容
 * @param lhs 左元素
 * @param rhs 右元素
 */
void phot_swap(phot_elem *lhs, phot_elem *rhs);

/**
 * @brief 释放元素占用的资源
 * @param e 待释放的元素
 */
void phot_free(phot_elem *e);

/**
 * @brief 获取元素的类型
 * @param e 元素
 * @return 元素类型
 */
phot_type phot_get_type(const phot_elem *e);
/**
 * @brief 判断两个元素是否相等
 * @param lhs 左元素
 * @param rhs 右元素
 * @return 是否相等
 */
bool phot_is_equal(const phot_elem *lhs, const phot_elem *rhs);

/**
 * @brief 设置元素为 null，实际上是释放其占用的资源
 * @param e 待设置的元素
 */
#define phot_set_null(e) phot_free(e)

/**
 * @brief 设置布尔元素的值
 * @param e 待设置的元素
 * @param boolean 布尔值
 */
void phot_set_bool(phot_elem *e, bool boolean);
/**
 * @brief 获取布尔元素的值
 * @param e 目标元素
 * @return 布尔值
 */
bool phot_get_bool(const phot_elem *e);

/**
 * @brief 设置数字元素的值
 * @param e 待设置的元素
 * @param num 数字值
 */
void phot_set_num(phot_elem *e, double num);
/**
 * @brief 获取数字元素的值
 * @param e 目标元素
 * @return 数字值
 */
double phot_get_num(const phot_elem *e);

/**
 * @brief 设置字符串元素的值
 * @param e 待设置的元素
 * @param str 字符串
 * @param len 长度
 */
void phot_set_str(phot_elem *e, const char *str, size_t len);
/**
 * @brief 获取字符串元素的值
 * @param e 目标元素
 * @return 字符串
 */
const char *phot_get_str(const phot_elem *e);
/**
 * @brief 获取字符串元素的长度
 * @param e 目标元素
 * @return 字符串长度
 */
size_t phot_get_str_len(const phot_elem *e);

/**
 * @brief 设置数组元素的值
 * @param e 待设置的元素
 * @param cap 指定的容量
 */
void phot_set_arr(phot_elem *e, size_t cap);
/**
 * @brief 获取数组元素的长度
 * @param e 目标元素
 * @return 数组长度
 */
size_t phot_get_arr_len(const phot_elem *e);
/**
 * @brief 获取数组元素的容量
 * @param e 目标元素
 * @return 数组容量
 */
size_t phot_get_arr_cap(const phot_elem *e);
/**
 * @brief 将数组元素的容量扩充至 cap
 * @param e 目标元素
 * @param cap 指定的容量
 */
void phot_reserve_arr(phot_elem *e, size_t cap);
/**
 * @brief 将数组元素的容量收缩至实际长度
 * @param e 目标元素
 */
void phot_shrink_arr(phot_elem *e);
/**
 * @brief 清空数组元素
 * @param e 目标元素
 */
void phot_clear_arr(phot_elem *e);
/**
 * @brief 获取数组元素中 index 处的元素
 * @param e 目标元素
 * @param index 索引
 * @return 取得的元素
 */
phot_elem *phot_get_arr_elem(const phot_elem *e, size_t index);
/**
 * @brief 在数组尾部添加一个元素，未实际写入
 * @param e 目标元素
 * @return 待添加元素的目标地址
 */
phot_elem *phot_push_arr(phot_elem *e);
/**
 * @brief 从数组尾部删除一个元素
 * @param e 目标元素
 */
void phot_pop_arr(phot_elem *e);
/**
 * @brief 在数组 index 处插入一个元素，未实际写入
 * @param e 目标元素
 * @param index 索引
 * @return 待插入元素的目标地址
 */
phot_elem *phot_insert_arr(phot_elem *e, size_t index);
/**
 * @brief 从数组中 index 处删除 count 个元素
 * @param e 目标元素
 * @param index 索引
 * @param count 数量
 */
void phot_erase_arr(phot_elem *e, size_t index, size_t count);

/**
 * @brief 设置对象元素的值
 * @param e 目标元素
 * @param cap 指定的容量
 */
void phot_set_obj(phot_elem *e, size_t cap);
/**
 * @brief 获取对象元素的长度
 * @param e 目标元素
 * @return 取得的对象长度
 */
size_t phot_get_obj_len(const phot_elem *e);
/**
 * @brief 获取对象元素的容量
 * @param e 目标元素
 * @return 取得的对象容量
 */
size_t phot_get_obj_cap(const phot_elem *e);
/**
 * @brief 将对象元素的容量扩充至 cap
 * @param e 目标元素
 * @param cap 指定的容量
 */
void phot_reserve_obj(phot_elem *e, size_t cap);
/**
 * @brief 将对象元素的容量收缩至实际长度
 * @param e 目标元素
 */
void phot_shrink_obj(phot_elem *e);
/**
 * @brief 清空对象元素
 * @param e 目标元素
 */
void phot_clear_obj(phot_elem *e);
/**
 * @brief 获取对象元素中 index 处的键
 * @param e 目标元素
 * @param index 索引
 * @return 取得的键
 */
const char *phot_get_obj_key(const phot_elem *e, size_t index);
/**
 * @brief 获取对象元素中 index 处的键的长度
 * @param e 目标元素
 * @param index 索引
 * @return 取得的键长度
 */
size_t phot_get_obj_key_len(const phot_elem *e, size_t index);
/**
 * @brief 获取对象元素中 index 处的值
 * @param e 目标元素
 * @param index 索引
 * @return 取得的值
 */
phot_elem *phot_get_obj_value(const phot_elem *e, size_t index);
/**
 * @brief 查找对象元素中键为 key 的成员的索引
 * @param e 目标元素
 * @param key 键
 * @param klen 键长度
 * @return 取得的索引
 */
size_t phot_find_obj_index(const phot_elem *e, const char *key, size_t klen);
/**
 * @brief 查找对象元素中键为 key 的成员的值
 * @param e 目标元素
 * @param key 键
 * @param klen 键长度
 * @return 取得的值
 */
phot_elem *phot_find_obj_value(const phot_elem *e, const char *key, size_t klen);
/**
 * @brief 设置对象元素中键为 key 的成员的值，若不存在则添加
 * @param e 目标元素
 * @param key 键
 * @param klen 键长度
 * @return 取得的值
 */
phot_elem *phot_set_obj_value(phot_elem *e, const char *key, size_t klen);
/**
 * @brief 移除对象元素中 index 处的成员
 * @param e 目标元素
 * @param index 索引
 */
void phot_remove_obj_member(phot_elem *e, size_t index);

#endif  // PHOTJSON_H_
