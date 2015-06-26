#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <iostream>

#define COMMAND_LEN 512
#define SLEEP_TIME 5000
#ifdef _DEBUG
# define SERVICE_MONITOR "ServiceMonitor_d.exe"
#else
# define SERVICE_MONITOR "ServiceMonitor.exe"
#endif

#define DEFAULT_PORT_DAEMON			 8088

#pragma   comment(lib,   "ws2_32.lib")

// ==========================================================================
// Get the Log Path from argv[0]
void GetLogPath(char* binaryPath);
// The input file is .xml?
bool isXML(char* inputFile);
// The input file is .bat?
bool isBAT(char* inputFile);
// The input file is numeric?
bool isNum(char* inputFile);
// The input char is '.'?
bool isDot(char inputChar);
// ==========================================================================

// ==========================================================================
// Run the command in a new process
int runProcess(const char* command, bool bNewConsole = true);
// ==========================================================================

// ==========================================================================
int runDaemon();
int run();
int stop();
int list();
int start(char* service);
int start(char* service, char* paramArray[], int size);
int shutDown(char* service);
int check(char* service);
int detach(char* service);
int attach(char* service, char* interval="");
int detachAll();
int runFromConfig(char* configFile);
// ==========================================================================

// ==========================================================================
// Promote the authentification
void EnableDebugPrivilege();
// ==========================================================================

// ==========================================================================
// Process is running or not
bool IsAlreadyRunning(const std::string& serviceFullPath);
// Daemon is running or not
bool IsAlreadyRunningDaemon();
bool socketCommunicateWithDaemonServer(char* flag, int port);
// Print error info
void printUsageError();
// Parse the full path to service name
bool parseFullPathToName(const char* serviceFullPath, char* serviceName);
// ==========================================================================

//define a global variable for the output directory of the log file
std::string g_Log;



//=============================================================================
// Name: main
// Desc: 
//=============================================================================
int main(int argc, char** argv)
{
  GetLogPath(argv[0]);

  if (argc < 2)
  {
    printUsageError();
    return -1;
  }

  if (!IsAlreadyRunningDaemon() && (strcmp(argv[1], "start")==0 || strcmp(argv[1], "runAll")==0 || strcmp(argv[1], "daemon")==0 || strcmp(argv[1], "attach")==0))
  {
    printf("The daemon process is not active, starting it now");
    runDaemon();
    Sleep(SLEEP_TIME);
  }

  if (strcmp(argv[1], "runAll") == 0)
  {
    run();
  }
  else if (strcmp(argv[1], "stopAll") == 0)
  {
    if (!IsAlreadyRunningDaemon())
      return 1;

    stop();
  }
  else if (strcmp(argv[1], "list") == 0)
  {
    if (!IsAlreadyRunningDaemon())
      return 1;

    list();
  }
  else if (strcmp(argv[1], "start") == 0)
  {
    if (argc >= 4)
    {
      if (IsAlreadyRunning(argv[2]))
        return 2;

      char** paramArray = new char*[argc-3];
      for (int i=0; i < argc-3; i++)
        paramArray[i] = argv[i+3];

      start(argv[2], paramArray, argc-3);
      delete[] paramArray;
    }
    else if (argc == 3)
    {
      if (IsAlreadyRunning(argv[2]))
        return 2;

      start(argv[2]);
    }
  }
  else if (strcmp(argv[1], "shutdown") == 0)
  {
    if (!IsAlreadyRunningDaemon())
      return 1;

    shutDown(argv[2]);
  }
  else if (strcmp(argv[1], "monitor") == 0)
  {
   // TODO
  }
  else if (strcmp(argv[1], "check") == 0)
  {
    if (!IsAlreadyRunning(argv[2]))
    {
      printf("The service %s is NOT running", argv[2]);
      return 2;
    }

    check(argv[2]);
  }
  else if (strcmp(argv[1], "detach") == 0)
  {
    if (!IsAlreadyRunningDaemon())
      return 1;

    if (!IsAlreadyRunning(argv[2]))
      return 2;

    detach(argv[2]);
  }
  else if (strcmp(argv[1], "detachAll") == 0)
  {
    if (!IsAlreadyRunningDaemon())
      return 1;

    detachAll();
  }
  else if (strcmp(argv[1], "attach") == 0)
  {
    if (!IsAlreadyRunning(argv[2]))
      return 2;

    if (argc == 4)
      attach(argv[2], argv[3]);
    else if (argc == 3)
      attach(argv[2]);
  }
  else if (isXML(argv[1]))
  {
    runFromConfig(argv[1]);
  }
  else
  {
    // Unknown/invalid command
    printUsageError();
  }

  return 0;
}

