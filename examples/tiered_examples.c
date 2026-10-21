/**
 * @file sonic_rs_examples.c
 * @brief Easy, Medium, and Hard C usage examples + high-resolution benchmark suite
 *        for the `sonic-rs-c` library (Windows MSVC & Linux/macOS).
 *
 * Compile & run on Windows (MSVC):
 *   cl /nologo /O2 /W4 /MD /Idist\include sonic_rs_examples.c dist\lib\sonic_rs_c.lib
 *
 * Compile & run on Linux / macOS (GCC / Clang):
 *   cc -O2 -std=c11 -Idist/include sonic_rs_examples.c dist/lib/libsonic_rs_c.a -lpthread -ldl -lm -o sonic_rs_examples
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sonic_rs.h"

/* Cross-platform nanosecond monotonic timer (Windows MSVC QPC & POSIX clock_gettime) */
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static uint64_t sonic_now_ns(void) {
    static LARGE_INTEGER freq = { 0 };
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1e9 / (double)freq.QuadPart);
}
#else
#include <time.h>
static uint64_t sonic_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
#endif

/* Sample payloads used by both functional runs and benchmark loops */
static const char *EASY_JSON =
    "{"
    "  \"host\": \"api.example.com\","
    "  \"port\": 8443,"
    "  \"tls_enabled\": true,"
    "  \"timeout_sec\": 2.5"
    "}";

static const char *MEDIUM_JSON =
    "{"
    "  \"order_id\": 9001,"
    "  \"customer\": { \"name\": \"Olena\", \"tier\": \"gold\" },"
    "  \"items\": ["
    "    { \"sku\": \"KB-01\", \"qty\": 1, \"price\": 129.99 },"
    "    { \"sku\": \"MS-02\", \"qty\": 2, \"price\": 49.50 }"
    "  ],"
    "  \"temp_flag\": true"
    "}";

static const char *HARD_JSON =
    "{"
    "  \"batch_id\": \"b-2026-10-21\","
    "  \"cluster\": { \"dc\": \"kyiv-1\", \"rack\": 4 },"
    "  \"events\": ["
    "    { \"id\": 101, \"status\": \"ok\",    \"latency_us\": 120,  \"payload\": { \"retries\": 0 } },"
    "    { \"id\": 102, \"status\": \"error\", \"latency_us\": 9450, \"payload\": { \"retries\": 3, \"code\": \"E_TIMEOUT\" } },"
    "    { \"id\": 103, \"status\": \"ok\",    \"latency_us\": 85,   \"payload\": { \"retries\": 0 } }"
    "  ]"
    "}";

/* ========================================================================== *
 * 1. EASY EXAMPLE: Parse JSON, Read Scalar Fields, and Clean Up              *
 * ========================================================================== */
static int run_easy_once(const char *json_str, size_t json_len, bool verbose) {
    sonic_value_t *root = sonic_parse(json_str, json_len);
    if (!root) {
        if (verbose) {
            fprintf(stderr, "Parse failed: %s\n", sonic_last_error_message());
        }
        return -1;
    }

    const sonic_value_t *host_val = sonic_object_get_cstr(root, "host");
    const sonic_value_t *port_val = sonic_object_get_cstr(root, "port");
    const sonic_value_t *tls_val  = sonic_object_get_cstr(root, "tls_enabled");
    const sonic_value_t *tout_val = sonic_object_get_cstr(root, "timeout_sec");

    sonic_str_view_t host = { NULL, 0 };
    int64_t port = 0;
    bool tls_enabled = false;
    double timeout_sec = 0.0;

    sonic_value_as_str_view(host_val, &host);
    sonic_value_as_i64(port_val, &port);
    sonic_value_as_bool(tls_val, &tls_enabled);
    sonic_value_as_f64(tout_val, &timeout_sec);

    if (verbose) {
        printf("=== [1] EASY EXAMPLE: DOM Parsing & Scalar Extraction ===\n");
        printf("Host:        %.*s\n", (int)host.len, host.ptr);
        printf("Port:        %" PRId64 "\n", port);
        printf("TLS Enabled: %s\n", tls_enabled ? "true" : "false");
        printf("Timeout (s): %.2f\n\n", timeout_sec);
    }

    sonic_value_free(root);
    return (port == 8443 && tls_enabled) ? 0 : -1;
}

