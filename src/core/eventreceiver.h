#pragma once

#include <string>

namespace Core {

class WorkManager;

/* This is the base class for all event handling in the core library.
 * Any class that wishes to register for any kind of events should inherit
 * from this class.
 */
class EventReceiver {
public:
  EventReceiver();
  virtual ~EventReceiver();

  /* tick() is called by registering for time-based events with TickPoke.
   * @param message: the message that belongs to the tick event. */

  virtual void tick(int message);

  /* signal is called by dispatching a signal event from a signal handler.
   * @param sig: the signal number
   * @param value: additional user-supplied data */
  virtual void signal(int sig, int value);

  /* FDNew is called when a new connection has been established on a
   * listening socket registered with IOManager.
   * @param sockId: the identifier for the socket.
   */
  virtual void FDNew(int sockId);

  /* FDConnecting is called when a client connection registered through
   * IOManager is currently trying to connect, after a host name
   * has been remotely resolved.
   * @param sockId: the identifier for the socket.
   * @param addr: The IP address being connected to.
   */
  virtual void FDConnecting(int sockId, const std::string& addr);

  /* FDConnected is called when a client connection registered through
   * IOManager has connected successfully.
   * @param sockId: the identifier for the socket.
   */
  virtual void FDConnected(int sockId);

  /* FDData(int) is called when there's new data available on an
   * external file descriptor registered through IOManager.
   * @param sockId: the identifier for the socket/file descriptor.
   */
  virtual void FDData(int sockId);

  /* FDData(int) is called when there's new data available on a
   * socket registered through IOManager. The provided data has been
   * decoded in case SSL was enabled on the socket.
   * @param sockId: the identifier for the socket/file descriptor.
   * @param data: a pointer to the data buffer
   * @param len: the length of the data buffer
   */
  virtual void FDData(int sockId, char* data, unsigned int len);

  /* FDDisconnected is called when a socket registered through IOManager
   * has been disconnected by the remote end.
   * @param sockId: the identifier for the socket.
   */
  virtual void FDDisconnected(int sockId);

  /* FDFail is called when a socket registered through IOManager fails
   * to connect or bind.
   * @param sockId: the identifier for the socket.
   * @param error: the error message for the event.
   */
  virtual void FDFail(int sockId, const std::string& error);

  /* FDSSLSuccess is called when the SSL negotiation for a socket registered
   * through IOManager has completed.
   * @param sockId: the identifier for the socket.
   * @param cipher: The negotiated cipher.
   */
  virtual void FDSSLSuccess(int sockId, const std::string& cipher);

  /* FDSSLFail is called when the SSL negotiation for a socket registered
   * through IOManager has failed.
   * @param sockId: the identifier for the socket.
   */
  virtual void FDSSLFail(int sockId);

  /* FDSendComplete is called when a socket registered through IOManager
   * has finished sending data.
   * @param sockId: the identifier for the socket.
   */
  virtual void FDSendComplete(int sockId);

  /* AsyncTaskComplete is a callback for asynchronous tasks created
   * through the WorkManager.
   * @param type: the type of the task (for bookkeeping by the callee)
   * @param data: data for the task
   */
  virtual void asyncTaskComplete(int type, void* data);

  /* Same as above */
  virtual void asyncTaskComplete(int type, int data);

  /* Application messages are messages that are defined by the application.
   * This library only transfers them.
   * @param messageType: The type is defined by the application
   * @param messageData: Data is defined by the application
   */
  virtual void receivedApplicationMessage(int messageType, void* messageData);

  /* Used internally */
  virtual void workerReady();
  void bindWorkManager(WorkManager* workManager);

private:
  WorkManager* workmanager;
};

} // namespace Core
