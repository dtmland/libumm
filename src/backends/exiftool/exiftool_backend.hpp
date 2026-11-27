#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>

#include <span>

#include "umm/backend.hpp"

namespace umm::internal {

class ChildProcess;

class ExifToolBackend final : public Backend {
 public:
  explicit ExifToolBackend(ExifToolConfig config);
  ~ExifToolBackend() override;

  std::string id() const override;
  BackendAvailability availability() const override;
  Result<BaseDocument> readBase(const std::filesystem::path& media) override;
  Result<BaseDocument> readBase(std::span<const std::byte> bytes,
                                std::string_view file_type) override;
  Result<std::vector<Result<BaseDocument>>> readBaseMany(
      std::span<const std::filesystem::path> paths) override;
  Result<void> writeBase(const std::filesystem::path& media,
                        const BaseChanges& changes) override;
  Result<void> typeCapabilities(std::string_view media_type) const override;

  void configure(ExifToolConfig config);
  std::uint64_t spawnCount() const;
  std::size_t liveProcessCount() const;
  void killChildForTest();

 private:
  struct Slot {
    std::unique_ptr<ChildProcess> child;
    ExifToolConfig config;
    std::filesystem::path perl;
    std::filesystem::path script;
    bool native_exe = false;
    std::string absence_reason;
    bool busy = false;
    bool retire = false;
    std::atomic<bool> spawned{false};
  };

  Slot* acquire();
  void release(Slot* slot);
  Result<void> ensure_process(Slot& slot);
  Result<std::string> execute(const std::string& command);
  void shutdown_slot(Slot& slot);
  void resolve_slot(Slot& slot);
  std::string resolve() const;
  int process_limit() const;
  void shutdown_all();

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::list<std::unique_ptr<Slot>> slots_;
  ExifToolConfig config_;
  mutable std::filesystem::path perl_;
  mutable std::filesystem::path script_;
  mutable bool native_exe_{false};
  mutable std::string absence_reason_;
  mutable std::string version_;
  std::atomic<std::uint64_t> spawn_count_{0};
};

std::unique_ptr<Backend> make_exiftool_backend(ExifToolConfig config);

}  // namespace umm::internal
