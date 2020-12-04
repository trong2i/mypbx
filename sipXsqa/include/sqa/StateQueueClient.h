/*
 * Copyright (c) eZuce, Inc. All rights reserved.
 * Contributed to SIPfoundry under a Contributor Agreement
 *
 * This software is free software; you can redistribute it and/or modify it under
 * the terms of the Affero General Public License (AGPL) as published by the
 * Free Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 */

#ifndef StateQueueClient_H
#define	StateQueueClient_H

#include <cassert>
#include <zmq.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <os/OsLogger.h>
#include <os/OsServiceOptions.h>

#include "StateQueueMessage.h"
#include "BlockingQueue.h"

#define SQA_LINGER_TIME_MILLIS 5000
#define SQA_TERMINATE_STRING "__TERMINATE__"
#define SQA_CONN_MAX_READ_BUFF_SIZE 65536
#define SQA_CONN_READ_TIMEOUT 1000
#define SQA_CONN_WRITE_TIMEOUT 1000
#define SQA_KEY_MIN 22172
#define SQA_KEY_ALPHA 22180
#define SQA_KEY_DEFAULT SQA_KEY_MIN
#define SQA_KEY_MAX 22200
#define SQA_KEEP_ALIVE_TICKS 30

// Defines the interval, in seconds, to wait between keep alive loop calls
#define SQA_KEEP_ALIVE_LOOP_INTERVAL_SECS 1

class StateQueueClient : public boost::enable_shared_from_this<StateQueueClient>, private boost::noncopyable
{
public:

  enum Type
  {
    Publisher,
    Worker,
    Watcher
  };

  class BlockingTcpClient
  {
  public:
    typedef boost::shared_ptr<BlockingTcpClient> Ptr;

    class ConnectTimer
    {
    public:
      ConnectTimer(BlockingTcpClient* pOwner) :
        _pOwner(pOwner)
      {
        _pOwner->startConnectTimer();
      }

      ~ConnectTimer()
      {
        _pOwner->cancelConnectTimer();
      }
      BlockingTcpClient* _pOwner;
    };

    class ReadTimer
    {
    public:
      ReadTimer(BlockingTcpClient* pOwner) :
        _pOwner(pOwner)
      {
        _pOwner->startReadTimer();
      }

      ~ReadTimer()
      {
        _pOwner->cancelReadTimer();
      }
      BlockingTcpClient* _pOwner;
    };

    class WriteTimer
    {
    public:
      WriteTimer(BlockingTcpClient* pOwner) :
        _pOwner(pOwner)
      {
        _pOwner->startWriteTimer();
      }

      ~WriteTimer()
      {
        _pOwner->cancelWriteTimer();
      }
      BlockingTcpClient* _pOwner;
    };

    const std::string& className()
    {
      static const std::string className("StateQueueClient::BlockingTcpClient");

      return className;
    }

    BlockingTcpClient(
      boost::asio::io_service& ioService,
      int readTimeout = SQA_CONN_READ_TIMEOUT,
      int writeTimeout = SQA_CONN_WRITE_TIMEOUT,
      short key = SQA_KEY_DEFAULT) :
      _ioService(ioService),
      _resolver(_ioService),
      _pSocket(0),
      _isConnected(false),
      _readTimeout(readTimeout),
      _writeTimeout(writeTimeout),
      _key(key),
      _readTimer(_ioService),
      _writeTimer(_ioService),
      _connectTimer(_ioService)
    {
    }

    ~BlockingTcpClient()
    {
        if (_pSocket)
        {
            delete _pSocket;
            _pSocket = 0;
        }
    }

    void setReadTimeout(boost::asio::ip::tcp::socket& socket, int milliseconds)
    {
      struct timeval tv;
      tv.tv_sec  = 0;
      tv.tv_usec = milliseconds * 1000;
      setsockopt(socket.native(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void setWriteTimeout(boost::asio::ip::tcp::socket& socket, int milliseconds)
    {
      struct timeval tv;
      tv.tv_sec  = 0;
      tv.tv_usec = milliseconds * 1000;
      setsockopt(socket.native(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }

    void startReadTimer()
    {
      boost::system::error_code ec;
      _readTimer.expires_from_now(boost::posix_time::milliseconds(_readTimeout), ec);
      _readTimer.async_wait(boost::bind(&BlockingTcpClient::onReadTimeout, this, boost::asio::placeholders::error));
    }

    void startWriteTimer()
    {
      boost::system::error_code ec;
      _writeTimer.expires_from_now(boost::posix_time::milliseconds(_writeTimeout), ec);
      _writeTimer.async_wait(boost::bind(&BlockingTcpClient::onWriteTimeout, this, boost::asio::placeholders::error));
    }

    void startConnectTimer()
    {
      boost::system::error_code ec;
      _connectTimer.expires_from_now(boost::posix_time::milliseconds(_readTimeout), ec);
      _connectTimer.async_wait(boost::bind(&BlockingTcpClient::onConnectTimeout, this, boost::asio::placeholders::error));
    }

    void cancelReadTimer()
    {
      boost::system::error_code ec;
      _readTimer.cancel(ec);
    }

    void cancelWriteTimer()
    {
      boost::system::error_code ec;
      _writeTimer.cancel(ec);
    }

    void cancelConnectTimer()
    {
      boost::system::error_code ec;
      _connectTimer.cancel(ec);
    }

    void onReadTimeout(const boost::system::error_code& e)
    {
      if (e)
        return;
      close();
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "- " << _readTimeout << " milliseconds.");
    }


    void onWriteTimeout(const boost::system::error_code& e)
    {
      if (e)
        return;
      close();
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "- " << _writeTimeout << " milliseconds.");
    }

    void onConnectTimeout(const boost::system::error_code& e)
    {
      if (e)
        return;
      close();
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "- " << _readTimeout << " milliseconds.");
    }

    void close()
    {
      if (_pSocket)
      {
        boost::system::error_code ignored_ec;
       _pSocket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
       _pSocket->close(ignored_ec);
       _isConnected = false;
       OS_LOG_INFO(FAC_NET, CLASS_INFO() "- socket deleted.");
      }
    }

    bool timedWaitUntilDataAvailable(boost::function<void(const boost::system::error_code&)> onTimeoutCb,
                                      int timeoutMs,
                                      short int requestedEvents)
    {
      int error = 0;
      bool ret = false;
      int nativeSocket = _pSocket->native();

      struct pollfd fds[1] = {{nativeSocket, requestedEvents, 0}};


      int pollResult = poll(fds, sizeof(fds) / sizeof(fds[0]), timeoutMs);
      if (1 == pollResult)
      {
        if (fds[0].revents & POLLERR)
        {
          error = errno;
        }
        else if (fds[0].revents & requestedEvents)
        {
          ret = true;
        }
        else
        {
          OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                        << "unexpected return from poll(): pollResult = " << pollResult
                        << ", fds[0].revents =" << fds[0].revents);
        }
      }
      else if(0 == pollResult)
      { // timeout
        const boost::system::error_code e;

        onTimeoutCb(e);
        error = ETIMEDOUT;
      }
      else
      {
        error = errno;
      }

      if (0 != error)
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                      << "(" << nativeSocket << ", " << timeoutMs << " ms) error: " <<
                      error << "=" <<  strerror(error));
      }

