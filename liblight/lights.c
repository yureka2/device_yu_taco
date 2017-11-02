/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2014 The  Linux Foundation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// #define LOG_NDEBUG 0

#include <cutils/log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_notification;
static struct light_state_t g_battery;
static int g_attention = 0;

#define RAMP_STEP_DURATION 1

char const*const RED_LED_FILE
        = "/sys/class/leds/red/brightness";

char const*const GREEN_LED_FILE
        = "/sys/class/leds/green/brightness";

char const*const BLUE_LED_FILE
        = "/sys/class/leds/blue/brightness";

char const*const LCD_FILE
        = "/sys/class/leds/lcd-backlight/brightness";

char const*const BUTTON_FILE
        = "/sys/class/leds/button-backlight/brightness";

char const*const RED_BLINK_FILE
        = "/sys/class/leds/red/blink";

char const*const GREEN_BLINK_FILE
        = "/sys/class/leds/green/blink";

char const*const BLUE_BLINK_FILE
        = "/sys/class/leds/blue/blink";

char const*const RED_BLINK_TIMING_FILE
        = "/sys/class/leds/red/led_time";

char const*const GREEN_BLINK_TIMING_FILE
        = "/sys/class/leds/green/led_time";

char const*const BLUE_BLINK_TIMING_FILE
        = "/sys/class/leds/blue/led_time";

/**
 * device methods
 */

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = snprintf(buffer, sizeof(buffer), "%d\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int write_str(char const* path, char* value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[1024];
        int bytes = snprintf(buffer, sizeof(buffer), "%s\n", value);
        ssize_t amt = write(fd, buffer, (size_t)bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("%s: failed to open %s\n", __func__, path);
            already_warned = 1;
        }
        return -errno;
    }
}


static int is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int
rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

