#pragma once

#include <cstdint>
#include <memory>

#include "umm/backend.hpp"

namespace umm::internal {

class ExifToolBackend final : public Backend {
 public:
  explicit ExifToolBackend(ExifToolConfig config);
  ~ExifToolBackend() override;

  std::string id() const override;
  BackendAvailability availability() const override;
  Result<BaseDocument> readBase(const std::filesystem::path& media) override;
  Result<void> writeBase(const std::filesystem::path& media,
                        const BaseChanges& changes) override;
  Result<void> typeCapabilities(std::string_view media_type) const override;

  void configure(ExifToolConfig config);
  std::uint64_t spawnCount() const { return spawn_count_; }
  void killChildForTest();

 private:
  void shutdown();
  Result<void> ensure_process();
  Result<std::string> execute(const std::string& command);
  void resolve() const;

  ExifToolConfig config_;
  mutable std::filesystem::path perl_;
  mutable std::filesystem::path script_;
  mutable bool native_exe_{false};
  mutable std::string absence_reason_;
  mutable std::string version_;
  std::unique_ptr<class ChildProcess> child_;
  std::uint64_t spawn_count_{0};
};

std::unique_ptr<Backend> make_exiftool_backend(ExifToolConfig config);

}  // namespace umm::internal
