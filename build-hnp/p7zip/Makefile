include ../utils/Makefrag

all: download/v17.05.tar.gz
	rm -rf temp build
	mkdir -p temp build
	cd temp && tar vxf ../download/v17.05.tar.gz
	#cd temp/p7zip-17.05/ && git init . && git add --all && git commit -m "init" # help to update 0001-fix-ohos-build.patch with git diff
	cd temp/p7zip-17.05/ && patch -Np1 < ../../0001-fix-ohos-build.patch
	cd temp/p7zip-17.05/ && make -j $(shell nproc) all2
	mkdir -p ../sysroot
	cd temp/p7zip-17.05/ && ./install.sh bin lib/p7zip man doc ../../build/
	cp -rv build/bin build/lib ../sysroot/ | tee file.lst

download/v17.05.tar.gz:
	mkdir -p download
	cd download && wget -O v17.05.tar.gz https://github.com/p7zip-project/p7zip/archive/refs/tags/v17.05.tar.gz
