#include "settingsloadersaver.h"

#include <ctime>
#include <regex>
#include <vector>
#include <set>

#include "core/tickpoke.h"
#include "core/iomanager.h"
#include "core/sslmanager.h"
#include "crypto.h"
#include "globalcontext.h"
#include "datafilehandler.h"
#include "eventlog.h"
#include "remotecommandhandler.h"
#include "externalfileviewing.h"
#include "skiplist.h"
#include "skiplistitem.h"
#include "proxymanager.h"
#include "proxy.h"
#include "localstorage.h"
#include "uibase.h"
#include "sitemanager.h"
#include "site.h"
#include "path.h"
#include "engine.h"
#include "hourlyalltracking.h"
#include "sectionmanager.h"

#define AUTO_SAVE_INTERVAL 600000 // 10 minutes

namespace {

std::string replaceRegexEnd(const std::string& pattern) {
  return std::regex_replace(pattern, std::regex("\\$"), "#end#");
}

std::string restoreRegexEnd(const std::string& pattern) {
  return std::regex_replace(pattern, std::regex("#end#"), "$");
}

std::string trackerHoursToString(const HourlyAllTracking& tracker) {
  std::string out(std::to_string(tracker.getStartTimestamp()) + ";");
  for (unsigned long long int num : tracker.getHours()) {
    out += std::to_string(num) + ",";
  }
  return out.substr(0, out.length() - 1);
}

void stringToTrackerHours(HourlyAllTracking& tracker, const std::string& in) {
  size_t split = in.find(";");
  unsigned int timestamp = std::stoul(in.substr(0, split));
  std::vector<unsigned long long int> hours;
  size_t start = split + 1;
  size_t end;
  while ((end = in.find(",", start)) != std::string::npos) {
    hours.push_back(std::stoull(in.substr(start, end - start)));
    start = end + 1;
  }
  hours.push_back(std::stoull(in.substr(start)));
  tracker.setHours(hours, timestamp);
}

}

SettingsLoaderSaver::SettingsLoaderSaver() :
  dfh(std::make_shared<DataFileHandler>()){

}

bool SettingsLoaderSaver::enterKey(const std::string & key) {
  if (dfh->isInitialized()) {
    return false;
  }
  if (dataExists()) {
    bool result = dfh->readEncrypted(key);
    if (result) {
      global->getEventLog()->log("DataLoaderSaver", "Data decryption successful.");
      loadSettings();
      startAutoSaver();
      return true;
    }
    global->getEventLog()->log("DataLoaderSaver", "Data decryption failed.");
    return false;
  }
  else {
    dfh->newDataFile(key);
    startAutoSaver();
    return true;
  }
}

bool SettingsLoaderSaver::dataExists() const {
  return dfh->fileExists();
}

bool SettingsLoaderSaver::changeKey(const std::string & key, const std::string & newkey) {
  return dfh->changeKey(key, newkey);
}

