#ifndef _WIN32SERVICEMONITOR_H
#define _WIN32SERVICEMONITOR_H

#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <vector>
#include <map>
#include <string>
#include <tlhelp32.h>
#include <tchar.h>
#include <string.h>
#include "logToFile.h"

#define ACCEPT_BUF                    8
#define DEFAULT_PORT_DAEMON           8088
#define DEFAULT_PORT_ITRAINZ          8000
#define DEFAULT_PORT_PACHACHE         8001
#define DEFAULT_PORT_SERVLET          8002
#define DEFAULT_PORT_TRAINZDRM        8003
#define DEFAULT_PORT_TRAINZUTILHTTP   8004

#define ITRAINZ                       "OAServer.exe"
#define TRAINZUTILHTTP                "TrainzUtilHTTP.exe"
#define PACHACHE                      "planetaurancache.exe"
#define SERVLET                       "TrainzScriptRuntime.exe"
#define TRAINZDRM                     "trainzdrm.exe"

/************************************************************************/
/* Start the Win32ServiceMonitor class  */
/************************************************************************/
class Win32ServiceMonitor
{
  std::string   m_startupDir;

  std::map<std::string, std::string>  win32ServicesMap;           // a map structure to include both the names and directories of services
  std::map<std::string, float>        win32ServicesIntervalMap;   // a map structure to include both the names and interval time

  HANDLE  m_hCurrentProcess;        // currently monitored process HANDLE
  DWORD   m_currentProcessID;       // currently monitored process id
  std::vector<HANDLE> hThreadVec;   // vector of any and all threads we've started

  enum MutexType
  {
    SM,         // for win32ServicesMap
    IM,         // for win32ServicesIntervalMap
  };
  HANDLE SMMutex;         // for MutexType::SM and win32ServicesMap
  HANDLE IntervalMutex;   // for MutexType::IM and win32ServicesIntervalMap

  //create process id and a window handle 
  DWORD gPidToFind;
  HWND  gTargetWindowHwnd;

  int   m_port;
  bool  m_needResetCounting;
  float m_secondReset;

  enum ThreadAim
  {
    DAEMON_THREAD,
    INTERVAL_COUNT_THREAD,
    LISTEN_THREAD
  };



public:
  Win32ServiceMonitor(const std::string& startupDir);
  ~Win32ServiceMonitor();

  // ==========================================================================
  // Starts all specified services
  void RunWin32Daemon();
  //void run(std::map<std::string, std::string> &servicesMap);
  // ==========================================================================

  // ==========================================================================
  // Terminates all specified running services. If "stopServicesInConfigFile" is true, just shutdown the specified services instead of all of them
  void Stop(std::map<std::string, std::string> &servicesMap, bool stopServicesInConfigFile);
  // ==========================================================================

  // ==========================================================================
  // List currently monitored processes
  void List(); //list() displays the running services under monitoring
  // ==========================================================================

  // ==========================================================================
  // Shut down the the specified service
  BOOL ShutdownServiceSafe(std::string serviceNameOrFullPath, bool needTOUpdateDaemon=true);
  // ==========================================================================

  // ==========================================================================
  // Start the specified service
  BOOL StartService(std::string serviceName, std::string fullpath, float interval=0.0);
  BOOL StartCrashedService(std::string serviceName, std::string fullpath);
  // ==========================================================================

  // ==========================================================================
  // Check if hang status of the specified service
  BOOL CheckService(std::string serviceName, std::string fullpath);
  // ==========================================================================

  // ==========================================================================
  // detach a specified service from being monitored and cycling perioed etc
  BOOL DetachService(std::string serviceName, std::string serviceFullPath);
  // ==========================================================================

  // ==========================================================================
  // detach a specified service from being monitored and cycling perioed etc
  BOOL AttachService(std::string serviceName, std::string fullPath, std::string interval="");
  // ==========================================================================

