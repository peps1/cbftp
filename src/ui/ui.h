#pragma once

#include <string>
#include <list>
#include <vector>
#include <ncursesw/curses.h>

#include "uicommand.h"

#include "../core/threading.h"
#include "../core/semaphore.h"
#include "../core/eventreceiver.h"
#include "../core/blockingqueue.h"
#include "../core/pointer.h"
#include "../uibase.h"
#include "../settingsloadersaver.h"

class UIWindow;
class InfoWindow;
class LegendWindow;
class FileList;
class Site;
class DataFileHandler;
class TransferStatus;
class LoginScreen;
class NewKeyScreen;
class MainScreen;
class ConfirmationScreen;
class EditSiteScreen;
class SiteStatusScreen;
class RawDataScreen;
class RawCommandScreen;
class BrowseScreen;
class AddSectionScreen;
class NewRaceScreen;
class RaceStatusScreen;
class GlobalOptionsScreen;
class SkipListScreen;
class ChangeKeyScreen;
class EventLogScreen;
class ProxyOptionsScreen;
class EditProxyScreen;
class ViewFileScreen;
class NukeScreen;
class FileViewerSettingsScreen;
class ScoreBoardScreen;
class SelectSitesScreen;
class TransfersScreen;
class TransferJobStatusScreen;
class AllRacesScreen;
class AllTransferJobsScreen;
class TransferStatusScreen;

enum LegendMode {
  LEGEND_DISABLED = 123,
  LEGEND_SCROLLING = 124,
  LEGEND_STATIC = 125
};

