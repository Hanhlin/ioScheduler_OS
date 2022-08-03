all: iosched.cpp
	g++ -gdwarf-3 -std=c++11 iosched.cpp -o iosched
clean:
	rm -f iosched *~