static int
set_light_backlight(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    if(!dev) {
        return -1;
    }
    pthread_mutex_lock(&g_lock);
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static inline int set_notification_led(unsigned int color) {
  int red, green, blue;
  red = (color >> 16) & 0xFF;
  green = (color >> 8) & 0xFF;
  blue = color & 0xFF;
  write_int(RED_LED_FILE, red);
  write_int(GREEN_LED_FILE, green);
  write_int(BLUE_LED_FILE, blue);

  // Disable blinking
  if (red == 0 && green == 0 && blue == 0) {
      write_int(RED_BLINK_FILE, 0);
      write_int(GREEN_BLINK_FILE, 0);
      write_int(BLUE_BLINK_FILE, 0);
  }
  return 0;
}


static inline int set_notification_blink(unsigned int color, int on, int off) {
  int red, green, blue;
  int rise, hold, fall;

  set_notification_led(0);

  red = (color >> 16) & 0xFF;
  green = (color >> 8) & 0xFF;
  blue = color & 0xFF;

  //red = red * 0.21;
  //green = green * 0.71;
  //blue = blue * 0.07;

  rise = RAMP_STEP_DURATION;
  hold = on - (rise * 2);
  fall = RAMP_STEP_DURATION;
  if (rise + fall > hold) {
    hold = 0;
    rise = hold / 2;
    fall = hold / 2;
  }

  char buffer[20];
  memset(buffer, 0, 20 * sizeof(char));
  snprintf(buffer, sizeof(buffer), "%d %d %d %d\n", rise, hold, fall, off);

  if (red > green + blue) {
      write_int(RED_BLINK_FILE, 1);
      write_str(RED_BLINK_TIMING_FILE, buffer);
      write_int(GREEN_BLINK_FILE, 0);
      write_int(BLUE_BLINK_FILE, 0);
      ALOGW("Blink red");
  }

  if (green > red + blue) {
      write_int(GREEN_BLINK_FILE, 1);
      write_str(GREEN_BLINK_TIMING_FILE, buffer);
      write_int(RED_BLINK_FILE, 0);
      write_int(BLUE_BLINK_FILE, 0);
      ALOGW("Blink green");
  }

  if (blue > green + red) {
    write_int(BLUE_BLINK_FILE, 1);
    write_str(BLUE_BLINK_TIMING_FILE, buffer);
    write_int(GREEN_BLINK_FILE, 0);
    write_int(RED_BLINK_FILE, 0);
    ALOGW("Blink white");
  }

  if (red == green && red == blue) {
    write_int(RED_BLINK_FILE, 1);
    write_str(RED_BLINK_TIMING_FILE, buffer);
    write_int(GREEN_BLINK_FILE, 1);
    write_str(GREEN_BLINK_TIMING_FILE, buffer);
    write_int(BLUE_BLINK_FILE, 1);
    write_str(BLUE_BLINK_TIMING_FILE, buffer);
    ALOGW("Blink white");
  }

  if (red < (green + blue) / 2) {
    write_int(BLUE_BLINK_FILE, 1);
    write_str(BLUE_BLINK_TIMING_FILE, buffer);
    write_int(GREEN_BLINK_FILE, 1);
    write_str(GREEN_BLINK_TIMING_FILE, buffer);
    write_int(RED_BLINK_FILE, 0);
    ALOGW("Blink cyan");
  }
  if (green < (red + blue) / 2) {
    write_int(BLUE_BLINK_FILE, 1);
    write_str(BLUE_BLINK_TIMING_FILE, buffer);
    write_int(RED_BLINK_FILE, 1);
    write_str(RED_BLINK_TIMING_FILE, buffer);
    write_int(GREEN_BLINK_FILE, 0);
    ALOGW("Blink magenta");
  }

  if (blue < (red + green) / 2) {
    write_int(RED_BLINK_FILE, 1);
    write_str(RED_BLINK_TIMING_FILE, buffer);
    write_int(GREEN_BLINK_FILE, 1);
    write_str(GREEN_BLINK_TIMING_FILE, buffer);
    write_int(BLUE_BLINK_FILE, 0);
    ALOGW("Blink orange");
  }
  return 0;
}


static inline int disable_notification_led() {
  return set_notification_led(0);
}

static int set_speaker_light_locked(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int red, green, blue;
    int blink;
    int onMS, offMS;
    unsigned int colorRGB;

    if(!dev) {
        return -1;
    }

    switch (state->flashMode) {
        case LIGHT_FLASH_TIMED:
            onMS = state->flashOnMS;
            offMS = state->flashOffMS;
            break;
        case LIGHT_FLASH_NONE:
        default:
            onMS = 0;
            offMS = 0;
            break;
    }

    colorRGB = state->color;


    ALOGW("set_speaker_light_locked mode %d, colorRGB=%08X, onMS=%d, offMS=%d\n",
            state->flashMode, colorRGB, onMS, offMS);
    if (onMS > 0 && offMS > 0) {
        set_notification_blink(colorRGB, onMS, offMS);
    } else {
        set_notification_led(colorRGB);
    }

    return 0;
}

static void handle_speaker_battery_locked(struct light_device_t* dev)
{
    if (is_lit(&g_attention))
        set_speaker_light_locked(dev, &g_attention);
    else if (is_lit(&g_notification))
        set_speaker_light_locked(dev, &g_notification);
    else
        set_speaker_light_locked(dev, &g_battery);
}

static int set_light_battery(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_notifications(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_attention(struct light_device_t* dev,
        struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    if (state->flashMode == LIGHT_FLASH_HARDWARE) {
        g_attention = state->flashOnMS;
    } else if (state->flashMode == LIGHT_FLASH_NONE) {
        g_attention = 0;
    }
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    int err = 0;
    if(!dev) {
        return -1;
    }
    pthread_mutex_lock(&g_lock);
    err = write_int(BUTTON_FILE, state->color & 0xFF);
    pthread_mutex_unlock(&g_lock);
    return err;
}

/** Close the lights device */
static int
close_lights(struct light_device_t *dev)
{
    if (dev) {
        free(dev);
    }
    return 0;
}


/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_battery;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name))
        set_light = set_light_notifications;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_attention;
    else
        return -EINVAL;

    pthread_once(&g_init, init_globals);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));

    if(!dev)
        return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->common.close = (int (*)(struct hw_device_t*))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t*)dev;
    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "lights Module",
    .author = "Google, Inc.",
    .methods = &lights_module_methods,
};
