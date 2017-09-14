/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdio.h>
#include <random>

#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/nullsocketserver.h"
#include "webrtc/rtc_base/ptr_util.h"
#include "webrtc/rtc_base/thread.h"
#include "webrtc/test/gtest.h"

namespace rtc {

namespace {

#if defined(MEMORY_SANITIZER)
void UseOfUninitializedValue() {
  int* buf = new int[2];
  std::random_device engine;
  if (buf[engine() % 2]) {  // Non-deterministic conditional.
    printf("Externally visible action.");
  }
  delete[] buf;
}

TEST(SanitizersDeathTest, MemorySanitizer) {
  EXPECT_DEATH(UseOfUninitializedValue(), "use-of-uninitialized-value");
}
#endif

#if defined(ADDRESS_SANITIZER)
void HeapUseAfterFree() {
  char *buf = new char[2];
  delete[] buf;
  buf[0] = buf[1];
}

TEST(SanitizersDeathTest, AddressSanitizer) {
  EXPECT_DEATH(HeapUseAfterFree(), "heap-use-after-free");
}
#endif

#if defined(UNDEFINED_SANITIZER)
// For ubsan:
void SignedIntegerOverflow() {
  int32_t x = 1234567890;
  x *= 2;
}

// For ubsan_vptr:
struct Base {
  virtual void f() {}
  virtual ~Base() {}
};
struct Derived : public Base {
};

void InvalidVptr() {
  Base b;
  auto* d = static_cast<Derived*>(&b);  // Bad downcast.
  d->f();  // Virtual function call with object of wrong dynamic type.
}

TEST(SanitizersDeathTest, UndefinedSanitizer) {
  EXPECT_DEATH({ SignedIntegerOverflow(); InvalidVptr(); }, "runtime error");
}
#endif

#if defined(THREAD_SANITIZER)
class IncrementThread : public Thread {
 public:
  explicit IncrementThread(int* value)
      : Thread(rtc::MakeUnique<NullSocketServer>()),
        value_(value) {}

  void Run() override {
    ++*value_;
    Thread::Current()->SleepMs(100);
  }

  // Un-protect Thread::Join for the test.
  void Join() {
    Thread::Join();
  }

 private:
  int* value_;

  RTC_DISALLOW_COPY_AND_ASSIGN(IncrementThread);
};

void DataRace() {
  int value = 0;
  IncrementThread thread1(&value);
  IncrementThread thread2(&value);
  thread1.Start();
  thread2.Start();
  thread1.Join();
  thread2.Join();
  // TSan seems to mess with gtest's death detection.
  // Fail intentionally, and rely on detecting the error message.
  RTC_CHECK(false);
}

TEST(SanitizersDeathTest, ThreadSanitizer) {
  EXPECT_DEATH(DataRace(), "data race");
}
#endif

}  // namespace

}  // namespace rtc