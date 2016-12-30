#include <stdio.h>

#include "common/platform.h"
#include "common/cs_file.h"
#include "fw/src/mgos_app.h"
#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_sys_config.h"
#include "fw/src/mgos_timers.h"
#include "fw/src/mgos_hal.h"
#include "mjs.h"

#if CS_PLATFORM == CS_P_ESP8266
/* On ESP-12E there is a blue LED connected to GPIO2 (aka U1TX). */
#define GPIO 2
#elif CS_PLATFORM == CS_P_CC3200
/* On CC3200 LAUNCHXL pin 64 is the red LED. */
#define GPIO 64 /* The red LED on LAUNCHXL */
#elif CS_PLATFORM == CS_P_MBED
#define GPIO 64
#else
#error Unknown platform
#endif

static int get_gpio(void) {
  return GPIO;
}

void *stub_dlsym(void *handle, const char *name) {
  (void) handle;
  if (strcmp(name, "mgos_gpio_write") == 0) return mgos_gpio_write;
  if (strcmp(name, "mgos_set_timer") == 0) return mgos_set_timer;
  if (strcmp(name, "get_gpio") == 0) return get_gpio;
  return NULL;
}

enum mgos_app_init_result mgos_app_init(void) {
  struct mjs *mjs = mjs_create();
  mjs_set_ffi_resolver(mjs, stub_dlsym);

  mgos_gpio_set_mode(GPIO, MGOS_GPIO_MODE_OUTPUT);

  mjs_err_t err = mjs_exec_file(mjs, "my.js", NULL);

  if (err != MJS_OK) {
    printf("MJS exec error: %s\n", mjs_strerror(mjs, err));
    return MGOS_APP_INIT_ERROR;
  }

  return MGOS_APP_INIT_SUCCESS;
}
