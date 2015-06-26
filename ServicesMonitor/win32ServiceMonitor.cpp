#include "win32ServiceMonitor.h"
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <Wincon.h>
#include <ws2tcpip.h>
#include <ctime>
#include <sstream>
#include <string.h>

#pragma   comment(lib,   "ws2_32.lib")

#define SLEEP_NETSTART 250000
#define SLEEP_MONITOR 1000
#define HOUR_TO_SEC 3600


extern bool g_bHasInterruptRequest;
bool g_bWantShutdown = false;

//=============================================================================
// Name: Win32ServiceMonitor
// Desc: 
//=============================================================================
Win32ServiceMonitor::Win32ServiceMonitor(const std::string& startupDir)
 : m_startupDir(startupDir)
 , m_hCurrentProcess(NULL)
 , SMMutex(NULL)
 , IntervalMutex(NULL)
 , gPidToFind(NULL)
 , gTargetWindowHwnd(NULL)
 , m_port(DEFAULT_PORT_DAEMON)
 , m_needResetCounting(false)
 , m_secondReset(0.0f)
{
  SMMutex = CreateMutex(NULL,              // default security attributesb
                        FALSE,             // initially not owned
                        NULL);             // unnamed mutex

  if (SMMutex == NULL) 
    printf("CreateMutex SMMutex error: %d\n", GetLastError());

  IntervalMutex = CreateMutex(NULL,         // default security attributesb
                              FALSE,        // initially not owned
                              NULL);        // unnamed mutex

  if (IntervalMutex == NULL) 
    printf("CreateMutex IntervalMutex error: %d\n", GetLastError());
}


//=============================================================================
// Name: ~Win32ServiceMonitor
// Desc: 
//=============================================================================
Win32ServiceMonitor::~Win32ServiceMonitor()
{
  for (std::vector<HANDLE>::iterator itr = hThreadVec.begin(); itr != hThreadVec.end(); ++itr)
    CloseHandle(*itr);
  CloseHandle(m_hCurrentProcess);
  CloseHandle(SMMutex);
  CloseHandle(IntervalMutex);
}


// ============================================================================
// Name: InitialiseProcessSnapshot
// Desc: As the name describes
// ============================================================================
HANDLE Win32ServiceMonitor::InitialiseProcessSnapshot() const
{
  // Take a snapshot of all processes in the system.
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
  {
    LOG(NORMAL_OUTPUT, "InitialiseProcessSnapshot> Failed to create process snapshot");
    return NULL;
  }

  return hSnapshot;
}

// ============================================================================
// Name: ClearMemory
// Desc: 
// ============================================================================
void Win32ServiceMonitor::ClearMemory(STARTUPINFO *si, PROCESS_INFORMATION *pi)
{
  ZeroMemory(si, sizeof(*si));
  (*si).cb = sizeof(*si);
  ZeroMemory(pi, sizeof(*pi));
}


// ============================================================================
// Name: StartNewProcess
// Desc: Launch a new process
// Parm: si - 
// Parm: pi - 
// Parm: full path - full path of the service to the new process
// ============================================================================
bool Win32ServiceMonitor::StartNewProcess(STARTUPINFO *io_si, PROCESS_INFORMATION *io_pi, const std::string& fullpath)
{
  ClearMemory(io_si, io_pi);

  // Start the child process
  BOOL result = CreateProcess(NULL,                     // No module name (use command line)
                              (LPSTR)fullpath.c_str(),  // Command line
                              NULL,                     // Process handle not inheritable
                              NULL,                     // Thread handle not inheritable
                              FALSE,                    // Set handle inheritance to FALSE
                              CREATE_NEW_PROCESS_GROUP, // No creation flags
                              NULL,                     // Use parent's environment block
                              NULL,                     // Use parent's starting directory 
                              io_si,                    // Pointer to STARTUPINFO structure
                              io_pi);                   // Pointer to PROCESS_INFORMATION structure

  if (result)
    LOG(START_SERVICE, fullpath);
  else
    LOG(NORMAL_OUTPUT, "Failed to create process with error %d (%s)", GetLastError(), fullpath.c_str());

  return result == TRUE;
}


// ============================================================================
// Name: StartNewProcess
// Desc: Launch a new process
// Parm: fullpath - full path of the service to the new process
// ============================================================================
bool Win32ServiceMonitor::StartNewProcess(const std::string& fullpath)
{
  if (m_hCurrentProcess)
  {
    LOG(NORMAL_OUTPUT, "ERROR> Attempt made to start a process without clearing previous process handle");
    return false;
  }

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ClearMemory(&si, &pi);

  // Start the child process
  BOOL rtnResult = CreateProcess( NULL,                     // Use module "C:\\Program Files (x86)\\JPSoft\\TCCLE12\\tcc.exe"
                                  (LPSTR)fullpath.c_str(),  // Command line "C:\\src\\trainz\\main\\buildscripts\\netstart-trainzutilhttp.bat"
                                  NULL,                     // Process handle not inheritable
                                  NULL,                     // Thread handle not inheritable
                                  FALSE,                    // Set handle inheritance to FALSE
                                  CREATE_NEW_PROCESS_GROUP, //CREATE_NEW_CONSOLE,          // CREATE_NEW_PROCESS_GROUP,   // Creation flags  & DETACHED_PROCESS
                                  NULL,                     // Use parent's environment block
                                  NULL,                     // Use parent's starting directory 
                                  &si,                      // Pointer to STARTUPINFO structure
                                  &pi);                     // Pointer to PROCESS_INFORMATION structure

  if (rtnResult)
  {
    SetConsoleTitle(fullpath.c_str());
    LOG(START_SERVICE, fullpath);

    m_hCurrentProcess = pi.hProcess;
    m_currentProcessID = pi.dwProcessId;
  }

  return rtnResult == TRUE;
}


// ============================================================================
// Name: GetWin32RunningServices
// Desc: Get all currently running Win32 processes
// ============================================================================
BOOL Win32ServiceMonitor::GetWin32RunningServices(std::vector<std::string>& io_runningServices)
{
  io_runningServices.clear();

  HANDLE hSnapshot = InitialiseProcessSnapshot();

  PROCESSENTRY32 processInfo;
  processInfo.dwSize = sizeof(PROCESSENTRY32);

  // Retrieve information about the first process, and exit if unsuccessful
  if (!Process32First(hSnapshot, &processInfo))
  {
    // show cause of failure
    LOG(NORMAL_OUTPUT, "Failed to get active process list with error %d", GetLastError());
    CloseHandle(hSnapshot);
    return FALSE;
  }

  // Now walk the snapshot of processes, and
  // display information about each process in turn
  do
  {
    io_runningServices.push_back(ToLower(processInfo.szExeFile));
  } while(Process32Next(hSnapshot, &processInfo));

  CloseHandle(hSnapshot);
  return TRUE;
}


