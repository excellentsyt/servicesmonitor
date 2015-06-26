#ifndef _LOGTOFILE_H_
#define _LOGTOFILE_H_

#include <time.h>
#include <stdio.h>
#include <string>
#include <stdarg.h>
#include <iostream>

enum SMLog
{
	START_SERVICE,               //Log start service
	SHUTDOWN_SERVICE,            //Log shutdown service
	NORMAL_OUTPUT,               //Log normal output
	NO_PRINT					 //No log printed onto the screen
};

// Get current time
std::string getCurrentTime();

// Check if the file size is bigger than a limit
void checkFileSize();

// The actual log action
void logAction(std::string log, int IMPORTANT=0);

//
void LOG(SMLog status, const std::string& serviceNameOrOutput);
void LOG(SMLog status, const char * format, ...);
void LOG(const char * format, ...);

//
void vlog(const char * format, va_list args, SMLog status=NO_PRINT);

#endif