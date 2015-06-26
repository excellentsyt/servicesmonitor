#include "serviceMonitor.h"
#include "services.h"
#include <iostream>
#include <process.h>
#include <fstream>

using namespace std;


// ============================================================================
// Name: ParseConfig()
// Desc: Parse the xml config file
// Parm: configFile - the actual configuration file
// ============================================================================
bool ServiceMonitor::ParseConfig(const std::string& configFile)
{
  TiXmlDocument doc(configFile);
  bool loadOkay = doc.LoadFile();
  if (!loadOkay)
  {
    LOG("Could not load config file '<config_<fun>.xml>'. Error='%s'. Exiting.", doc.ErrorDesc());
    return false;
  }

  TiXmlNode* node = 0;
  TiXmlElement* optionElement = 0;
  TiXmlElement* commandlineElement = 0;
  TiXmlElement* serviceElement = 0;

  // Get the "Option" element.
  // It is a child of the document, and can be selected by name.
  node = doc.FirstChild("option");
  assert(node);
  optionElement = node->ToElement();
  assert(optionElement);


  node = optionElement->FirstChildElement();
  assert(node);

  commandlineElement = node->ToElement();
  assert(commandlineElement);


  if (commandlineElement->QueryStringAttribute("name", &m_cfg.command) != TIXML_SUCCESS)
    cout << "Check commandline->name in the xml file." << endl;
  if (commandlineElement->QueryStringAttribute("option", &m_cfg.option) != TIXML_SUCCESS)
    cout << "Check commandline->option in the xml file." << endl;


  node = commandlineElement->FirstChildElement();
  assert(node);

  serviceElement = node->ToElement();
  assert(serviceElement);


  TiXmlElement* service_property_element = 0;
  while (serviceElement != NULL)
  {
    ServiceItem item;

    service_property_element = serviceElement->FirstChildElement();
    while (service_property_element != NULL)
    {
      std::string attribute;
      service_property_element->QueryStringAttribute("name", &attribute);

      if (attribute.compare("serviceName") == 0)
      {
        item.serviceName = service_property_element->GetText();
      }
      else if (attribute.compare("location") == 0)
      {
        string serviceLocation = service_property_element->GetText();
        item.location = serviceLocation + item.serviceName;
      }
      else if (attribute.compare("runScript") == 0)
      {
        item.runScript = service_property_element->GetText();
      }
      else if (attribute.compare("startEXEScript") == 0)
      {
        item.startEXEScript = service_property_element->GetText();
      }

      service_property_element = service_property_element->NextSiblingElement();
    }

    m_cfg.attrVec.push_back(item);
    serviceElement = serviceElement->NextSiblingElement();
  }

  return true;
}


// ============================================================================
// Name: checkRequiredServices
// Desc: Initialise the service names and paths (this is the default ones)
// ============================================================================
void ServiceMonitor::CheckRequiredServices()
{
  std::string service_fullpath;
  if (SERVICE1)
  {
    service_fullpath = main_path_service1 + service1_name;
    ServiceItem item;
    item.serviceName = service1_name;
    item.location = service_fullpath;
    item.runScript = runScript_service1;
    item.startEXEScript = startEXEScript_service1;
    m_cfg.attrVec.push_back(item);
  }
  if (SERVICE2)
  {
    service_fullpath = main_path_service2 + service2_name;
    ServiceItem item;
    item.serviceName = service2_name;
    item.location = service_fullpath;
    item.runScript = runScript_service2;
    item.startEXEScript = startEXEScript_service2;
    m_cfg.attrVec.push_back(item);
  }
  if (SERVICE3)
  {
    service_fullpath = main_path_service3 + service3_name;
    ServiceItem item;
    item.serviceName = service3_name;
    item.location = service_fullpath;
    item.runScript = runScript_service3;
    item.startEXEScript = startEXEScript_service3;
    m_cfg.attrVec.push_back(item);
  }
}


// ============================================================================
// Name: RunDaemon
// Desc: Run the tool daemon process
// ============================================================================
void ServiceMonitor::RunDaemon()
{
#ifdef _WIN32_
  m_win32SM.RunWin32Daemon();
#endif
}

void ServiceMonitor::Run()
{
#ifdef _WIN32_
  std::vector<ServiceItem>::iterator iter;
  for (iter = m_cfg.attrVec.begin(); iter != m_cfg.attrVec.end(); ++iter)
  {
    if (!iter->runScript.empty())
    {
      //TODO: this has been changed
      /*if(!iter->startEXEScript.empty())
      {
      start(iter->runScript,iter->startEXEScript);	
      }
      else if (!iter->location.empty())
      {
      start(iter->runScript,iter->location);	
      }*/
    }
    else if (!iter->startEXEScript.empty())
    {
      Start(iter->startEXEScript);
    }
    else if (!iter->location.empty())
    {
      Start(iter->location); //location is servicefullpath
    }
    else
    {
      LOG(NORMAL_OUTPUT, "The config file was not set correctly.");
    }
  }
#endif
}


