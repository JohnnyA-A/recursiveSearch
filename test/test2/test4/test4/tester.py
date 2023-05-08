from random import randint
f = open("file_" + str(randint(0x2342, 0x856755)), "wb")
for i in range(0, 1000):
    tmp = randint(0x10000000, 0xffffffff)
    f.write(tmp.to_bytes(4, byteorder='big'))
    if i == 666:
        a = 0xc0a80801
        f.write(a.to_bytes(4, byteorder='big'))
f.close()
