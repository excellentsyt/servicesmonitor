#include <stdio.h>
#include <stdlib.h>
#include "serviceMonitor.h"

bool IsXML(char* inputFile);
float StringToNumber(const std::string& str);

// ==========================================================================
// Print error info
void PrintUsageError();
// ==========================================================================

//define a global variable for the output directory of the log file
std::string g_LogPath;


bool g_bHasInterruptRequest = false;
static BOOL WINAPI HandleConsoleInterrupt(__in DWORD dwCtrlType)
{
  g_bHasInterruptRequest = true;
  return TRUE;
}


//=============================================================================
// Name: 
// Desc: 
//=============================================================================
int main(int argc, char** argv)
{
  if (argc <= 1)
  {
    PrintUsageError();
    return 0;
  }

  ::SetConsoleCtrlHandler(&HandleConsoleInterrupt, TRUE);

  std::string curDir = argv[0];
  {
    size_t len = strlen(argv[0]);
    for (size_t pos = len; pos > 0; --pos)
    {
      if (argv[0][pos] == '/' || argv[0][pos] == '\\')
      {
        curDir = curDir.substr(0, pos + 1);
        break;
      }
    }

    g_LogPath.append(curDir);
    g_LogPath.append("SMLog.txt");
  }


  ServiceMonitor sm(curDir);

  if (strcmp(argv[1], "runAll") == 0)
  {
    sm.Run();
  }
  else if (strcmp(argv[1], "stopAll") == 0)
  {
    sm.Stop();
  }
  else if (strcmp(argv[1], "list") == 0)
  {
    sm.List();
  }
  else if (strcmp(argv[1], "start") == 0)
  {
    if (argc == 3)
    {
      sm.Start(argv[2]);
    }
    else if (argc == 4)
    {
      float number = StringToNumber(argv[3]);
      sm.Start(argv[2], number);
    }
    else
    {
      printf("It is not possible to reach here ...\n");
    }
  }
  else if (strcmp(argv[1], "shutdown") == 0)
  {
    if (argc < 3)
      PrintUsageError();
    else
      sm.ShutDown(argv[2]);
  }
  else if (strcmp(argv[1], "check") == 0)
  {
    if (argc < 3)
      PrintUsageError();
    else
      sm.Check(argv[2]);
  }
  else if (strcmp(argv[1], "detach") == 0)
  {
    if (argc < 3)
      PrintUsageError();
    else
      sm.Detach(argv[2]);
  }
  else if (strcmp(argv[1], "attach") == 0)
  {
    if (argc < 3)
    {
      PrintUsageError();
    }
    else
    {
      if (argc == 4)
        sm.Attach(argv[2], argv[3]);
      else if (argc == 3)
        sm.Attach(argv[2]);
    }
  }
  else if (strcmp(argv[1], "detachAll") == 0)
  {
    sm.DetachAll();
  }
  else if (strcmp(argv[1], "daemon") == 0)
  {
    sm.RunDaemon();
  }
  else if (strcmp(argv[1], "testopenfile") == 0)
  {
    sm.testopenfile();
  }
  else if (IsXML(argv[1]))
  {
    sm.ReadConfig(argv[1]);
  }

  return 0;
}

bool IsXML(char* inputFile)
{
  if (strstr(inputFile, ".xml") != NULL)
    return true;

  return false;
}


// ============================================================================
// Name: StringToNumber
// Desc: 
// ============================================================================
float StringToNumber(const std::string& str)
{
  if (str.length() == 0)
    return 0.f;

  return (float)atof(str.c_str());
}


void PrintUsageError()
{
  std::cout << "usage:" << std::endl
            << "| ServiceMonitorClient.exe runAll" << std::endl
            << "| ServiceMonitorClient.exe stopAll" << std::endl
            << "| ServiceMonitorClient.exe shutdown <serviceName_or_fullpath>" << std::endl
            << "| ServiceMonitorClient.exe start <serviceFullPath>.exe <interval_time>(optional)" << std::endl
            //<< "| ServiceMonitorClient.exe start <runScript>.bat <serviceFullpath>.exe | <startEXEScript>.bat" << std::endl
            //<< "| ServiceMonitorClient.exe monitor <serviceName_or_fullpath> (NOT USED YET)" << std::endl
            << "| ServiceMonitorClient.exe check <serviceName>" << std::endl
            << "| ServiceMonitorClient.exe list" << std::endl
            << "| ServiceMonitorClient.exe detach <serviceName_or_fullPath>" << std::endl
            << "| ServiceMonitorClient.exe detachAll" << std::endl
            << "| ServiceMonitorClient.exe attach <serviceFullPath> [|<interval_restart>]" << std::endl
            << "| ServiceMonitorClient.exe daemon" << std::endl
            << "| ServiceMonitorClient.exe <file_name>.xml" << std::endl;
}
