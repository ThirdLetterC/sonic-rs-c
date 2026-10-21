#ifndef SONIC_RS_H
#define SONIC_RS_H

/**
 * @file sonic_rs.h
 * @brief Embeddable C99/C11/C++ FFI wrapper for the Rust `sonic-rs` SIMD JSON crate.
 *
 * Platform linking notes:
 * - **Windows (MSVC `cl.exe` / `link.exe`)**:
 *   Link against `sonic_rs_c.lib` (or `libsonic_rs_c.a` — identical COFF archive).
 *   Required Windows system libraries (`ws2_32.lib`, `userenv.lib`, `advapi32.lib`,
 *   `bcrypt.lib`, `ntdll.lib`) are auto-linked via `#pragma comment(lib, ...)` below.
 *   Compile C/C++ code with `/MD` (default Rust dynamic CRT) or `/MT` (if Rust was built
 *   with `-C target-feature=+crt-static`).
 * - **Linux / macOS**:
 *   Link against `libsonic_rs_c.a` and `-lpthread -ldl -lm`.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Automatically link required Windows system libraries for Rust's stdlib when using MSVC */
#if defined(_MSC_VER) && !defined(SONIC_RS_NO_AUTOLINK_SYS_LIBS)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ntdll.lib")
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define SONIC_INLINE static __inline
#else
#define SONIC_INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes returned by sonic-rs C FFI operations.
 */
typedef enum sonic_status_t {
    /** Operation succeeded. */
    SONIC_OK = 0,
    /** A required pointer argument was NULL. */
    SONIC_ERR_NULL_PTR = 1,
    /** Input bytes were not valid UTF-8. */
    SONIC_ERR_UTF8 = 2,
    /** JSON syntax or parsing error (call `sonic_last_error_message()` for details). */
    SONIC_ERR_PARSE = 3,
    /** Operation requested a JSON type that does not match the actual value type. */
    SONIC_ERR_TYPE_MISMATCH = 4,
    /** Key or JSON pointer path was not found. */
    SONIC_ERR_NOT_FOUND = 5,
    /** Array index was out of bounds. */
    SONIC_ERR_INDEX_OUT_OF_BOUNDS = 6,
    /** JSON serialization failed. */
    SONIC_ERR_SERIALIZE = 7,
    /** Unspecified internal error. */
    SONIC_ERR_INTERNAL = 255
} sonic_status_t;

/**
 * @brief JSON value type tag corresponding to `sonic_rs::JsonType`.
 */
typedef enum sonic_json_type_t {
    SONIC_TYPE_NULL = 0,
    SONIC_TYPE_BOOLEAN = 1,
    SONIC_TYPE_NUMBER = 2,
    SONIC_TYPE_STRING = 3,
    SONIC_TYPE_ARRAY = 4,
    SONIC_TYPE_OBJECT = 5
} sonic_json_type_t;

/**
 * @brief Discriminant for a single step in a JSON pointer path (`sonic_pointer_node_t`).
 */
typedef enum sonic_path_kind_t {
    /** Object key lookup (`key_ptr`, `key_len`). */
    SONIC_PATH_KEY = 0,
    /** Array index lookup (`index`). */
    SONIC_PATH_INDEX = 1
} sonic_path_kind_t;

/**
 * @brief Opaque handle wrapping a `sonic_rs::Value` DOM node.
 *
 * Root values returned by `sonic_parse*`, `sonic_value_new_*`, `sonic_value_clone`,
 * `sonic_object_remove*`, `sonic_array_pop`, or `sonic_array_remove` are OWNED by the caller
 * and must be freed via `sonic_value_free()` (unless ownership is transferred into a parent
 * container via `sonic_object_insert*` or `sonic_array_push`).
 *
 * Pointers returned by `sonic_object_get*`, `sonic_array_get*`, and `sonic_value_pointer*`
 * are BORROWED views tied to the lifetime of their root `sonic_value_t` and must NOT be passed
 * to `sonic_value_free()`.
 */
typedef struct sonic_value_t sonic_value_t;

/**
 * @brief Non-owning UTF-8 string slice view (`ptr` is NOT guaranteed to be null-terminated
 * when slicing raw JSON via `sonic_get_raw`, so always use `len`).
 */
