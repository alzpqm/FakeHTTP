include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-fakehttp
PKG_VERSION:=99.2
PKG_RELEASE:=15
PKG_LICENSE:=GPL-3.0-or-later
PKG_MAINTAINER:=FakeHTTP maintainers
PKGARCH:=all
PKG_BUILD_DEPENDS:=luci-base/host
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-fakehttp
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for FakeHTTP
  DEPENDS:=+fakehttp +luci-base +rpcd-mod-file
endef

define Package/luci-app-fakehttp/description
 LuCI web interface for configuring and controlling the FakeHTTP procd
 service through UCI.
endef

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)
	$(STAGING_DIR_HOSTPKG)/bin/po2lmo \
		$(CURDIR)/po/zh_Hant/fakehttp.po \
		$(PKG_BUILD_DIR)/fakehttp.zh-tw.lmo
endef

define Package/luci-app-fakehttp/install
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DIR) $(1)/usr/lib/lua/luci/i18n
	$(INSTALL_DATA) $(CURDIR)/root/usr/share/luci/menu.d/luci-app-fakehttp.json $(1)/usr/share/luci/menu.d/luci-app-fakehttp.json
	$(INSTALL_DATA) $(CURDIR)/root/usr/share/rpcd/acl.d/luci-app-fakehttp.json $(1)/usr/share/rpcd/acl.d/luci-app-fakehttp.json
	$(INSTALL_DATA) $(CURDIR)/htdocs/luci-static/resources/view/fakehttp.js $(1)/www/luci-static/resources/view/fakehttp.js
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/fakehttp.zh-tw.lmo $(1)/usr/lib/lua/luci/i18n/fakehttp.zh-tw.lmo
endef

define Package/luci-app-fakehttp/postinst
#!/bin/sh
[ -n "$${IPKG_INSTROOT}" ] || {
	rm -f /tmp/luci-indexcache.*
	rm -rf /tmp/luci-modulecache/
	/etc/init.d/rpcd reload >/dev/null 2>&1
}
exit 0
endef

$(eval $(call BuildPackage,luci-app-fakehttp))
