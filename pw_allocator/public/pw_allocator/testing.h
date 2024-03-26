// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.
#pragma once

#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/block_allocator.h"
#include "pw_allocator/buffer.h"
#include "pw_allocator/metrics.h"
#include "pw_allocator/tracking_allocator.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace internal {

struct RecordedParameters {
  size_t allocate_size = 0;
  void* deallocate_ptr = nullptr;
  size_t deallocate_size = 0;
  void* resize_ptr = nullptr;
  size_t resize_old_size = 0;
  size_t resize_new_size = 0;
};

/// Simple memory allocator for testing.
///
/// This allocator records the most recent parameters passed to the `Allocator`
/// interface methods, and returns them via accessors.
class AllocatorForTestImpl : public Allocator {
 public:
  AllocatorForTestImpl(Allocator& allocator, RecordedParameters& params)
      : Allocator(allocator.capabilities()),
        allocator_(allocator),
        params_(params) {}

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override;

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override;

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override;

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  Allocator& allocator_;
  RecordedParameters& params_;
};

}  // namespace internal
namespace test {

// A token that can be used in tests.
constexpr pw::tokenizer::Token kToken = PW_TOKENIZE_STRING("test");

/// This metrics struct enables all metrics for tests except those related to
/// `requested_bytes`, since `TrackingAllocator` adds additional overhead if the
/// `requested_bytes` are enabled.
struct TestMetrics {
  PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(peak_allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(cumulative_allocated_bytes);

  PW_ALLOCATOR_METRICS_ENABLE(num_allocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_deallocations);
  PW_ALLOCATOR_METRICS_ENABLE(num_resizes);
  PW_ALLOCATOR_METRICS_ENABLE(num_reallocations);

  PW_ALLOCATOR_METRICS_ENABLE(num_failures);
  PW_ALLOCATOR_METRICS_ENABLE(unfulfilled_bytes);
};

/// An `AllocatorForTest` that is automatically initialized on construction.
template <size_t kBufferSize, typename MetricsType = TestMetrics>
class AllocatorForTest : public Allocator {
 public:
  using AllocatorType = FirstFitBlockAllocator<uint32_t>;
  using BlockType = AllocatorType::BlockType;

  AllocatorForTest()
      : Allocator(AllocatorType::kCapabilities),
        recorder_(*allocator_, params_),
        tracker_(kToken, recorder_) {
    EXPECT_EQ(allocator_->Init(allocator_.as_bytes()), OkStatus());
  }

  ~AllocatorForTest() override {
    for (auto* block : allocator_->blocks()) {
      BlockType::Free(block);
    }
    allocator_->Reset();
  }

  const metric::Group& metric_group() const { return tracker_.metric_group(); }
  metric::Group& metric_group() { return tracker_.metric_group(); }

  const MetricsType& metrics() const { return tracker_.metrics(); }

  size_t allocate_size() const { return params_.allocate_size; }
  void* deallocate_ptr() const { return params_.deallocate_ptr; }
  size_t deallocate_size() const { return params_.deallocate_size; }
  void* resize_ptr() const { return params_.resize_ptr; }
  size_t resize_old_size() const { return params_.resize_old_size; }
  size_t resize_new_size() const { return params_.resize_new_size; }

  /// Resets the recorded parameters to an initial state.
  void ResetParameters() { params_ = internal::RecordedParameters{}; }

  /// Allocates all the memory from this object.
  void Exhaust() {
    for (auto* block : allocator_->blocks()) {
      block->MarkUsed();
    }
  }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override { return tracker_.Allocate(layout); }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    tracker_.Deallocate(ptr, layout);
  }

  /// @copydoc Allocator::Reallocate
  void* DoReallocate(void* ptr, Layout layout, size_t new_size) override {
    return tracker_.Reallocate(ptr, layout, new_size);
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    return tracker_.Resize(ptr, layout, new_size);
  }

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override {
    return tracker_.GetCapacity();
  }

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override {
    return tracker_.GetRequestedLayout(ptr);
  }

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override {
    return tracker_.GetUsableLayout(ptr);
  }

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override {
    return tracker_.GetAllocatedLayout(ptr);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    return tracker_.Query(ptr, layout);
  }

  WithBuffer<AllocatorType, kBufferSize> allocator_;
  internal::RecordedParameters params_;
  internal::AllocatorForTestImpl recorder_;
  TrackingAllocatorImpl<MetricsType> tracker_;
};

}  // namespace test
}  // namespace pw::allocator