typedef struct sonic_str_view_t {
    const char *ptr;
    size_t len;
} sonic_str_view_t;

/**
 * @brief Owned byte buffer allocated by Rust for serialized JSON output.
 * Must be released with `sonic_buf_free()`.
 */
typedef struct sonic_buf_t {
    uint8_t *data;
    size_t len;
    size_t capacity;
} sonic_buf_t;

/**
 * @brief A single segment in a JSON path query (either an object key or an array index).
 */
typedef struct sonic_pointer_node_t {
    sonic_path_kind_t kind;
    const char *key_ptr;
    size_t key_len;
    size_t index;
} sonic_pointer_node_t;

/* ========================================================================== *
 * Helper Macros & Inline Constructors for Path Nodes (MSVC / GCC / Clang)    *
 * ========================================================================== */

/** Brace initializer for a key path node from a C string literal (use in array initializers). */
#define SONIC_KEY_LIT(lit) \
    { SONIC_PATH_KEY, (lit), sizeof(lit) - 1, 0 }

/** Brace initializer for a key path node from a pointer and explicit length. */
#define SONIC_KEY_LEN(ptr, length) \
    { SONIC_PATH_KEY, (ptr), (length), 0 }

/** Brace initializer for an array index path node. */
#define SONIC_INDEX(idx) \
    { SONIC_PATH_INDEX, NULL, 0, (size_t)(idx) }

/** Inline helper to construct a key `sonic_pointer_node_t` in any expression context. */
SONIC_INLINE sonic_pointer_node_t sonic_node_key(const char *key_ptr, size_t key_len) {
    sonic_pointer_node_t node;
    node.kind = SONIC_PATH_KEY;
    node.key_ptr = key_ptr;
    node.key_len = key_len;
    node.index = 0;
    return node;
}

/** Inline helper to construct an array index `sonic_pointer_node_t` in any expression context. */
SONIC_INLINE sonic_pointer_node_t sonic_node_index(size_t index) {
    sonic_pointer_node_t node;
    node.kind = SONIC_PATH_INDEX;
    node.key_ptr = NULL;
    node.key_len = 0;
    node.index = index;
    return node;
}

/* ========================================================================== *
 * Version & Thread-Local Error Reporting                                     *
 * ========================================================================== */

/** Return the null-terminated version string of the `sonic-rs-c` wrapper library. */
const char *sonic_version(void);

/** Return a human-readable static string for any `sonic_status_t` code. */
const char *sonic_status_str(sonic_status_t status);

/** Return the last `sonic_status_t` recorded on the calling thread. */
sonic_status_t sonic_last_error_code(void);

/**
 * Return a null-terminated error message for the last error on the calling thread,
 * or an empty string `""` if no error occurred. Pointer remains valid until the next
 * `sonic_*` call on the same thread.
 */
const char *sonic_last_error_message(void);

/** Clear the thread-local error state on the calling thread. */
void sonic_clear_error(void);

/* ========================================================================== *
 * DOM Parsing & Serialization                                                *
 * ========================================================================== */

/**
 * Parse a UTF-8 JSON buffer of length `len` into an owned `sonic_value_t*`.
 * Returns NULL on failure and sets the thread-local error.
 */
sonic_value_t *sonic_parse(const char *json, size_t len);

/**
 * Parse a null-terminated UTF-8 JSON C-string into an owned `sonic_value_t*`.
 * Returns NULL on failure and sets the thread-local error.
 */
sonic_value_t *sonic_parse_cstr(const char *json_cstr);

/**
 * Parse a JSON byte buffer into an owned `sonic_value_t*` using `sonic_rs::from_slice_unchecked`
 * (skips UTF-8 validation for maximum SIMD throughput).
 *
 * @warning Caller must guarantee that `json[0..len]` is valid UTF-8 and well-formed JSON.
 */
sonic_value_t *sonic_parse_unchecked(const uint8_t *json, size_t len);

/**
 * Serialize `val` to a newly allocated, null-terminated compact JSON C-string.
 * If `out_len` is non-NULL, writes the string byte length (excluding the NUL terminator).
 * Must be freed with `sonic_string_free()`.
 */
char *sonic_serialize_to_cstr(const sonic_value_t *val, size_t *out_len);