/* ========================================================================== *
 * 2. MEDIUM EXAMPLE: In-Place DOM Mutation, Array/Object Iteration &         *
 *    RFC 6901 JSON Pointer Navigation                                        *
 * ========================================================================== */
static int run_medium_once(const char *order_json, size_t order_len, bool verbose) {
    sonic_value_t *doc = sonic_parse(order_json, order_len);
    if (!doc) {
        if (verbose) {
            fprintf(stderr, "Order parse failed: %s\n", sonic_last_error_message());
        }
        return -1;
    }

    const sonic_value_t *tier_node = sonic_value_by_json_pointer(doc, "/customer/tier");
    sonic_str_view_t tier_view = { NULL, 0 };
    sonic_value_as_str_view(tier_node, &tier_view);

    sonic_value_t *items_arr = sonic_object_get_mut_cstr(doc, "items");
    sonic_value_t *new_item  = sonic_value_new_object();
    sonic_object_insert_cstr(new_item, "sku",   sonic_value_new_cstr("HD-03"));
    sonic_object_insert_cstr(new_item, "qty",   sonic_value_new_i64(1));
    sonic_object_insert_cstr(new_item, "price", sonic_value_new_f64(89.00));
    sonic_array_push(items_arr, new_item);

    double total_amount = 0.0;
    size_t item_count = sonic_array_len(items_arr);
    for (size_t i = 0; i < item_count; ++i) {
        const sonic_value_t *item = sonic_array_get(items_arr, i);
        int64_t qty = 0;
        double price = 0.0;
        sonic_value_as_i64(sonic_object_get_cstr(item, "qty"), &qty);
        sonic_value_as_f64(sonic_object_get_cstr(item, "price"), &price);
        total_amount += (double)qty * price;
    }
    sonic_object_insert_cstr(doc, "total_usd", sonic_value_new_f64(total_amount));

    sonic_value_t *removed = sonic_object_remove_cstr(doc, "temp_flag");
    sonic_value_free(removed);

    sonic_buf_t buf = { NULL, 0, 0 };
    sonic_status_t st = sonic_serialize_to_buf(doc, false, &buf);

    if (verbose) {
        printf("=== [2] MEDIUM EXAMPLE: DOM Mutation, Iteration & Serialization ===\n");
        printf("Customer tier (/customer/tier): %.*s\n", (int)tier_view.len, tier_view.ptr);
        printf("Top-level keys (%zu): ", sonic_object_len(doc));
        for (size_t i = 0; i < sonic_object_len(doc); ++i) {
            sonic_str_view_t key_view = { NULL, 0 };
            const sonic_value_t *val_ptr = NULL;
            if (sonic_object_entry_at(doc, i, &key_view, &val_ptr) == SONIC_OK) {
                printf("%.*s%s", (int)key_view.len, key_view.ptr,
                       (i + 1 < sonic_object_len(doc)) ? ", " : "\n");
            }
        }
        if (st == SONIC_OK) {
            printf("Updated compact JSON (%zu bytes):\n%.*s\n\n",
                   buf.len, (int)buf.len, (const char *)buf.data);
        }
    }

    sonic_buf_free(&buf);
    sonic_value_free(doc);
    return (st == SONIC_OK && total_amount > 300.0) ? 0 : -1;
}

/* ========================================================================== *
 * 3. HARD EXAMPLE: High-Throughput SIMD Lazy Slicing (`sonic_get_raw`),      *
 *    Selective Sub-Document DOM Parsing, Deep Path Mutation & Error Handling *
 * ========================================================================== */
