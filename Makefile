all: tftp.out

tftp.out: main.cpp
	clang++ -Wall -std=c++11 -o tftp.out main.cpp