class Ui : public EventReceiver, public UIBase, public SettingsAdder {
private:
  Thread<Ui> thread;
  BlockingQueue<UICommand> uiqueue;
  WINDOW * main;
  WINDOW * info;
  WINDOW * legend;
  std::vector<Pointer<UIWindow> > mainwindows;
  Pointer<UIWindow> topwindow;
  Pointer<InfoWindow> infowindow;
  Pointer<LegendWindow> legendwindow;
  Pointer<LoginScreen> loginscreen;
  Pointer<NewKeyScreen> newkeyscreen;
  Pointer<MainScreen> mainscreen;
  Pointer<ConfirmationScreen> confirmationscreen;
  Pointer<EditSiteScreen> editsitescreen;
  Pointer<SiteStatusScreen> sitestatusscreen;
  Pointer<RawDataScreen> rawdatascreen;
  Pointer<RawCommandScreen> rawcommandscreen;
  Pointer<BrowseScreen> browsescreen;
  Pointer<AddSectionScreen> addsectionscreen;
  Pointer<NewRaceScreen> newracescreen;
  Pointer<RaceStatusScreen> racestatusscreen;
  Pointer<GlobalOptionsScreen> globaloptionsscreen;
  Pointer<SkipListScreen> skiplistscreen;
  Pointer<ChangeKeyScreen> changekeyscreen;
  Pointer<EventLogScreen> eventlogscreen;
  Pointer<ProxyOptionsScreen> proxyoptionsscreen;
  Pointer<EditProxyScreen> editproxyscreen;
  Pointer<ViewFileScreen> viewfilescreen;
  Pointer<NukeScreen> nukescreen;
  Pointer<FileViewerSettingsScreen> fileviewersettingsscreen;
  Pointer<ScoreBoardScreen> scoreboardscreen;
  Pointer<SelectSitesScreen> selectsitesscreen;
  Pointer<TransfersScreen> transfersscreen;
  Pointer<TransferJobStatusScreen> transferjobstatusscreen;
  Pointer<AllRacesScreen> allracesscreen;
  Pointer<AllTransferJobsScreen> alltransferjobsscreen;
  Pointer<TransferStatusScreen> transferstatusscreen;
  int mainrow;
  int maincol;
  int col;
  int row;
  int ticker;
  bool haspushed;
  bool pushused;
  bool initret;
  bool legendenabled;
  bool infoenabled;
  bool dead;
  LegendMode legendmode;
  bool split;
  bool fullscreentoggle;
  std::string eventtext;
  Semaphore eventcomplete;
  std::list<Pointer<UIWindow> > history;
  void FDData(int);
  void refreshAll();
  void initIntern();
  void enableInfo();
  void disableInfo();
  void enableLegend();
  void disableLegend();
  void redrawAll();
  void switchToWindow(Pointer<UIWindow>);
  void switchToWindow(Pointer<UIWindow>, bool);
  void tick(int);
  void globalKeyBinds(int);
  void switchToLast();
public:
  Ui();
  ~Ui();
  void run();
  bool init();
  void backendPush();
  void terminalSizeChanged();
  void signal(int, int);
  void kill();
  void resizeTerm();
  void readConfiguration();
  void writeState();
  LegendMode legendMode() const;
  void setLegendMode(LegendMode);
  void returnToLast();
  void update();
  void setLegend();
  void setInfo();
  void setSplit(bool);
  void redraw();
  void erase();
  void erase(WINDOW *);
  void showCursor();
  void hideCursor();
  void moveCursor(unsigned int, unsigned int);
  void highlight(bool);
  void printStr(unsigned int, unsigned int, const std::string &);
  void printStr(unsigned int, unsigned int, const std::basic_string<unsigned int> &);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::string &);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::basic_string<unsigned int> &);
  void printStr(unsigned int, unsigned int, const std::string &, bool);
  void printStr(unsigned int, unsigned int, const std::basic_string<unsigned int> &, bool);
  void printStr(unsigned int, unsigned int, const std::string &, unsigned int);
  void printStr(unsigned int, unsigned int, const std::basic_string<unsigned int> &, unsigned int);
  void printStr(unsigned int, unsigned int, const std::string &, unsigned int, bool);
  void printStr(unsigned int, unsigned int, const std::basic_string<unsigned int> &, unsigned int, bool);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::string &, unsigned int, bool);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::basic_string<unsigned int> &, unsigned int, bool);
  void printStr(unsigned int, unsigned int, const std::string &, unsigned int, bool, bool);
  void printStr(unsigned int, unsigned int, const std::basic_string<unsigned int> &, unsigned int, bool, bool);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::string &, unsigned int, bool, bool);
  void printStr(WINDOW *, unsigned int, unsigned int, const std::basic_string<unsigned int> &, unsigned int, bool, bool);
  void printChar(unsigned int, unsigned int, unsigned int);
  void printChar(WINDOW *, unsigned int, unsigned int, unsigned int);
  void printChar(unsigned int, unsigned int, unsigned int, bool);
  void printChar(WINDOW *, unsigned int, unsigned int, unsigned int, bool);
  void goRawCommand(std::string);
  void goRawCommand(std::string, std::string);
  void goConfirmation(std::string);
  void goNuke(std::string, std::string, FileList *);
  void goViewFile(const std::string &, const std::string &, FileList *);
  void goViewFile(const std::string &, const std::string &);
  void goAddSection(std::string, std::string);
  void goNewRace(std::string, std::string, std::string);
  void goSelectSites(std::string, std::list<Pointer<Site> >, std::list<Pointer<Site> >);
  void goSkiplist();
  void goChangeKey();
  void goProxy();
  void goFileViewerSettings();
  void goSiteStatus(std::string);
  void goRaceStatus(unsigned int);
  void goTransferJobStatus(unsigned int);
  void goTransferStatus(Pointer<TransferStatus>);
  void goGlobalOptions();
  void goEventLog();
  void goScoreBoard();
  void goTransfers();
  void goEditSite(std::string);
  void goAddSite();
  void goBrowse(std::string);
  void goBrowseSplit(std::string);
  void goBrowseLocal();
  void goContinueBrowsing();
  void goAddProxy();
  void goEditProxy(std::string);
  void goRawData(std::string);
  void goRawDataJump(std::string, int);
  void goAllRaces();
  void goAllTransferJobs();
  void returnSelectSites(std::string);
  void key(std::string);
  void newKey(std::string);
  void confirmYes();
  void confirmNo();
  void returnNuke(int);
  void returnRaceStatus(unsigned int);
  void loadSettings(Pointer<DataFileHandler>);
  void saveSettings(Pointer<DataFileHandler>);
  void notify();
};