/**
 * Serialize `val` to a newly allocated, null-terminated pretty-printed JSON C-string.
 * If `out_len` is non-NULL, writes the string byte length (excluding the NUL terminator).
 * Must be freed with `sonic_string_free()`.
 */
char *sonic_serialize_pretty_to_cstr(const sonic_value_t *val, size_t *out_len);

/**
 * Serialize `val` into an owned `sonic_buf_t` byte buffer.
 * Release the buffer with `sonic_buf_free(out_buf)`.
 */
sonic_status_t sonic_serialize_to_buf(const sonic_value_t *val,
                                      bool pretty,
                                      sonic_buf_t *out_buf);

/** Free a C-string returned by `sonic_serialize_*_to_cstr` or `sonic_value_copy_cstr`. */
void sonic_string_free(char *str);

/** Free a byte buffer populated by `sonic_serialize_to_buf`. */
void sonic_buf_free(sonic_buf_t *buf);

/* ========================================================================== *
 * Value Lifecycle & Constructors                                             *
 * ========================================================================== */

/** Create a new owned JSON `null` value. */
sonic_value_t *sonic_value_new_null(void);

/** Create a new owned JSON boolean value. */
sonic_value_t *sonic_value_new_bool(bool v);

/** Create a new owned JSON signed 64-bit integer value. */
sonic_value_t *sonic_value_new_i64(int64_t v);

/** Create a new owned JSON unsigned 64-bit integer value. */
sonic_value_t *sonic_value_new_u64(uint64_t v);

/** Create a new owned JSON 64-bit floating-point value (returns NULL if NaN/Inf). */
sonic_value_t *sonic_value_new_f64(double v);

/** Create a new owned JSON string value from a UTF-8 buffer of `len` bytes. */
sonic_value_t *sonic_value_new_str(const char *s, size_t len);

/** Create a new owned JSON string value from a null-terminated UTF-8 C-string. */
sonic_value_t *sonic_value_new_cstr(const char *s);

/** Create a new owned empty JSON array `[]`. */
sonic_value_t *sonic_value_new_array(void);

/** Create a new owned empty JSON object `{}`. */
sonic_value_t *sonic_value_new_object(void);

/** Deep-clone any owned or borrowed `sonic_value_t` into a new owned `sonic_value_t*`. */
sonic_value_t *sonic_value_clone(const sonic_value_t *val);

/** Compare two `sonic_value_t` nodes for structural equality. */
bool sonic_value_equals(const sonic_value_t *lhs, const sonic_value_t *rhs);

/** Free an owned `sonic_value_t*`. Safe to call with NULL. */
void sonic_value_free(sonic_value_t *val);

/* ========================================================================== *
 * Type Inspection & Scalar Extraction                                        *
 * ========================================================================== */

sonic_json_type_t sonic_value_get_type(const sonic_value_t *val);
bool sonic_value_is_null(const sonic_value_t *val);
bool sonic_value_is_bool(const sonic_value_t *val);
bool sonic_value_is_true(const sonic_value_t *val);
bool sonic_value_is_false(const sonic_value_t *val);
bool sonic_value_is_number(const sonic_value_t *val);
bool sonic_value_is_i64(const sonic_value_t *val);
bool sonic_value_is_u64(const sonic_value_t *val);
bool sonic_value_is_f64(const sonic_value_t *val);
bool sonic_value_is_str(const sonic_value_t *val);
bool sonic_value_is_array(const sonic_value_t *val);
bool sonic_value_is_object(const sonic_value_t *val);

sonic_status_t sonic_value_as_bool(const sonic_value_t *val, bool *out);
sonic_status_t sonic_value_as_i64(const sonic_value_t *val, int64_t *out);
sonic_status_t sonic_value_as_u64(const sonic_value_t *val, uint64_t *out);
sonic_status_t sonic_value_as_f64(const sonic_value_t *val, double *out);

/**
 * Obtain a zero-copy string slice view into a JSON string value.
 * The returned pointer remains valid as long as `val` is not mutated or freed.
 */
sonic_status_t sonic_value_as_str_view(const sonic_value_t *val, sonic_str_view_t *out);

/**
 * Copy a JSON string value into a newly allocated null-terminated `char*`.
 * Returns NULL if `val` is not a string. Must be freed with `sonic_string_free()`.
 */
