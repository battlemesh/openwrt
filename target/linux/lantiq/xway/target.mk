ARCH:=mips
SUBTARGET:=xway
BOARDNAME:=XWAY
FEATURES:=squashfs atm mips16
CFLAGS+= -mtune=34kc -mdsp

DEFAULT_PACKAGES+=kmod-leds-gpio kmod-gpio-button-hotplug

define Target/Description
	Lantiq XWAY
endef
