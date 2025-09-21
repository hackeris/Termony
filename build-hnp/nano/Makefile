include ../utils/Makefrag

SOURCE_URL = https://gitee.com/nanqu_ait/termony-hnp/releases/download/nano/nano-8.6.0.tar.xz
SOURCE_FILE = nano-8.6.tar.xz
SOURCE_DIR = nano-8.6
CONFIG_ARGS = --prefix=$(PREFIX) --disable-static --enable-shared --host $(OHOS_ARCH)-unknown-linux-musl

# 在 nano 的 Makefile 中添加相对路径
SYSROOT_REL = ../../../../../sysroot
export CFLAGS += -I$(SYSROOT_REL)/include/ncursesw -I$(SYSROOT_REL)/include
export CPPFLAGS += -I$(SYSROOT_REL)/include/ncursesw -I$(SYSROOT_REL)/include

$(eval $(call define_autotools_package))
