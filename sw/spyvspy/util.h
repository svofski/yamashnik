#pragma once

class Util {
public:
    static int dosname(const char* fname, char* buf83);
    
    static int testSuite();
private:
	static int test_dosname_1(const char* fileName, const char* expect);
};