// ============================================================================
// Name: MonitorAllServices
// Desc: Start monitoring all services
// ============================================================================
void Win32ServiceMonitor::MonitorAllServices()
{
  EnableDebugPrivilege();

  SetConsoleTitle("Monitor Tool Daemon Process");
  LOG(NORMAL_OUTPUT, "Monitoring ...");

  std::map<std::string, int> serverDowntime;

  int counter = 0;
  while (true)
  {
    if (g_bHasInterruptRequest)
    {
      LOG(NORMAL_OUTPUT, "Received shutdown request, closing daemon (monitored processes will stay active)");

      // Wait a bit so users can read the above message
      std::string counter = ".....";
      while (counter.length())
      {
        LOG(NORMAL_OUTPUT, counter);
        counter.resize(counter.length() - 1);
        Sleep(1000);
      }
      return;
    }

    // Check for crashed processes every so often
    if (++counter > 10)
    {
      std::vector<std::string> runningServices;
      GetWin32RunningServices(runningServices);

      MutexLock(SM);

      // Monitor all services launch status (launched || stopped)
      for (std::map<std::string, std::string>::iterator itr1 = win32ServicesMap.begin(); itr1 != win32ServicesMap.end(); ++itr1)
      {
		bool found = false;
        for (std::vector<std::string>::iterator itr2 = runningServices.begin(); itr2 != runningServices.end(); ++itr2)
        {
          if (_stricmp(itr1->first.c_str(), (*itr2).c_str()) == 0)
          {
            found = true;
            serverDowntime[itr1->first] = 0;
            break;
          }
        }

        if (!found)
        {
          std::map<std::string, int>::iterator itr = serverDowntime.find(itr1->first);
          if (itr == serverDowntime.end())
          {
            serverDowntime[itr1->first] = 1;
          }
          else if (itr->second < 5)
          {
            serverDowntime[itr1->first] = itr->second + 1;
          }
          else
          {
            LOG(NORMAL_OUTPUT, "");
            LOG(NORMAL_OUTPUT, "Service '%s' has unexpectedly shut down, restarting it now", itr1->first.c_str());

            serverDowntime[itr1->first] = 0;
            StartCrashedService(itr1->first, itr1->second);
          }
        }
      }

      MutexUnlock(SM);

      // Currently disabled as it doesn't work with many iTrainz services
      //monitorHang();
    }

    // Sleep for the period of monitoring action
    Sleep(SLEEP_MONITOR);
  }
}


// ============================================================================
// Name: MonitorHang
// Desc: Monitor if a process is in "hang" status
// ============================================================================
/*void Win32ServiceMonitor::MonitorHang()
{
  std::map<std::string, DWORD>::iterator iter3;
  for(iter3 = win32ServicesProcessIdMap.begin(); iter3 != win32ServicesProcessIdMap.end(); iter3++)
  {
    // Handle of application window.
    // Get it by using "EnumWindows()".
    gPidToFind = iter3->second;
    EnumWindows((WNDENUMPROC)myWNDENUMPROC, (LPARAM)this); //After this, the "gTargetWindowHwnd" variable will have the window handle(HWND).

    DWORD result = 0;
    LRESULT rtn = 0;

    // Send the NULL message to the window.
    // SMTO_ABORTIFHUNG is specified to return immediately, if the process is hung.
    rtn = ::SendMessageTimeout(gTargetWindowHwnd, // Window Handle
                               WM_NULL,           // Message
                               0,                 // WParam
                               0,                 // LParam
                               SMTO_ABORTIFHUNG,  // Flags
                               10000,             // Timeout for 5 seconds
                               &result );         // Result

    // Check whether the WM_NULL message is processed.
    if (!rtn)
    {
      LOG(NORMAL_OUTPUT, "");
      LOG(NORMAL_OUTPUT, "Service '%s' appears to be hung, terminating is now", iter3->first.c_str());

      // Since the WM_NULL message is not processed,
      // the targeted window is hung and then it should be terminated immediately.

      HANDLE hangProcessHandle;
      std::map<std::string, HANDLE>::iterator tmpIter = win32ServicesHandleMap.find(ToLower(iter3->first));
      if (tmpIter != win32ServicesHandleMap.end())
        hangProcessHandle = tmpIter->second;

      win32ServicesMap.erase(iter3->first.c_str());
      TerminateService(hangProcessHandle);
      CloseHandle(hangProcessHandle);
    }
  }
}*/

// ============================================================================
// Name: Stop
// Desc: Stop services either included in the data structure "servicesMap" or 
//       all services currently under monitoring which is dependent on the 
//       value of stopServicesInConfigFile
// Parm: servicesMap - includes all services need to be stopped
//       stopServicesInConfigFile - if true, stop services in servicesMap
//       if false, stop all services currently under monitoring
//       default is false
// ============================================================================
void Win32ServiceMonitor::Stop(std::map<std::string, std::string> &servicesMap, bool stopServicesInConfigFile)
{
  // if this is true, just shutdown the specified services instead of all of them
  if (stopServicesInConfigFile)
  {
    for (std::map<std::string, std::string>::iterator itr = servicesMap.begin(); itr != servicesMap.end(); ++itr)
      ShutdownServiceSafe(itr->first);
  }
  else
  {
    SocketCommunicateWithDaemonServer("STP");
  }
}

// ============================================================================
// Name: StopAllServicesFromDeamon
// Desc: Stop all services directly from daemon itself (instead of sending 
//       the msg to daemon to stop)
// ============================================================================
void Win32ServiceMonitor::StopAllServicesFromDaemon()
{
  MutexLock(SM);
  std::map<std::string, std::string> clonedMap = win32ServicesMap;
  win32ServicesMap.clear();
  MutexUnlock(SM);

  MutexLock(IM);
  win32ServicesIntervalMap.clear();
  MutexUnlock(IM);

  for(std::map<std::string, std::string>::iterator itr = clonedMap.begin(); itr != clonedMap.end(); ++itr)
    ShutdownServiceSafe(itr->first);
}

