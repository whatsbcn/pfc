DIET = "$(ls bin-*/diet)"

all: dietlibc skd


dietlibc: dietlibc/bin/diet

skd: skd/bin/launcher skd/bin/client

dietlibc/bin/diet:
	cd dietlibc; make
	cd dietlibc; ln -s bin-* bin

skd/bin/launcher skd/bin/client:
	cd skd; make

clean:
	cd dietlibc; make clean
	cd dietlibc; rm -f bin
	cd skd; make clean
