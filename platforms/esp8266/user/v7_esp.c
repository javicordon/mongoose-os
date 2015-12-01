#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ets_sys.h>
#include <v7.h>
#include <sj_hal.h>
#include <sj_timers.h>
#include <sj_v7_ext.h>
#include <sj_i2c_js.h>
#include <sj_spi_js.h>
#include <sj_gpio_js.h>
#include <sj_adc_js.h>
#include "v7_esp.h"
#include "dht11.h"
#include "util.h"
#include "v7_esp_features.h"
#include "esp_pwm.h"
#include "esp_uart.h"
#include "esp_sj_wifi.h"
#include "sj_http.h"
#include "sj_mongoose_ws_client.h"
#include <sha1.h>

#ifndef RTOS_SDK

#include <osapi.h>
#include <gpio.h>
#include <os_type.h>
#include <user_interface.h>
#include <mem.h>

#else

#include <esp_system.h>

#endif /* RTOS_SDK */

#include <sj_mongoose.h>

struct v7 *v7;

#if V7_ESP_ENABLE__DHT11

static v7_val_t DHT11_read(struct v7 *v7) {
  int pin, temp, rh;
  v7_val_t pinv = v7_arg(v7, 0), result;

  if (!v7_is_number(pinv)) {
    printf("non-numeric pin\n");
    return v7_create_undefined();
  }
  pin = v7_to_number(pinv);

  if (!dht11_read(pin, &temp, &rh)) {
    return v7_create_null();
  }

  result = v7_create_object(v7);
  v7_own(v7, &result);
  v7_set(v7, result, "temp", 4, 0, v7_create_number(temp));
  v7_set(v7, result, "rh", 2, 0, v7_create_number(rh));
  v7_disown(v7, &result);
  return result;
}
#endif /* V7_ESP_ENABLE__DHT11 */

/*
 * Sets output for debug messages.
 * Available modes are:
 * 0 - no debug output
 * 1 - print debug output to UART0 (V7's console)
 * 2 - print debug output to UART1
 */
static v7_val_t Debug_mode(struct v7 *v7) {
  int mode, res;
  v7_val_t output_val = v7_arg(v7, 0);

  if (!v7_is_number(output_val)) {
    printf("Output is not a number\n");
    return v7_create_undefined();
  }

  mode = v7_to_number(output_val);

  uart_debug_init(0, 0);
  res = uart_redirect_debug(mode);

  return v7_create_number(res < 0 ? res : mode);
}

/*
 * Prints message to current debug output
 */
v7_val_t Debug_print(struct v7 *v7) {
  int i, num_args = v7_argc(v7);

  for (i = 0; i < num_args; i++) {
    v7_fprint(stderr, v7, v7_arg(v7, i));
    fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");

  return v7_create_undefined();
}

/*
 * dsleep(time_us[, option])
 *
 * time_us - time in microseconds.
 * option - it specified, system_deep_sleep_set_option is called prior to doing
 *to sleep.
 * The most useful seems to be 4 (keep RF off on wake up, reduces power
 *consumption).
 *
 */

static v7_val_t dsleep(struct v7 *v7) {
  v7_val_t time_v = v7_arg(v7, 0);
  uint32 time = v7_to_number(time_v);
  v7_val_t flags_v = v7_arg(v7, 1);
  uint8 flags = v7_to_number(flags_v);

  if (!v7_is_number(time_v) || time < 0) return v7_create_boolean(false);
  if (v7_is_number(flags_v)) {
    if (!system_deep_sleep_set_option(flags)) return v7_create_boolean(false);
  }

  system_deep_sleep(time);

  return v7_create_boolean(true);
}

/*
 * Crashes the process/CPU. Useful to attach a debugger until we have
 * breakpoints.
 */
static v7_val_t crash(struct v7 *v7) {
  (void) v7;

  *(int *) 1 = 1;
  return v7_create_undefined();
}

void init_v7(void *stack_base) {
  struct v7_create_opts opts;
  v7_val_t dht11, debug;

  opts.object_arena_size = 164;
  opts.function_arena_size = 26;
  opts.property_arena_size = 400;
  opts.c_stack_base = stack_base;
  v7 = v7_create_opt(opts);

  v7_set_method(v7, v7_get_global(v7), "dsleep", dsleep);
  v7_set_method(v7, v7_get_global(v7), "crash", crash);

#if V7_ESP_ENABLE__DHT11
  dht11 = v7_create_object(v7);
  v7_set(v7, v7_get_global(v7), "DHT11", 5, 0, dht11);
  v7_set_method(v7, dht11, "read", DHT11_read);
#else
  (void) dht11;
#endif /* V7_ESP_ENABLE__DHT11 */

  debug = v7_create_object(v7);
  v7_set(v7, v7_get_global(v7), "Debug", 5, 0, debug);
  v7_set_method(v7, debug, "mode", Debug_mode);
  v7_set_method(v7, debug, "print", Debug_print);

  sj_init_timers(v7);
  sj_init_v7_ext(v7);

  init_gpiojs(v7);
  init_adcjs(v7);
  init_i2cjs(v7);
  init_pwm(v7);
  init_spijs(v7);
  init_wifi(v7);

  mongoose_init();
  sj_init_http(v7);
  sj_init_ws_client(v7);

  v7_gc(v7, 1);
}

#ifndef V7_NO_FS
void init_smartjs() {
  v7_val_t res;
  if (v7_exec_file(v7, "smart.js", &res) != V7_OK) {
    printf("Init error: ");
    v7_println(v7, res);
  }
}
#endif