// ============================================================================
// Name: StartCrashedService
// Desc: Start a crashed service monitored by daemon process
// Parm: serviceName - name of this service
// Parm: fullpath - full path of this service
// ============================================================================
BOOL Win32ServiceMonitor::StartCrashedService(std::string serviceName, std::string fullpath)
{
  STARTUPINFO *si = new STARTUPINFO[1];
  PROCESS_INFORMATION *pi = new PROCESS_INFORMATION[1];
  HANDLE *hProcesses = new HANDLE[1];
  ClearMemory(&si[0],&pi[0]);

  std::string cmdLine = CreateCommand(serviceName, fullpath.c_str());

  // Start the child process
  BOOL rtnResult = CreateProcess( NULL,                     // Use module "C:\\Program Files (x86)\\JPSoft\\TCCLE12\\tcc.exe"
                                  (LPSTR)cmdLine.c_str(),   // Command line "C:\\src\\trainz\\main\\buildscripts\\netstart-trainzutilhttp.bat"
                                  NULL,                     // Process handle not inheritable
                                  NULL,                     // Thread handle not inheritable
                                  FALSE,                    // Set handle inheritance to FALSE
                                  CREATE_NEW_CONSOLE,       //CREATE_NEW_CONSOLE,          // CREATE_NEW_PROCESS_GROUP,   // Creation flags  & DETACHED_PROCESS
                                  NULL,                     // Use parent's environment block
                                  NULL,                     // Use parent's starting directory 
                                  &si[0],                   // Pointer to STARTUPINFO structure
                                  &pi[0]);                  // Pointer to PROCESS_INFORMATION structure
  if (rtnResult)
    hProcesses[0] = pi[0].hProcess;
  else
    LOG(NORMAL_OUTPUT, "Failed to create process with error %d (%s)", GetLastError(), cmdLine.c_str());

  delete si;
  delete pi;
  delete[] hProcesses;

  return rtnResult == TRUE;
}

// ============================================================================
// Name: CreateCommand
// Desc: 
// Parm: 
// ============================================================================
std::string Win32ServiceMonitor::CreateCommand(std::string serviceName, const char* fullpath)
{
  std::string commandLine;
  commandLine.append(m_startupDir);
#ifdef _DEBUG
  commandLine.append("ServiceMonitor_d.exe start ");
#else
  commandLine.append("ServiceMonitor.exe start ");
#endif

  commandLine.append("\"");
  commandLine.append(fullpath);
  commandLine.append("\" ");

  MutexLock(IM);
  std::map<std::string, float>::iterator intervalItr = win32ServicesIntervalMap.find(serviceName);
  if (intervalItr != win32ServicesIntervalMap.end())
    commandLine.append(NumberToString(intervalItr->second));
  MutexUnlock(IM);

  return commandLine;
}


// ============================================================================
// Name: startService
// Desc: Start a specified service with X hours interval to be restarted
// Parm: serviceName - name of this service
// Parm: fullpath - full path of this service
// Parm: interval - the time frame to restart it
// ============================================================================
BOOL Win32ServiceMonitor::StartService(std::string serviceName, std::string fullpath, float interval)
{
  //create a tmp map with only one item
  std::map<std::string, std::string> tmp;
  tmp.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(fullpath)));

  if (!IsStillAlive(serviceName))
  {
    if (StartNewProcess(fullpath))
    {
      m_port = DecidePort(serviceName);

      // add this process to daemon
      CreateThread(LISTEN_THREAD);
      UpdateDaemon(tmp, "ADD");
      if (interval > 0.0005)
        UpdateDaemon(tmp, interval, "INV");

      MonitorActiveService(serviceName, fullpath, interval);
    }
  }
  else
  {
    LOG(NORMAL_OUTPUT, "The service '%s' is already running and will not be started", serviceName.c_str());
  }

  return TRUE;
}

// ============================================================================
// Name: alreadyInWin32ServiceMap
// Desc: Check if a service has been put into the win32ServicesMap 
// Parm: serviceName - name of the service
// ============================================================================
bool Win32ServiceMonitor::AlreadyInWin32ServiceMap(std::string serviceName)
{
  bool found = false;

  MutexLock(SM);
  for (std::map<std::string, std::string>::iterator itr = win32ServicesMap.begin(); !found && itr != win32ServicesMap.end(); ++itr)
    if (_stricmp(serviceName.c_str(), itr->first.c_str()) == 0)
      found = true;
  MutexUnlock(SM);

  return found;
}

// ============================================================================
// Name: AllocateNewConsole
// Desc: Free a process from a console and then allocate it a new console
// ============================================================================
void Win32ServiceMonitor::AllocateNewConsole()
{
  FreeConsole();
  AllocConsole();
}


// ============================================================================
// Name: List
// Desc: List the current services under monitoring
// ============================================================================
void Win32ServiceMonitor::List()
{
  SocketCommunicateWithDaemonServer("LST");
}


// ============================================================================
// Name: ShutdownServiceSafe
// Desc: Shut down a specified service in safe mode such as sending a ctrl+break comb
// Parm: serviceNameOrFullPath - can be name of the service only or the 
//       full path of the service which will be parsed to get the name also
//       needTOUpdateDaemon - if true, the monitored services by daemon 
//       will be updated; if false, do not update daemon (should have 
//       been updated somewhere else
// ============================================================================
BOOL Win32ServiceMonitor::ShutdownServiceSafe(std::string serviceNameOrFullPath, bool needTOUpdateDaemon)
{
  std::string serviceName;
  ParseFullPathToName(serviceNameOrFullPath.c_str(), serviceName);

  if (needTOUpdateDaemon)
  {
    // Notify the daemon that this process is no longer being monitored
    // (do this first so that the daemon doesn't restart it)
    std::map<std::string, std::string> tmp;
    tmp.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(serviceNameOrFullPath)));
    UpdateDaemon(tmp, "REM");
  }

  // Send a communication to any ServiceMonitor which is monitoring this service
  UpdateServiceMonitor(serviceName, "SHD");

  return TRUE;
}

// ============================================================================
// Name: TerminateService
// Desc: Terminate a service from HANDLE hProcess
// Parm: hProcess - an open handle to the specified process
// ============================================================================
bool Win32ServiceMonitor::TerminateService(HANDLE hProcess)
{
  // This can be pretty bad, and result in catastrophic data loss. Give PLENTY
  // of warning, so that any user watching the server has adequate time to panic.
  LOG(NORMAL_OUTPUT, "WARNING: The monitored process is about to be terminated");
  std::string counter = "..........";
  while (counter.length())
  {
    LOG(NORMAL_OUTPUT, counter);
    counter.resize(counter.length() - 1);
    Sleep(1000);
  }

  if (!TerminateProcess(hProcess, 1))
  {
    LOG("Failed to terminate process %d with error %d", hProcess, GetLastError());
    return false;
  }

  return true;
}