void SettingsLoaderSaver::loadSettings() {
  std::vector<std::string> lines;
  dfh->getDataFor("IOManager", &lines);
  std::vector<std::string>::iterator it;
  std::string line;
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("defaultinterface")) {
      global->getIOManager()->setDefaultInterface(value);
    }
  }

  dfh->getDataFor("SSLManager", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("certkeypair") && value.size()) {
      size_t splitter = value.find("$");
      if (splitter == std::string::npos) {
        continue;
      }
      std::string certificate = value.substr(0, splitter);
      std::string privatekey = value.substr(splitter + 1);
      Core::BinaryData certdata;
      Core::BinaryData privkeydata;
      Crypto::base64Decode(Core::BinaryData(certificate.begin(), certificate.end()), certdata);
      Crypto::base64Decode(Core::BinaryData(privatekey.begin(), privatekey.end()), privkeydata);
      Core::SSLManager::addCertKeyPair(privkeydata, certdata);
    }
  }

  dfh->getDataFor("RemoteCommandHandler", &lines);
  bool enable = false;
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("enabled")) {
      if (!value.compare("true")) {
        enable = true;
      }
    }
    else if (!setting.compare("port")) {
      global->getRemoteCommandHandler()->setPort(std::stoi(value));
    }
    else if (!setting.compare("password")) {
      global->getRemoteCommandHandler()->setPassword(value);
    }
    else if (!setting.compare("notify")) {
      // begin backward compatibility r977
      if (!value.compare("true")) {
        global->getRemoteCommandHandler()->setNotify(RemoteCommandNotify::ALL_COMMANDS);
        continue;
      }
      // end backward compatibility r977
      global->getRemoteCommandHandler()->setNotify(static_cast<RemoteCommandNotify>(std::stoi(value)));
    }
  }
  if (enable) {
    global->getRemoteCommandHandler()->setEnabled(true);
  }

  dfh->getDataFor("ExternalFileViewing", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("video")) {
      global->getExternalFileViewing()->setVideoViewer(value);
    }
    else if (!setting.compare("audio")) {
      global->getExternalFileViewing()->setAudioViewer(value);
    }
    else if (!setting.compare("image")) {
      global->getExternalFileViewing()->setImageViewer(value);
    }
    else if (!setting.compare("pdf")) {
      global->getExternalFileViewing()->setPDFViewer(value);
    }
  }

  global->getSkipList()->clearEntries();
  dfh->getDataFor("SkipList", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("entry")) {
      loadSkipListEntry(global->getSkipList(), value);
    }
  }


  dfh->getDataFor("ProxyManager", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok1 = line.find('$');
    size_t tok2 = line.find('=', tok1);
    std::string name = line.substr(0, tok1);
    std::string setting = line.substr(tok1 + 1, (tok2 - tok1 - 1));
    std::string value = line.substr(tok2 + 1);
    Proxy * proxy = global->getProxyManager()->getProxy(name);
    if (proxy == NULL) {
      proxy = new Proxy(name);
      global->getProxyManager()->addProxy(proxy);
    }
    if (!setting.compare("addr")) {
      proxy->setAddr(value);
    }
    else if (!setting.compare("port")) {
      proxy->setPort(value);
    }
    else if (!setting.compare("authmethod")) {
      proxy->setAuthMethod(std::stoi(value));
    }
    else if (!setting.compare("user")) {
      proxy->setUser(value);
    }
    else if (!setting.compare("pass")) {
      proxy->setPass(value);
    }
  }
  lines.clear();
  dfh->getDataFor("ProxyManagerDefaults", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("useproxy")) {
      global->getProxyManager()->setDefaultProxy(value);
    }
  }
  global->getProxyManager()->sortProxys();

  dfh->getDataFor("LocalStorage", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("temppath")) {
      global->getLocalStorage()->setTempPath(value);
    }
    else if (!setting.compare("downloadpath")) {
      global->getLocalStorage()->setDownloadPath(value);
    }
    else if (!setting.compare("useactivemodeaddr")) {
      if (!value.compare("true")) global->getLocalStorage()->setUseActiveModeAddress(true);
    }
    else if (!setting.compare("activemodeaddr")) {
      global->getLocalStorage()->setActiveModeAddress(value);
    }
    else if (!setting.compare("activeportfirst")) {
      global->getLocalStorage()->setActivePortFirst(std::stoi(value));
    }
    else if (!setting.compare("activeportlast")) {
      global->getLocalStorage()->setActivePortLast(std::stoi(value));
    }
  }

  std::list<std::pair<std::string, std::string> > exceptsources;
  std::list<std::pair<std::string, std::string> > excepttargets;
  dfh->getDataFor("SiteManager", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok1 = line.find('$');
    size_t tok2 = line.find('=', tok1);
    std::string name = line.substr(0, tok1);
    std::string setting = line.substr(tok1 + 1, (tok2 - tok1 - 1));
    std::string value = line.substr(tok2 + 1);
    std::shared_ptr<Site> site = global->getSiteManager()->getSite(name);
    if (!site) {
      site = std::make_shared<Site>(name);
      global->getSiteManager()->addSiteLoad(site);
    }
    if (!setting.compare("addr")) {
      site->setAddresses(value);
    }
    else if (!setting.compare("user")) {
      site->setUser(value);
    }
    else if (!setting.compare("pass")) {
      site->setPass(value);
    }
    else if (!setting.compare("basepath")) {
      site->setBasePath(value);
    }
    else if (!setting.compare("idletime")) {
      site->setMaxIdleTime(std::stoi(value));
    }
    else if (!setting.compare("pret")) {
      if (!value.compare("true")) site->setPRET(true);
    }
    else if (!setting.compare("binary")) {
      if (!value.compare("true")) site->setForceBinaryMode(true);
    }
    else if (!setting.compare("xdupe")) {
      if (!value.compare("false")) site->setUseXDUPE(false);
    }
    // begin backward compatibility r938
    else if (!setting.compare("sslconn")) {
      if (!value.compare("false")) site->setTLSMode(TLSMode::NONE);
    }
    // end backward compatibility r938
    else if (!setting.compare("tlsmode")) {
      site->setTLSMode(static_cast<TLSMode>(std::stoi(value)));
    }
    else if (!setting.compare("sslfxpforced")) {
      if (!value.compare("true")) site->setSSLTransferPolicy(SITE_SSL_ALWAYS_ON);
    }
    else if (!setting.compare("ssltransfer")) {
      site->setSSLTransferPolicy(std::stoi(value));
    }
    else if (!setting.compare("sscn")) {
      if (!value.compare("false")) site->setSupportsSSCN(false);
    }
    else if (!setting.compare("cpsv")) {
      if (!value.compare("false")) site->setSupportsCPSV(false);
    }
    else if (!setting.compare("listcommand")) {
      site->setListCommand(std::stoi(value));
    }
    else if (!setting.compare("disabled")) {
      if (!value.compare("true")) site->setDisabled(true);
    }
    else if (!setting.compare("allowupload")) {
      // compatibility: 889 and below
      if (!value.compare("false")) {
        site->setAllowUpload(SITE_ALLOW_TRANSFER_NO);
      }
      // compatibility end
      else {
        site->setAllowUpload(static_cast<SiteAllowTransfer>(std::stoi(value)));
      }
    }
    else if (!setting.compare("allowdownload")) {
      // compatibility: 889 and below
      if (!value.compare("false")) {
        site->setAllowDownload(SITE_ALLOW_TRANSFER_NO);
      }
      // compatibility end
      else {
        site->setAllowDownload(static_cast<SiteAllowTransfer>(std::stoi(value)));
      }
    }
    else if (!setting.compare("priority")) {
      site->setPriority(static_cast<SitePriority>(std::stoi(value)));
    }
    else if (!setting.compare("brokenpasv")) {
      if (!value.compare("true")) site->setBrokenPASV(true);
    }
    else if (!setting.compare("logins")) {
      site->setMaxLogins(std::stoi(value));
    }
    else if (!setting.compare("maxdn")) {
      site->setMaxDn(std::stoi(value));
    }
    else if (!setting.compare("maxdnpre")) {
      site->setMaxDnPre(std::stoi(value));
    }
    else if (!setting.compare("maxdncomplete")) {
      site->setMaxDnComplete(std::stoi(value));
    }
    else if (!setting.compare("maxdntransferjob")) {
      site->setMaxDnTransferJob(std::stoi(value));
    }
    else if (!setting.compare("maxup")) {
      site->setMaxUp(std::stoi(value));
    }
    else if (!setting.compare("freeslot")) {
      if (!value.compare("true")) {
        site->setLeaveFreeSlot(true);
      }
    }
    else if (!setting.compare("section")) {
      size_t split = value.find('$');
      std::string sectionname = value.substr(0, split);
      std::string sectionpath = value.substr(split + 1);
      site->addSection(sectionname, sectionpath);
    }
    else if (!setting.compare("avgspeed")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      int avgspeed = std::stoi(value.substr(split + 1));
      site->setAverageSpeed(sitename, avgspeed);
    }
    else if (!setting.compare("affil")) {
      site->addAffil(value);
    }
    else if (!setting.compare("proxytype")) {
      site->setProxyType(std::stoi(value));
    }
    else if (!setting.compare("proxyname")) {
      site->setProxy(value);
    }
    else if (!setting.compare("transfersourcepolicy")) {
      site->setTransferSourcePolicy(std::stoi(value));
    }
    else if (!setting.compare("transfertargetpolicy")) {
      site->setTransferTargetPolicy(std::stoi(value));
    }
    else if (!setting.compare("exceptsourcesite")) {
      exceptsources.push_back(std::pair<std::string, std::string>(name, value));
    }
    else if (!setting.compare("excepttargetsite")) {
      excepttargets.push_back(std::pair<std::string, std::string>(name, value));
    }
    else if (!setting.compare("skiplistentry")) {
      loadSkipListEntry((SkipList *)&site->getSkipList(), value);
    }
    else if (!setting.compare("sizeup24")) {
      stringToTrackerHours(site->getSizeUp(), value);
    }
    else if (!setting.compare("sizeup")) {
      site->getSizeUp().setAll(std::stoll(value));
    }
    else if (!setting.compare("filesup24")) {
      stringToTrackerHours(site->getFilesUp(), value);
    }
    else if (!setting.compare("filesup")) {
      site->getFilesUp().setAll(std::stoi(value));
    }
    else if (!setting.compare("sizedown24")) {
      stringToTrackerHours(site->getSizeDown(), value);
    }
    else if (!setting.compare("sizedown")) {
      site->getSizeDown().setAll(std::stoll(value));
    }
    else if (!setting.compare("filesdown24")) {
      stringToTrackerHours(site->getFilesDown(), value);
    }
    else if (!setting.compare("filesdown")) {
      site->getFilesDown().setAll(std::stoi(value));
    }
    else if (!setting.compare("sitessizeup24")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      stringToTrackerHours(site->getSiteSizeUp(sitename), value.substr(split + 1));
    }
    else if (!setting.compare("sitessizeup")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      unsigned long long int size = std::stoll(value.substr(split + 1));
      site->getSiteSizeUp(sitename).setAll(size);
    }
    else if (!setting.compare("sitesfilesup24")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      stringToTrackerHours(site->getSiteFilesUp(sitename), value.substr(split + 1));
    }
    else if (!setting.compare("sitesfilesup")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      unsigned int files = std::stoi(value.substr(split + 1));
      site->getSiteFilesUp(sitename).setAll(files);
    }
    else if (!setting.compare("sitessizedown24")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      stringToTrackerHours(site->getSiteSizeDown(sitename), value.substr(split + 1));
    }
    else if (!setting.compare("sitessizedown")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      unsigned long long int size = std::stoll(value.substr(split + 1));
      site->getSiteSizeDown(sitename).setAll(size);
    }
    else if (!setting.compare("sitesfilesdown24")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      stringToTrackerHours(site->getSiteFilesDown(sitename), value.substr(split + 1));
    }
    else if (!setting.compare("sitesfilesdown")) {
      size_t split = value.find('$');
      std::string sitename = value.substr(0, split);
      unsigned int files = std::stoi(value.substr(split + 1));
      site->getSiteFilesDown(sitename).setAll(files);
    }
  }
  for (std::list<std::pair<std::string, std::string> >::const_iterator it2 = exceptsources.begin(); it2 != exceptsources.end(); it2++) {
    std::shared_ptr<Site> site = global->getSiteManager()->getSite(it2->first);
    std::shared_ptr<Site> except = global->getSiteManager()->getSite(it2->second);
    if (!!site && !!except) {
      site->addExceptSourceSite(except);
    }
  }
  for (std::list<std::pair<std::string, std::string> >::const_iterator it2 = excepttargets.begin(); it2 != excepttargets.end(); it2++) {
    std::shared_ptr<Site> site = global->getSiteManager()->getSite(it2->first);
    std::shared_ptr<Site> except = global->getSiteManager()->getSite(it2->second);
    if (!!site && !!except) {
      site->addExceptTargetSite(except);
    }
  }

  dfh->getDataFor("SiteManagerDefaults", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("username")) {
      global->getSiteManager()->setDefaultUserName(value);
    }
    else if (!setting.compare("password")) {
      global->getSiteManager()->setDefaultPassword(value);
    }
    else if (!setting.compare("maxlogins")) {
      global->getSiteManager()->setDefaultMaxLogins(std::stoi(value));
    }
    else if (!setting.compare("maxup")) {
      global->getSiteManager()->setDefaultMaxUp(std::stoi(value));
    }
    else if (!setting.compare("maxdown")) {
      global->getSiteManager()->setDefaultMaxDown(std::stoi(value));
    }
    // begin backward compatibility r938
    else if (!setting.compare("sslconn")) {
      if (!value.compare("false")) {
        global->getSiteManager()->setDefaultTLSMode(TLSMode::NONE);
      }
    }
    // end backward compatibility r938
    else if (!setting.compare("tlsmode")) {
      global->getSiteManager()->setDefaultTLSMode(static_cast<TLSMode>(std::stoi(value)));
    }
    else if (!setting.compare("sslfxpforced")) {
      if (!value.compare("true")) {
        global->getSiteManager()->setDefaultSSLTransferPolicy(SITE_SSL_ALWAYS_ON);
      }
    }
    else if (!setting.compare("ssltransfer")) {
      global->getSiteManager()->setDefaultSSLTransferPolicy(std::stoi(value));
    }
    else if (!setting.compare("maxidletime")) {
      global->getSiteManager()->setDefaultMaxIdleTime(std::stoi(value));
    }
  }

  dfh->getDataFor("Engine", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("preparedraceexpirytime")) {
      global->getEngine()->setPreparedRaceExpiryTime(std::stoi(value));
    }
    if (!setting.compare("racestarterexpiry")) {
      global->getEngine()->setNextPreparedRaceStarterTimeout(std::stoi(value));
    }
  }

  dfh->getDataFor("SectionManager", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok1 = line.find('$');
    size_t tok2 = line.find('=', tok1);
    std::string name = line.substr(0, tok1);
    std::string setting = line.substr(tok1 + 1, (tok2 - tok1 - 1));
    std::string value = line.substr(tok2 + 1);
    Section * section = global->getSectionManager()->getSection(name);
    if (!section) {
      global->getSectionManager()->addSection(name);
      section = global->getSectionManager()->getSection(name);
    }
    if (!setting.compare("jobs")) {
      section->setNumJobs(std::stoi(value));
    }
    if (!setting.compare("hotkey")) {
      section->setHotKey(std::stoi(value));
    }
    if (!setting.compare("skiplistentry")) {
      loadSkipListEntry(&section->getSkipList(), value);
    }
  }

  dfh->getDataFor("Statistics", &lines);
  for (it = lines.begin(); it != lines.end(); it++) {
    line = *it;
    if (line.length() == 0 ||line[0] == '#') continue;
    size_t tok = line.find('=');
    std::string setting = line.substr(0, tok);
    std::string value = line.substr(tok + 1);
    if (!setting.compare("sizeup24")) {
      stringToTrackerHours(global->getStatistics()->getSizeUp(), value);
    }
    else if (!setting.compare("sizeup")) {
      global->getStatistics()->getSizeUp().setAll(std::stoll(value));
    }
    else if (!setting.compare("filesup24")) {
      stringToTrackerHours(global->getStatistics()->getFilesUp(), value);
    }
    else if (!setting.compare("filesup")) {
      global->getStatistics()->getFilesUp().setAll(std::stoi(value));
    }
    else if (!setting.compare("sizedown24")) {
      stringToTrackerHours(global->getStatistics()->getSizeDown(), value);
    }
    else if (!setting.compare("sizedown")) {
      global->getStatistics()->getSizeDown().setAll(std::stoll(value));
    }
    else if (!setting.compare("filesdown24")) {
      stringToTrackerHours(global->getStatistics()->getFilesDown(), value);
    }
    else if (!setting.compare("filesdown")) {
      global->getStatistics()->getFilesDown().setAll(std::stoi(value));
    }
    else if (!setting.compare("sizefxp24")) {
      stringToTrackerHours(global->getStatistics()->getSizeFXP(), value);
    }
    else if (!setting.compare("sizefxp")) {
      global->getStatistics()->getSizeFXP().setAll(std::stoll(value));
    }
    else if (!setting.compare("filesfxp24")) {
      stringToTrackerHours(global->getStatistics()->getFilesFXP(), value);
    }
    else if (!setting.compare("filesfxp")) {
      global->getStatistics()->getFilesFXP().setAll(std::stoi(value));
    }
    else if (!setting.compare("spreadjobs")) {
      global->getStatistics()->setSpreadJobs(std::stoi(value));
    }
    else if (!setting.compare("transferjobs")) {
      global->getStatistics()->setTransferJobs(std::stoi(value));
    }
  }

  global->getSiteManager()->sortSites();

  for (std::list<SettingsAdder *>::iterator it = settingsadders.begin(); it != settingsadders.end(); it++) {
    (*it)->loadSettings(dfh);
  }
}

