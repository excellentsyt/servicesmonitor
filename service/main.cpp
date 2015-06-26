#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <fstream>

#define sleeptime 1000

int main(int argc, char* argv[])
{
	//std::fstream fileDiff("C:\\locToolOutput.txt", std::fstream::in | std::fstream::out | std::fstream::ate);
	//fileDiff << "This is for test." << std::endl;


	while(1){
		printf("Running service 1 ...\n");
		Sleep(sleeptime);
	}

	//fileDiff.close();
    return 0;
}