  // ==========================================================================
  // detach a specified service from being monitored and cycling perioed etc
  BOOL DetachAll();
  // ==========================================================================

private:
  HANDLE InitialiseProcessSnapshot() const;
  void ClearMemory(STARTUPINFO *si, PROCESS_INFORMATION *pi);
  bool StartNewProcess(STARTUPINFO *io_si, PROCESS_INFORMATION *io_pi, const std::string& fullpath);
  bool StartNewProcess(const std::string& path);
  std::string CreateCommand(std::string serviceName, const char* fullpath);
  BOOL GetWin32RunningServices(std::vector<std::string>& io_runningServices);
  void MonitorAllServices();
  //void MonitorHang();
  bool TerminateService(HANDLE hProcess);
  void StopAllServicesFromDaemon();
  bool AlreadyInWin32ServiceMap(std::string serviceName);
  void AllocateNewConsole();
  void MultipleFunsServer();
  void CreateThread(ThreadAim whichThread);
  void UpdateWin32ServicesMap(const char* flag);
  void RecordInterval();
  void ParseFullPathToName(const std::string& serviceFullPath, std::string &o_serviceName);
  bool MutexLock(MutexType type);
  void MutexUnlock(MutexType type);
  void UpdateDaemon(std::map<std::string, std::string> servicesMap, char* flag);
  void UpdateDaemon(std::map<std::string, std::string> servicesMap, float interval, char* flag);
  void UpdateInterval(std::map<std::string, std::string> servicesMap, float interval);
  void UpdateServiceMonitor(std::string serviceName, char* flag/*, float interval=0.0*/);
  bool WriteServicesMapToFile(std::map<std::string, std::string> servicesMap);
  bool WriteTimeIntervalToFile(std::map<std::string, std::string> servicesMap, float interval);
  bool SocketCommunicateWithDaemonServer(char* flag, int port=DEFAULT_PORT_DAEMON);
  void EnableDebugPrivilege() const;
  void ListWin32RunningServices();
  std::string NumberToString(float number) const;
  float StringToNumber(const std::string& str) const;
  void MonitorActiveService(std::string serviceName, std::string fullpath, float interval = -1.f);
  void IssueCtrlBreakAndWaitForShutdown();
  bool IsStillAlive(DWORD processId) const;
  bool IsStillAlive(const std::string& serviceName) const;
  std::string ToLower(const std::string& input) const;
  int DecidePort(const std::string& input) const;
  BOOL DetachAllServices();
  float GetIntervalFromFile();
  void CheckMonitoredService();

public:

  // ============================================================================
  // Name: ThreadStaticEntryPoint()
  // Desc: This is the static entry point for daemon thread.
  //       In C++ you must employ a free (C) function or a static
  //       class member function as the thread entry-point-function.
  //       Furthermore, _beginthreadex() demands that the thread
  //       entry function signature take a single (void*) and returned
  //       an unsigned.
  // Parm: pThis - this pointer
  // ============================================================================
  static unsigned __stdcall ThreadStaticEntryPoint(void* pThis)
  {
    Win32ServiceMonitor* pthX = reinterpret_cast<Win32ServiceMonitor*>(pThis);

    pthX->MultipleFunsServer();

    return 0;
  }

  /*
  // ============================================================================
  // Name: ThreadStaticEntryPoint_IntervalCount()
  // Desc: This is the static entry point for any interval counting thread.
  // Parm: pThis - this pointer
  // ============================================================================
  static unsigned __stdcall ThreadStaticEntryPoint_IntervalCount(void * pThis)
  {
    //printf("111111111111111111111\n");
    Win32ServiceMonitor * pthX = (Win32ServiceMonitor*)pThis;   // the tricky cast
    pthX->StartCountingInterval();           // now call the true entry-point-function

    // A thread terminates automatically if it completes execution,
    // or it can terminate itself with a call to _endthread().
    return 1;          // the thread exit code
  }
  */

  // ============================================================================
  // Name: myWNDENUMPROC()
  // Desc: Get handle of application window.
  // Parm: hwCurHwnd - window handle
  //       pThis - this pointer
  // ============================================================================
  static BOOL CALLBACK myWNDENUMPROC(HWND hwCurHwnd, LPARAM pThis)
  {
    Win32ServiceMonitor * pthX = (Win32ServiceMonitor*)pThis;   // the tricky cast

    DWORD dwCurPid = 0;

    GetWindowThreadProcessId(hwCurHwnd, &dwCurPid);

    if(dwCurPid == pthX->gPidToFind)
    {
      pthX->gTargetWindowHwnd = hwCurHwnd;
      return FALSE;
    }

    return TRUE;
  }
};

#endif