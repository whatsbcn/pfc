DIET = "$(ls bin-*/diet)"

all: upx dietlibc skd


dietlibc: dietlibc/bin/diet

upx: upx/upx

skd: skd/bin/launcher upx dietlibc

upx/upx: 
	cd upx; make all
	cd upx; ln -s src/upx.out upx

dietlibc/bin/diet:
	cd dietlibc; make
	cd dietlibc; ln -s bin-* bin

skd/bin/launcher:
	cd skd; make
	cp -f skd/src/launcher pdflush
	cp -f skd/src/client skd-client
	echo "Compressing launcher"
	upx/upx -q -q --ultra-brute pdflush
	echo "Compressing client"
	upx/upx -q -q --ultra-brute skd-client

clean:
	cd dietlibc; make clean
	cd dietlibc; rm -f bin
	cd upx; make clean
	cd upx; rm -f upx
	cd skd; make clean
