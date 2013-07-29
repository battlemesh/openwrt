#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define Profile/olinuxino
  NAME:=Olimex Olinuxino boards
  PACKAGES += imx-bootlets kmod-usb-mxs-phy kmod-usb-net kmod-usb-net-smsc95xx \
          kmod-gpio-mcp23s08 kmod-leds-gpio kmod-ledtrig-heartbeat kmod-rtc-stmp3xxx
endef

define Profile/olinuxino/Description
	Olimex Olinuxino boards
endef
$(eval $(call Profile,olinuxino))