// ============================================================================
// Name: CheckService
// Desc: 
// Parm: 
// ============================================================================
BOOL Win32ServiceMonitor::CheckService(std::string serviceName, std::string serviceFullPath)
{
  //create a tmp map with only one item
  std::map<std::string, std::string> tmp;
  tmp.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(serviceFullPath)));

  UpdateDaemon(tmp, "CHK");

  return TRUE;
}

// ============================================================================
// Name: CheckMonitoredService
// Desc: 
// Parm: 
// ============================================================================
void Win32ServiceMonitor::CheckMonitoredService()
{
  std::string serviceFullPath, serviceName;
  std::ifstream fp("services.txt");

  if (!fp)
  {
    LOG(NORMAL_OUTPUT,"Cannot open the services.txt file. The monitored processes cannot be updated.");
  }

  while (getline(fp, serviceFullPath))
  {
    ParseFullPathToName(serviceFullPath.c_str(), serviceName);
    if (IsStillAlive(serviceName))
    {
      MutexLock(SM);
      if (win32ServicesMap.find(ToLower(serviceName))!= win32ServicesMap.end())
        LOG(NORMAL_OUTPUT, "The service %s is still running and being monitored ...", serviceName.c_str());
      else
        LOG(NORMAL_OUTPUT, "The service %s is still running but NOT being monitored ...", serviceName.c_str());
      MutexUnlock(SM);
    }
    else
    {
     LOG(NORMAL_OUTPUT, "The service %s is NOT running ...", serviceName.c_str());
    }
  }
}



// ============================================================================
// Name: DetachService
// Desc: 
// Parm: 
// ============================================================================
 BOOL Win32ServiceMonitor::DetachService(std::string serviceName, std::string serviceFullPath)
{
  if (IsStillAlive(serviceName))
  {
    std::map<std::string, std::string> tmp;
    tmp.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(serviceFullPath)));
    UpdateDaemon(tmp, "REM");
    UpdateServiceMonitor(serviceName, "DET");
  }

  return TRUE;
}

// ============================================================================
// Name: DetachAll
// Desc: 
// Parm: 
// ============================================================================
 BOOL Win32ServiceMonitor::DetachAll()
{
  //detachAll //send msg from just-started service monitor(with 'detachAll' option) to daemon
  SocketCommunicateWithDaemonServer("DTA");
  return TRUE;
}

 // ============================================================================
// Name: DetachAllServices
// Desc: 
// Parm: 
// ============================================================================
BOOL Win32ServiceMonitor::DetachAllServices()
{
  LOG(NORMAL_OUTPUT, "");

  MutexLock(SM);
  for (std::map<std::string, std::string>::iterator itr = win32ServicesMap.begin(); itr != win32ServicesMap.end(); ++itr)
  {
    LOG(NORMAL_OUTPUT, "Detaching from monitored service '%s'", itr->first.c_str());
    SocketCommunicateWithDaemonServer("DET", DecidePort(itr->first));
  }
  win32ServicesMap.clear();
  MutexUnlock(SM);

  MutexLock(IM);
  win32ServicesIntervalMap.clear();
  MutexUnlock(IM);

  LOG(NORMAL_OUTPUT, "All services detached. Nothing is being monitored.");
  return true;
}

// ============================================================================
// Name: AttachService
// Desc: 
// Parm: 
// ============================================================================

BOOL Win32ServiceMonitor::AttachService(std::string serviceName, std::string fullPath, std::string interval)
{
  if (!IsStillAlive(serviceName))
  {
    LOG(NORMAL_OUTPUT, "Cannot attach, the service '%s' is not active", serviceName.c_str());
    return FALSE;
  }

  //create a tmp map with only one item
  std::map<std::string, std::string> tmp;
  tmp.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(fullPath)));
  float hours = 0.0;

  if (interval != "")
    hours = StringToNumber(interval);

  UpdateDaemon(tmp, "ADD"); // add this process to daemon
  if (hours > 0.0005)
  {
    UpdateServiceMonitor(serviceName, "ATT");
    UpdateDaemon(tmp, hours, "INV");
  }

  return TRUE;
}

// ============================================================================
// Name: RunWin32Daemon
// Desc: Run a daemon process to this tool
// ============================================================================
void Win32ServiceMonitor::RunWin32Daemon()
{
  LOG(NO_PRINT, "");
  LOG(NO_PRINT, "============================================================================");
  CreateThread(DAEMON_THREAD);
  LOG(NO_PRINT, "");

  //EnableDebugPrivilege();
  MonitorAllServices();
}


// ============================================================================
// Name: CreateThread
// Desc: Create a new thread
// Parm: 
// ============================================================================
void Win32ServiceMonitor::CreateThread(ThreadAim whichThread)
{
  //THREADPARAMETER *threadParas = new THREADPARAMETER();
  //THREADPARAMETER threadParas; //= (THREADPARAMETER*)malloc(sizeof(THREADPARAMETER));

  //THREADPARAMETER threadParas;// = new THREADPARAMETER();
  //THREAD_PARAMETER *threadParas = new THREAD_PARAMETER();
  //threadParas.port = DEFAULT_PORT_DAEMON;
  //threadParas.ptrThis = this;

  if (whichThread == DAEMON_THREAD)
    LOG(NORMAL_OUTPUT, "Starting a new daemon process");
  else if (whichThread == LISTEN_THREAD)
    LOG(NORMAL_OUTPUT, "Starting a new listening thread");

  // Create the thread.
  unsigned threadID;
  HANDLE hThread = (HANDLE)_beginthreadex( NULL, 0, &ThreadStaticEntryPoint, this, 0, &threadID );
  hThreadVec.push_back(hThread);

  // Wait until the thread terminates. If you comment out the line   
  // below, Counter will not be correct because the thread has not   
  // terminated, and Counter most likely has not been incremented to   
  // 1000000 yet.   
  //WaitForSingleObject( hThread, INFINITE );  
  //printf( "Counter should be 1000000; it is-> %d", Counter );  

  // Destroy the thread object.   
  //CloseHandle( hThread );  
}

