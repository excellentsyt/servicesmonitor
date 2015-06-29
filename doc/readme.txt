Documentation of Server Monitor 1.0

************************
*******Objectives*******
************************
We currently run a number of Win32 executables which provide network support for the game client installations. At the current time, these are started manually and not automatically monitored, which means that a software failure can lead to a 48-hour service outage before being detected and rectified.

We need a tool which monitors and takes certain automated actions. The tool should be kept as simple as possible (probably a Win32 console app written in C/++)  for ease of maintenance as it will be extended with new functionality over time.

************************
******Requirements******
************************
The initial requirements for the tool are as follows:

* The tool is configured via a text file.
* The tool has a list of "Services", executables specified by absolute file path (executable name alone is not sufficient.)
* When in "runAll" mode, the tool monitors the Win32 Process List and starts each Service if it is not running. 
* When in "stopAll" mode, the tool causes each Service to shut down.
* The tool can be configured with a specific command-line for starting each Service.
* The tool can be configured with a specific command-line for stopping each Service. After this command-line completes, the tool should forcibly terminate the Service executable if it is still running.
* The tool can be configured with a specific command-line for testing whether a Service is running correctly. This command-line is only used once the tool has established that the Service executable is actually running. The tool uses the return value from this command-line to determine if the executable is running correctly (0) or has failed (non-zero.) If the executable has failed, it is terminated and restarted.
* Service start/stop actions taken by the tool are logged to the console.
* Services should be rechecked at least once per minute when in "run" mode, but not so often as to cause noticeable processor usage by the tool.
* The tool should assume that any executables, including the Services and the command-lines for starting and stopping the Services, can hang (fail to exit.) Whenever an executable is run, it should be monitored and terminated if a timeout is exceeded.
* The tool should provide an interface to allow a specific service to stopped/started without affecting other services. Once stopped the tool should stop monitoring that service until it is started again via the tool.

* Any win32 API calls should be kept to a separate "platform abstraction" source file. This will assist us if we need to port this tool to other platforms at a later date.

*******************************
*************Usage*************
*******************************
Basically the executable name is "ServiceMonitorClient" and you can run "ServiceMonitorClient help" to check all other available options such as
usage:
"ServiceMonitorClient.exe runAll"
"ServiceMonitorClient.exe stopAll"
"ServiceMonitorClient.exe shutdown <serviceName_or_fullpath>"
"ServiceMonitorClient.exe start <serviceFullPath>.exe <interval_time>(optional)" 
"ServiceMonitorClient.exe check <serviceName_or_fullpath>"
"ServiceMonitorClient.exe list"
"ServiceMonitorClient.exe detach <serviceName_or_fullPath>"
"ServiceMonitorClient.exe detachAll"
"ServiceMonitorClient.exe attach <serviceFullPath> <interval_restart>"
"ServiceMonitorClient.exe daemon"
"ServiceMonitorClient.exe <file_name>.xml"
.

This prgram involves two executables "ServiceMonitorClient.exe" and "ServiceMonitor.exe". The "ServiceMonitorClient.exe" is an interface and the actually performance of the tool is from "ServiceMonitor.exe".

This program has a daemon process which monitors the specified running services. The daemon process will be triggered by either running "server daemon" or by other options such as 'start' and 'runAll'. If the daemon process has been run before running those options, it won't be launched again. If the daemon process has not been activated, it will be launched first.

Option "runAll" will run all those default programs that have not been run at the moment which are pre-setup in the "services.h".

Option "stopAll" will stop all the currently monitored running programs and let daemon process do not monitor them anymore before they are started again.

Option "shutdown" will stop a program and force the daemon process to not monitor it anymore. The program can be either specified by a program name or the program directory. 

Option "daemon" starts the daemon process solely. The daemon process will be run automatically if either 'start' or 'runAll' or 'attach' option is used and daemon is not running. If daemon is not running and either 'stopAll' or 'shutdown' or 'check' or 'list' or 'detachAll' option is used, it will return 1.

Option "start" will start a program and make the daemon process to monitor its performance. The program has to be specified by its full directory. 

Option "check" will test a program performance. If it is running correctly and being monitored, it will return a "correctly running and being monitored" message; if it is running correctly but without being monitored, it will return a "correctly running but NOT being monitored"; otherwise, if it is not running at all, it will return something like "Not running at all". In addtion, if the 'check' option is run in case that the checked service is not actually running, it will return 2. 

Option "list" does not have any followers. It displays currently running programs under monitoring.

Option "detach" stops the monitoring from the service as well as the potential restarting action if there is any at that moment. After that, the service will keep running but alone.

Option "detachAll" does the 'detach' action to all the services under monitoring.

Option "attach" recovers the monitoring to the service as well as the restarting action whose interval time must be specified by you via the fourth commandline argument.

Option "<file_name>.xml" will load up an xml file and perform as configured in the file.

*******************************
******what it does not do******
*******************************
1. At the moment this "server monitor" tool can only be run on Windows platform though it could optionally be expanded to run on a linux or mac machine in the future.

2. The programs which will be monitored by this "server monitor" tool have to have at least one window if you want to detect the "hang" status when necessary. This is because the "hang" status checking is implemented by sending message to a program window via SendMessageTimeOut() function. If they do not have any window, the tool cannot correctly response to the "hang" status of any monitored program (it will return wrong information) and therefore this function should be diabled to avoid wrong information.

**************************************
**********external resources**********
**************************************
This tool uses a 3rd party C++ XML parser that can be easily integrated into this tool.