#include "logToFile.h"
#include <Windows.h>

#define MESSAGE_LEN_MAX       4096

extern std::string g_LogPath;
//bool firstTimeFileOpened = true;


// ============================================================================
// Name: getCurrentTime()
// Desc: Obtain the current time in the format of "%d/%m/%Y %H:%M:%S" 
//       and return a string of the time
// ============================================================================
std::string getCurrentTime()
{
  time_t t = time(NULL);
  char tmp[64]; 
  strftime(tmp, sizeof(tmp), "%d/%m/%Y %H:%M:%S", localtime(&t)); 
  return std::string(tmp);
}

// ============================================================================
// Name: getCurrentTime()
// Desc: Obtain the current time in the format of "%d/%m/%Y %H:%M:%S" 
//       and return a string of the time
// ============================================================================
void checkFileSize()
{
  HANDLE hfile = CreateFile(g_LogPath.c_str(),
                            GENERIC_READ|GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL,
                            OPEN_ALWAYS, //OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
                            NULL);

  if (hfile == INVALID_HANDLE_VALUE)
  {
    printf("The current log file path is: %s\ndwError = %d\n", g_LogPath.c_str(), GetLastError());
    //MessageBox(NULL, "CreateFile error!", NULL, MB_OK);
    return;
  }

  DWORD dwsize = GetFileSize(hfile, NULL);
  CloseHandle(hfile);

  if (dwsize == INVALID_FILE_SIZE)
  {
    printf("dwError = %d\n", GetLastError());
    printf("GetFileSize error!\n");
  }
  else
  {
    if (dwsize > 2 * 1024 * 1024)
    {
      DeleteFile(g_LogPath.c_str());
      HANDLE hfileNew = CreateFile(g_LogPath.c_str(),
                            GENERIC_READ|GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL,
                            OPEN_ALWAYS, //OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
                            NULL);

      if (hfileNew == INVALID_HANDLE_VALUE)
      {
        printf("The current log file path is: %s\ndwError = %d\n", g_LogPath.c_str(), GetLastError());
        return;
      }
    }
  }
}

// ============================================================================
// Name: LOG()
// Desc: The log interface
// Parm: status - see "enum SMLog"
//       serviceName - name of logged service
// ============================================================================
void LOG(SMLog status, const std::string& serviceNameOrOutput)
{
  std::string msg = getCurrentTime();
  msg.append(" : ");

  if (status == START_SERVICE)
    msg.append("Start service - ");
  else if (status == SHUTDOWN_SERVICE)
    msg.append("Shutdown service - ");

  msg.append(serviceNameOrOutput);

  logAction(msg, 1);
}

// ============================================================================
// Name: LOG()
// Desc: The log interface
// Parm: status - see "enum SMLog"
//       
// ============================================================================
void LOG(SMLog status, const char * format, ...)
{
  va_list args;

  va_start(args, format);
  vlog(format, args, status);
  va_end(args);
}


// ============================================================================
// Name: LOG()
// Desc: Log interface with variable argument
// Parm: format - the variable format
// ============================================================================
void LOG(const char * format, ...)
{
  va_list args;

  va_start(args, format);
  vlog(format, args);
  va_end(args);
}

// ============================================================================
// Name: vlog()
// Desc: Log variable format to string
// Parm: format - 
//       args -
// ============================================================================
void vlog(const char * format, va_list args, SMLog status)
{
  std::string message = "";
  char formattedMessage[MESSAGE_LEN_MAX];
  formattedMessage[MESSAGE_LEN_MAX-1] = '\0';
  vsnprintf(formattedMessage, MESSAGE_LEN_MAX - 1, format, args);

  message.append(getCurrentTime());
  message.append(" : ");

  if (status == START_SERVICE)
    message.append("Start service - ");
  else if (status == SHUTDOWN_SERVICE)
    message.append("Shutdown service - ");

  message.append(formattedMessage);

  if (status == NO_PRINT)
    logAction(message);
  else
    logAction(message, 1);
}

// ============================================================================
// Name: logAction()
// Desc: The actual log action
// Parm: log - the string logged
// ============================================================================
void logAction(std::string log, int IMPORTANT)
{
  checkFileSize();

  log.append("\r\n");

  if (FILE* eventLog = fopen(g_LogPath.c_str(), "a+b"))
  {
    fputs(log.c_str(), eventLog);
    fflush(eventLog);
    fclose(eventLog);
  }
  else
  {
    printf("Failed to open file \"SMLog.txt\" ...\n");
  }

  if (IMPORTANT)
    printf("%s", log.c_str());
}

