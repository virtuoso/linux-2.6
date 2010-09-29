TARGET = epson_sfm_write_s3c6410
# CC = /usr/local/arm/4.3.1-eabi-armv6/usr/bin/arm-linux-g++
CC = /usr/local/arm/4.3.2-eabi-arm/bin/arm-generic-linux-gnueabi-g++

SOURCES = sfm_write.cpp

# LIBS = -lm ./libstdc++.a

all:
#	$(CC) -o $(TARGET) $(SOURCES) $(LIBS)
	$(CC) -o $(TARGET) $(SOURCES)

clean:
	rm -rf *.o $(TARGET) epson_sfm_write_s3c6410

