from random import randint
f = open("file_" + str(randint(0x2342, 0x856755)), "wb")
a = 0xc0a80801
f.write(a.to_bytes(4, byteorder='big'))
f.close()
