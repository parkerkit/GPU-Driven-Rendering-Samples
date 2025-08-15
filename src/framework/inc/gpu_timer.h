#pragma once

#include <glad/gl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace fw {
/// @brief 使用时间戳查询测量 GPU 区间，延迟读取结果，不主动等待 GPU。
/// @details 在当前 OpenGL Context 所在线程创建、调用和销毁；Begin/End 必须成对调用。
/// 多个计时器可以嵌套使用，区间可能包含 GPU 等待和 CPU 提交间隙。
class GpuTimer final {
public:
  GpuTimer();
  ~GpuTimer() noexcept;

  GpuTimer(const GpuTimer&) = delete;
  GpuTimer& operator=(const GpuTimer&) = delete;
  GpuTimer(GpuTimer&&) = delete;
  GpuTimer& operator=(GpuTimer&&) = delete;

  /// @brief 收集已就绪的旧结果并开始新区间；槽位占满时跳过本次采样。
  void Begin() noexcept;
  void End() noexcept;

  /// @brief 返回最近完成的区间耗时，单位为毫秒；尚无结果时返回空值。
  [[nodiscard]] std::optional<double> GetElapsedMilliseconds() const noexcept;

  /// @brief 取出自上次取样后已完成区间的平均耗时，并清空累计值；无新样本时返回空值。
  [[nodiscard]] std::optional<double> TakeAverageMilliseconds() noexcept;
  /// @brief 测量条件改变后重新累计；重置前尚未完成的查询也不会计入新平均值。
  /// @details 在 Begin/End 区间之外调用，不提前复用仍在等待 GPU 的查询槽位。
  void ResetSamples() noexcept;

private:
  struct QuerySlotState {
    std::array<GLuint, 2> queries{};
    std::uint64_t sequence{};
    bool is_pending{false};
  };

  static constexpr std::size_t k_slot_count{4};

  void CollectResults() noexcept;
  void ReleaseQueries() noexcept;

  std::array<QuerySlotState, k_slot_count> m_slots{};
  std::size_t m_next_slot{};
  std::size_t m_active_slot{k_slot_count};
  std::uint64_t m_sequence{};
  std::uint64_t m_completed_sequence{};
  std::uint64_t m_discard_through_sequence{};
  std::optional<double> m_elapsed_ms{};
  double m_sample_sum_ms{};
  std::size_t m_sample_count{};
};
}  // namespace fw
