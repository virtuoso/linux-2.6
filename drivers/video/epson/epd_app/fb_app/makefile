TARGET = epson_fbtest_s3c6410
CC = /usr/local/arm/4.3.1-eabi-armv6/usr/bin/arm-linux-g++

SOURCES = fbtest.cpp binary_result_500x510.c image_api.cpp k_binary_48x48.c bg_binary_100x100.c

# LIBS = -lm ./libstdc++.a

all:
#	$(CC) -o $(TARGET) $(SOURCES) $(LIBS)
	$(CC) -o $(TARGET) $(SOURCES)

clean:
	rm -rf *.o $(TARGET) epson_fbtest_s3c6410