int Win32ServiceMonitor::DecidePort(const std::string& serviceName) const
{
  int port = 0;
  if (_stricmp(serviceName.c_str(), TRAINZUTILHTTP) == 0)
    port = DEFAULT_PORT_TRAINZUTILHTTP;
  else if (_stricmp(serviceName.c_str(), ITRAINZ) == 0)
    port = DEFAULT_PORT_ITRAINZ;
  else if (_stricmp(serviceName.c_str(), PACHACHE) == 0)
    port = DEFAULT_PORT_PACHACHE;
  else if (_stricmp(serviceName.c_str(), SERVLET) == 0)
    port = DEFAULT_PORT_SERVLET;
  else if (_stricmp(serviceName.c_str(), TRAINZDRM) == 0)
    port = DEFAULT_PORT_TRAINZDRM;
  else
  {
    // TODO.Terry: This really shouldn't be hardcoded like this. A set range of
    // possible ports should be defined and one automatically assigned when
    // starting a process. With this setup any new monitored services require
    // an update to the servicemonitor code, which is not good.
    LOG(NORMAL_OUTPUT, "ERROR: Communication port not found for service '%s'", serviceName.c_str());
    __asm int 3;
  }

  //LOG(NORMAL_OUTPUT,"threadParas.port = %d", port);
  return port;
}


// ============================================================================
// Name: MultipleFunsServer
// Desc: Launch a socket connection server to listen on a specified port at 
// the same time it is responsible to update daemon according to the info 
// received
// ============================================================================
void Win32ServiceMonitor::MultipleFunsServer()
{
  //LOG("MultipleFunsServer> m_port = %d", m_port);

  // Initialize Winsock
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != NO_ERROR)
  {
    LOG(NORMAL_OUTPUT, "WSA Server startup failed with error: %d", iResult);
    return;
  }


  SOCKET serverSocket = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP);
  if (serverSocket == INVALID_SOCKET)
  {
    LOG(NORMAL_OUTPUT, "Failed to open server socket with error: %d", WSAGetLastError());
    WSACleanup();
    return;
  }


  // Init connection params and bind socket
  struct sockaddr_in serverAddress;
  memset(&serverAddress, 0, sizeof(sockaddr_in));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
  serverAddress.sin_port = htons(m_port);

  if (bind(serverSocket, (sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR)
  {
    LOG(NORMAL_OUTPUT, "Failed to bind server socket with error %d", WSAGetLastError());
    closesocket(serverSocket);
    WSACleanup();
    return;
  }

  // Listen for incoming connection requests on the created socket 
  if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
  {
    LOG(NORMAL_OUTPUT, "Failed to listen on server socket with error %d", WSAGetLastError());
    closesocket(serverSocket);
    WSACleanup();
    return;
  }

  LOG(NORMAL_OUTPUT, "ServiceMonitor is listening on port %d", m_port);

  // Begin listening until requested to close
  char recvbuf[ACCEPT_BUF];
  while (!g_bHasInterruptRequest && !g_bWantShutdown)
  {
    // Wait here until someone tries to connect
    SOCKET acceptSocket = accept(serverSocket, NULL, NULL);
    if (acceptSocket == INVALID_SOCKET)
    {
      LOG(NORMAL_OUTPUT, "Connection accept failed with error: %d", WSAGetLastError());
      continue;
    }

    int iResult = recv(acceptSocket, recvbuf, ACCEPT_BUF, 0);
    closesocket(acceptSocket);

    if (iResult <= 0)
    {
      LOG("Incoming client connection accepted, but no data received", iResult);
      continue;
    }

    LOG("Incoming client connection accepted, %d bytes received", iResult);

    recvbuf[iResult] = '\0';
    LOG("Received command '%s'", recvbuf);

    if (strcmp(recvbuf, "STP") == 0)
    {
      StopAllServicesFromDaemon();
    }
    else if (strcmp(recvbuf, "LST") == 0)
    {
      ListWin32RunningServices();
    }
    else if (strcmp(recvbuf, "INV") == 0)
    {
      RecordInterval();
    }
    else if (strcmp(recvbuf, "TET") == 0)
    {
      // connection test - this is sent by when a new servicemonitorclient.exe is launched
      LOG("Connection test successful");
    }
    else if (strcmp(recvbuf, "DET") == 0)
    {
      // detach, detaches this ServiceMonitor but doesn't close the process
      g_bWantShutdown = true;
      break;
    }
    else if (strcmp(recvbuf, "DTA") == 0)
    {
      // detachAll, detaches from all monitored services
      DetachAllServices();
    }
    else if (strcmp(recvbuf, "ATT") == 0)
    {
      m_needResetCounting = true;
      m_secondReset = GetIntervalFromFile();
    }
    else if (strcmp(recvbuf, "CHK") == 0)
    {
      CheckMonitoredService();
    }
    else if (strcmp(recvbuf, "SHD") == 0)
    {
      LOG(NORMAL_OUTPUT, "Recieved shutdown request, issuing command to monitored service");
      IssueCtrlBreakAndWaitForShutdown();
    }
    else if (strcmp(recvbuf, "ADD") != 0 || strcmp(recvbuf, "REM") != 0)
    {
      UpdateWin32ServicesMap(recvbuf);
    }
    else
    {
      LOG(NORMAL_OUTPUT, "Received unknown or invalid server command %s", recvbuf);
    }
  }


  closesocket(serverSocket);
  WSACleanup();
}

// ============================================================================
// Name: GetIntervalFromFile
// Desc: 
// ============================================================================
float Win32ServiceMonitor::GetIntervalFromFile()
{
  std::ifstream fp("time_interval.txt");
  if (!fp)
  {
    LOG(NORMAL_OUTPUT, "Failed to open time_interval.txt, time interval not updated");
    return 0.f;
  }

  std::string intervalStr;
  if (!getline(fp, intervalStr))
  {
    LOG(NORMAL_OUTPUT, "Failed to read from time_interval.txt, time interval not updated");
    return 0.f;
  }

  return StringToNumber(intervalStr);
}


// ============================================================================
// Name: MonitorActiveService
// Desc: Monitors a spawned service, keeping this process running until it
//       shuts down or recieves a shutdown request. Also performs scheduled
//       process restarting.
// Parm: serviceName - the name of the service
// Parm: fullpath - the full path of the service
// Parm" interval - OPTIONAL - the interval that service needs to be restarted
// ============================================================================
void Win32ServiceMonitor::MonitorActiveService(std::string serviceName, std::string fullpath, float interval)
{
  time_t intervalSeconds = (time_t)(interval * HOUR_TO_SEC);
  time_t start = time(NULL);

  if (intervalSeconds > 0)
    LOG(NO_PRINT, "Setting initial restart timer on '%s' (%lld)", serviceName.c_str(), intervalSeconds);

  while (!g_bWantShutdown)
  {
    if (g_bHasInterruptRequest)
    {
      LOG(NORMAL_OUTPUT, "Received shutdown request, passing on to monitored service '%s'", serviceName.c_str());
      IssueCtrlBreakAndWaitForShutdown();
      return;
    }

    if (m_needResetCounting)
    {
      LOG(NO_PRINT, "Restarting timer on '%s' after request", serviceName.c_str());
      start = time(NULL);
      m_needResetCounting = false;
    }

    if (intervalSeconds > 0)
    {
      time_t diff = time(NULL) - start;
      if (diff < intervalSeconds)
      {
        if (!IsStillAlive(m_currentProcessID))
        {
          // After the service has been terminated, the servicemonitor.exe should quit.
          printf("Monitored process has been shut down!");
          return;
        }
      }
      else
      {
        LOG(NO_PRINT, "Restart timer expired on '%s', restarting service", serviceName.c_str());

        IssueCtrlBreakAndWaitForShutdown();

        StartNewProcess(fullpath);
        start = time(NULL);
      }
    }
    else
    {
      // if the service is shut down, this listening server will shut down
      if (!IsStillAlive(m_currentProcessID))
      {
        printf("Monitored process has been shut down!");
        return;
      }
    }

    // Sleep for a bit, then check again
    Sleep(1000);

  } // while(true)
}

//=============================================================================
// Name: IssueCtrlBreakAndWaitForShutdown
// Desc: 
//=============================================================================
void Win32ServiceMonitor::IssueCtrlBreakAndWaitForShutdown()
{
  GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0 /*m_currentProcessID*/); //using "0" to be sure that the console and wrapper is shutdown after the service is shut down

  int time = 0;
  while (IsStillAlive(m_currentProcessID))
  {
    // wait for up to 1 min before terminating the process (120 as we sleep for 1/2s)
    if (time > 120)
    {
      LOG(NORMAL_OUTPUT, "Service still hasn't shut down, terminating");
      TerminateService(m_hCurrentProcess);
      CloseHandle(m_hCurrentProcess);
      m_hCurrentProcess = NULL;
      m_currentProcessID = 0;
      return;
    }

    Sleep(500);
    time++;
  }

  LOG(NORMAL_OUTPUT, "Service has safely shut down");
  m_hCurrentProcess = NULL;

  m_currentProcessID = 0;
  ///////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////
}