void SettingsLoaderSaver::saveSettings() {
  if (!dfh->isInitialized()) {
    return;
  }

  dfh->clearOutputData();

  if (global->getIOManager()->hasDefaultInterface()) {
    dfh->addOutputLine("IOManager", "defaultinterface=" + global->getIOManager()->getDefaultInterface());
  }

  for (const std::pair<Core::BinaryData, Core::BinaryData>& pair : Core::SSLManager::certKeyPairs()) {
    Core::BinaryData key;
    Core::BinaryData cert;
    Crypto::base64Encode(pair.first, key);
    Crypto::base64Encode(pair.second, cert);
    dfh->addOutputLine("SSLManager", "certkeypair=" + std::string(cert.begin(), cert.end()) + "$" + std::string(key.begin(), key.end()));
  }

  if (global->getRemoteCommandHandler()->isEnabled()) dfh->addOutputLine("RemoteCommandHandler", "enabled=true");
  dfh->addOutputLine("RemoteCommandHandler", "port=" + std::to_string(global->getRemoteCommandHandler()->getUDPPort()));
  dfh->addOutputLine("RemoteCommandHandler", "password=" + global->getRemoteCommandHandler()->getPassword());
  dfh->addOutputLine("RemoteCommandHandler", "notify=" + std::to_string(static_cast<int>(global->getRemoteCommandHandler()->getNotify())));

  {
    std::string filetag = "ExternalFileViewing";
    dfh->addOutputLine(filetag, "video=" + global->getExternalFileViewing()->getVideoViewer());
    dfh->addOutputLine(filetag, "audio=" + global->getExternalFileViewing()->getAudioViewer());
    dfh->addOutputLine(filetag, "image=" + global->getExternalFileViewing()->getImageViewer());
    dfh->addOutputLine(filetag, "pdf=" + global->getExternalFileViewing()->getPDFViewer());
  }


  addSkipList(global->getSkipList(), "SkipList", "entry=");

  {
    std::vector<Proxy *>::const_iterator it;
    std::string filetag = "ProxyManager";
    std::string defaultstag = "ProxyManagerDefaults";
    for (it = global->getProxyManager()->begin(); it != global->getProxyManager()->end(); it++) {
      Proxy * proxy = *it;
      std::string name = proxy->getName();
      dfh->addOutputLine(filetag, name + "$addr=" + proxy->getAddr());
      dfh->addOutputLine(filetag, name + "$port=" + proxy->getPort());
      dfh->addOutputLine(filetag, name + "$user=" + proxy->getUser());
      dfh->addOutputLine(filetag, name + "$pass=" + proxy->getPass());
      dfh->addOutputLine(filetag, name + "$authmethod=" + std::to_string(proxy->getAuthMethod()));
    }
    if (global->getProxyManager()->getDefaultProxy() != NULL) {
      dfh->addOutputLine(defaultstag, "useproxy=" + global->getProxyManager()->getDefaultProxy()->getName());
    }
  }

  {
    std::string filetag = "LocalStorage";
    dfh->addOutputLine(filetag, "temppath=" + global->getLocalStorage()->getTempPath().toString());
    dfh->addOutputLine(filetag, "downloadpath=" + global->getLocalStorage()->getDownloadPath().toString());
    if (global->getLocalStorage()->getUseActiveModeAddress()) dfh->addOutputLine(filetag, "useactivemodeaddr=true");
    dfh->addOutputLine(filetag, "activemodeaddr=" + global->getLocalStorage()->getActiveModeAddress());
    dfh->addOutputLine(filetag, "activeportfirst=" + std::to_string(global->getLocalStorage()->getActivePortFirst()));
    dfh->addOutputLine(filetag, "activeportlast=" + std::to_string(global->getLocalStorage()->getActivePortLast()));
  }

  {
    std::vector<std::shared_ptr<Site> >::const_iterator it;
    std::string filetag = "SiteManager";
    std::string defaultstag = "SiteManagerDefaults";
    for (it = global->getSiteManager()->begin(); it != global->getSiteManager()->end(); it++) {
      const std::shared_ptr<Site> & site = *it;
      std::string name = site->getName();
      dfh->addOutputLine(filetag, name + "$addr=" + site->getAddressesAsString());
      dfh->addOutputLine(filetag, name + "$user=" + site->getUser());
      dfh->addOutputLine(filetag, name + "$pass=" + site->getPass());
      dfh->addOutputLine(filetag, name + "$basepath=" + site->getBasePath().toString());
      dfh->addOutputLine(filetag, name + "$logins=" + std::to_string(site->getInternMaxLogins()));
      dfh->addOutputLine(filetag, name + "$maxup=" + std::to_string(site->getInternMaxUp()));
      dfh->addOutputLine(filetag, name + "$maxdn=" + std::to_string(site->getInternMaxDown()));
      dfh->addOutputLine(filetag, name + "$maxdnpre=" + std::to_string(site->getInternMaxDownPre()));
      dfh->addOutputLine(filetag, name + "$maxdncomplete=" + std::to_string(site->getInternMaxDownComplete()));
      dfh->addOutputLine(filetag, name + "$maxdntransferjob=" + std::to_string(site->getInternMaxDownTransferJob()));
      dfh->addOutputLine(filetag, name + "$idletime=" + std::to_string(site->getMaxIdleTime()));
      dfh->addOutputLine(filetag, name + "$tlsmode=" + std::to_string(static_cast<int>(site->getTLSMode())));
      dfh->addOutputLine(filetag, name + "$ssltransfer=" + std::to_string(site->getSSLTransferPolicy()));
      if (!site->supportsSSCN()) dfh->addOutputLine(filetag, name + "$sscn=false");
      if (!site->supportsCPSV()) dfh->addOutputLine(filetag, name + "$cpsv=false");
      dfh->addOutputLine(filetag, name + "$listcommand=" + std::to_string(site->getListCommand()));
      if (site->needsPRET()) dfh->addOutputLine(filetag, name + "$pret=true");
      if (site->forceBinaryMode()) dfh->addOutputLine(filetag, name + "$binary=true");
      if (!site->useXDUPE()) dfh->addOutputLine(filetag, name + "$xdupe=false");
      if (site->getDisabled()) dfh->addOutputLine(filetag, name + "$disabled=true");
      dfh->addOutputLine(filetag, name + "$allowupload=" + std::to_string(site->getAllowUpload()));
      dfh->addOutputLine(filetag, name + "$allowdownload=" + std::to_string(site->getAllowDownload()));
      dfh->addOutputLine(filetag, name + "$priority=" + std::to_string(static_cast<int>(site->getPriority())));
      if (site->hasBrokenPASV()) dfh->addOutputLine(filetag, name + "$brokenpasv=true");
      if (site->getLeaveFreeSlot()) {
        dfh->addOutputLine(filetag, name + "$freeslot=true");
      }
      int proxytype = site->getProxyType();
      dfh->addOutputLine(filetag, name + "$proxytype=" + std::to_string(proxytype));
      if (proxytype == SITE_PROXY_USE) {
        dfh->addOutputLine(filetag, name + "$proxyname=" + site->getProxy());
      }
      std::map<std::string, Path>::const_iterator sit;
      for (sit = site->sectionsBegin(); sit != site->sectionsEnd(); sit++) {
        dfh->addOutputLine(filetag, name + "$section=" + sit->first + "$" + sit->second.toString());
      }
      std::map<std::string, int>::const_iterator sit2;
      for (sit2 = site->avgspeedBegin(); sit2 != site->avgspeedEnd(); sit2++) {
        dfh->addOutputLine(filetag, name + "$avgspeed=" + sit2->first + "$" + std::to_string(sit2->second));
      }
      if (site->getSizeUp().getLast24Hours()) {
        dfh->addOutputLine(filetag, name + "$sizeup24=" + trackerHoursToString(site->getSizeUp()));
      }
      dfh->addOutputLine(filetag, name + "$sizeup=" + std::to_string(site->getSizeUp().getAll()));
      if (site->getFilesUp().getLast24Hours()) {
        dfh->addOutputLine(filetag, name + "$filesup24=" + trackerHoursToString(site->getFilesUp()));
      }
      dfh->addOutputLine(filetag, name + "$filesup=" + std::to_string(site->getFilesUp().getAll()));
      if (site->getSizeDown().getLast24Hours()) {
        dfh->addOutputLine(filetag, name + "$sizedown24=" + trackerHoursToString(site->getSizeDown()));
      }
      dfh->addOutputLine(filetag, name + "$sizedown=" + std::to_string(site->getSizeDown().getAll()));
      if (site->getFilesDown().getLast24Hours()) {
        dfh->addOutputLine(filetag, name + "$filesdown24=" + trackerHoursToString(site->getFilesDown()));
      }
      dfh->addOutputLine(filetag, name + "$filesdown=" + std::to_string(site->getFilesDown().getAll()));
      std::map<std::string, HourlyAllTracking>::const_iterator sit5;
      for (sit5 = site->sizeUpBegin(); sit5 != site->sizeUpEnd(); sit5++) {
        if (sit5->second.getLast24Hours()) {
          dfh->addOutputLine(filetag, name + "$sitessizeup24=" + sit5->first + "$" + trackerHoursToString(sit5->second));
        }
        dfh->addOutputLine(filetag, name + "$sitessizeup=" + sit5->first + "$" + std::to_string(sit5->second.getAll()));
      }
      for (sit5 = site->filesUpBegin(); sit5 != site->filesUpEnd(); sit5++) {
        if (sit5->second.getLast24Hours()) {
          dfh->addOutputLine(filetag, name + "$sitesfilesup24=" + sit5->first + "$" + trackerHoursToString(sit5->second));
        }
        dfh->addOutputLine(filetag, name + "$sitesfilesup=" + sit5->first + "$" + std::to_string(sit5->second.getAll()));
      }
      for (sit5 = site->sizeDownBegin(); sit5 != site->sizeDownEnd(); sit5++) {
        if (sit5->second.getLast24Hours()) {
          dfh->addOutputLine(filetag, name + "$sitessizedown24=" + sit5->first + "$" + trackerHoursToString(sit5->second));
        }
        dfh->addOutputLine(filetag, name + "$sitessizedown=" + sit5->first + "$" + std::to_string(sit5->second.getAll()));
      }
      for (sit5 = site->filesDownBegin(); sit5 != site->filesDownEnd(); sit5++) {
        if (sit5->second.getLast24Hours()) {
          dfh->addOutputLine(filetag, name + "$sitesfilesdown24=" + sit5->first + "$" + trackerHoursToString(sit5->second));
        }
        dfh->addOutputLine(filetag, name + "$sitesfilesdown=" + sit5->first + "$" + std::to_string(sit5->second.getAll()));
      }
      std::set<std::string>::const_iterator sit3;
      for (sit3 = site->affilsBegin(); sit3 != site->affilsEnd(); sit3++) {
        dfh->addOutputLine(filetag, name + "$affil=" + *sit3);
      }
      dfh->addOutputLine(filetag, name + "$transfersourcepolicy=" + std::to_string(site->getTransferSourcePolicy()));
      dfh->addOutputLine(filetag, name + "$transfertargetpolicy=" + std::to_string(site->getTransferTargetPolicy()));
      std::set<std::shared_ptr<Site> >::const_iterator sit4;
      for (sit4 = site->exceptSourceSitesBegin(); sit4 != site->exceptSourceSitesEnd(); sit4++) {
        dfh->addOutputLine(filetag, name + "$exceptsourcesite=" + (*sit4)->getName());
      }
      for (sit4 = site->exceptTargetSitesBegin(); sit4 != site->exceptTargetSitesEnd(); sit4++) {
        dfh->addOutputLine(filetag, name + "$excepttargetsite=" + (*sit4)->getName());
      }
      addSkipList((SkipList *)&site->getSkipList(), filetag, name + "$skiplistentry=");
    }
    dfh->addOutputLine(defaultstag, "username=" + global->getSiteManager()->getDefaultUserName());
    dfh->addOutputLine(defaultstag, "password=" + global->getSiteManager()->getDefaultPassword());
    dfh->addOutputLine(defaultstag, "maxlogins=" + std::to_string(global->getSiteManager()->getDefaultMaxLogins()));
    dfh->addOutputLine(defaultstag, "maxup=" + std::to_string(global->getSiteManager()->getDefaultMaxUp()));
    dfh->addOutputLine(defaultstag, "maxdown=" + std::to_string(global->getSiteManager()->getDefaultMaxDown()));
    dfh->addOutputLine(defaultstag, "maxidletime=" + std::to_string(global->getSiteManager()->getDefaultMaxIdleTime()));
    dfh->addOutputLine(defaultstag, "ssltransfer=" + std::to_string(global->getSiteManager()->getDefaultSSLTransferPolicy()));
    dfh->addOutputLine(defaultstag, "tlsmode=" + std::to_string(static_cast<int>(global->getSiteManager()->getDefaultTLSMode())));
  }

  {
    std::string filetag = "SectionManager";
    for (auto it = global->getSectionManager()->begin(); it != global->getSectionManager()->end(); ++it) {
      const Section & section = it->second;
      dfh->addOutputLine(filetag, section.getName() + "$jobs=" + std::to_string(section.getNumJobs()));
      dfh->addOutputLine(filetag,  section.getName() + "$hotkey=" + std::to_string(section.getHotKey()));
      addSkipList(&section.getSkipList(), filetag, section.getName() + "$skiplistentry=");
    }
  }

  {
    dfh->addOutputLine("Engine", "preparedraceexpirytime=" + std::to_string(global->getEngine()->getPreparedRaceExpiryTime()));
    dfh->addOutputLine("Engine", "racestarterexpiry=" + std::to_string(global->getEngine()->getNextPreparedRaceStarterTimeout()));
  }
  {
    std::string filetag = "Statistics";
    if (global->getStatistics()->getSizeUp().getLast24Hours()) {
      dfh->addOutputLine(filetag, "sizeup24=" + trackerHoursToString(global->getStatistics()->getSizeUp()));
    }
    dfh->addOutputLine(filetag, "sizeup=" + std::to_string(global->getStatistics()->getSizeUp().getAll()));
    if (global->getStatistics()->getFilesUp().getLast24Hours()) {
      dfh->addOutputLine(filetag, "filesup24=" + trackerHoursToString(global->getStatistics()->getFilesUp()));
    }
    dfh->addOutputLine(filetag, "filesup=" + std::to_string(global->getStatistics()->getFilesUp().getAll()));
    if (global->getStatistics()->getSizeDown().getLast24Hours()) {
      dfh->addOutputLine(filetag, "sizedown24=" + trackerHoursToString(global->getStatistics()->getSizeDown()));
    }
    dfh->addOutputLine(filetag, "sizedown=" + std::to_string(global->getStatistics()->getSizeDown().getAll()));
    if (global->getStatistics()->getFilesDown().getLast24Hours()) {
      dfh->addOutputLine(filetag, "filesdown24=" + trackerHoursToString(global->getStatistics()->getFilesDown()));
    }
    dfh->addOutputLine(filetag, "filesdown=" + std::to_string(global->getStatistics()->getFilesDown().getAll()));
    if (global->getStatistics()->getSizeFXP().getLast24Hours()) {
      dfh->addOutputLine(filetag, "sizefxp24=" + trackerHoursToString(global->getStatistics()->getSizeFXP()));
    }
    dfh->addOutputLine(filetag, "sizefxp=" + std::to_string(global->getStatistics()->getSizeFXP().getAll()));
    if (global->getStatistics()->getFilesFXP().getLast24Hours()) {
      dfh->addOutputLine(filetag, "filesfxp24=" + trackerHoursToString(global->getStatistics()->getFilesFXP()));
    }
    dfh->addOutputLine(filetag, "filesfxp=" + std::to_string(global->getStatistics()->getFilesFXP().getAll()));
    dfh->addOutputLine(filetag, "spreadjobs=" + std::to_string(global->getStatistics()->getSpreadJobs()));
    dfh->addOutputLine(filetag, "transferjobs=" + std::to_string(global->getStatistics()->getTransferJobs()));
  }

  for (std::list<SettingsAdder *>::iterator it = settingsadders.begin(); it != settingsadders.end(); it++) {
    (*it)->saveSettings(dfh);
  }

  dfh->writeFile();
}

