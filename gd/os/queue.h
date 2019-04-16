/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <unistd.h>
#include <functional>
#include <mutex>
#include <queue>

#include "os/handler.h"
#ifdef OS_LINUX_GENERIC
#include "os/linux_generic/reactive_semaphore.h"
#endif
#include "os/log.h"

namespace bluetooth {
namespace os {

template <typename T>
class Queue {
 public:
  // A function moving data from enqueue end buffer to queue, it will be continually be invoked until queue
  // is full. Enqueue end should make sure buffer isn't empty and UnregisterEnqueue when buffer become empty.
  using EnqueueCallback = std::function<std::unique_ptr<T>()>;
  // A function moving data form queue to dequeue end buffer, it will be continually be invoked until queue
  // is empty. TryDequeue should be use in this function to get data from queue.
  using DequeueCallback = std::function<void()>;
  // Create a queue with |capacity| is the maximum number of messages a queue can contain
  explicit Queue(size_t capacity);
  ~Queue();
  // Register |callback| that will be called on |handler| when the queue is able to enqueue one piece of data.
  // This will cause a crash if handler or callback has already been registered before.
  void RegisterEnqueue(Handler* handler, EnqueueCallback callback);
  // Unregister current EnqueueCallback from this queue, this will cause a crash if not registered yet.
  void UnregisterEnqueue();
  // Register |callback| that will be called on |handler| when the queue has at least one piece of data ready
  // for dequeue. This will cause a crash if handler or callback has already been registered before.
  void RegisterDequeue(Handler* handler, DequeueCallback callback);
  // Unregister current DequeueCallback from this queue, this will cause a crash if not registered yet.
  void UnregisterDequeue();

  // Try to dequeue an item from this queue. Return nullptr when there is nothing in the queue.
  std::unique_ptr<T> TryDequeue();

 private:
  void EnqueueCallbackInternal();
  // An internal queue that holds at most |capacity| pieces of data
  std::queue<std::unique_ptr<T>> queue_;
  // A mutex that guards data in this queue
  std::mutex mutex_;
  // Current dequeue callback
  EnqueueCallback enqueue_callback_;
  // Current enqueue callback
  DequeueCallback dequeue_callback_;

  class QueueEndpoint {
   public:
#ifdef OS_LINUX_GENERIC
    explicit QueueEndpoint(unsigned int initial_value)
        : reactive_semaphore_(initial_value), handler_(nullptr), reactable_(nullptr) {}
    ReactiveSemaphore reactive_semaphore_;
#endif
    Handler* handler_;
    Reactor::Reactable* reactable_;
  };

  QueueEndpoint enqueue_;
  QueueEndpoint dequeue_;
};

#ifdef OS_LINUX_GENERIC
#include "os/linux_generic/queue.tpp"
#endif

}  // namespace os
}  // namespace bluetooth