      return ret;
    }

    bool timedWaitUntilReadDataAvailable()
    {
      // check for normal or out-of-band
      return timedWaitUntilDataAvailable(boost::bind(&BlockingTcpClient::onReadTimeout, this, _1),
                                          _readTimeout,
                                          POLLIN | POLLPRI);
    }

    bool timedWaitUntilWriteDataAvailable()
    {
      return timedWaitUntilDataAvailable(boost::bind(&BlockingTcpClient::onWriteTimeout, this, _1),
                                        _writeTimeout,
                                        POLLOUT);
    }

    bool connect(const std::string& serviceAddress, const std::string& servicePort)
    {
      //
      // Close the previous socket;
      //
      close();

      if (_pSocket)
      {
        delete _pSocket;
        _pSocket = 0;
      }

      _pSocket = new boost::asio::ip::tcp::socket(_ioService);

      OS_LOG_INFO(FAC_NET, CLASS_INFO() "creating new connection to " << serviceAddress << ":" << servicePort);

      _serviceAddress = serviceAddress;
      _servicePort = servicePort;

      try
      {
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), serviceAddress.c_str(), servicePort.c_str());
        boost::asio::ip::tcp::resolver::iterator hosts = _resolver.resolve(query);

        ConnectTimer timer(this);
        //////////////////////////////////////////////////////////////////////////
        // Only works in 1.47 version of asio.  1.46 doesnt have this utility func
        // boost::asio::connect(*_pSocket, hosts);
        _pSocket->connect(hosts->endpoint()); // so we use the connect member
        //////////////////////////////////////////////////////////////////////////
        _isConnected = true;
        OS_LOG_INFO(FAC_NET, CLASS_INFO() "creating new connection to " << serviceAddress << ":" << servicePort << " SUCESSFUL.");
      }
      catch(std::exception e)
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "failed with error " << e.what());
        _isConnected = false;
      }

      return _isConnected;
    }

    
    bool connect()
    {
      //
      // Initialize State Queue Agent Publisher if an address is provided
      //
      //if (_serviceAddress.empty() || _servicePort.empty())
         std::string sqaControlAddress;
        std::string sqaControlPort;
        std::ostringstream sqaconfig;
        sqaconfig << SIPX_CONFDIR << "/" << "sipxsqa-client.ini";
        OsServiceOptions configOptions(sqaconfig.str());
        std::string controlAddress;
        std::string controlPort;
    {
         if (configOptions.parseOptions())
        {
          bool enabled = false;
          if (configOptions.getOption("enabled", enabled, enabled) && enabled)
          {
            configOptions.getOption("sqa-control-address", _serviceAddress);
            configOptions.getOption("sqa-control-port", _servicePort);
          }
          else
          {
            OS_LOG_ERROR(FAC_NET, "BlockingTcpClient::connect() this:" << this << " Unable to read connection information from " << sqaconfig.str());
            return false;
          }
        }
      }

      if(_serviceAddress.empty() || _servicePort.empty())
      {
        OS_LOG_ERROR(FAC_NET, "BlockingTcpClient::connect() this:" << this << " remote address is not set");
        return false;
      }
      
      return connect(_serviceAddress, _servicePort);
    }

    bool send(const StateQueueMessage& request)
    {
      assert(_pSocket);
      std::string data = request.data();

      if (data.size() > SQA_CONN_MAX_READ_BUFF_SIZE - 1) /// Account for the terminating char "_"
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "data size: " << data.size() << " maximum buffer length of " << SQA_CONN_MAX_READ_BUFF_SIZE - 1);
        return false;
      }

      short version = 1;
      unsigned long len = (unsigned long)data.size() + 1; /// Account for the terminating char "_"
      std::stringstream strm;
      strm.write((char*)(&version), sizeof(version));
      strm.write((char*)(&_key), sizeof(_key));
      strm.write((char*)(&len), sizeof(len));
      strm << data << "_";
      std::string packet = strm.str();
      boost::system::error_code ec;
      bool ok = false;
      
      {
        if (false == timedWaitUntilWriteDataAvailable())
        {
          OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                      << "timedWaitUntilWriteDataAvailable failed: "
                      << "Unable to send request");

          _isConnected = false;
          return false;
        }

        //ok = boost::asio::write(*_pSocket, boost::asio::buffer(packet.c_str(), packet.size()),  boost::asio::transfer_all(), ec) > 0;
        ok = _pSocket->write_some(boost::asio::buffer(packet.c_str(), packet.size()), ec) > 0;
      }

      if (!ok || ec)
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "write_some error: " << ec.message());
        _isConnected = false;
        return false;
      }
      return true;
    }

    bool receive(StateQueueMessage& response)
    {
      assert(_pSocket);
      unsigned long len = getNextReadSize();
      if (!len)
      {
        OS_LOG_INFO(FAC_NET, CLASS_INFO() "next read size is empty.");
        return false;
      }

      char responseBuff[len];
      boost::system::error_code ec;
      {
        if (false == timedWaitUntilReadDataAvailable())
        {
          OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                      << "timedWaitUntilReadDataAvailable failed: "
                      << "Unable to receive response");

          _isConnected = false;
          return false;
        }

        _pSocket->read_some(boost::asio::buffer((char*)responseBuff, len), ec);
      }

      if (ec)
      {
        if (boost::asio::error::eof == ec)
        {
          OS_LOG_INFO(FAC_NET, CLASS_INFO() "remote closed the connection, read_some error: " << ec.message());
        }
        else
        {
          OS_LOG_ERROR(FAC_NET, CLASS_INFO() "read_some error: " << ec.message());
        }

        _isConnected = false;
        return false;
      }
      std::string responseData(responseBuff, len);
      return response.parseData(responseData);
    }

    bool sendAndReceive(const StateQueueMessage& request, StateQueueMessage& response)
    {
      if (send(request))
        return receive(response);
      return false;
    }

    unsigned long getNextReadSize()
    {
      short version = 1;
      bool hasVersion = false;
      bool hasKey = false;
      unsigned long remoteLen = 0;
      while (!hasVersion || !hasKey)
      {
        short remoteVersion;
        short remoteKey;

        //TODO: Refactor the code below to do one read for the three fields
        //
        // Read the version (must be 1)
        //
        while (true)
        {

          boost::system::error_code ec;
          if (false == timedWaitUntilReadDataAvailable())
          {
            OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                        << "timedWaitUntilReadDataAvailable failed: "
                        << "Unable to read version");

            _isConnected = false;
            return 0;
          }

          _pSocket->read_some(boost::asio::buffer((char*)&remoteVersion, sizeof(remoteVersion)), ec);
          if (ec)
          {
            if (boost::asio::error::eof == ec)
            {
              OS_LOG_ERROR(FAC_NET, CLASS_INFO() "remote closed the connection, read_some error: " << ec.message());
            }
            else
            {
              OS_LOG_INFO(FAC_NET, CLASS_INFO()
                  << "Unable to read version "
                  << "ERROR: " << ec.message());
        	  }

            _isConnected = false;
            return 0;
          }
          else
          {
            if (remoteVersion == version)
            {
              hasVersion = true;
              break;
            }
          }
        }

        while (true)
        {

          boost::system::error_code ec;
          if (false == timedWaitUntilReadDataAvailable())
          {
            OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                        << "timedWaitUntilReadDataAvailable failed: "
                        << "Unable to read secret key");

            _isConnected = false;
            return 0;
          }

          _pSocket->read_some(boost::asio::buffer((char*)&remoteKey, sizeof(remoteKey)), ec);
          if (ec)
          {
            if (boost::asio::error::eof == ec)
            {
              OS_LOG_ERROR(FAC_NET, CLASS_INFO() "remote closed the connection, read_some error: " << ec.message());
            }
            else
            {
              OS_LOG_INFO(FAC_NET, CLASS_INFO()
                  << "Unable to read secret key "
                  << "ERROR: " << ec.message());
            }

            _isConnected = false;
            return 0;
          }
          else
          {
            if (remoteKey >= SQA_KEY_MIN && remoteKey <= SQA_KEY_MAX)
            {
              hasKey = true;
              break;
            }
          }
        }
      }

      boost::system::error_code ec;
      if (false == timedWaitUntilReadDataAvailable())
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                    << "timedWaitUntilReadDataAvailable failed: "
                    << "Unable to read secret packet length");

        _isConnected = false;
        return 0;
      }

      _pSocket->read_some(boost::asio::buffer((char*)&remoteLen, sizeof(remoteLen)), ec);
      if (ec)
      {
        if (boost::asio::error::eof == ec)
        {
          OS_LOG_ERROR(FAC_NET, CLASS_INFO() "remote closed the connection, read_some error: " << ec.message());
        }
        else
        {
          OS_LOG_INFO(FAC_NET, CLASS_INFO()
              << "Unable to read secret packet length "
              << "ERROR: " << ec.message());
        }

        _isConnected = false;
        return 0;
      }

      return remoteLen;
    }

    bool isConnected() const
    {
      return _isConnected;
    }

    std::string getLocalAddress()
    {
      try
      {
        if (!_pSocket)
          return "";
        return _pSocket->local_endpoint().address().to_string();
      }
      catch(...)
      {
        return "";
      }
    }
  private:
    boost::asio::io_service& _ioService;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket *_pSocket;
    std::string _serviceAddress;
    std::string _servicePort;
    bool _isConnected;
    int _readTimeout;
    int _writeTimeout;
    short _key;
    boost::asio::deadline_timer _readTimer;
    boost::asio::deadline_timer _writeTimer;
    boost::asio::deadline_timer _connectTimer;
    friend class StateQueueClient;
  };

