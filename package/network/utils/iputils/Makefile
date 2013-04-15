# 
# Copyright (C) 2006-2010 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=iputils
PKG_VERSION:=20101006
PKG_RELEASE:=2

PKG_SOURCE:=$(PKG_NAME)-s$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=http://www.skbuff.net/iputils
PKG_MD5SUM:=a36c25e9ec17e48be514dc0485e7376c

PKG_MAINTAINER:=Jo-Philipp Wich <jow@openwrt.org>

PKG_BUILD_DEPENDS:=sysfsutils
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-s$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk


define Package/iputils/Default
  SECTION:=net
  CATEGORY:=Network
  URL:=http://www.skbuff.net/iputils
endef


define Package/iputils-arping
$(call Package/iputils/Default)
  TITLE:=iputils - arping
  DEPENDS+= +libsysfs
endef

define Package/iputils-arping/description
  Program arping from iputils.
  Sends ARP REQUEST to a neighbour host.
endef


define Package/iputils-clockdiff
$(call Package/iputils/Default)
  TITLE:=iputils - clockdiff
endef

define Package/iputils-clockdiff/description
  Program clockdiff from iputils.
  Measures clock difference between hosts.
endef


define Package/iputils-ping
$(call Package/iputils/Default)
  TITLE:=iputils - ping
endef

define Package/iputils-ping/description
  Program ping from iputils.
  Sends ICMP ECHO_REQUEST to network hosts (IPv4).
endef


define Package/iputils-ping6
$(call Package/iputils/Default)
  TITLE:=iputils - ping6
  DEPENDS+= @IPV6
endef

define Package/iputils-ping6/description
  Program ping6 from iputils.
  Sends ICMP ECHO_REQUEST to network hosts (IPv6).
endef


define Package/iputils-tftpd
$(call Package/iputils/Default)
  TITLE:=iputils - tftpd
endef

define Package/iputils-tftpd/description
  Program tftpd from iputils
  Trivial File Transfer Protocol server.
endef


define Package/iputils-tracepath
$(call Package/iputils/Default)
  TITLE:=iputils - tracepath
endef

define Package/iputils-tracepath/description
  Program tracepath from iputils.
  Traces path to a network host discovering MTU along this path (IPv4).
endef


define Package/iputils-tracepath6
$(call Package/iputils/Default)
  TITLE:=iputils - tracepath6
  DEPENDS+= @IPV6
endef

define Package/iputils-tracepath6/description
  Program tracepath6 from iputils.
  Traces path to a network host discovering MTU along this path (IPv6).
endef


define Package/iputils-traceroute6
$(call Package/iputils/Default)
  TITLE:=iputils - traceroute6
  DEPENDS+= @IPV6
endef

define Package/iputils-traceroute6/description
  Program traceroute6 from iputils.
  Traces path to a network host (IPv6).
endef

ifeq ($(BOARD),brcm47xx)
  TARGET_CFLAGS += -O2 
endif

MAKE_FLAGS += \
	CFLAGS="$(TARGET_CFLAGS) $(TARGET_CPPFLAGS) $(TARGET_LDFLAGS)" \
	CONFIG_IPV6="$(CONFIG_IPV6)" \
	CONFIG_USE_UCLIBC="$(CONFIG_USE_UCLIBC)" \

define Package/iputils-arping/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/arping $(1)/usr/bin/
endef

define Package/iputils-clockdiff/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/clockdiff $(1)/usr/bin/
endef

define Package/iputils-ping/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ping $(1)/usr/bin/
endef

define Package/iputils-ping6/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ping6 $(1)/usr/bin/
endef

define Package/iputils-tftpd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tftpd $(1)/usr/sbin/
endef

define Package/iputils-tracepath/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tracepath $(1)/usr/bin/
endef

define Package/iputils-tracepath6/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/tracepath6 $(1)/usr/bin/
endef

define Package/iputils-traceroute6/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/traceroute6 $(1)/usr/bin/
endef

define Package/iputils-arping/preinst
#!/bin/sh
echo "Removing /usr/bin/arping symlink for busybox"
[ -h "$${IPKG_INSTROOT}/usr/bin/arping" ] && rm -f "$${IPKG_INSTROOT}/usr/bin/arping"
exit 0
endef

define Package/iputils-arping/postrm
#!/bin/sh
echo "Re-enabling /usr/bin/arping symlink for busybox"
ln -sf ../../bin/busybox "$${IPKG_INSTROOT}/usr/bin/arping"
exit 0
endef

define Package/iputils-ping/preinst
#!/bin/sh
echo "Removing /bin/ping symlink for busybox"
[ -h "$${IPKG_INSTROOT}/bin/ping" ] && rm -f "$${IPKG_INSTROOT}/bin/ping"
exit 0
endef

define Package/iputils-ping/postrm
#!/bin/sh
echo "Re-enabling /bin/ping symlink for busybox"
ln -sf busybox "$${IPKG_INSTROOT}/bin/ping"
exit 0
endef

define Package/iputils-ping6/preinst
#!/bin/sh
echo "Removing /bin/ping6 symlink for busybox"
[ -h "$${IPKG_INSTROOT}/bin/ping6" ] && rm -f "$${IPKG_INSTROOT}/bin/ping6"
exit 0
endef

define Package/iputils-ping6/postrm
#!/bin/sh
echo "Re-enabling /bin/ping6 symlink for busybox"
ln -sf busybox "$${IPKG_INSTROOT}/bin/ping6"
exit 0
endef

$(eval $(call BuildPackage,iputils-arping))
$(eval $(call BuildPackage,iputils-clockdiff))
$(eval $(call BuildPackage,iputils-ping))
$(eval $(call BuildPackage,iputils-tftpd))
$(eval $(call BuildPackage,iputils-tracepath))
$(eval $(call BuildPackage,iputils-ping6))
$(eval $(call BuildPackage,iputils-tracepath6))
$(eval $(call BuildPackage,iputils-traceroute6))