// ============================================================================
// Name: IsStillALive
// Desc: decide if the service is still alive
// ============================================================================
bool Win32ServiceMonitor::IsStillAlive(DWORD processId) const
{
  EnableDebugPrivilege();
  HANDLE hSnapshot = InitialiseProcessSnapshot();

  PROCESSENTRY32 processInfo;
  processInfo.dwSize = sizeof(PROCESSENTRY32);

  if (!Process32First(hSnapshot, &processInfo))
  {
    // show cause of failure
    LOG(NORMAL_OUTPUT, "IsStillAlive> Process32First failed with error %d", GetLastError());
    CloseHandle(hSnapshot);
    return true;
  }

  do
  {
    if (processInfo.th32ProcessID == processId)
    {
      // Found the process with the desired id, return true
      CloseHandle(hSnapshot);
      return true;
    }

  } while(Process32Next(hSnapshot, &processInfo));

  // Process not found
  CloseHandle(hSnapshot);
  return false;
}


// ============================================================================
// Name: IsStillALive
// Desc: decide if the service is still alive
// ============================================================================
bool Win32ServiceMonitor::IsStillAlive(const std::string& serviceName) const
{
  EnableDebugPrivilege();

  HANDLE hSnapshot = InitialiseProcessSnapshot();

  PROCESSENTRY32 processInfo;
  processInfo.dwSize = sizeof(PROCESSENTRY32);

  if (!Process32First(hSnapshot, &processInfo))
  {
    // show cause of failure
    LOG(NORMAL_OUTPUT, "IsStillAlive> Process32First failed with error %d", GetLastError());
    CloseHandle(hSnapshot);
    return true;
  }

  do
  {
    if (_stricmp(serviceName.c_str(), (const char*)processInfo.szExeFile) == 0)
    {
      // Found a process with the desired name, return true
      CloseHandle(hSnapshot);
      return true;
    }

  } while (Process32Next(hSnapshot, &processInfo));

  // Process not found
  CloseHandle(hSnapshot);
  return false;
}


// ============================================================================
// Name: ListWin32RunningServices
// Desc: List all services under monitoring
// ============================================================================
void Win32ServiceMonitor::ListWin32RunningServices()
{
  MutexLock(SM);

  if (win32ServicesMap.size() == 0)
  {
    LOG(NORMAL_OUTPUT, "No services currently being monitored");
  }
  else
  {
    std::string servicesStr;
    for (std::map<std::string, std::string>::iterator itr = win32ServicesMap.begin(); itr != win32ServicesMap.end(); ++itr)
    {
      servicesStr.append("\n");
      servicesStr.append(itr->second.c_str());
    }

    LOG(NORMAL_OUTPUT, "Currently monitoring: %s", servicesStr.c_str());
  }

  MutexUnlock(SM);
}


// ============================================================================
// Name: UpdateWin32ServicesMap
// Desc: Update the services list under monitoring
// Parm: flag - can be "ADD" - add a service; "REM" - delete a service;
// ============================================================================
void Win32ServiceMonitor::UpdateWin32ServicesMap(const char* flag)
{
  std::ifstream fp("services.txt");
  if (!fp)
  {
    LOG(NORMAL_OUTPUT, "Cannot open the services.txt file. The monitored processes cannot be updated.");
    return;
  }

  std::string serviceFullPath, serviceName;
  while (getline(fp, serviceFullPath))
  {
    ParseFullPathToName(serviceFullPath.c_str(), serviceName);
    //LOG("updateWin32ServicesMap> %s", serviceName);

    MutexLock(SM);

    if (strcmp(flag, "ADD")==0)
    {
      // check if the map has already had this element
      if (win32ServicesMap.find(ToLower(serviceName)) == win32ServicesMap.end())
      {
        LOG(NORMAL_OUTPUT, "Adding new monitored service '%s'", serviceName.c_str());
        win32ServicesMap.insert(std::make_pair<std::string, std::string>(ToLower(serviceName), ToLower(serviceFullPath)));
      }
    }
    else if (strcmp(flag, "REM")==0)
    {
      std::map<std::string,std::string>::iterator itr;
      for (itr = win32ServicesMap.begin(); itr != win32ServicesMap.end(); ++itr)
      {
        if (_stricmp(serviceName.c_str(), itr->first.c_str()) == 0)
        {
          LOG(NORMAL_OUTPUT, "No longer monitoring service '%s'", serviceName.c_str());
          win32ServicesMap.erase(itr);
          break;
        }
      }

      if (itr == win32ServicesMap.end())
        LOG(NORMAL_OUTPUT, "Service '%s' is not being monitored by this daemon", serviceName.c_str());

      MutexLock(IM);
      std::map<std::string, float>::iterator intItr = win32ServicesIntervalMap.find(ToLower(serviceName));
      if (intItr != win32ServicesIntervalMap.end())
        win32ServicesIntervalMap.erase(intItr);
      MutexUnlock(IM);
    }
    else
    {
      LOG(NORMAL_OUTPUT, "ERROR: Unknown command issued to daemon '%s'", flag);
    }

    MutexUnlock(SM);
  }

  fp.close();
}

