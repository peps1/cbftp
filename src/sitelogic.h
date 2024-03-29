#pragma once

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/eventreceiver.h"

class TransferMonitor;
class ConnStateTracker;
class FTPConn;
class RawBuffer;
class SiteRace;
class FileList;
class Site;
class Race;
class SiteLogicRequest;
class SiteLogicRequestReady;
class PotentialTracker;
class FileStore;
class TransferJob;
class CommandOwner;
class Path;
class SiteTransferJob;
class FileListData;
class DelayedCommand;

enum class TransferType {
  REGULAR,
  PRE,
  COMPLETE,
  TRANSFERJOB
};

class SiteLogic : public Core::EventReceiver {
  private:
    std::shared_ptr<Site> site;
    std::vector<FTPConn *> conns;
    std::vector<ConnStateTracker> connstatetracker;
    std::vector<std::shared_ptr<SiteRace>> currentraces;
    std::list<std::shared_ptr<SiteRace>> recentlylistedraces;
    std::list<std::shared_ptr<SiteTransferJob> > transferjobs;
    RawBuffer * rawcommandrawbuf;
    RawBuffer * aggregatedrawbuf;
    int slotsdn;
    int slotsup;
    int available;
    PotentialTracker * ptrack;
    unsigned int loggedin;
    std::list<SiteLogicRequest> requests;
    std::list<SiteLogicRequestReady> requestsready;
    int requestidcounter;
    bool poke;
    int currtime;
    int timesincelastrequestready;
    std::list<DelayedCommand> delayedcommands;
    void handleConnection(int id, bool backfromrefresh = false);
    bool handleRequest(int);
    void handleRecursiveLogic(int id, FileList * fl = nullptr);
    void addRecentList(const std::shared_ptr<SiteRace> & sr);
    bool wasRecentlyListed(const std::shared_ptr<SiteRace> & sr) const;
    void refreshChangePath(int, const std::shared_ptr<SiteRace> & race, bool);
    void initTransfer(int);
    void handleFail(int);
    void handleTransferFail(int, int);
    void handleTransferFail(int, int, int);
    void reportTransferErrorAndFinish(int, int);
    void reportTransferErrorAndFinish(int, int, int);
    void getFileListConn(int id, bool hiddenfiles = false);
    void getFileListConn(int, const std::shared_ptr<CommandOwner> & co, FileList *);
    void passiveModeCommand(int);
    static void * run(void *);
    bool lockTransferConn(FileList *, int *, TransferMonitor *, const std::shared_ptr<CommandOwner> &, bool);
    void setRequestReady(unsigned int, void *, bool, bool returnslot = true);
    void cleanupConnection(int);
    void checkFailListRequest(int);
    void clearReadyRequest(SiteLogicRequestReady &);
    bool setPathExists(int, int, bool);
    bool handlePreTransfer(int);
    bool handleCommandDelete(int, bool);
    bool makeTargetDirectory(int, bool, const std::shared_ptr<CommandOwner> & co);
    std::shared_ptr<SiteRace> getRace(unsigned int id) const;
  public:
    SiteLogic(const std::string &);
    ~SiteLogic();
    void runInstance();
    std::shared_ptr<SiteRace> addRace(std::shared_ptr<Race> &, const std::string &, const std::string &);
    void resetRace(const std::shared_ptr<SiteRace> & race);
    void addTransferJob(std::shared_ptr<SiteTransferJob> & tj);
    void tick(int);
    void connectFailed(int);
    void userDenied(int);
    void userDeniedSiteFull(int);
    void userDeniedSimultaneousLogins(int);
    void loginKillFailed(int);
    void passDenied(int);
    void TLSFailed(int);
    void listRefreshed(int);
    void commandSuccess(int, int);
    void commandFail(int);
    void commandFail(int, int);
    void gotPath(int, const std::string &);
    void rawCommandResultRetrieved(int, const std::string &);
    void gotPassiveAddress(int, const std::string &, int);
    void timedout(int);
    void disconnected(int);
    void activateOne();
    void activateAll();
    void haveConnectedActivate(unsigned int);
    const std::shared_ptr<Site> & getSite() const;
    std::shared_ptr<SiteRace> getRace(const std::string & race) const;
    void lockConnList(int);
    bool lockDownloadConn(FileList *, int *, const std::shared_ptr<CommandOwner> &, TransferMonitor *);
    bool lockUploadConn(FileList *, int *, const std::shared_ptr<CommandOwner> &, TransferMonitor *);
    void returnConn(int, bool);
    void setNumConnections(unsigned int);
    bool downloadSlotAvailable(TransferType type = TransferType::REGULAR) const;
    bool uploadSlotAvailable() const;
    int slotsAvailable() const;
    int getCurrLogins() const;
    int getCurrDown() const;
    int getCurrUp() const;
    int getCleanlyClosedConnectionsCount() const;
    void connectConn(int);
    void disconnectConn(int id, bool hard = false);
    void finishTransferGracefully(int);
    void listCompleted(int, int, FileList *, const std::shared_ptr<CommandOwner> & co);
    void issueRawCommand(unsigned int, const std::string &);
    RawBuffer * getRawCommandBuffer() const;
    RawBuffer * getAggregatedRawBuffer() const;
    void raceGlobalComplete(const std::shared_ptr<SiteRace> & sr);
    void raceLocalComplete(const std::shared_ptr<SiteRace> & sr, int uploadslotsleft, bool reportdone = true);
    void transferComplete(int, bool isdownload, bool returntransferslot = true);
    bool getSlot(bool isdownload, TransferType type);
    int requestFileList(const Path &);
    int requestRawCommand(const std::string &);
    int requestRawCommand(const Path &, const std::string &, bool);
    int requestWipe(const Path &, bool);
    int requestDelete(const Path &, bool, bool, bool);
    int requestNuke(const Path &, int, const std::string &);
    int requestOneIdle();
    int requestAllIdle(const Path & path, int);
    int requestAllIdle(int);
    int requestMakeDirectory(const Path& path, const std::string & dirname);
    bool requestReady(int) const;
    bool requestStatus(int) const;
    void abortRace(const std::shared_ptr<SiteRace> & race);
    void removeRace(const std::shared_ptr<SiteRace> & race);
    void abortTransfers(const std::shared_ptr<CommandOwner> & co);
    FileListData * getFileListData(int) const;
    std::string getRawCommandResult(int);
    bool finishRequest(int);
    void pushPotential(int, const std::string &, const std::shared_ptr<SiteLogic> &);
    bool potentialCheck(int);
    int getPotential();
    void updateName();
    const std::vector<FTPConn *> * getConns() const;
    FTPConn * getConn(int) const;
    std::string getStatus(int) const;
    void preparePassiveTransfer(int id, const std::string & file, bool fxp, bool ssl, bool sslclient = false);
    void prepareActiveTransfer(int id, const std::string & file , bool fxp, const std::string & host, int port, bool ssl, bool sslclient = false);
    void preparePassiveList(int, TransferMonitor *, bool);
    void prepareActiveList(int, TransferMonitor *, const std::string &, int, bool);
    void download(int);
    void upload(int);
    void list(int);
    void listAll(int);
    const ConnStateTracker * getConnStateTracker(int) const;
};
