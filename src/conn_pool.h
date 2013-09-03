//  Copyright (c) 2007-2008 Facebook
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// See accompanying file LICENSE or visit the Scribe site at:
// http://developers.facebook.com/scribe/
//
// @author Bobby Johnson
// @author James Wang
// @author Jason Sobel

#ifndef SCRIBE_CONN_POOL_H
#define SCRIBE_CONN_POOL_H

#include "common.h"

class SSLOptions;

/* return codes for ScribeConn and ConnPool */
#define CONN_FATAL        (-1) /* fatal error. close everything */
#define CONN_OK           (0)  /* success */
#define CONN_TRANSIENT    (1)  /* transient error */

// Number of times we re-opened the connection
#define NUMBER_OF_RECONNECTS "number of reconnections"

#define NEVER_RECONNECT   (-1)
#define NO_THRESHOLD      (-2)

// Basic scribe class to manage network connections. Used by network store

class scribeConn {
 public:
  scribeConn(const std::string& host, unsigned long port, int timeout,
      int msgThresholdBeforeReconnect, int allowableDeltaBeforeReconnect, boost::shared_ptr<SSLOptions> sslOptions);
  scribeConn(const std::string &service, const server_vector_t &servers, int timeout,
      int msgThresholdBeforeReconnect, int allowableDeltaBeforeReconnect);
  virtual ~scribeConn();

  void addRef();
  void releaseRef();
  unsigned getRef();
  void setRef(unsigned);

  void lock();
  void unlock();

  bool isOpen();
  bool open();
  void close();
  int send(boost::shared_ptr<logentry_vector_t> messages);

 private:
  std::string connectionString();
  void reopenConnectionIfNeeded();

 protected:
  boost::shared_ptr<apache::thrift::transport::TSocket> socket;
  boost::shared_ptr<apache::thrift::transport::TFramedTransport> framedTransport;
  boost::shared_ptr<apache::thrift::protocol::TBinaryProtocol> protocol;
  boost::shared_ptr<scribe::thrift::scribeClient> resendClient;

  unsigned refCount;

  bool serviceBased;
  boost::shared_ptr<SSLOptions> sslOptions;
  boost::shared_ptr<apache::thrift::transport::TSSLSocketFactory> sslSocketFactory;
  std::string serviceName;
  server_vector_t serverList;
  std::string remoteHost;
  unsigned long remotePort;
  long sentSinceLastReconnect;
  int timeout; // connection, send, and recv timeout
  pthread_mutex_t mutex;
  time_t lastHeartbeat;
  int msgThresholdBeforeReconnect;
  int allowableDeltaBeforeReconnect;
  int currThresholdBeforeReconnect;
  std::map<std::string, int> sendCounts; // Periodically logged for diagnostics
#ifdef USE_ZOOKEEPER
  std::string zkRegistrationZnode; // Where to autodiscover a remote scribe
#endif

};

// key is hostname:port or the service
typedef std::map<std::string, boost::shared_ptr<scribeConn> > conn_map_t;

// key is hostname:port or the service
typedef std::map<std::string, int> msg_threshold_map_t;

// Scribe class to manage connection pooling
// Maintains a map of (<host,port> or service) to scribeConn class.
// used to ensure that there is only one connection from one particular
// scribe server to any host,port or service.
// see the global g_connPool in store.cpp
class ConnPool {
 public:
  ConnPool();
  virtual ~ConnPool();

  bool open(const std::string& host, unsigned long port, int timeout, boost::shared_ptr<SSLOptions> sslOptions);
  bool open(const std::string &service, const server_vector_t &servers, int timeout);

  void close(const std::string& host, unsigned long port);
  void close(const std::string &service);

  int send(const std::string& host, unsigned long port,
            boost::shared_ptr<logentry_vector_t> messages);
  int send(const std::string &service,
            boost::shared_ptr<logentry_vector_t> messages);
  void mergeReconnectThresholds(msg_threshold_map_t *newMap,
      int newThreshold, int newDelta);
  static std::string makeKey(const std::string& name, unsigned long port);

 private:
  bool openCommon(const std::string &key, boost::shared_ptr<scribeConn> conn);
  void closeCommon(const std::string &key);
  int sendCommon(const std::string &key,
                  boost::shared_ptr<logentry_vector_t> messages);

 protected:
  pthread_mutex_t mapMutex;
  conn_map_t connMap;
  int defThresholdBeforeReconnect;
  int allowableDeltaBeforeReconnect;
  msg_threshold_map_t msgThresholdMap;
};

#endif // !defined SCRIBE_CONN_POOL_H