// ============================================================================
// Name: RecordInterval
// Desc: read 'time_interval.txt' file and obtain the interval and service 
//       name that run with the interval
// ============================================================================
void Win32ServiceMonitor::RecordInterval()
{
  std::ifstream fp("time_interval.txt");
  if (!fp)
  {
    LOG(NORMAL_OUTPUT, "Cannot open the time_interval.txt file, the time interval cannot be updated");
    return;
  }

  std::string intervalStr;
  if (getline(fp, intervalStr))
  {
    //LOG("RecordInterval> intervalStr=%s", intervalStr.c_str());
    float intervalFloat = StringToNumber(intervalStr);
  
    std::string serviceFullPath, serviceName;
    if (getline(fp, serviceFullPath))
    {
      ParseFullPathToName(serviceFullPath.c_str(), serviceName);
      //LOG("RecordInterval> serviceName=%s", serviceName.c_str());

      MutexLock(IM);
      win32ServicesIntervalMap[ToLower(serviceName)] = intervalFloat;
      MutexUnlock(IM);
    }
  }

  fp.close();
}

// ============================================================================
// Name: ParseFullPathToName
// Desc: Parse a string and obtain the service name (.exe)
// Parm: serviceFullPath - the input string
//       serviceName - the output result (what we need)
// ============================================================================
void Win32ServiceMonitor::ParseFullPathToName(const std::string& serviceFullPath, std::string &o_serviceName)
{
  // TODO.Siyang: This appears extremely similiar to ServiceMonitor::parseFullPathToName
  //              And it doesn't seem to be the only such function. Please attempt to 
  //              remove unnecessarily duplicated code when you can

  if (strstr(serviceFullPath.c_str(), ".bat") != NULL)
  {
    //if the input is a .bat run script full path
    std::string tmp;
    std::ifstream fp(serviceFullPath.c_str());
    if (!fp)
      LOG(NORMAL_OUTPUT,"Cannot open the input .bat script file");

    while (getline(fp, tmp))
    {
      if (tmp.find("start")!=std::string::npos && tmp.find(".exe")!=std::string::npos)
      {
        //"start" and ".exe" have to be in the same line
        ParseFullPathToName(tmp.c_str(), o_serviceName);
        break;
      }
    }

    fp.close();

    //remove the last double quote in the service name when obtaining it from .bat file
    size_t pos = o_serviceName.find('\"');
    if (pos != std::string::npos)
      o_serviceName.at(pos) = '\0';
  }
  else if (const char* extensionPos = strstr(serviceFullPath.c_str(), ".exe"))
  {
    const char* start = extensionPos;
    while (*start != '\\' && *start != '//' && start >= serviceFullPath.c_str())
      --start;

    char tmp[MAX_PATH];
    int len = (int)(extensionPos + 4 - start - 1);
    strncpy_s(tmp, MAX_PATH, start + 1, len);
    tmp[len] = '\0';

    o_serviceName = tmp;
  }
  else
  {
    o_serviceName = serviceFullPath;
  }
}


// ============================================================================
// Name: MutexLock
// Desc: Lock the mutex
// ============================================================================
bool Win32ServiceMonitor::MutexLock(MutexType type)
{
  DWORD d;
  //d = WaitForSingleObject(SMMutex, INFINITE);
  if (type == SM)
    d = MsgWaitForMultipleObjects(1,&SMMutex,FALSE,INFINITE,QS_ALLINPUT); //Cannot use "WaitForSingleObject" otherwise get dead lock
  else if (type == IM)
    d = MsgWaitForMultipleObjects(1,&IntervalMutex,FALSE,INFINITE,QS_ALLINPUT);
  
  /*if (d == WAIT_ABANDONED)
  {
    printf("The thread got ownership of an abandoned mute.\n");
    return false;
  }
  else if (d == WAIT_OBJECT_0)
  {
    printf("WAIT_OBJECT_0\n");
    return true;
  }
  else if (d == WAIT_TIMEOUT)
  {
    printf("WAIT_TIMEOUT\n");
    return false;
  }
  else if (d == WAIT_FAILED)
  {
    printf("WAIT_FAILED\n");
    return false;
  }*/

  return true;
}


// ============================================================================
// Name: MutexUnlock()
// Desc: Unlock the mutex
// ============================================================================
void Win32ServiceMonitor::MutexUnlock(MutexType type)
{
  if (type == SM)
  {
    if (!ReleaseMutex(SMMutex)) 
      LOG("The mutex SMMutex is not released properly");
  }  
  else if (type == IM)
  {
    if (!ReleaseMutex(IntervalMutex)) 
      LOG("The mutex IntervalMutex is not released properly");
  }
}


// ============================================================================
// Name: UpdateDaemon
// Desc: Write the updated services into a .txt file and then create a socket 
// connection to the daemon server to send a msg for operation on daemon
// Parm: servicesMap - a std::map structure to store the updated services with 
//       their names and full paths
//       flag - can be "ADD" - add a service; "REM" - delete a service;
// ============================================================================
void Win32ServiceMonitor::UpdateDaemon(std::map<std::string, std::string> servicesMap, char* flag)
{
  if (!WriteServicesMapToFile(servicesMap))
  {
    LOG(NORMAL_OUTPUT, "Cannot write updated services into a file ...");
    return;
  }

  if (!SocketCommunicateWithDaemonServer(flag))
    return;
}

// ============================================================================
// Name: UpdateDaemon
// Desc: Write the time interval to the .txt file and then create a socket 
// connection to the daemon server to send a msg for operation on daemon
// Parm: 
// ============================================================================
void Win32ServiceMonitor::UpdateDaemon(std::map<std::string, std::string> servicesMap, float interval, char* flag)
{
  if (!WriteTimeIntervalToFile(servicesMap, interval))
  {
    LOG(NORMAL_OUTPUT,"Cannot write the interval time into the file ...");
    return;
  }

  if (!SocketCommunicateWithDaemonServer(flag))
    return;
}

