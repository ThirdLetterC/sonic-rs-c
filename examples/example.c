#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sonic_rs.h"

int main(void) {
    printf("sonic-rs C wrapper version: %s\n", sonic_version());

    const char *raw_json =
        "{"
        "  \"service\": \"sonic-rs-c\","
        "  \"active\": true,"
        "  \"threads\": 8,"
        "  \"latency_ms\": 0.42,"
        "  \"tags\": [\"simd\", \"json\", \"ffi\"],"
        "  \"metrics\": { \"qps\": 125000 }"
        "}";

    /* 1. SIMD Lazy Lookup directly on unparsed JSON (sonic_rs::get / LazyValue) */
    sonic_pointer_node_t path[] = {
        SONIC_KEY_LIT("tags"),
        SONIC_INDEX(0)
    };
    sonic_str_view_t raw_slice = { NULL, 0 };
    sonic_json_type_t lazy_type = SONIC_TYPE_NULL;

    if (sonic_get_raw(raw_json, strlen(raw_json), path, 2, false, &raw_slice, &lazy_type) == SONIC_OK) {
        printf("Lazy lookup /tags/0 (type=%d): %.*s\n",
               (int)lazy_type, (int)raw_slice.len, raw_slice.ptr);
    }

    /* 2. Parse into a mutable DOM (sonic_rs::Value) */
    sonic_value_t *root = sonic_parse_cstr(raw_json);
    if (!root) {
        fprintf(stderr, "Parse error: %s\n", sonic_last_error_message());
        return 1;
    }

    /* Read fields from the DOM */
    const sonic_value_t *service_val = sonic_object_get_cstr(root, "service");
    sonic_str_view_t service_str = { NULL, 0 };
    if (sonic_value_as_str_view(service_val, &service_str) == SONIC_OK) {
        printf("Service: %.*s\n", (int)service_str.len, service_str.ptr);
    }

    /* Traverse via RFC 6901 JSON Pointer */
    const sonic_value_t *qps_val = sonic_value_by_json_pointer(root, "/metrics/qps");
    int64_t qps = 0;
    if (sonic_value_as_i64(qps_val, &qps) == SONIC_OK) {
        printf("Initial QPS: %" PRId64 "\n", qps);
    }

    /* 3. Mutate the DOM in-place */
    sonic_object_insert_cstr(root, "region", sonic_value_new_cstr("eu-central-1"));

    sonic_value_t *tags = sonic_object_get_mut_cstr(root, "tags");
    sonic_array_push(tags, sonic_value_new_cstr("rust"));

    /* 4. Serialize back to pretty-printed JSON */
    size_t out_len = 0;
    char *pretty = sonic_serialize_pretty_to_cstr(root, &out_len);
    if (pretty) {
        printf("\nSerialized JSON (%zu bytes):\n%s\n", out_len, pretty);
        sonic_string_free(pretty);
    }

    sonic_value_free(root);
    return 0;
}
