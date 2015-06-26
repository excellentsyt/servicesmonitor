#ifndef _SERVICEMONITOR_H
#define _SERVICEMONITOR_H

#define _WIN32_

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <string>

#include "logToFile.h"
#include "tinyxml.h"

#ifdef _WIN32_
  #include "win32ServiceMonitor.h"
#endif


//=============================================================================
// Name: ServiceMonitor
// Desc: 
//=============================================================================
class ServiceMonitor
{
public:
  ServiceMonitor(const std::string& startupDir)
    : m_win32SM(startupDir)
  {
    CheckRequiredServices();
  }


  // ==========================================================================
  // Run Daemon process
  void RunDaemon();
  // ==========================================================================

  // ==========================================================================
  // Run all specified services
  void Run();
  // ==========================================================================

  // ==========================================================================
  // Stop all specified services
  void Stop(bool servicesMapReset = false);
  // ==========================================================================

  // ==========================================================================
  // List currently monitored services
  void List();
  // ==========================================================================

  // ==========================================================================
  // Shut down a service
  void ShutDown(std::string serviceName);
  // ==========================================================================

  // ==========================================================================
  // Start a service
  void Start(std::string serviceFullPath);
  void Start(std::string serviceFullPath, float hours);
  // ==========================================================================

  // ==========================================================================
  // Check a service status if hang, terminate and restart it
  void Check(std::string serviceFullPath);
  // ==========================================================================

   // ==========================================================================
  void Detach(std::string serviceFullPath);
  void DetachAll();
  // ==========================================================================

  // ==========================================================================
  void Attach(std::string fullPath, std::string interval = "");
  // ==========================================================================

  // ==========================================================================
  void testopenfile();
  // ==========================================================================

  // ==========================================================================
  // Perform as what the config file sets
  bool ReadConfig(const std::string& configFile);
  // ==========================================================================
	
private:
  // ==========================================================================
  // Check what are those defaultly starting services
  void CheckRequiredServices();
  // ==========================================================================

  // ==========================================================================
  // Parse the input config file
  bool ParseConfig(const std::string& configFile);
  // ==========================================================================

  // ==========================================================================
  // Reset servicesMap to get the customized configuration 
  void ResetConfig(); 
  // ==========================================================================

  // ==========================================================================
  // Parse a full path input (location and service name all together) into location and service name
  void ParseFullPathToName(const std::string& serviceFullPath,  std::string &o_serviceName);
  // ==========================================================================


#ifdef _WIN32_
  Win32ServiceMonitor m_win32SM;
#endif

  struct ServiceItem
  {
    std::string serviceName;        // service name
    std::string location;          // fullpath
    std::string runScript;         // the run script
    std::string startEXEScript;    // the start script
  };

  //std::map<std::string, std::string> servicesMap; //define a map structure to include both the names and directories of services

  //define a internal class to contain info from configuration file
  struct Config
  {
    std::string command;
    std::string option;
    std::vector<ServiceItem> attrVec;
  };

  Config m_cfg;
};

#endif