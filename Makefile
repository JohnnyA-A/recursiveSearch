CFLAGS=-Wall -Wextra -Werror -O2
TARGETS=lab1aaaN3248 libaaaN3248.so

.PHONY: all clean

all: $(TARGETS)

clean:
	rm -rf *.o $(TARGETS)


lab1aaaN3248: lab1aaaN3248.c plugin_api.h
	gcc $(CFLAGS) -o lab1aaaN3248 lab1aaaN3248.c -ldl

libaaaN3248.so: libaaaN3248.c plugin_api.h
	gcc $(CFLAGS) -shared -fPIC -o libaaaN3248.so libaaaN3248.c -ldl -lm