// ============================================================================
// Name: Stop
// Desc: Stop all specified services
// Parm: servicesMapReset - decide if resetting the servicesMap. if true which 
// means using config file instead of default, if false which means using 
// default.
// ============================================================================
void ServiceMonitor::Stop(bool servicesMapReset)
{
#ifdef _WIN32_
  std::map<std::string, std::string> servicesMap; //define a map structure to include both the names and directories of services
  std::vector<ServiceItem>::iterator iter;
  for (iter = m_cfg.attrVec.begin(); iter != m_cfg.attrVec.end(); ++iter)
  {
    servicesMap.insert(std::make_pair<std::string,std::string>(iter->serviceName, iter->location));
  }
  m_win32SM.Stop(servicesMap, servicesMapReset);
#endif
}


// ============================================================================
// Name: List
// Desc: List all services currently under monitoring 
// ============================================================================
void ServiceMonitor::List()
{
#ifdef _WIN32_
  m_win32SM.List();
#endif
}


// ============================================================================
// Name: ShutDown
// Desc: Shut down a specified service
// ============================================================================
void ServiceMonitor::ShutDown(std::string serviceNameOrFullPath)
{
#ifdef _WIN32_
  m_win32SM.ShutdownServiceSafe(serviceNameOrFullPath);
#endif
}


// ============================================================================
// Name: Start
// Desc: Start a specified service
// Parm: serviceFullPath - this param is the full path of where the .exe/.bat  
//       file is
// ============================================================================
void ServiceMonitor::Start(std::string serviceFullPath)
{
  string serviceName;
  ParseFullPathToName(serviceFullPath, serviceName);
#ifdef _WIN32_
  m_win32SM.StartService(serviceName, serviceFullPath);
#endif
}

// ============================================================================
// Name: Start
// Desc: Start a specified service with time interval to re-start it
// Parm: serviceFullPath - this param is the full path of where the .exe/.bat  
//       file is
// ============================================================================
void ServiceMonitor::Start(std::string serviceFullPath, float hours)
{
  string serviceName;
  ParseFullPathToName(serviceFullPath, serviceName);
#ifdef _WIN32_
  m_win32SM.StartService(serviceName, serviceFullPath, hours);
#endif
}

// ============================================================================
// Name: Check
// Desc: check the status of a specified service
// ============================================================================
void ServiceMonitor::Check(std::string serviceFullPath)
{
  string serviceName;
  ParseFullPathToName(serviceFullPath, serviceName);
#ifdef _WIN32_
  m_win32SM.CheckService(serviceName, serviceFullPath);
#endif
}

// ============================================================================
// Name: Detach
// Desc: detach a specified service from being monitored and cycling etc.
// ============================================================================
void ServiceMonitor::Detach(std::string serviceFullPath)
{
  string serviceName;
  ParseFullPathToName(serviceFullPath, serviceName);
#ifdef _WIN32_
  m_win32SM.DetachService(serviceName, serviceFullPath);
#endif
}

// ============================================================================
// Name: Attach
// Desc: attach a specified service with being monitored or cycling etc.
// ============================================================================
void ServiceMonitor::Attach(std::string fullPath, std::string interval)
{
  string serviceName;
  ParseFullPathToName(fullPath, serviceName);
#ifdef _WIN32_
  m_win32SM.AttachService(serviceName, fullPath, interval);
#endif
}

// ============================================================================
// Name: Detach
// Desc: detach a specified service from being monitored and cycling etc.
// ============================================================================
void ServiceMonitor::DetachAll()
{
#ifdef _WIN32_
  m_win32SM.DetachAll();
#endif
}

