#include "display.h"
#include "board_pins.h"
#include "touch.h"
#include <Arduino.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_sh8601.h>
#include <lvgl.h>
#include <read_lcd_id_bsp.h>
namespace display {
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
static lv_disp_drv_t driver;
static lv_disp_draw_buf_t buffers;
static lv_color_t *rotationBuffer;
static uint8_t panelId;
static uint32_t frames = 0;
uint32_t frameCount() {
  return frames;
}
static const uint8_t zero = 0, full = 255, control = 0x20, c4 = 0x80, scan[] = {1, 0xD1};
// Exact model-specific sequences from official 07_LVGL_Test/lcd_bsp.c (Demo V3).
static const sh8601_lcd_init_cmd_t sh[] = {
    {0x11, &zero, 0, 120}, {0x44, scan, 2, 0},   {0x35, &zero, 1, 0}, {0x53, &control, 1, 10},
    {0x51, &zero, 1, 10},  {0x29, &zero, 0, 10}, {0x51, &full, 1, 0}};
static const sh8601_lcd_init_cmd_t co[] = {
    {0x11, &zero, 0, 80}, {0xC4, &c4, 1, 0},    {0x53, &control, 1, 1}, {0x63, &full, 1, 1},
    {0x51, &zero, 1, 1},  {0x29, &zero, 0, 10}, {0x51, &full, 1, 0}};
static bool ready(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx) {
  lv_disp_flush_ready(static_cast<lv_disp_drv_t *>(ctx));
  return false;
}
static void flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
  // Rotate the rendered stripe directly into one fixed DMA buffer. LVGL's
  // generic 90-degree software rotation allocates temporary chunks and waits
  // synchronously after each transfer, which caused stalls and torn stripes.
  const int sourceWidth = a->x2 - a->x1 + 1;
  const int sourceHeight = a->y2 - a->y1 + 1;
  for (int sy = 0; sy < sourceHeight; ++sy)
    for (int sx = 0; sx < sourceWidth; ++sx)
      rotationBuffer[sx * sourceHeight + (sourceHeight - 1 - sy)] = px[sy * sourceWidth + sx];
  const int rotatedX1 = 465 - a->y2;
  const int rotatedY1 = a->x1;
  int offset = panelId == 0x86 ? 0 : 6;
  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, rotatedX1 + offset, rotatedY1,
                                            rotatedX1 + sourceHeight + offset,
                                            rotatedY1 + sourceWidth, rotationBuffer));
  if (lv_disp_flush_is_last(drv))
    ++frames;
}
const char *controller() {
  return panelId == 0x86 ? "SH8601" : "CO5300 (official ID fallback)";
}
void brightness(uint8_t v) {
  ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, 0x02005100, &v, 1));
}
void begin() {
  pinMode(pins::oledEnable, OUTPUT);
  digitalWrite(pins::oledEnable, HIGH);
  panelId = read_lcd_id();
  Serial.printf("[display] ID=0x%02X %s, 466x466\n", panelId, controller());
  spi_bus_config_t b = {};
  b.sclk_io_num = pins::clock;
  b.data0_io_num = pins::d0;
  b.data1_io_num = pins::d1;
  b.data2_io_num = pins::d2;
  b.data3_io_num = pins::d3;
  b.max_transfer_sz = 466 * 40 * 2;
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &b, SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t cfg = {};
  cfg.cs_gpio_num = pins::cs;
  cfg.dc_gpio_num = -1;
  cfg.spi_mode = 0;
  cfg.pclk_hz = 40000000;
  cfg.trans_queue_depth = 10;
  cfg.on_color_trans_done = ready;
  cfg.user_ctx = &driver;
  cfg.lcd_cmd_bits = 32;
  cfg.lcd_param_bits = 8;
  cfg.flags.quad_mode = 1;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &cfg, &io));
  sh8601_vendor_config_t v = {};
  v.flags.use_qspi_interface = 1;
  v.init_cmds = panelId == 0x86 ? sh : co;
  v.init_cmds_size = 7;
  esp_lcd_panel_dev_config_t p = {};
  p.reset_gpio_num = pins::reset;
  p.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  p.bits_per_pixel = 16;
  p.vendor_config = &v;
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io, &p, &panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
  lv_init();
  auto a = static_cast<lv_color_t *>(heap_caps_malloc(466 * 32 * 2, MALLOC_CAP_DMA));
  auto c = static_cast<lv_color_t *>(heap_caps_malloc(466 * 32 * 2, MALLOC_CAP_DMA));
  rotationBuffer = static_cast<lv_color_t *>(heap_caps_malloc(466 * 34 * 2, MALLOC_CAP_DMA));
  assert(a && c && rotationBuffer);
  lv_disp_draw_buf_init(&buffers, a, c, 466 * 32);
  lv_disp_drv_init(&driver);
  driver.hor_res = 466;
  driver.ver_res = 466;
  driver.draw_buf = &buffers;
  driver.flush_cb = flush;
  driver.rounder_cb = [](lv_disp_drv_t *, lv_area_t *r) {
    r->x1 &= ~1;
    r->y1 &= ~1;
    r->x2 |= 1;
    r->y2 |= 1;
  };
  // The panel cannot swap axes in hardware. Rotation is performed once in the
  // DMA flush above, avoiding LVGL's slower synchronous generic path.
  lv_disp_t *display = lv_disp_drv_register(&driver);
  touch::begin();
  static lv_indev_drv_t input;
  lv_indev_drv_init(&input);
  input.type = LV_INDEV_TYPE_POINTER;
  input.disp = display;
  input.read_cb = touch::read;
  lv_indev_drv_register(&input);
  brightness(150);
}
void testScreen() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
  auto label = lv_label_create(lv_scr_act());
  lv_label_set_text_fmt(label, "FLIGHTDOT\n466 x 466\n%s\nToque para testar", controller());
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(label);
  for (int i = 0; i < 3; i++) {
    auto o = lv_obj_create(lv_scr_act());
    lv_obj_set_size(o, 65, 45);
    lv_obj_set_pos(o, 125 + i * 75, 310);
    lv_obj_set_style_bg_color(o, lv_color_hex(i == 0 ? 0xff0000 : i == 1 ? 0x00ff00 : 0x0000ff), 0);
  }
  lv_obj_add_event_cb(
      lv_scr_act(),
      [](lv_event_t *) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_get_act(), &p);
        Serial.printf("[touch] x=%d y=%d\n", p.x, p.y);
      },
      LV_EVENT_PRESSED, nullptr);
}
} // namespace display