char *sonic_value_copy_cstr(const sonic_value_t *val);

/* ========================================================================== *
 * Object Operations (`sonic_rs::Object`)                                     *
 * ========================================================================== */

/** Return the number of key-value pairs in a JSON object (or 0 if not an object). */
size_t sonic_object_len(const sonic_value_t *obj);

/** Return true if `obj` is an empty JSON object (or not an object). */
bool sonic_object_is_empty(const sonic_value_t *obj);

/** Check whether `obj` contains `key[0..key_len]`. */
bool sonic_object_contains_key(const sonic_value_t *obj, const char *key, size_t key_len);

/** Check whether `obj` contains null-terminated `key`. */
bool sonic_object_contains_key_cstr(const sonic_value_t *obj, const char *key);

/**
 * Borrow a property from `obj` by `key[0..key_len]`.
 * Returns a borrowed pointer (do NOT pass to `sonic_value_free()`), or NULL if missing.
 */
const sonic_value_t *sonic_object_get(const sonic_value_t *obj, const char *key, size_t key_len);

/**
 * Borrow a property from `obj` by null-terminated `key`.
 * Returns a borrowed pointer (do NOT pass to `sonic_value_free()`), or NULL if missing.
 */
const sonic_value_t *sonic_object_get_cstr(const sonic_value_t *obj, const char *key);

/**
 * Mutably borrow a property from `obj` by `key[0..key_len]`.
 * Returns a borrowed mutable pointer (do NOT pass to `sonic_value_free()`), or NULL if missing.
 */
sonic_value_t *sonic_object_get_mut(sonic_value_t *obj, const char *key, size_t key_len);

/**
 * Mutably borrow a property from `obj` by null-terminated `key`.
 * Returns a borrowed mutable pointer (do NOT pass to `sonic_value_free()`), or NULL if missing.
 */
sonic_value_t *sonic_object_get_mut_cstr(sonic_value_t *obj, const char *key);

/**
 * Insert or overwrite `key[0..key_len]` in `obj` with `val`.
 *
 * @note Takes ownership of `val` on `SONIC_OK` (do NOT call `sonic_value_free(val)` after
 * a successful insert). On error, `val` is NOT consumed and remains owned by the caller.
 */
sonic_status_t sonic_object_insert(sonic_value_t *obj,
                                   const char *key,
                                   size_t key_len,
                                   sonic_value_t *val);

/**
 * Insert or overwrite null-terminated `key` in `obj` with `val`.
 * Takes ownership of `val` on `SONIC_OK`.
 */
sonic_status_t sonic_object_insert_cstr(sonic_value_t *obj,
                                        const char *key,
                                        sonic_value_t *val);

/**
 * Remove `key[0..key_len]` from `obj` and return the removed value as a new owned `sonic_value_t*`
 * (or NULL if the key was not present). Caller must free the returned pointer with `sonic_value_free()`.
 */
sonic_value_t *sonic_object_remove(sonic_value_t *obj, const char *key, size_t key_len);

/**
 * Remove null-terminated `key` from `obj` and return the removed value as an owned `sonic_value_t*`.
 */
sonic_value_t *sonic_object_remove_cstr(sonic_value_t *obj, const char *key);

/**
 * Access the `index`-th key-value pair of `obj` (0 <= index < `sonic_object_len(obj)`).
 * Writes a zero-copy view of the key into `*out_key` (if non-NULL) and a borrowed pointer
 * to the value into `*out_val` (if non-NULL).
 */
sonic_status_t sonic_object_entry_at(const sonic_value_t *obj,
                                     size_t index,
                                     sonic_str_view_t *out_key,
                                     const sonic_value_t **out_val);

/** Remove all key-value pairs from `obj`. */
sonic_status_t sonic_object_clear(sonic_value_t *obj);

/* ========================================================================== *
 * Array Operations (`sonic_rs::Array`)                                       *
 * ========================================================================== */

/** Return the number of elements in a JSON array (or 0 if not an array). */
size_t sonic_array_len(const sonic_value_t *arr);

/** Return true if `arr` is an empty JSON array (or not an array). */
bool sonic_array_is_empty(const sonic_value_t *arr);

