//Define some default services to be launched when running "server.exe run"
#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <string>

#define SERVICE1	1
#define SERVICE2	1
#define SERVICE3	1

std::string service1_name = "service1.exe";
std::string service2_name = "service2.exe";
std::string service3_name = "service3.exe";

std::string main_path_service1 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\";
std::string main_path_service2 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\";
std::string main_path_service3 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\";

std::string runScript_service1 = "";
std::string runScript_service2 = "";
std::string runScript_service3 = "";

std::string startEXEScript_service1 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\run_service1.bat";
std::string startEXEScript_service2 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\run_service1.bat";
std::string startEXEScript_service3 = "C:\\Users\\cbergmann\\Desktop\\DevelopmentFiles\\monitor_tool\\apps\\run_service1.bat";


#endif