void SettingsLoaderSaver::tick(int) {
  saveSettings();
}

void SettingsLoaderSaver::addSettingsAdder(SettingsAdder * sa) {
  bool found = false;
  for (std::list<SettingsAdder *>::iterator it = settingsadders.begin(); it != settingsadders.end(); it++) {
    if (*it == sa) {
      found = true;
      break;
    }
  }
  if (!found) {
    settingsadders.push_back(sa);
    if (dfh->isInitialized()) {
      sa->loadSettings(dfh);
    }
  }
}

void SettingsLoaderSaver::removeSettingsAdder(SettingsAdder * sa) {
  for (std::list<SettingsAdder *>::iterator it = settingsadders.begin(); it != settingsadders.end(); it++) {
    if (*it == sa) {
      settingsadders.erase(it);
      break;
    }
  }
}

void SettingsLoaderSaver::startAutoSaver() {
  global->getTickPoke()->startPoke(this, "SettingsLoaderSaver", AUTO_SAVE_INTERVAL, 0);
}

void SettingsLoaderSaver::addSkipList(const SkipList * skiplist, const std::string & owner, const std::string & entry) {
  std::list<SkiplistItem>::const_iterator it;
  for (it = skiplist->entriesBegin(); it != skiplist->entriesEnd(); it++) {
    std::string entryline = std::string(it->matchRegex() ? "true" : "false") + "$" +
        replaceRegexEnd(it->matchPattern()) + "$" +
        (it->matchFile() ? "true" : "false") + "$" +
        (it->matchDir() ? "true" : "false") + "$" +
        std::to_string(it->matchScope()) + "$" +
        std::to_string(it->getAction());
    dfh->addOutputLine(owner, entry + entryline);
  }
}