protected:
  typedef BlockingQueue<BlockingTcpClient::Ptr> ClientPool;
  typedef BlockingQueue<std::string> EventQueue;

  Type _type;
  boost::asio::io_service _ioService;
  boost::thread* _pIoServiceThread;
  boost::scoped_ptr<boost::thread> _pKeepAliveThread;
  int _sleepCount;
  std::size_t _poolSize;
  std::string _serviceAddress;
  std::string _servicePort;
  ClientPool _clientPool;
  bool _terminate;
  zmq::context_t* _zmqContext;
  zmq::socket_t* _zmqSocket;
  boost::thread* _pEventThread;
  std::string _zmqEventId;
  std::string _applicationId;
  EventQueue _eventQueue;
  std::vector<BlockingTcpClient::Ptr> _clientPointers;
  int _expires;
  int _subscriptionExpires;
  int _backoffCount;
  bool _refreshSignin;
  int _currentSigninTick;
  std::string _localAddress;
  int _keepAliveTicks;
  int _currentKeepAliveTicks;
  int _isAlive;

public:
  const std::string& className()
  {
    static const std::string className("StateQueueClient");

    return className;
  }

  StateQueueClient(
        Type type,
        const std::string& applicationId,
        const std::string& serviceAddress,
        const std::string& servicePort,
        const std::string& zmqEventId,
        std::size_t poolSize,
        int readTimeout = SQA_CONN_READ_TIMEOUT,
        int writeTimeout = SQA_CONN_WRITE_TIMEOUT,
        int keepAliveTicks = SQA_KEEP_ALIVE_TICKS
        ) :
    _type(type),
    _ioService(),
    _pIoServiceThread(0),
    _pKeepAliveThread(0),
    _sleepCount(0),
    _poolSize(poolSize),
    _serviceAddress(serviceAddress),
    _servicePort(servicePort),
    _clientPool(_poolSize),
    _terminate(false),
    _zmqContext(new zmq::context_t(1)),
    _zmqSocket(0),
    _pEventThread(0),
    _applicationId(applicationId),
    _eventQueue(1000),
    _expires(10),
    _subscriptionExpires(1800),
    _backoffCount(0),
    _refreshSignin(false),
    _currentSigninTick(-1),
    _keepAliveTicks(keepAliveTicks),
    _currentKeepAliveTicks(keepAliveTicks),
    _isAlive(true)
  {

      if (_type != Publisher)
      {
        createZmqSocket();
      }

      for (std::size_t i = 0; i < _poolSize; i++)
      {
        BlockingTcpClient* pClient = new BlockingTcpClient(_ioService, readTimeout, writeTimeout, i == 0 ? SQA_KEY_ALPHA : SQA_KEY_DEFAULT );
        pClient->connect(_serviceAddress, _servicePort);

        if (_localAddress.empty())
          _localAddress = pClient->getLocalAddress();

        BlockingTcpClient::Ptr client(pClient);
        _clientPointers.push_back(client);
        _clientPool.enqueue(client);
      }

      _pKeepAliveThread.reset(
          new boost::thread(
              boost::bind(&StateQueueClient::keepAliveThreadRun, this)));
      _pIoServiceThread = new boost::thread(boost::bind(&boost::asio::io_service::run, &_ioService));

      if (_type == Watcher)
        _zmqEventId = "sqw.";
      else
        _zmqEventId = "sqa.";

      _zmqEventId += zmqEventId;

      OS_LOG_INFO(FAC_NET, "StateQueueClient-" << applicationId <<  " for event " <<  _zmqEventId << " CREATED");

      if (_type != Publisher)
      {
        _pEventThread = new boost::thread(boost::bind(&StateQueueClient::eventLoop, this));
      }
      else
      {
        std::string publisherAddress;
        signin(publisherAddress);
      }
  }

  StateQueueClient(
        Type type,
        const std::string& applicationId,
        const std::string& zmqEventId,
        std::size_t poolSize,
        int readTimeout = SQA_CONN_READ_TIMEOUT,
        int writeTimeout = SQA_CONN_WRITE_TIMEOUT,
        int keepAliveTicks = SQA_KEEP_ALIVE_TICKS
        ) :
    _type(type),
    _ioService(),
    _pIoServiceThread(0),
    _pKeepAliveThread(0),
    _sleepCount(0),
    _poolSize(poolSize),
    _clientPool(_poolSize),
    _terminate(false),
    _zmqContext(new zmq::context_t(1)),
    _zmqSocket(0),
    _pEventThread(0),
    _applicationId(applicationId),
    _eventQueue(1000),
    _expires(10),
    _subscriptionExpires(1800),
    _backoffCount(0),
    _refreshSignin(false),
    _currentSigninTick(-1),
    _keepAliveTicks(keepAliveTicks),
    _currentKeepAliveTicks(keepAliveTicks),
    _isAlive(true)
  {
      if (_type != Publisher)
      {
        createZmqSocket();
      }

      for (std::size_t i = 0; i < _poolSize; i++)
      {
        BlockingTcpClient* pClient = new BlockingTcpClient(_ioService, readTimeout, writeTimeout, i == 0 ? SQA_KEY_ALPHA : SQA_KEY_DEFAULT);
        pClient->connect();

        if (_localAddress.empty())
          _localAddress = pClient->getLocalAddress();

        BlockingTcpClient::Ptr client(pClient);
        _clientPointers.push_back(client);
        _clientPool.enqueue(client);
      }

      setServiceAddressAndPort();

      _pKeepAliveThread.reset(
          new boost::thread(
              boost::bind(&StateQueueClient::keepAliveThreadRun, this)));
      _pIoServiceThread = new boost::thread(boost::bind(&boost::asio::io_service::run, &_ioService));

      if (_type == Watcher)
        _zmqEventId = "sqw.";
      else
        _zmqEventId = "sqa.";

      _zmqEventId += zmqEventId;

      OS_LOG_INFO(FAC_NET, "StateQueueClient-" << applicationId <<  " for event " <<  _zmqEventId << " CREATED");

      if (_type != Publisher)
      {
        _pEventThread = new boost::thread(boost::bind(&StateQueueClient::eventLoop, this));
      }
      else
      {
        std::string publisherAddress;
        signin(publisherAddress);
      }
  }

  ~StateQueueClient()
  {
    terminate();
  }

  void setServiceAddressAndPort()
  {
    if (_terminate)
    {
      return;
    }

    BlockingTcpClient::Ptr pClient = *_clientPointers.begin();
    _serviceAddress = pClient->_serviceAddress;
    _servicePort = pClient->_servicePort;
  }

  void createZmqSocket()
  {
    _zmqSocket = new zmq::socket_t(*_zmqContext,ZMQ_SUB);
    int linger = SQA_LINGER_TIME_MILLIS; // milliseconds
    //int recvTimeoutMs = SQA_CONN_READ_TIMEOUT;// milliseconds
    //int sendTimeoutMs = SQA_CONN_WRITE_TIMEOUT;// milliseconds
    _zmqSocket->setsockopt(ZMQ_LINGER, &linger, sizeof(int));
    // WARNING: at one test it worked only with this fix; later it worked also without
    // this fix
    //_zmqSocket->setsockopt(ZMQ_RCVTIMEO, &recvTimeoutMs, sizeof(int));
    //_zmqSocket->setsockopt(ZMQ_SNDTIMEO, &sendTimeoutMs, sizeof(int));
  }

  void destroyZmqSocket()
  {
    if (_zmqSocket)
    {
      // this function verifies if the socket was already closed and close it
      // only if it was not
      _zmqSocket->close();

      delete _zmqSocket;
      _zmqSocket = 0;
    }
  }

  const std::string& getLocalAddress()
  {
    return _localAddress;
  }

  void terminate()
  {
    if (_terminate || !_zmqContext)
      return;

    logout();

     _terminate = true;

    // WARNING: delete _zmqContext will call internally zmq_term
    // Context termination is performed in the following steps:
    //
    // 1. Any blocking operations currently in progress on sockets open
    //    within context shall return immediately with an error code of
    //    ETERM. With the exception of zmq_close(), any further operations on
    //    sockets open within context shall fail with an error code of ETERM.

    // 2. After interrupting all blocking calls, zmq_term() shall block until
    //    the following conditions are satisfied:

    //    o   All sockets open within context have been closed with
    //        zmq_close().

    //    o   For each socket within context, all messages sent by the
    //        application with zmq_send() have either been physically
    //        transferred to a network peer, or the socket's linger period
    //        set with the ZMQ_LINGER socket option has expired.

    // We need to have zmq_socket->close called from another thread otherwise
    // zmq_term will block

    delete _zmqContext;
    _zmqContext = 0;

    OS_LOG_INFO(FAC_NET, CLASS_INFO() "waiting for event thread to exit.");
    if (_pEventThread)
    {
      _pEventThread->join();
      delete _pEventThread;
      _pEventThread = 0;
    }

    destroyZmqSocket();

    if (_pIoServiceThread)
    {
      _pIoServiceThread->join();
      delete _pIoServiceThread;
      _pIoServiceThread = 0;
    }

    if (_pKeepAliveThread)
    {
      _pKeepAliveThread->join();
    }

    OS_LOG_INFO(FAC_NET, CLASS_INFO() "Ok");
  }

  void setExpires(int expires) { _expires = expires; }

  bool isConnected()
  {
    for (std::vector<BlockingTcpClient::Ptr>::const_iterator iter = _clientPointers.begin();
            iter != _clientPointers.end(); iter++)
    {
      BlockingTcpClient::Ptr pClient = *iter;
      if (pClient->isConnected())
        return true;
    }
    return false;
  }

  void keepAliveLoop()
  {
    if (!_terminate)
    {
      _sleepCount++;

      if (_sleepCount >= _currentKeepAliveTicks)
      {
        _sleepCount = 0;
      }

      if (_refreshSignin && (--_currentSigninTick == 0))
      {
        std::string publisherAddress;
        signin(publisherAddress);
        OS_LOG_INFO(FAC_NET, CLASS_INFO() "refreshed signin @ " << publisherAddress);
      }

      if (!_sleepCount)
      {
        //
        // send keep-alives
        //
        for (std::size_t i = 0; i < _poolSize; i++)
        {
          StateQueueMessage ping;
          StateQueueMessage pong;
          ping.setType(StateQueueMessage::Ping);
          ping.set("message-app-id", _applicationId.c_str());
          if (sendAndReceive(ping, pong))
          {
            if (pong.getType() == StateQueueMessage::Pong)
            {
              // reinitialize service address and service port for StateQueueClient class
              // they were only updated for BlockingTcpClient class
              setServiceAddressAndPort();

              OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "Keep-alive response received from " << _serviceAddress << ":" << _servicePort);
              //
              // Reset it back to the default value
              //
              _currentKeepAliveTicks = _keepAliveTicks;
              _isAlive = true;
            }
          }
          else
          {
            //
            // Reset the keep-alive to 1 so we attempt to reconnect every second
            //
            _currentKeepAliveTicks = 1;
            _isAlive = false;
          }
        }
      }
    }
  }

  void keepAliveThreadRun()
  // Runs the keep alive loop at 1 secs interval
  {
    OS_LOG_INFO(FAC_SIP, CLASS_INFO()
        << "starting");

    while (!_terminate)
    {
      // sleep the current thread
      boost::this_thread::sleep(boost::posix_time::seconds(SQA_KEEP_ALIVE_LOOP_INTERVAL_SECS));

      keepAliveLoop();

    }

    OS_LOG_INFO(FAC_SIP, CLASS_INFO()
        << "exiting");
  }

