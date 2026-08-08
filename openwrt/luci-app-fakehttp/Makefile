include $(TOPDIR)/rules.mk

PKG_NAME:=luci-app-fakehttp
PKG_VERSION:=99.2
PKG_RELEASE:=4
PKG_LICENSE:=GPL-3.0-or-later
PKG_MAINTAINER:=FakeHTTP maintainers
PKGARCH:=all

include $(INCLUDE_DIR)/package.mk

define Package/luci-app-fakehttp
  SECTION:=luci
  CATEGORY:=LuCI
  SUBMENU:=3. Applications
  TITLE:=LuCI support for FakeHTTP
  DEPENDS:=+fakehttp +luci-base
endef

define Package/luci-app-fakehttp/description
 LuCI web interface for configuring and controlling the FakeHTTP procd
 service through UCI.
endef

define Build/Compile
endef

define Package/luci-app-fakehttp/install
	$(INSTALL_DIR) $(1)/usr/share/luci/menu.d
	$(INSTALL_DIR) $(1)/usr/share/rpcd/acl.d
	$(INSTALL_DIR) $(1)/www/luci-static/resources/view
	$(INSTALL_DATA) $(CURDIR)/root/usr/share/luci/menu.d/luci-app-fakehttp.json $(1)/usr/share/luci/menu.d/luci-app-fakehttp.json
	$(INSTALL_DATA) $(CURDIR)/root/usr/share/rpcd/acl.d/luci-app-fakehttp.json $(1)/usr/share/rpcd/acl.d/luci-app-fakehttp.json
	$(INSTALL_DATA) $(CURDIR)/htdocs/luci-static/resources/view/fakehttp.js $(1)/www/luci-static/resources/view/fakehttp.js
endef

$(eval $(call BuildPackage,luci-app-fakehttp))
