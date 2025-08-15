#include "gpu_timer.h"

#include <cassert>
#include <stdexcept>

namespace fw {
GpuTimer::GpuTimer() {
  for (QuerySlotState& slot : m_slots) {
    glGenQueries(2, slot.queries.data());
    if (slot.queries[0] == 0 || slot.queries[1] == 0) {
      ReleaseQueries();
      throw std::runtime_error{"Failed to create GPU timer queries."};
    }
  }
}

GpuTimer::~GpuTimer() noexcept { ReleaseQueries(); }

void GpuTimer::ReleaseQueries() noexcept {
  for (QuerySlotState& slot : m_slots) {
    glDeleteQueries(2, slot.queries.data());
    slot.queries = {};
  }
}

void GpuTimer::CollectResults() noexcept {
  for (QuerySlotState& slot : m_slots) {
    if (!slot.is_pending) {
      continue;
    }
    GLint is_available{};
    glGetQueryObjectiv(slot.queries[1], GL_QUERY_RESULT_AVAILABLE, &is_available);
    if (is_available == GL_FALSE) {
      continue;
    }
    // 结束时间戳就绪后，同一区间的开始时间戳也已完成。
    GLuint64 begin_ns{};
    GLuint64 end_ns{};
    glGetQueryObjectui64v(slot.queries[0], GL_QUERY_RESULT, &begin_ns);
    glGetQueryObjectui64v(slot.queries[1], GL_QUERY_RESULT, &end_ns);
    if (slot.sequence > m_discard_through_sequence) {
      const double milliseconds{static_cast<double>(end_ns - begin_ns) / 1000000.0};
      // 每个查询完成后仅累计一次，不能反复使用界面上一份显示结果。
      m_sample_sum_ms += milliseconds;
      ++m_sample_count;
      if (slot.sequence > m_completed_sequence) {
        m_elapsed_ms = milliseconds;
        m_completed_sequence = slot.sequence;
      }
    }
    slot.is_pending = false;
  }
}

void GpuTimer::Begin() noexcept {
  assert(m_active_slot == k_slot_count);
  CollectResults();
  QuerySlotState& slot{m_slots[m_next_slot]};
  if (slot.is_pending) {
    return;
  }
  m_active_slot = m_next_slot;
  slot.sequence = ++m_sequence;
  glQueryCounter(slot.queries[0], GL_TIMESTAMP);
  m_next_slot = (m_next_slot + 1) % k_slot_count;
}

void GpuTimer::End() noexcept {
  if (m_active_slot == k_slot_count) {
    return;
  }
  QuerySlotState& slot{m_slots[m_active_slot]};
  glQueryCounter(slot.queries[1], GL_TIMESTAMP);
  slot.is_pending = true;
  m_active_slot = k_slot_count;
}

std::optional<double> GpuTimer::GetElapsedMilliseconds() const noexcept { return m_elapsed_ms; }

std::optional<double> GpuTimer::TakeAverageMilliseconds() noexcept {
  CollectResults();
  if (m_sample_count == 0) {
    return std::nullopt;
  }
  const double average_ms{m_sample_sum_ms / static_cast<double>(m_sample_count)};
  m_sample_sum_ms = 0.0;
  m_sample_count = 0;
  return average_ms;
}

void GpuTimer::ResetSamples() noexcept {
  assert(m_active_slot == k_slot_count);
  m_discard_through_sequence = m_sequence;
  m_elapsed_ms.reset();
  m_sample_sum_ms = 0.0;
  m_sample_count = 0;
}
}  // namespace fw