private:
  bool subscribe(const std::string& eventId, const std::string& sqaAddress)
  {
    assert(_type != Publisher);

    OS_LOG_INFO(FAC_NET, CLASS_INFO() "eventId=" << eventId << " address=" << sqaAddress);
    try
    {
      _zmqSocket->connect(sqaAddress.c_str());
      _zmqSocket->setsockopt(ZMQ_SUBSCRIBE, eventId.c_str(), eventId.size());

    }catch(std::exception e)
    {
      OS_LOG_INFO(FAC_NET, CLASS_INFO() "eventId=" << eventId << " address=" << sqaAddress << " FAILED!  Error: " << e.what());
      return false;
    }
    return true;
  }

  bool signin(std::string& publisherAddress)
  {
    std::ostringstream ss;
    ss << "sqa.signin."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));

    std::string id = ss.str();

    StateQueueMessage request;
    request.setType(StateQueueMessage::Signin);
    request.set("message-id", id.c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("subscription-expires", _subscriptionExpires);
    request.set("subscription-event", _zmqEventId.c_str());

    std::string clientType;
    if (_type == Publisher)
    {
      request.set("service-type", "publisher");
      clientType = "publisher";
    }
    else if (_type == Worker)
    {
      request.set("service-type", "worker");
      clientType = "worker";
    }
    else if (_type == Watcher)
    {
      request.set("service-type", "watcher");
      clientType = "watcher";
    }

    OS_LOG_NOTICE(FAC_NET, CLASS_INFO() "Type=" << clientType << " SIGNIN");


    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    bool ok = response.get("message-data", publisherAddress);

    if (ok)
    {
      _refreshSignin = true;
      _currentSigninTick = _subscriptionExpires * .75;
      OS_LOG_NOTICE(FAC_NET, CLASS_INFO()  "Type=" << clientType << " SQA=" << publisherAddress << " SUCCEEDED");
    }

    return ok;
  }

  bool logout()
  {
    std::ostringstream ss;
    ss << "sqa.logout."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));

    std::string id = ss.str();

    StateQueueMessage request;
    request.setType(StateQueueMessage::Logout);
    request.set("message-id", id.c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("subscription-event", _zmqEventId.c_str());

    std::string clientType;
    if (_type == Publisher)
    {
      request.set("service-type", "publisher");
      clientType = "publisher";
    }
    else if (_type == Worker)
    {
      request.set("service-type", "worker");
      clientType = "worker";
    }
    else if (_type == Watcher)
    {
      request.set("service-type", "watcher");
      clientType = "watcher";
    }
    
    StateQueueMessage response;
    return sendAndReceive(request, response);
  }

  void subscribeForEvents()
  {
    std::string publisherAddress;
    const int retryTime = 1000;

    while(!_terminate)
    {
      if (signin(publisherAddress))
      {
        break;
      }
      else
      {
        OS_LOG_WARNING(FAC_NET, CLASS_INFO()
                  << "Network Queue did no respond.  Retrying SIGN IN after " << retryTime << " ms.");
        boost::this_thread::sleep(boost::posix_time::milliseconds(retryTime));
      }
    }

    while (!_terminate)
    {
      if (subscribe(_zmqEventId, publisherAddress))
      {
        break;
      }
      else
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO()
            << "Is unable to SUBSCRIBE to SQA service @ " << publisherAddress);
        boost::this_thread::sleep(boost::posix_time::milliseconds(retryTime));
      }
    }
  }

  void eventLoop()
  {
    bool firstHit = true;

    subscribeForEvents();

    assert(_type != Publisher);

    while (!_terminate)
    {
      // WARNING: at one test it worked only with this fix; later it worked also without
      // this fix
//      if (false == _isAlive)
//      {
//        OS_LOG_INFO(FAC_NET, "StateQueueClient::eventLoop connection is not alive, closing socket, resubscribe");
//
//        destroyZmqSocket();
//        createZmqSocket();
//
//        subscribeForEvents();
//      }

      std::string id;
      std::string data;
      int count = 0;
      if (readEvent(id, data, count))
      {
        if (_terminate)
          break;

        OS_LOG_INFO(FAC_NET, CLASS_INFO() "received event: " << id);
        OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "received data: " << std::endl << data);

        if (_type == Worker)
        {
          OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "popping data: " << id);
          do_pop(firstHit, count, id, data);
        }else if (_type == Watcher)
        {
          OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "watching data: " << id);
          do_watch(firstHit, count, id, data);
        }
      }
      else
      {
        if (_terminate)
        {
          break;
        }
      }
      firstHit = false;
    }

    if (_type == Watcher)
    {
      do_watch(firstHit, 0, SQA_TERMINATE_STRING, SQA_TERMINATE_STRING);
    }
    else if (_type == Worker)
    {
      do_pop(firstHit, 0, SQA_TERMINATE_STRING, SQA_TERMINATE_STRING);
    }

    // WARNING: This should not be removed. zmq_term will block until all sockets are closed
    _zmqSocket->close();

    OS_LOG_INFO(FAC_NET, CLASS_INFO() "TERMINATED.");
  }

  void do_pop(bool firstHit, int count, const std::string& id, const std::string& data)
  {
    //
    // Check if we are the last succesful popper.
    // If count >= 2 we will skip the next message
    // after we have successfully popped
    //

    if (id.substr(0, 3) == "sqw")
    {
      OS_LOG_WARNING(FAC_NET, CLASS_INFO() "do_pop dropping event " << id);
      return;
    }

    if (id == "__TERMINATE__")
    {
      StateQueueMessage terminate;
      terminate.setType(StateQueueMessage::Pop);
      terminate.set("message-id", "__TERMINATE__");
      terminate.set("message-data", "__TERMINATE__");
      _eventQueue.enqueue(terminate.data());
      return;
    }

    if (!firstHit && count >= 2 && _backoffCount < count - 1 )
    {
      _backoffCount++; //  this will ensure that we participate next time
      boost::this_thread::yield();
    }
    //
    // Check if we are in the exlude list
    //
    if (data.find(_applicationId.c_str()) != std::string::npos)
    {
      //
      // We are still considered the last popper so don't toggle?
      //
      _backoffCount++;
      OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "do_pop is not allowed to pop " << id);
      return;
    }
    //
    // Pop it
    //
    StateQueueMessage pop;
    pop.setType(StateQueueMessage::Pop);
    pop.set("message-id", id.c_str());
    pop.set("message-app-id", _applicationId.c_str());
    pop.set("message-expires", _expires);

    OS_LOG_INFO(FAC_NET, CLASS_INFO() << _applicationId
              << " Popping event " << id);

    StateQueueMessage popResponse;
    if (!sendAndReceive(pop, popResponse))
    {
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "do_pop unable to send pop command for event " << id);
      _backoffCount++;
      return;
    }

    //
    // Check if Pop is successful
    //
    std::string messageResponse;
    popResponse.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      popResponse.get("message-error", messageResponseError);
      OS_LOG_DEBUG(FAC_NET, CLASS_INFO()
              << "Dropping event " << id
              << " Error: " << messageResponseError);
      _backoffCount++;
    }
    else
    {
      std::string messageId;
      popResponse.get("message-id", messageId);
      std::string messageData;
      popResponse.get("message-data", messageData);
      OS_LOG_DEBUG(FAC_NET, CLASS_INFO()
              << "Popped event " << messageId << " -- " << messageData);
      _eventQueue.enqueue(popResponse.data());
      _backoffCount = 0;
    }
  }
  
  void do_watch(bool firstHit, int count, const std::string& id, const std::string& data)
  {
    OS_LOG_DEBUG(FAC_NET, CLASS_INFO() "Received watcher data " << id);
    StateQueueMessage watcherData;
    watcherData.set("message-id", id);
    watcherData.set("message-data", data);
    _eventQueue.enqueue(watcherData.data());
  }

  bool readEvent(std::string& id, std::string& data, int& count)
  {
    assert(_type != Publisher);

    try
    {
      if (!zmq_receive(_zmqSocket, id))
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "0mq failed failed to receive ID segment.");
        return false;
      }

      std::string address;
      if (!zmq_receive(_zmqSocket, address))
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "0mq failed failed to receive ADDR segment.");
        return false;
      }

      //
      // Read the data vector
      //
      if (!zmq_receive(_zmqSocket, data))
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "0mq failed failed to receive DATA segment.");
        return false;
      }

      //
      // Read number of subscribers active
      //
      std::string strcount;
      if (!zmq_receive(_zmqSocket, strcount))
      {
        OS_LOG_ERROR(FAC_NET, CLASS_INFO() "0mq failed failed to receive COUNT segment.");
        return false;
      }

        count = boost::lexical_cast<int>(strcount);
    }
    catch(std::exception e)
    {
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "Unknown exception: " << e.what());
      return false;
    }
    return true;
  }

  static void zmq_free (void *data, void *hint)
  {
      free (data);
  }
  //  Convert string to 0MQ string and send to socket
  static bool zmq_send (zmq::socket_t & socket, const std::string & data)
  {
    char * buff = (char*)malloc(data.size());
    memcpy(buff, data.c_str(), data.size());
    zmq::message_t message((void*)buff, data.size(), zmq_free, 0);
    bool rc = socket.send(message);
    return (rc);
  }

  //  Sends string as 0MQ string, as multipart non-terminal
  static bool zmq_sendmore (zmq::socket_t & socket, const std::string & data)
  {
    char * buff = (char*)malloc(data.size());
    memcpy(buff, data.c_str(), data.size());
    zmq::message_t message((void*)buff, data.size(), zmq_free, 0);
    bool rc = socket.send(message, ZMQ_SNDMORE);
    return (rc);
  }

  static bool zmq_receive (zmq::socket_t *socket, std::string& value)
  {
      zmq::message_t message;
      socket->recv(&message);
      if (!message.size())
        return false;
      value = std::string(static_cast<char*>(message.data()), message.size());
      return true;
  }

  bool sendNoResponse(const StateQueueMessage& request)
  {
    if (!_isAlive)
    {
      OS_LOG_WARNING(FAC_NET, CLASS_INFO() "Connection is not alive.");
      return false;
    }

    BlockingTcpClient::Ptr conn;
    if (!_clientPool.dequeue(conn))
    {
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "Unable to retrieve a TCP connection for pool.");
      return false;
    }

    if (!conn->isConnected() && !conn->connect(_serviceAddress, _servicePort))
    {
      //
      // Put it back to the queue.  The server is down.
      //
      _clientPool.enqueue(conn);
      return false;
    }

    bool sent = conn->send(request);
    _clientPool.enqueue(conn);
    return sent;
  }

  bool sendAndReceive(const StateQueueMessage& request, StateQueueMessage& response)
  {

    if (!_isAlive && request.getType() != StateQueueMessage::Ping)
    {
      OS_LOG_WARNING(FAC_NET, CLASS_INFO() "Connection is not alive.");
      //
      // Only allow ping requests to get through when connection is not alive
      //
      return false;
    }

    BlockingTcpClient::Ptr conn;
    if (!_clientPool.dequeue(conn))
    {
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "Unable to retrieve a TCP connection for pool.");
      return false;
    }

    if (!conn->isConnected() && !conn->connect())
    {
      //
      // Put it back to the queue.  The server is down.
      //
      _clientPool.enqueue(conn);
      return false;
    }

    bool sent = conn->sendAndReceive(request, response);
    _clientPool.enqueue(conn);
    return sent;
  }

  bool pop(StateQueueMessage& ev)
  {
    std::string data;
    if (_eventQueue.dequeue(data))
    {
      ev.parseData(data);
      return true;
    }
    return false;
  }

  bool enqueue(const std::string& id, const std::string& data, int expires = 30, bool publish= false)
  {
    //
    // Enqueue it
    //
    StateQueueMessage enqueueRequest;
    if (!publish)
      enqueueRequest.setType(StateQueueMessage::Enqueue);
    else
      enqueueRequest.setType(StateQueueMessage::EnqueueAndPublish);
    enqueueRequest.set("message-id", id.c_str());
    enqueueRequest.set("message-app-id", _applicationId.c_str());
    enqueueRequest.set("message-expires", _expires);
    enqueueRequest.set("message-data", data);

    StateQueueMessage enqueueResponse;
    if (!sendAndReceive(enqueueRequest, enqueueResponse))
    {
      OS_LOG_ERROR(FAC_NET, CLASS_INFO() "FAILED");
      return false;
    }

    //
    // Check if Queue is successful
    //
    std::string messageResponse;
    enqueueResponse.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      enqueueResponse.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << id
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }

  bool internal_publish(const std::string& id, const std::string& data, bool noresponse = false)
  {
    //
    // Enqueue it
    //
    StateQueueMessage enqueueRequest;
    enqueueRequest.setType(StateQueueMessage::Publish);
    enqueueRequest.set("message-id", id.c_str());
    enqueueRequest.set("message-app-id", _applicationId.c_str());
    enqueueRequest.set("message-data", data);


    OS_LOG_INFO(FAC_NET, CLASS_INFO() << "publishing data ID=" << id);

    if (noresponse)
    {
      enqueueRequest.set("noresponse", noresponse);
      return sendNoResponse(enqueueRequest);
    }
    else
    {
      StateQueueMessage enqueueResponse;
      if (!sendAndReceive(enqueueRequest, enqueueResponse))
        return false;

      //
      // Check if Queue is successful
      //
      std::string messageResponse;
      enqueueResponse.get("message-response", messageResponse);
      if (messageResponse != "ok")
      {
        std::string messageResponseError;
        enqueueResponse.get("message-error", messageResponseError);
        OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                    << "Failed to publish " << id
                    << " Error: " << messageResponseError);
        return false;
      }

      return true;
    }
  }

  bool internal_publish_and_persist(int workspace, const std::string& id, const std::string& data, int expires)
  {
    StateQueueMessage enqueueRequest;
    enqueueRequest.setType(StateQueueMessage::PublishAndPersist);
    enqueueRequest.set("message-id", id.c_str());
    enqueueRequest.set("message-data-id", id.c_str());
    enqueueRequest.set("message-app-id", _applicationId.c_str());
    enqueueRequest.set("message-data", data);
    enqueueRequest.set("message-expires", expires);
    enqueueRequest.set("workspace", workspace);

    OS_LOG_INFO(FAC_NET, CLASS_INFO() "publishing data ID=" << id);

    StateQueueMessage enqueueResponse;
    if (!sendAndReceive(enqueueRequest, enqueueResponse))
      return false;

    //
    // Check if Queue is successful
    //
    std::string messageResponse;
    enqueueResponse.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      enqueueResponse.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to publish " << id
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }


