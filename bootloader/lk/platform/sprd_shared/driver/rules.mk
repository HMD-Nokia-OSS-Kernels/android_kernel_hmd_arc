LOCAL_DIR := $(GET_LOCAL_DIR)

GLOBAL_INCLUDES += \
	platform/sprd_shared/include \
	platform/sprd_shared/soc/$(PLATFORM)/include

# drivers
MODULE_DEPS += \
	platform/sprd_shared/lib \
	$(LOCAL_DIR)/adi \
	$(LOCAL_DIR)/usb \
	$(LOCAL_DIR)/serial \
	$(LOCAL_DIR)/storage \
	$(LOCAL_DIR)/watchdog \
	$(LOCAL_DIR)/led \
	$(LOCAL_DIR)/vibrator \
	$(LOCAL_DIR)/i2c \
	$(LOCAL_DIR)/keypad \
	$(LOCAL_DIR)/gpio \
	$(LOCAL_DIR)/efuse \
	$(LOCAL_DIR)/adc \
	$(LOCAL_DIR)/power \
	$(LOCAL_DIR)/pwm \
	$(LOCAL_DIR)/misc \
	$(LOCAL_DIR)/rtc \
	$(LOCAL_DIR)/spi \
	$(LOCAL_DIR)/video/sprd \
	$(LOCAL_DIR)/video/common \
	$(LOCAL_DIR)/battery \
	$(LOCAL_DIR)/sprd_boot_monitor \
	$(LOCAL_DIR)/log_pointing

ifeq ($(SPRD_ETB_DUMP_LK),1)
MODULE_DEPS += \
	$(LOCAL_DIR)/hwtracing/coresight
endif

ifeq ($(SPRD_SENSOR_HUB_LK),1)
MODULE_DEPS += \
	$(LOCAL_DIR)/sensor_hub
endif

ifeq ($(SPRD_PINCTRL_LK),1)
MODULE_DEPS += \
	$(LOCAL_DIR)/pinctrl
GLOBAL_DEFINES += \
	CONFIG_PINCTRL_LK
endif

ifeq ($(SPRD_SOC_DUMP_SUPPORT),1)
MODULE_DEPS += \
	$(LOCAL_DIR)/autosave
GLOBAL_DEFINES += \
	CONFIG_SPRD_SOCDUMP
endif

ifeq ($(CONFIG_UDC),1)
MODULE_DEPS += \
	$(LOCAL_DIR)/udc

GLOBAL_DEFINES += \
	CONFIG_UDC
ifeq ($(CONFIG_UDC_LCD_MIPI),1)
GLOBAL_DEFINES += \
	CONFIG_UDC_LCD_MIPI
endif
endif