// ============================================================================
// Name: UpdateInterval
// Desc: Write the time interval to the .txt file and then create a socket 
// connection to the daemon server to send a msg for operation on daemon
// Parm: 
// ============================================================================
void Win32ServiceMonitor::UpdateInterval(std::map<std::string, std::string> servicesMap, float interval)
{
  if (!WriteTimeIntervalToFile(servicesMap, interval))
  {
    LOG(NORMAL_OUTPUT,"Cannot write the interval time into the file ...");
    return;
  }
}

// ============================================================================
// Name: UpdateServiceMonitor
// Desc: Write the updated services into a .txt file and then create a socket 
// connection to the daemon server to send a msg for operation on daemon
// Parm: servicesMap - a std::map structure to store the updated services with 
//       their names and full paths
//       flag - can be "ADD" - add a service; "REM" - delete a service;
// ============================================================================
void Win32ServiceMonitor::UpdateServiceMonitor(std::string serviceName, char* flag)
{
  int port = DecidePort(serviceName);
  LOG("Updating monitored service %s:%s:%d", flag, serviceName.c_str(), port);

  if (!SocketCommunicateWithDaemonServer(flag, port))
    LOG("Failed to update monitored service %s:%s:%d", flag, serviceName.c_str(), port);
}

// ============================================================================
// Name: WriteServicesMapToFile
// Desc: Write full path of the services into a .txt file
// Parm: servicesMap - a std::map structure to store the updated services with
// ============================================================================
bool Win32ServiceMonitor::WriteServicesMapToFile(std::map<std::string, std::string> servicesMap)
{
  // make sure to clear the file before writing
  std::ofstream fp("services.txt", std::ios_base::trunc);
  std::map<std::string, std::string>::iterator iter;
  if(servicesMap.size() != 1)
  {
	LOG(NORMAL_OUTPUT, "This is WRONG, the size must be 1...");
	return false;
  }

  if (fp.is_open())
  {
    for (iter=servicesMap.begin(); iter!=servicesMap.end(); ++iter)
      fp << iter->second <<std::endl;
  }
  else
  {
    LOG(NORMAL_OUTPUT, "Unable to open the services map file ...");
    fp.close();
    return false;
  }

  fp.close();
  return true;
}

// ============================================================================
// Name: WriteTimeIntervalToFile
// Desc: Write full path of the services into a .txt file
// Parm: servicesMap - a std::map structure to store the updated services with
// ============================================================================
bool Win32ServiceMonitor::WriteTimeIntervalToFile(std::map<std::string, std::string> servicesMap, float interval)
{
  // make sure to clear the file before writing
  std::ofstream fp("time_interval.txt", std::ios_base::trunc);
  std::map<std::string, std::string>::iterator iter;

  if (fp.is_open())
  {
    fp << interval << std::endl;
    iter = servicesMap.begin();
    fp << iter->second << std::endl;
  }
  else
  {
    LOG(NORMAL_OUTPUT, "Unable to open the file ...");
    fp.close();
    return false;
  }

  fp.close();
  return true;
}

// ============================================================================
// Name: SocketCommunicateWithDaemonServer
// Desc: Set up a socket connection to send command msg to the daemon server
// Parm: flag - can be "ADD" - add a service; "REM" - delete a service;
//       "STP" - stop all services currently under monitoring
//       "LST" - list all services currently under monitoring
// ============================================================================
bool Win32ServiceMonitor::SocketCommunicateWithDaemonServer(char* flag, int port)
{
  bool success = false;
  WSADATA wsaData;

  // Initialize Winsock
  int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != NO_ERROR)
  {
    LOG(NORMAL_OUTPUT, "WSA Client Startup failed with error: %d", iResult);
  }
  else
  {
    // Create a socket for connecting to server
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
      LOG(NORMAL_OUTPUT, "Failed to get available socket error: %d", WSAGetLastError());
    }
    else
    {
      // Specify connection params and connect to server
      struct sockaddr_in clientService; 
      clientService.sin_family = AF_INET;
      clientService.sin_addr.s_addr = inet_addr("127.0.0.1");
      clientService.sin_port = htons(port);

      iResult = connect(clientSocket, (SOCKADDR*) &clientService, sizeof(clientService));
      if (iResult == SOCKET_ERROR)
      {
        LOG(NORMAL_OUTPUT, "Failed to connect to ServiceMonitor daemon with error: %d", WSAGetLastError());
      }
      else
      {
        // Send a buffer
        int sendResult = send(clientSocket, flag, (int)strlen(flag), 0);
        if (sendResult == SOCKET_ERROR)
        {
          LOG(NORMAL_OUTPUT, "Failed to send data to ServiceMonitor daemon with error: %d", WSAGetLastError());
        }
        else
        {
          //LOG("Bytes Sent: %d", sendResult);
          success = true;

          // shutdown the connection since no more data will be sent
          if (shutdown(clientSocket, SD_SEND) == SOCKET_ERROR)
            LOG(NORMAL_OUTPUT, "Failed to disconnect from ServiceMonitor daemon with error: %d", WSAGetLastError());
        }
      }

      closesocket(clientSocket);
      clientSocket = NULL;
    }
  }

  WSACleanup();
  return success;
}


// ============================================================================
// Name: EnableDebugPrivilege
// Desc: Set process privilege enabled
// ============================================================================
void Win32ServiceMonitor::EnableDebugPrivilege() const
{
  HANDLE hToken;
  TOKEN_PRIVILEGES tokenPriv;
  LUID luidDebug;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) != FALSE)
  {
    if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luidDebug) != FALSE)
    {
      tokenPriv.PrivilegeCount           = 1;
      tokenPriv.Privileges[0].Luid       = luidDebug;
      tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
      AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(tokenPriv), NULL, NULL);
    }
  }
}

// ============================================================================
// Name: NumberToString
// Desc: 
// ============================================================================
std::string Win32ServiceMonitor::NumberToString(float number) const
{
  std::stringstream convert;
  convert << number;
  return convert.str();
}

// ============================================================================
// Name: StringToNumber
// Desc: convert string to number
// ============================================================================
float Win32ServiceMonitor::StringToNumber(const std::string& str) const
{
  if (str.length() == 0)
    return 0.f;

  return (float)atof(str.c_str());
}

// ============================================================================
// Name: ToLower
// Desc: convert string to number
// ============================================================================
std::string Win32ServiceMonitor::ToLower(const std::string& input) const
{
  std::string tmp;
  for (size_t i = 0; i < input.length(); ++i)
    tmp.append(1, (char)tolower(input.at(i)));

  return tmp;
}