static int run_hard_once(const char *telemetry_batch, size_t batch_len, bool verbose) {
    if (verbose) {
        printf("=== [3] HARD EXAMPLE: SIMD Lazy Slicing & Selective Sub-DOM Pipeline ===\n");
    }

    sonic_pointer_node_t dc_path[] = {
        SONIC_KEY_LIT("cluster"),
        SONIC_KEY_LIT("dc")
    };
    sonic_str_view_t dc_raw = { NULL, 0 };
    sonic_json_type_t dc_type = SONIC_TYPE_NULL;

    if (sonic_get_raw(telemetry_batch, batch_len, dc_path, 2, false, &dc_raw, &dc_type) != SONIC_OK) {
        return -1;
    }
    if (verbose) {
        printf("SIMD lazy-extracted /cluster/dc (type=%d): %.*s\n",
               (int)dc_type, (int)dc_raw.len, dc_raw.ptr);
    }

    for (size_t idx = 0; idx < 3; ++idx) {
        sonic_pointer_node_t status_path[3];
        status_path[0] = sonic_node_key("events", 6);
        status_path[1] = sonic_node_index(idx);
        status_path[2] = sonic_node_key("status", 6);

        sonic_str_view_t status_raw = { NULL, 0 };
        bool unchecked = (idx > 0);
        if (sonic_get_raw(telemetry_batch, batch_len, status_path, 3, unchecked, &status_raw, NULL) != SONIC_OK) {
            break;
        }

        if (status_raw.len == 7 && memcmp(status_raw.ptr, "\"error\"", 7) == 0) {
            sonic_pointer_node_t event_path[2];
            event_path[0] = sonic_node_key("events", 6);
            event_path[1] = sonic_node_index(idx);

            sonic_str_view_t event_raw = { NULL, 0 };
            if (sonic_get_raw(telemetry_batch, batch_len, event_path, 2, true, &event_raw, NULL) == SONIC_OK) {
                sonic_value_t *event_dom = sonic_parse(event_raw.ptr, event_raw.len);
                if (!event_dom) return -1;

                sonic_pointer_node_t payload_path[] = { SONIC_KEY_LIT("payload") };
                sonic_value_t *payload_obj = sonic_value_pointer_mut(event_dom, payload_path, 1);
                if (payload_obj) {
                    sonic_object_insert_cstr(payload_obj, "escalated", sonic_value_new_bool(true));
                    sonic_object_insert_cstr(payload_obj, "dc_raw",
                                             sonic_value_new_str(dc_raw.ptr, dc_raw.len));
                }

                if (verbose) {
                    printf("Matched error event at index %zu (raw slice = %.*s)\n",
                           idx, (int)event_raw.len, event_raw.ptr);
                    char *enriched_json = sonic_serialize_to_cstr(event_dom, NULL);
                    if (enriched_json) {
                        printf("Enriched error event DOM: %s\n", enriched_json);
                        sonic_string_free(enriched_json);
                    }
                }

                sonic_value_free(event_dom);
            }
        }
    }

    if (verbose) {
        const char *corrupt_json = "{\"id\": 500, \"metrics\": [1, 2, tru]}";
        sonic_value_t *bad = sonic_parse_cstr(corrupt_json);
        if (!bad) {
            sonic_status_t code = sonic_last_error_code();
            printf("Caught expected error [%s (code=%d)]: %s\n\n",
                   sonic_status_str(code), (int)code, sonic_last_error_message());
            sonic_clear_error();
        }
    }

    return 0;
}

/* Pure SIMD lazy lookup microbenchmark helper (unchecked get_raw only, zero DOM allocation) */
static int run_hard_lazy_only_once(const char *telemetry_batch, size_t batch_len) {
    sonic_pointer_node_t path[] = {
        SONIC_KEY_LIT("events"),
        SONIC_INDEX(1),
        SONIC_KEY_LIT("status")
    };
    sonic_str_view_t raw = { NULL, 0 };
    return (sonic_get_raw(telemetry_batch, batch_len, path, 3, true, &raw, NULL) == SONIC_OK &&
            raw.len == 7) ? 0 : -1;
}

/* ========================================================================== *
 * 4. BENCHMARK RUNNER: Measures ns/op, ops/sec, and MB/s for All Tiers       *
 * ========================================================================== */