/**
 * Borrow the element at `index` from `arr`.
 * Returns a borrowed pointer (do NOT pass to `sonic_value_free()`), or NULL if out of bounds.
 */
const sonic_value_t *sonic_array_get(const sonic_value_t *arr, size_t index);

/**
 * Mutably borrow the element at `index` from `arr`.
 * Returns a borrowed mutable pointer (do NOT pass to `sonic_value_free()`), or NULL if out of bounds.
 */
sonic_value_t *sonic_array_get_mut(sonic_value_t *arr, size_t index);

/**
 * Append `val` to the end of `arr`.
 * Takes ownership of `val` on `SONIC_OK` (do NOT call `sonic_value_free(val)` after success).
 */
sonic_status_t sonic_array_push(sonic_value_t *arr, sonic_value_t *val);

/**
 * Pop and return the last element of `arr` as an owned `sonic_value_t*` (or NULL if empty).
 * Caller must free the returned value with `sonic_value_free()`.
 */
sonic_value_t *sonic_array_pop(sonic_value_t *arr);

/**
 * Remove and return the element at `index` from `arr` as an owned `sonic_value_t*` (or NULL if out of bounds).
 * Caller must free the returned value with `sonic_value_free()`.
 */
sonic_value_t *sonic_array_remove(sonic_value_t *arr, size_t index);

/** Remove all elements from `arr`. */
sonic_status_t sonic_array_clear(sonic_value_t *arr);

/* ========================================================================== *
 * JSON Pointer & Lazy SIMD Queries (`sonic_rs::get` / `LazyValue`)           *
 * ========================================================================== */

/**
 * Traverse a parsed `sonic_value_t` DOM using an array of `sonic_pointer_node_t` steps.
 * Returns a borrowed pointer to the target `sonic_value_t`, or NULL if not found.
 */
const sonic_value_t *sonic_value_pointer(const sonic_value_t *root,
                                         const sonic_pointer_node_t *nodes,
                                         size_t node_count);

/**
 * Mutably traverse a parsed `sonic_value_t` DOM using an array of `sonic_pointer_node_t` steps.
 * Returns a borrowed mutable pointer to the target `sonic_value_t`, or NULL if not found.
 */
sonic_value_t *sonic_value_pointer_mut(sonic_value_t *root,
                                       const sonic_pointer_node_t *nodes,
                                       size_t node_count);

/**
 * Traverse a parsed `sonic_value_t` DOM using an RFC 6901 JSON Pointer string
 * (e.g., `"/users/0/name"` or `""` for root).
 * Returns a borrowed pointer to the target `sonic_value_t`, or NULL if not found.
 */
const sonic_value_t *sonic_value_by_json_pointer(const sonic_value_t *root,
                                                 const char *rfc6901_path);

/**
 * SIMD-accelerated lazy lookup (`sonic_rs::get` / `sonic_rs::get_unchecked`) directly on
 * an unparsed JSON text buffer `json[0..json_len]`.
 *
 * Extracts the raw JSON slice (`LazyValue::as_raw_str()`) at the path specified by
 * `nodes[0..node_count]` without building a DOM, writing a zero-copy slice pointing
 * directly into `json` to `*out_raw`. If `out_type` is non-NULL, writes the `sonic_json_type_t`
 * of the extracted `LazyValue`.
 *
 * @param unchecked If true, uses `sonic_rs::get_unchecked` (skips full validation for maximum speed).
 */
sonic_status_t sonic_get_raw(const char *json,
                             size_t json_len,
                             const sonic_pointer_node_t *nodes,
                             size_t node_count,
                             bool unchecked,
                             sonic_str_view_t *out_raw,
                             sonic_json_type_t *out_type);

/**
 * SIMD-accelerated lazy lookup on an unparsed JSON buffer using an RFC 6901 JSON Pointer
 * string (e.g., `"/a/b/1"`). Writes a zero-copy raw JSON slice into `*out_raw`.
 */
sonic_status_t sonic_get_by_json_pointer(const char *json,
                                         size_t json_len,
                                         const char *rfc6901_path,
                                         bool unchecked,
                                         sonic_str_view_t *out_raw,
                                         sonic_json_type_t *out_type);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* SONIC_RS_H */
