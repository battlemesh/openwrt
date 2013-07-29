#
# Copyright (C) 2013 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

define KernelPackage/rtc-stmp3xxx
    SUBMENU:=$(OTHER_MENU)
    TITLE:=STMP3xxx SoC built-in RTC support
    DEPENDS:=@TARGET_imx23
    $(call AddDepends/rtc)
    KCONFIG:=\
	CONFIG_RTC_CLASS=y \
	CONFIG_RTC_DRV_STMP=m
    FILES:=$(LINUX_DIR)/drivers/rtc/rtc-stmp3xxx.ko
    AUTOLOAD:=$(call AutoLoad,50,rtc-stmp3xxx)
endef

$(eval $(call KernelPackage,rtc-stmp3xxx))

define KernelPackage/wdt-stmp3xxx
    SUBMENU:=$(OTHER_MENU)
    TITLE:=STMP3xxx Watchdog timer
    DEPENDS:=kmod-rtc-stmp3xxx
    KCONFIG:=CONFIG_STMP3XXX_RTC_WATCHDOG
    FILES:=$(LINUX_DIR)/drivers/$(WATCHDOG_DIR)/stmp3xxx_rtc_wdt.ko
    AUTOLOAD:=$(call AutoLoad,51,stmp3xxx_rtc_wdt)
endef

define KernelPackage/wdt-stmp3xxx/description
    Kernel module for STMP3xxx watchdog timer.
endef

$(eval $(call KernelPackage,wdt-stmp3xxx))

define KernelPackage/usb-chipidea-imx
    TITLE:=Support for ChipIdea controllers on i.MX
    DEPENDS:=+kmod-usb-chipidea @TARGET_imx23
    FILES:=\
	$(LINUX_DIR)/drivers/usb/chipidea/ci13xxx_imx.ko
    AUTOLOAD:=$(call AutoLoad,52,ci13xxx_imx,1)
    $(call AddDepends/usb)
endef

define KernelPackage/usb-chipidea-imx/description
    Kernel support for USB ChipIdea controllers on i.MX
endef

$(eval $(call KernelPackage,usb-chipidea-imx,1))

define KernelPackage/usb-mxs-phy
    TITLE:=Support for Freescale MXS USB PHY controllers
    DEPENDS:=+kmod-usb-chipidea-imx
    KCONFIG:= \
	CONFIG_USB_MXS_PHY
    FILES:=$(LINUX_DIR)/drivers/usb/phy/phy-mxs-usb.ko
    AUTOLOAD:=$(call AutoLoad,50,phy-mxs-usb,1)
    $(call AddDepends/usb)
endef

define KernelPackage/usb-mxs-phy/description
    Kernel support for Freescale MXS USB PHY controllers
endef

$(eval $(call KernelPackage,usb-mxs-phy,1))

define KernelPackage/usb-net-smsc95xx
    TITLE:=SMSC95xx USB/2.0 Ethernet driver
    DEPENDS:=@TARGET_imx23
    KCONFIG:=CONFIG_USB_NET_SMSC95XX
    FILES:=$(LINUX_DIR)/drivers/net/usb/smsc95xx.ko
    AUTOLOAD:=$(call AutoLoad,64,smsc95xx)
    $(call AddDepends/usb-net)
endef

define KernelPackage/usb-net-smsc95xx/description
    Kernel support for SMSC95xx USB/2.0 Ethernet driver
endef

$(eval $(call KernelPackage,usb-net-smsc95xx))