void GetLogPath(char* binaryPath)
{
  int lenArgv = strlen(binaryPath);
  //int lenFileName = strlen("SMLog.txt");
  char* logPath = new char[lenArgv + 20];
  strcpy(logPath, binaryPath);
  char* p = logPath;
  char* q = p;
  p = strchr(q, '\\');
  while(p != NULL)
  {
	q = p + 1;;
	p = strchr(q, '\\');
  }
  *q = '\0';
  //strcat(logPath,"SMLog.txt");
  g_Log = logPath;
  //std::cout << "LogFile location 1 is: " << g_Log << std::endl;
  delete logPath;
}

bool isXML(char* inputFile)
{
  if(strstr(inputFile, ".xml") != NULL)
	return true;
  else
	return false;
}

bool isBAT(char* inputFile)
{
  if(strstr(inputFile, ".bat") != NULL)
	return true;
  else
	return false;
}

bool isNum(char* inputFile)
{
  int len = strlen(inputFile);
  while(len > 0)
  {
	if(!isdigit(inputFile[len-1]) && !isDot(inputFile[len-1]))
	  return false;
	else if (!isdigit(inputFile[len-1]) && isDot(inputFile[len-1]) && len != 2)
	  return false;
	len--;
  }
  return true;
}

bool isDot(char inputChar)
{
  if(inputChar == '.')
	return true;
  else
	return false;
}

void printUsageError(){
  std::cout << "usage:" << std::endl
	<< "| ServiceMonitorClient.exe runAll" << std::endl
	<< "| ServiceMonitorClient.exe stopAll" << std::endl
	<< "| ServiceMonitorClient.exe shutdown <serviceName_or_fullpath>" << std::endl
	<< "| ServiceMonitorClient.exe start <serviceFullPath>.exe <interval_time>(optional)" << std::endl
	//<< "| ServiceMonitorClient.exe start <runScript>.bat <serviceFullpath>.exe | <startEXEScript>.bat" << std::endl
	//<< "| ServiceMonitorClient.exe monitor <serviceName_or_fullpath> (NOT USED YET)" << std::endl
	<< "| ServiceMonitorClient.exe check <serviceName_or_fullpath>" << std::endl
	<< "| ServiceMonitorClient.exe list" << std::endl
	<< "| ServiceMonitorClient.exe detach <serviceName_or_fullPath>" << std::endl
	<< "| ServiceMonitorClient.exe detachAll" << std::endl
	<< "| ServiceMonitorClient.exe attach <serviceFullPath> <interval_restart>" << std::endl
	<< "| ServiceMonitorClient.exe daemon" << std::endl
	<< "| ServiceMonitorClient.exe <file_name>.xml" << std::endl;
}

int runProcess(const char* command, bool bNewConsole)
{
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  ZeroMemory(&pi, sizeof(pi));

  DWORD creationFlags = bNewConsole ? CREATE_NEW_CONSOLE : 0;

  // Start the child process. 
  if (!CreateProcess( NULL,               // No module name (use command line)
                      (LPSTR)command,     // Command line
                      NULL,               // Process handle not inheritable
                      NULL,               // Thread handle not inheritable
                      FALSE,              // Set handle inheritance to FALSE
                      creationFlags,      // No creation flags
                      NULL,               // Use parent's environment block
                      NULL,               // Use parent's starting directory 
                      &si,                // Pointer to STARTUPINFO structure
                      &pi))               // Pointer to PROCESS_INFORMATION structure
  {
    printf("CreateProcess failed (%d).\n", GetLastError());
    return false;
  }

  return true;
}

