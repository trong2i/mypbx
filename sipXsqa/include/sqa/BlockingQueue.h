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

#ifndef BLOCKINGQUEUE_H
#define	BLOCKINGQUEUE_H

#include <queue>
#include <cassert>
#include <boost/thread.hpp>
#include <boost/noncopyable.hpp>
#include "Semaphore.h"

template <typename T>
class BlockingQueue : boost::noncopyable
{
protected:
  std::queue<T> _queue;
  Semaphore _semaphore;
  std::size_t _maxQueueSize;
  typedef boost::recursive_mutex mutex;
  typedef boost::lock_guard<mutex> mutex_lock;
  mutex _mutex;
  bool _terminating;
public:
  BlockingQueue(std::size_t maxQueueSize) :
    _maxQueueSize(maxQueueSize),
    _terminating(false)
  {
  }

  ~BlockingQueue()
  {
    terminate();
  }

  void terminate()
  {
    //
    // Unblock dequeue
    //
    mutex_lock lock(_mutex);
    _terminating = true;
    _semaphore.signal();
  }

  bool dequeue(T& data)
  {
    _semaphore.wait();
    mutex_lock lock(_mutex);
    if (_queue.empty() || _terminating)
      return false;
    data = _queue.front();
    _queue.pop();
    return true;
  }
  
  bool dequeueAll(std::vector<T>& dataVec)
  {
    mutex_lock lock(_mutex);
    if (_queue.empty() || _terminating)
      return false;

    while (!_queue.empty())
    {
      dataVec.push_back(_queue.front());
      _queue.pop();
    }

    return true;
  }

  bool enqueue(const T& data)
  {
    _mutex.lock();
    if (_maxQueueSize && _queue.size() > _maxQueueSize)
    {
      _mutex.unlock();
      return false;
    }
    _queue.push(data);
    _mutex.unlock();
    _semaphore.signal();

    return true;
  }

  void clear()
  {
    std::queue<T> empty;

    mutex_lock lock(_mutex);
    std::swap(_queue, empty);
  }

};

#endif	/* BLOCKINGQUEUE_H */

