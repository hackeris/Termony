include ../utils/Makefrag

SOURCE_URL = $(GNU_MIRROR)/gnu/nano/nano-8.6.tar.xz
SOURCE_FILE = nano-8.6.tar.xz
SOURCE_DIR = nano-8.6
CONFIG_ARGS = --prefix=$(PREFIX) --disable-static --enable-shared --host $(OHOS_ARCH)-unknown-linux-musl

$(eval $(call define_autotools_package))
