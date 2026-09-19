/**
 * @file lv_conf.h
 * Optimized Configuration for Waveshare ESP32-S3 4.3" Touch LCD (Type B)
 * Targeted Environment: 2023 Husqvarna 701 Enduro Dashboard
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   🎯 SYSTEM SETTINGS
 *====================*/
#define LV_USE_BUILTIN_MALLOC_PROGRAMMATICALLY 0

/* Force LVGL engine to target the board's 8MB OPI external PSRAM layer */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

/* Refresh rate adjustments for display smooth layouts */
#define LV_DEF_REFR_PERIOD 16 /* ~60Hz Frame Calculation Tick */

/*====================
   🎨 GRAPHICS ENGINE ADVANCED CONFIGURATION
 *====================*/
#define LV_COLOR_DEPTH 16     /* 16-bit RGB565 required by the Waveshare panel interface */

/* Anti-aliasing handles smoothing needle gauge curves under vibration */
#define LV_USE_DRAW_SW_AA_SCALE_NORMAL 1 
#define LV_DRAW_SW_SUPPORT_RGB565      1

/*====================
   🖥️ LAYOUTS & WIDGET CONFIGURATIONS
 *====================*/
/* Enable core UI layouts required for custom instrumentation layout rendering */
#define LV_USE_LABEL    1
#define LV_USE_BUTTON   1
#define LV_USE_BAR      1 /* Ideal for progressive fuel meters */
#define LV_USE_ARC      1 /* Ideal for circular tachometer (RPM) meters */

/*====================
   🔤 FONT MAPPING INTERFACE
 *====================*/
/* Enable the system default Montserrat font sizes used in our C++ structure */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1 /* Engine Temp Text Size */
#define LV_FONT_MONTSERRAT_28 1 /* Virtual Fuel Gauge Readout Size */
#define LV_FONT_MONTSERRAT_48 1 /* Large Center Gear Position Gauge Size */

/* Set system baseline text engine mapping defaults */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   🛠️ HARDWARE PROFILE OPTIMIZATIONS
 *====================*/
/* Disable execution-blocking alert layers to maintain 500kbps CAN integrity */
#define LV_USE_LOG      0
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0

#endif /*LV_CONF_H*/