void SettingsLoaderSaver::loadSkipListEntry(SkipList * skiplist, std::string value) {
  size_t split = value.find('$');
  if (split != std::string::npos) {
    try {
      std::string patterntype = restoreRegexEnd(value.substr(0, split));
      bool regex = false;
      std::string pattern;
      // begin compatibility r1023
      if (patterntype != "true" && patterntype != "false") {
        pattern = patterntype;
      }
      else
      // end compatibility r1023
      {
        regex = patterntype == "true";
        value = value.substr(split + 1);
        split = value.find('$');
        pattern = restoreRegexEnd(value.substr(0, split));
      }
      value = value.substr(split + 1);
      split = value.find('$');
      bool file = value.substr(0, split) == "true" ? true : false;
      value = value.substr(split + 1);
      split = value.find('$');
      bool dir = value.substr(0, split) == "true" ? true : false;
      value = value.substr(split + 1);
      split = value.find('$');
      int scope = std::stoi(value.substr(0, split));
      std::string actionstring = value.substr(split + 1);

      SkipListAction action = static_cast<SkipListAction>(std::stoi(actionstring));
      skiplist->addEntry(regex, pattern, file, dir, scope, action);
    }
    catch (std::invalid_argument&) {
    }
    catch (std::out_of_range&) {
    }
  }
}
