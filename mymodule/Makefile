obj-m += kernel_module/intercept-module.o

ccflags-y := \
  -DDEBUG \
  -ggdb3 \
  -std=gnu99 \
  -Werror \
  -Wframe-larger-than=1000000000 \
  -Wno-declaration-after-statement \
  $(CCFLAGS)

#CFLAGS_intercept-module.o := -D_FORTIFY_SOURCE=0

km:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
kmc:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
kmup:
	gcc -Wall kernel_module/user-processing.c -o kernel_module/user
kmupc:
	rm kernel_module/user
c:
	gcc -Wall helper/client.c -o helper/c
cc:
	rm helper/c
s:
	gcc -Wall helper/server.c -o helper/s
sc:
	rm helper/s