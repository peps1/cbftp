#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/eventreceiver.h"
#include "rawbuffercallback.h"
#include "path.h"

enum TransferMonitorType {
 TM_TYPE_FXP,
 TM_TYPE_DOWNLOAD,
 TM_TYPE_UPLOAD,
 TM_TYPE_LIST
};

enum Status {
  TM_STATUS_IDLE,
  TM_STATUS_AWAITING_PASSIVE,
  TM_STATUS_AWAITING_ACTIVE,
  TM_STATUS_TRANSFERRING,
  TM_STATUS_TRANSFERRING_SOURCE_COMPLETE,
  TM_STATUS_TRANSFERRING_TARGET_COMPLETE,
  TM_STATUS_SOURCE_ERROR_AWAITING_TARGET,
  TM_STATUS_TARGET_ERROR_AWAITING_SOURCE
};

enum TransferError {
  TM_ERR_PRET,
  TM_ERR_RETRSTOR,
  TM_ERR_RETRSTOR_COMPLETE,
  TM_ERR_OTHER,
  TM_ERR_LOCK_DOWN,
  TM_ERR_LOCK_UP,
  TM_ERR_DUPE,
  TM_ERR_CLEANUP
};

class CommandOwner;
class SiteLogic;
class FileList;
class TransferManager;
class TransferStatus;
class LocalTransfer;
class LocalFileList;

class TransferMonitor : public Core::EventReceiver, public RawBufferCallback {
  private:
    Status status;
    std::string sfile;
    std::string dfile;
    int src;
    int dst;
    int storeid;
    std::shared_ptr<SiteLogic> sls;
    std::shared_ptr<SiteLogic> sld;
    FileList * fls;
    FileList * fld;
    std::shared_ptr<LocalFileList> localfl;
    Path spath;
    Path dpath;
    bool clientactive;
    bool fxpdstactive;
    bool ssl;
    bool sourcesslclient;
    TransferMonitorType type;
    int timestamp;
    int startstamp;
    int partialcompletestamp;
    TransferManager * tm;
    std::shared_ptr<TransferStatus> ts;
    int latestsrctouch;
    int latestdsttouch;
    bool hiddenfiles;
    LocalTransfer * lt;
    int ticker;
    TransferError error;
    std::list<std::pair<std::string, std::string> > rawbufqueue;
    std::shared_ptr<CommandOwner> srcco;
    std::shared_ptr<CommandOwner> dstco;
    void finish();
    void setTargetSizeSpeed(unsigned long long int, int);
    void reset();
    void transferFailed(const std::shared_ptr<TransferStatus> &, TransferError);
    void updateFXPSizeSpeed();
    void updateLocalTransferSizeSpeed();
    void checkForDeadFXPTransfers();
    void startClientTransfer();
  public:
    TransferMonitor(TransferManager *);
    ~TransferMonitor();
    void tick(int);
    void sourceComplete();
    void targetComplete();
    void sourceError(TransferError);
    void targetError(TransferError);
    void passiveReady(const std::string &, int);
    void activeReady();
    void activeStarted();
    void sslDetails(const std::string &, bool);
    bool idle() const;
    std::shared_ptr<TransferStatus> getTransferStatus() const;
    void engageFXP(const std::string &, const std::shared_ptr<SiteLogic> &, FileList *,
      const std::string &, const std::shared_ptr<SiteLogic> &, FileList *,
      const std::shared_ptr<CommandOwner> &, const std::shared_ptr<CommandOwner> &);
    void engageDownload(const std::string &, const std::shared_ptr<SiteLogic> &,
      FileList *, const std::shared_ptr<LocalFileList> &, const std::shared_ptr<CommandOwner> &);
    void engageUpload(const std::string &, const std::shared_ptr<LocalFileList> &,
      const std::shared_ptr<SiteLogic> &, FileList *, const std::shared_ptr<CommandOwner> &);
    void engageList(const std::shared_ptr<SiteLogic> &, int, bool, FileList *,
        const std::shared_ptr<CommandOwner> &);
    Status getStatus() const;
    bool willFail() const;
    void newRawBufferLine(const std::pair<std::string, std::string> &);
};