static void run_benchmarks(size_t iterations) {
    const size_t easy_len   = strlen(EASY_JSON);
    const size_t medium_len = strlen(MEDIUM_JSON);
    const size_t hard_len   = strlen(HARD_JSON);

    printf("=== [4] PERFORMANCE BENCHMARKS (%zu iterations per tier) ===\n", iterations);
    printf("%-34s | %8s | %10s | %12s | %10s\n",
           "Benchmark Tier", "Payload", "Latency", "Throughput", "Bandwidth");
    printf("-----------------------------------+----------+------------+--------------+-----------\n");

    /* Warmup */
    for (size_t i = 0; i < 1000; ++i) {
        run_easy_once(EASY_JSON, easy_len, false);
        run_medium_once(MEDIUM_JSON, medium_len, false);
        run_hard_once(HARD_JSON, hard_len, false);
        run_hard_lazy_only_once(HARD_JSON, hard_len);
    }

    /* 1. Easy Benchmark */
    uint64_t t0 = sonic_now_ns();
    for (size_t i = 0; i < iterations; ++i) {
        if (run_easy_once(EASY_JSON, easy_len, false) != 0) abort();
    }
    uint64_t dt_easy = sonic_now_ns() - t0;
    double ns_easy   = (double)dt_easy / (double)iterations;
    double ops_easy  = 1e9 / ns_easy;
    double mbs_easy  = ((double)easy_len * ops_easy) / (1024.0 * 1024.0);
    printf("%-34s | %6zu B | %7.1f ns | %9.2f M/s | %7.1f MB/s\n",
           "1. Easy (Parse + 4 Scalar Reads)", easy_len, ns_easy, ops_easy / 1e6, mbs_easy);

    /* 2. Medium Benchmark */
    t0 = sonic_now_ns();
    for (size_t i = 0; i < iterations; ++i) {
        if (run_medium_once(MEDIUM_JSON, medium_len, false) != 0) abort();
    }
    uint64_t dt_med = sonic_now_ns() - t0;
    double ns_med   = (double)dt_med / (double)iterations;
    double ops_med  = 1e9 / ns_med;
    double mbs_med  = ((double)medium_len * ops_med) / (1024.0 * 1024.0);
    printf("%-34s | %6zu B | %7.1f ns | %9.2f M/s | %7.1f MB/s\n",
           "2. Medium (Parse + Mutate + Ser)", medium_len, ns_med, ops_med / 1e6, mbs_med);

    /* 3a. Hard Hybrid Pipeline Benchmark */
    t0 = sonic_now_ns();
    for (size_t i = 0; i < iterations; ++i) {
        if (run_hard_once(HARD_JSON, hard_len, false) != 0) abort();
    }
    uint64_t dt_hard = sonic_now_ns() - t0;
    double ns_hard   = (double)dt_hard / (double)iterations;
    double ops_hard  = 1e9 / ns_hard;
    double mbs_hard  = ((double)hard_len * ops_hard) / (1024.0 * 1024.0);
    printf("%-34s | %6zu B | %7.1f ns | %9.2f M/s | %7.1f MB/s\n",
           "3a. Hard (Lazy Filter + Sub-DOM)", hard_len, ns_hard, ops_hard / 1e6, mbs_hard);

    /* 3b. Hard Pure SIMD Unchecked Lazy Lookup Benchmark */
    t0 = sonic_now_ns();
    for (size_t i = 0; i < iterations; ++i) {
        if (run_hard_lazy_only_once(HARD_JSON, hard_len) != 0) abort();
    }
    uint64_t dt_lazy = sonic_now_ns() - t0;
    double ns_lazy   = (double)dt_lazy / (double)iterations;
    double ops_lazy  = 1e9 / ns_lazy;
    double mbs_lazy  = ((double)hard_len * ops_lazy) / (1024.0 * 1024.0);
    printf("%-34s | %6zu B | %7.1f ns | %9.2f M/s | %7.1f MB/s\n",
           "3b. Hard (Pure SIMD Lazy Unchecked)", hard_len, ns_lazy, ops_lazy / 1e6, mbs_lazy);
}

int main(void) {
    if (run_easy_once(EASY_JSON, strlen(EASY_JSON), true) != 0) return 1;
    if (run_medium_once(MEDIUM_JSON, strlen(MEDIUM_JSON), true) != 0) return 1;
    if (run_hard_once(HARD_JSON, strlen(HARD_JSON), true) != 0) return 1;

    run_benchmarks(100000);
    return 0;
}
