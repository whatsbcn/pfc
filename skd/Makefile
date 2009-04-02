DIET = "$(ls bin-*/diet)"

all: dietlibc


dietlibc: dietlibc/bin/diet

dietlibc/bin/diet:
	cd dietlibc; make
	cd dietlibc; ln -s bin-* bin

clean:
	cd dietlibc; make clean
	cd dietlibc; rm -f bin
	cd skd; make clean