int runDaemon(){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " daemon");
  int ret = runProcess(command);
  return ret;
}

int run(){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " runAll");
  int ret = runProcess(command);
  //int ret = runProcess(SERVICE_MONITOR " run");
  return ret;
}
int stop(){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " stopAll");
  int ret = runProcess(command, false);
  //int ret = runProcess(SERVICE_MONITOR " stop");
  return ret;
}

int list(){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " list");
  int ret = runProcess(command, false);
  //int ret = runProcess(SERVICE_MONITOR " list");
  return ret;
}

int start(char* service)
{
  return start(service, NULL, 0);
}

int start(char* service, char* paramArray[], int size)
{
  //printf("Service is %s\n", service); 

  std::string commandLine;
  commandLine.append(g_Log.c_str());
  commandLine.append(SERVICE_MONITOR " start ");

  commandLine.append("\"");
  commandLine.append(service);
  commandLine.append("\" ");
  for (int i = 0; i < size; i++)
  {
    commandLine.append(" ");
    commandLine.append(paramArray[i]);
  }


  return runProcess(commandLine.c_str());
}

int shutDown(char* service){
  char* command = new char[COMMAND_LEN];
  //strcpy(command, SERVICE_MONITOR " shutdown ");
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " shutdown ");

  strcat(command, "\"");
  strcat(command, service);
  strcat(command, "\"");
  int ret = runProcess(command, false);
  delete command;
  return ret;
}
int check(char* service){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " check ");
  //strcpy(command, SERVICE_MONITOR " check ");

  strcat(command, "\"");
  strcat(command, service);
  strcat(command, "\"");
  int ret = runProcess(command, false);
  delete command;
  return ret;
}

int detach(char* service){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " detach ");

  strcat(command, "\"");
  strcat(command, service);
  strcat(command, "\"");
  int ret = runProcess(command, false);
  delete command;
  return ret;
}

int attach(char* service, char* interval){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " attach ");

  strcat(command, "\"");
  strcat(command, service);
  strcat(command, "\"");
  strcat(command, " ");
  strcat(command, interval);
  int ret = runProcess(command, false);
  delete command;
  return ret;
}

int detachAll()
{
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " detachAll");
  int ret = runProcess(command, false);
  //int ret = runProcess(SERVICE_MONITOR " stop");
  return ret;
}

int runFromConfig(char* configFile){
  char* command = new char[COMMAND_LEN];
  strcpy(command, g_Log.c_str());
  strcat(command, SERVICE_MONITOR " ");
  //strcpy(command, SERVICE_MONITOR " ");

  strcat(command, configFile);
  int ret = runProcess(command, false);
  delete command;
  return ret;
}

bool IsAlreadyRunning(const std::string& serviceFullPath)
{
  char serviceName[256];
  if (!parseFullPathToName(serviceFullPath.c_str(), serviceName))
    return false;

  EnableDebugPrivilege();

  // Take a snapshot of all processes in the system.
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
  {
    printf("IsAlreadyRunning> ERROR: Failed to capture process snapshot");
    return false;
  }

  PROCESSENTRY32 processInfo;
  processInfo.dwSize = sizeof(PROCESSENTRY32);

  // Retrieve the first process info
  if (!Process32First(hSnapshot, &processInfo))
  {
    printf("IsAlreadyRunning> ERROR: Failed to query first process");
    CloseHandle(hSnapshot);
    return false;
  }

  // Walk over each returned process, searching for the one we want
  do
  {
    if (stricmp(processInfo.szExeFile, serviceName) == 0)
    {
      // Found the process, return true
      CloseHandle(hSnapshot);
      return true;
    }

  } while (Process32Next(hSnapshot, &processInfo));

  // Process not found, return false
  return false;
}

