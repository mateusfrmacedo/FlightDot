#pragma once
#include <esp_heap_caps.h>
// Put LVGL objects in PSRAM; QSPI DMA buffers remain in internal RAM.
static inline void *plano_lv_malloc(size_t n) {
  return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
static inline void plano_lv_free(void *p) {
  heap_caps_free(p);
}
static inline void *plano_lv_realloc(void *p, size_t n) {
  return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