public:
  
  bool pop(std::string& id, std::string& data)
  {
    StateQueueMessage message;
    if (!pop(message))
      return false;
    return message.get("message-id", id) && message.get("message-data", data);
  }

  bool watch(std::string& id, std::string& data)
  {
    OS_LOG_INFO(FAC_NET, CLASS_INFO() "(" << id << ") INVOKED" );
    StateQueueMessage message;
    if (!pop(message))
      return false;
    return message.get("message-id", id) && message.get("message-data", data);
  }

  bool enqueue(const std::string& data, int expires = 30, bool publish = false)
  {
    if (_type != Publisher)
      return false;

    std::ostringstream ss;
    ss << _zmqEventId << "."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    return enqueue(ss.str(), data, expires, publish);
  }


  bool publish(const std::string& eventId, const std::string& data, bool noresponse)
  {
    if (_type != Publisher)
      return false;

    std::ostringstream ss;
    ss << "sqw." << eventId << "."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    return internal_publish(ss.str(), data, noresponse);
  }

  bool publish(const std::string& eventId, const char* data, int dataLength, bool noresponse)
  {
    std::string buff = std::string(data, dataLength);
    return publish(eventId, data, dataLength);
  }

  bool publishAndPersist(int workspace, const std::string& eventId, const std::string& data, int expires)
  {
    if (_type != Publisher)
      return false;

    std::ostringstream ss;
    ss << "sqw." << eventId;
    
    return internal_publish_and_persist(workspace, ss.str(), data, expires);
  }


  bool erase(const std::string& id)
  {
    StateQueueMessage request;
    request.setType(StateQueueMessage::Erase);
    request.set("message-id", id.c_str());
    request.set("message-app-id", _applicationId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to erase " << id
                  << " Error: " << messageResponseError);
      return false;
    }
    OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Successfully erased " << id);
    return true;
  }

  bool persist(int workspace, const std::string& dataId, int expires)
  {
    std::ostringstream ss;
    ss << "sqa.persist."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::Persist);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("message-expires", _expires);
    request.set("workspace", workspace);
    request.set("message-data-id", dataId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }

  bool set(int workspace, const std::string& dataId, const std::string& data, int expires)
  {
    std::ostringstream ss;
    ss << "sqa.set."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));

    StateQueueMessage request;
    request.setType(StateQueueMessage::Set);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("message-expires", _expires);
    request.set("workspace", workspace);
    request.set("message-data-id", dataId.c_str());
    request.set("message-data", data);

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }

  bool mset(int workspace, const std::string& mapId, const std::string& dataId, const std::string& data, int expires)
  {
    std::ostringstream ss;
    ss << "sqa.mset."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));

    StateQueueMessage request;
    request.setType(StateQueueMessage::MapSet);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("message-expires", _expires);
    request.set("workspace", workspace);
    request.set("message-map-id", mapId.c_str());
    request.set("message-data-id", dataId.c_str());
    request.set("message-data", data);

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }

  bool get(int workspace, const std::string& dataId, std::string& data)
  {
    std::ostringstream ss;
    ss << "sqa.get."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::Get);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("workspace", workspace);
    request.set("message-data-id", dataId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return response.get("message-data", data);
  }

  bool mget(int workspace, const std::string& mapId, const std::string& dataId, std::string& data)
  {
    std::ostringstream ss;
    ss << "sqa.get."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::MapGet);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("workspace", workspace);
    request.set("message-map-id", mapId.c_str());
    request.set("message-data-id", dataId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return response.get("message-data", data);
  }

  bool mgetm(int workspace, const std::string& mapId, std::map<std::string, std::string>& smap)
  {
    std::ostringstream ss;
    ss << "sqa.get."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::MapGetMultiple);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("workspace", workspace);
    request.set("message-map-id", mapId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << mapId
                  << " Error: " << messageResponseError);
      return false;
    }

    std::string data;
    response.get("message-data", data);

    StateQueueMessage message;
    if (!message.parseData(data))
      return false;

    return message.getMap(smap);
  }

  bool mgeti(int workspace, const std::string& mapId, const std::string& dataId, std::string& data)
  {
    std::ostringstream ss;
    ss << "sqa.get."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::MapGetInc);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("workspace", workspace);
    request.set("message-map-id", mapId.c_str());
    request.set("message-data-id", dataId.c_str());

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return response.get("message-data", data);
  }

  bool remove(int workspace, const std::string& dataId)
  {
    std::ostringstream ss;
    ss << "sqa.remove."
       << std::hex << std::uppercase
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0)) << "-"
       << std::setw(4) << std::setfill('0') << (int) ((float) (0x10000) * random () / (RAND_MAX + 1.0));
    //
    // Enqueue it
    //
    StateQueueMessage request;
    request.setType(StateQueueMessage::Remove);
    request.set("message-id", ss.str().c_str());
    request.set("message-app-id", _applicationId.c_str());
    request.set("message-data-id", dataId.c_str());
    request.set("workspace", workspace);

    StateQueueMessage response;
    if (!sendAndReceive(request, response))
      return false;

    std::string messageResponse;
    response.get("message-response", messageResponse);
    if (messageResponse != "ok")
    {
      std::string messageResponseError;
      response.get("message-error", messageResponseError);
      OS_LOG_ERROR(FAC_NET, CLASS_INFO()
                  << "Failed to enqueue " << dataId
                  << " Error: " << messageResponseError);
      return false;
    }

    return true;
  }
};


#endif	/* StateQueueClient_H */