bool IsAlreadyRunningDaemon()
{
  return socketCommunicateWithDaemonServer("TET", DEFAULT_PORT_DAEMON);
}

// ============================================================================
// Name: socketCommunicateWithDaemonServer()
// Desc: Set up a socket connection to send command msg to the daemon server
// Parm: flag - can be "ADD" - add a service; "REM" - delete a service;
//       "STP" - stop all services currently under monitoring
//       "LST" - list all services currently under monitoring
// ============================================================================
bool socketCommunicateWithDaemonServer(char* flag, int port)
{
  // Initialize Winsock
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
  if (iResult != NO_ERROR)
  {
    printf("WSA Client Startup failed with error: %d\n", iResult);
    return false;
  }

  // Create a SOCKET for connecting to server
  SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (clientSocket == INVALID_SOCKET)
  {
    printf("socket failed with error: %ld\n", WSAGetLastError());
    WSACleanup();
    return false;
  }

  // The sockaddr_in structure specifies the address family,
  // IP address, and port of the server to be connected to.
  struct sockaddr_in clientService; 
  clientService.sin_family = AF_INET;
  clientService.sin_addr.s_addr = inet_addr( "127.0.0.1" );
  clientService.sin_port = htons(port);

  // Connect to server.
  iResult = connect(clientSocket, (SOCKADDR*) &clientService, sizeof(clientService));
  if (iResult == SOCKET_ERROR)
  {
    //printf("connect failed with error: %d\n", WSAGetLastError() );
    closesocket(clientSocket);
    WSACleanup();
    return false;
  }

  // Send a buffer
  iResult = send(clientSocket, flag, (int)strlen(flag), 0);
  if (iResult == SOCKET_ERROR)
  {
    printf("send failed with error: %d\n", WSAGetLastError());
    closesocket(clientSocket);
    WSACleanup();
    return false;
  }

  // shutdown the connection since no more data will be sent
  iResult = shutdown(clientSocket, SD_SEND);
  if (iResult == SOCKET_ERROR)
  {
    printf("shutdown failed with error: %d\n", WSAGetLastError());
    closesocket(clientSocket);
    WSACleanup();
    return false;
  }

  return true;
}

void EnableDebugPrivilege()
{
  HANDLE hToken;
  TOKEN_PRIVILEGES tokenPriv;
  LUID luidDebug;
  if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken) != FALSE)
  {
	if(LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luidDebug) != FALSE)
	{
	  tokenPriv.PrivilegeCount           = 1;
	  tokenPriv.Privileges[0].Luid       = luidDebug;
	  tokenPriv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	  AdjustTokenPrivileges(hToken, FALSE, &tokenPriv, sizeof(tokenPriv), NULL, NULL);
	}
  }
}

// ============================================================================
// Name: parseFullPathToName()
// Desc: Parse a string and obtain the service name (.exe)
// Parm: serviceFullPath - the input string
//       serviceName - the output result (what we need)
// ============================================================================
bool parseFullPathToName(const char* serviceFullPath, char* serviceName)
{
	if (strstr(serviceFullPath,".exe") != NULL) //if the input is a .exe full path
	{ 
		char tmp[256];
		strcpy(tmp, serviceFullPath);
		char* p = tmp;
		char* q = p;
		p = strchr(q,'\\');
		while(p != NULL)
		{
			q = p+1;
			p = strchr(q,'\\');
		}
		strcpy(serviceName, q);
	}
	else
	{
		printf("the service is not a .exe file\n");
		return false;
	}
	//remove the last double quote in the service name when obtaining it from .bat file
	char *p;
	if((p=strchr(serviceName,'\"')) != NULL)
		*p='\0';

	return true;
}