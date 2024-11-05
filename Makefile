CC := cl
CFLAGS := /EHsc /std:c++20 -D_WIN32_WINNT=0x0601
CINC := /I .\ /I .\lib\boost\ /I .\lib\async-mqtt5\include\

BIN := tracer

all:
	$(CC) $(CFLAGS) $(CINC) .\tracer\main.cpp
