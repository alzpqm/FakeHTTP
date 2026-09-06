include $(TOPDIR)/rules.mk

PKG_NAME:=fakehttp
PKG_VERSION:=99.2
PKG_RELEASE:=15

PKG_LICENSE:=GPL-3.0-or-later
PKG_LICENSE_FILES:=LICENSE
PKG_MAINTAINER:=FakeHTTP maintainers
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

FAKEHTTP_SOURCE_DIR?=$(realpath $(CURDIR)/../..)
FAKEHTTP_BUILD_VERSION?=$(PKG_VERSION)-r$(PKG_RELEASE)

include $(INCLUDE_DIR)/package.mk

define Package/fakehttp
  SECTION:=net
  CATEGORY:=Network
  TITLE:=TCP HTTP obfuscation with NFQUEUE
  URL:=https://github.com/alzpqm/FakeHTTP
  DEPENDS:=+libnetfilter-queue +libnfnetlink +libmnl +kmod-nfnetlink-queue \
    +PACKAGE_firewall4:nftables-json +PACKAGE_firewall4:kmod-nft-queue \
    +PACKAGE_firewall:iptables-mod-nfqueue \
    +PACKAGE_firewall:iptables-mod-conntrack-extra
endef

define Package/fakehttp/description
 FakeHTTP obfuscates TCP connections as HTTP-like traffic using Linux
 Netfilter Queue. This package adds an OpenWrt procd service, UCI
 configuration, and a small setup helper.
endef

define Package/fakehttp/conffiles
/etc/config/fakehttp
endef

define Build/Prepare
	$(if $(wildcard $(FAKEHTTP_SOURCE_DIR)/src/mainfun.c),,$(error Set FAKEHTTP_SOURCE_DIR=/path/to/FakeHTTP when building this package))
	rm -rf $(PKG_BUILD_DIR)
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(FAKEHTTP_SOURCE_DIR)/Makefile $(FAKEHTTP_SOURCE_DIR)/LICENSE $(FAKEHTTP_SOURCE_DIR)/include $(FAKEHTTP_SOURCE_DIR)/src $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		STRIP="$(TARGET_STRIP)" \
		CFLAGS="$(TARGET_CFLAGS) $(TARGET_CPPFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		PREFIX=/usr \
		VERSION="$(FAKEHTTP_BUILD_VERSION)"
endef

define Package/fakehttp/install
	$(MAKE) -C $(PKG_BUILD_DIR) DESTDIR="$(1)" PREFIX=/usr install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/init.d $(1)/usr/sbin
	$(INSTALL_CONF) $(CURDIR)/files/etc/config/fakehttp $(1)/etc/config/fakehttp
	$(INSTALL_BIN) $(CURDIR)/files/etc/init.d/fakehttp $(1)/etc/init.d/fakehttp
	$(INSTALL_BIN) $(CURDIR)/files/usr/sbin/fakehttp-setup $(1)/usr/sbin/fakehttp-setup
endef

define Package/fakehttp/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	/etc/init.d/fakehttp enable >/dev/null 2>&1
	/etc/init.d/fakehttp restart >/dev/null 2>&1
}
exit 0
endef

define Package/fakehttp/prerm
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/fakehttp stop >/dev/null 2>&1
case "$${1:-}" in
remove|deinstall|uninstall)
	[ -n "$${IPKG_INSTROOT}" ] || /etc/init.d/fakehttp disable >/dev/null 2>&1
	;;
esac
exit 0
endef

$(eval $(call BuildPackage,fakehttp))