void ServiceMonitor::testopenfile()
{
  FILE* eventLog;
  if (eventLog = fopen("C:\\Users\\stao\\Desktop\\Base\\testopenfile.txt", "a+b"))
  {
	fclose(eventLog);
	printf("Have opened the log file successfully.\n");
  }
  else
  {
	printf("Cannot open the log file. Please wait for it ...\n");
	while ((eventLog=fopen("C:\\Users\\stao\\Desktop\\Base\\testopenfile.txt", "a+b")) == NULL)
		Sleep(1000);
	printf("Have opened the log file successfully.\n");
  }
  
  HANDLE hfile = CreateFile("C:\\Users\\stao\\Desktop\\Base\\testopenfile.txt",
                            GENERIC_READ|GENERIC_WRITE,
                            FILE_SHARE_READ|FILE_SHARE_WRITE,
                            NULL,
                            OPEN_ALWAYS, //OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS,
                            NULL);

  if (hfile == INVALID_HANDLE_VALUE)
  {
    MessageBox(NULL, "CreateFile error!", NULL, MB_OK);
    return;
  }
  
  if (eventLog = fopen("C:\\Users\\stao\\Desktop\\Base\\testopenfile.txt", "a+b"))
  {
	fputs("test from one process\n", eventLog);
    fflush(eventLog);
  }
  Sleep(10000);
  fclose(eventLog);
  //CloseHandle(hfile);
}

// ============================================================================
// Name: ReadConfig
// Desc: Read the configuration file, then parse it and then perform the 
//       commands in the file
// Parm: configFile - the actual file
// ============================================================================
bool ServiceMonitor::ReadConfig(const std::string& configFile)
{
  // Clear the m_cfg from the default inputs and get the new inputs from the configure file
  ResetConfig();
  if (!ParseConfig(configFile))
    return false;

  if (m_cfg.command != "ServicesMonitor")
  {
    std::cout << "Wrong xml configuration file used." << std::endl;
    return false;
  }

  // Check the command. Is it necessary?
  if(m_cfg.option == "run")
  {
    Run();
  }
  else if(m_cfg.option == "stop")
  {
    Stop(true);
  }
  else if(m_cfg.option == "start")
  {
    if (!m_cfg.attrVec.front().runScript.empty())
    {
      //if(!m_cfg.attrVec.front().startEXEScript.empty())
      //  start(m_cfg.attrVec.front().runScript,m_cfg.attrVec.front().startEXEScript); //TODO: this has been changed.
      //else if (!m_cfg.attrVec.front().location.empty())
      //  start(m_cfg.attrVec.front().runScript,m_cfg.attrVec.front().location);	//TODO: this has been changed.
    }
    else if (!m_cfg.attrVec.front().startEXEScript.empty())
    {
      Start(m_cfg.attrVec.front().startEXEScript);
    }
    else if (!m_cfg.attrVec.front().location.empty())
    {
      Start(m_cfg.attrVec.front().location);
    }
    else
    {
      LOG(NORMAL_OUTPUT, "The config file was not set correctly.");
    }
  }
  else if(m_cfg.option == "shutdown")
  {
    ShutDown(m_cfg.attrVec.front().serviceName);
  }
  else if(m_cfg.option == "check")
  { 
    //TODO: test()
  }
  else
  {
    cout << "Check 'commandline->option' in the configuration file." << endl;
    return false;
  }

  return true;
}


// ============================================================================
// Name: ResetConfig
// Desc: Reset servicesMap to get the customized configuration
// ============================================================================
void ServiceMonitor::ResetConfig()
{
  m_cfg.command.clear();
  m_cfg.option.clear();
  m_cfg.attrVec.clear();
}


// ============================================================================
// Name: ParseFullPathToName
// Desc: Reset servicesMap to get the customized configuration
// Parm: serviceFullPath - the full path of the service (path + .exe/.bat)
// Parm: serviceName - the name of the service (.exe/.bat) obtained from the 
//       the serviceFullPath parameter
// ============================================================================
void ServiceMonitor::ParseFullPathToName(const std::string& serviceFullPath, std::string &o_serviceName)
{
  if (strstr(serviceFullPath.c_str(),".bat") != NULL)
  {
    //if the input is a .bat run script full path
    std::string tmp;
    std::ifstream fp(serviceFullPath.c_str());
    if (!fp)
      LOG(NORMAL_OUTPUT, "Cannot open the input .bat script file.");

    while (getline(fp, tmp))
    {
      //use "start " instead of "start" only to avoid the case that the line has any word including "start" letters
      if (tmp.find("start ") != std::string::npos && tmp.find(".exe") != std::string::npos)
      {
        //"start" and ".exe" have to be in the same line
        ParseFullPathToName(tmp, o_serviceName);
        break;
      }
    }

    fp.close();

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
    int len = extensionPos + 4 - start - 1;
    strncpy_s(tmp, MAX_PATH, start + 1, len);
    tmp[len] = '\0';

    o_serviceName = tmp;
  }
  else
  {
    LOG(NORMAL_OUTPUT, "ParseFullPathToName> Error: Unknown extension type %s", serviceFullPath.c_str());
  }
}

