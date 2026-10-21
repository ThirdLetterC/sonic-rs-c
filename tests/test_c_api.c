#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sonic_rs.h"

static void test_parse_and_scalars(void) {
    const char *json = "{\"b\":true,\"i\":-42,\"u\":100,\"f\":3.5,\"s\":\"hello\",\"n\":null}";
    sonic_value_t *root = sonic_parse(json, strlen(json));
    assert(root != NULL);
    assert(sonic_value_is_object(root));
    assert(sonic_object_len(root) == 6);

    bool b = false;
    assert(sonic_value_as_bool(sonic_object_get_cstr(root, "b"), &b) == SONIC_OK);
    assert(b == true);

    int64_t i = 0;
    assert(sonic_value_as_i64(sonic_object_get_cstr(root, "i"), &i) == SONIC_OK);
    assert(i == -42);

    uint64_t u = 0;
    assert(sonic_value_as_u64(sonic_object_get_cstr(root, "u"), &u) == SONIC_OK);
    assert(u == 100);

    double f = 0.0;
    assert(sonic_value_as_f64(sonic_object_get_cstr(root, "f"), &f) == SONIC_OK);
    assert(fabs(f - 3.5) < 1e-9);

    sonic_str_view_t sv = { NULL, 0 };
    assert(sonic_value_as_str_view(sonic_object_get_cstr(root, "s"), &sv) == SONIC_OK);
    assert(sv.len == 5 && memcmp(sv.ptr, "hello", 5) == 0);

    assert(sonic_value_is_null(sonic_object_get_cstr(root, "n")));

    sonic_value_free(root);
}

static void test_mutation_and_serialization(void) {
    sonic_value_t *obj = sonic_value_new_object();
    assert(obj != NULL);
    assert(sonic_object_is_empty(obj));

    assert(sonic_object_insert_cstr(obj, "name", sonic_value_new_cstr("sonic")) == SONIC_OK);
    assert(sonic_object_insert_cstr(obj, "count", sonic_value_new_i64(7)) == SONIC_OK);

    sonic_value_t *arr = sonic_value_new_array();
    assert(sonic_array_push(arr, sonic_value_new_i64(10)) == SONIC_OK);
    assert(sonic_array_push(arr, sonic_value_new_i64(20)) == SONIC_OK);
    assert(sonic_object_insert_cstr(obj, "items", arr) == SONIC_OK);

    assert(sonic_object_len(obj) == 3);
    assert(sonic_array_len(sonic_object_get_cstr(obj, "items")) == 2);

    sonic_value_t *cloned = sonic_value_clone(obj);
    assert(sonic_value_equals(obj, cloned));
    sonic_value_free(cloned);

    size_t len = 0;
    char *serialized = sonic_serialize_to_cstr(obj, &len);
    assert(serialized != NULL);
    assert(len > 0);
    sonic_string_free(serialized);

    sonic_buf_t buf = { NULL, 0, 0 };
    assert(sonic_serialize_to_buf(obj, false, &buf) == SONIC_OK);
    assert(buf.data != NULL && buf.len == len);
    sonic_buf_free(&buf);

    sonic_value_free(obj);
}

static void test_pointer_and_lazy_get(void) {
    const char *json = "{\"a\":{\"b\":[{\"k\":\"v0\"},{\"k\":\"v1\"}]}}";
    sonic_pointer_node_t path[] = {
        SONIC_KEY_LIT("a"),
        SONIC_KEY_LIT("b"),
        SONIC_INDEX(1),
        SONIC_KEY_LIT("k")
    };

    sonic_str_view_t raw = { NULL, 0 };
    sonic_json_type_t ty = SONIC_TYPE_NULL;
    assert(sonic_get_raw(json, strlen(json), path, 4, false, &raw, &ty) == SONIC_OK);
    assert(ty == SONIC_TYPE_STRING);
    assert(raw.len == 4 && memcmp(raw.ptr, "\"v1\"", 4) == 0);

    /* Also test inline node constructors */
    sonic_pointer_node_t dyn_path[2];
    dyn_path[0] = sonic_node_key("a", 1);
    dyn_path[1] = sonic_node_key("b", 1);
    sonic_str_view_t raw_arr = { NULL, 0 };
    assert(sonic_get_raw(json, strlen(json), dyn_path, 2, false, &raw_arr, &ty) == SONIC_OK);
    assert(ty == SONIC_TYPE_ARRAY);

    sonic_str_view_t raw2 = { NULL, 0 };
    assert(sonic_get_by_json_pointer(json, strlen(json), "/a/b/0/k", false, &raw2, NULL) == SONIC_OK);
    assert(raw2.len == 4 && memcmp(raw2.ptr, "\"v0\"", 4) == 0);
}

int main(void) {
    test_parse_and_scalars();
    test_mutation_and_serialization();
    test_pointer_and_lazy_get();
    printf("All sonic-rs C FFI tests passed!\n");
    return 0;
}
