/*
 * logger.h
 *
 *  Created on: 19/06/2009
 *      Author: rescue
 */

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <string>
#include <map>
#include <stdio.h>
#include <stdarg.h>

enum LogLevel {
	LOG_ERROR	= 5,
	LOG_IMPORTANT	= 20,
	LOG_INFORMATION	= 25,
	LOG_TRACE	= 35,
	LOG_PROGRESS	= 50,
	LOG_EVERYTHING	= 100
};

/**
 * Class for simplifying the writing of data to a log.
 */
class _Logger {
public :

	_Logger(std::string logName = "", int debugLevel = LOG_INFORMATION);
	virtual ~_Logger();
	
	/**
	 * Writes a message to the log. If message is important enough it also gets printed to stdout/stderr.
	 */
	virtual void log(int importance, const char * format, ...);
	virtual void vlog(int importance, const char * format, va_list args);
	
	/**
	 * Writes a message to the log. If message is important enough it also gets printed to stdout/stderr.
	 */
	virtual void log(const char * format, ...);
	virtual void vlog(const char * format, va_list args);
	
	/**
	 * Opens a file in the log directory with a given name.
	 */
	virtual FILE *openLogFile(std::string filename, const char *modes);
	
	/**
	 * Writes a message to a specified file in the log. If message is important enough it also gets printed to stdout/stderr.
	 */
	virtual void logToFile(FILE *logfile, std::string event, int importance = LOG_INFORMATION);
	
	
	/**
	 * Gets the directory to which the log is being written.
	 */
	std::string getLogDirectory() {
		return mainLogDir;
	}

	/**
	 * Gets the directory in which a file is located.
	 */
	static std::string getDirectory(const std::string& filename);

	/**
	 * Creates a directory.
	 */
	static bool createDirectory(const std::string& dir);

	/**
	 * Can be used to create an initial log. Returns the number of args used.
	 */
	static int initialise(int argc, char **argv);
protected :
	/**
	 * The highest level at which log messages appear on stdout.
	 */
	int debugLevel;
	
	/**
	 * The directory for the log.
	 */
	std::string mainLogDir;

	/**
	 * The main log file holding logged messages.
	 */
	FILE *eventLog;
};

extern Logger logger;

extern std::vector<std::string> unusedArgs;

#endif /* CASROBOT_LOGGER_H_ */
