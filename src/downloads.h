#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace safari {

enum class DownloadState {
    InProgress,
    Paused,
    Completed,
    Failed,
    Cancelled
};

struct DownloadItem {
    uint64_t id = 0;
    std::wstring filename;
    std::wstring filePath;
    std::wstring url;
    int64_t totalBytes = 0;
    int64_t receivedBytes = 0;
    DownloadState state = DownloadState::InProgress;

    double progress() const {
        if (totalBytes <= 0) return 0.0;
        return (double)receivedBytes / (double)totalBytes;
    }
};

class DownloadsManager {
public:
    static DownloadsManager& instance();

    const std::vector<DownloadItem>& items() const { return m_items; }

    uint64_t addDownload(uint64_t id, const std::wstring& filename, const std::wstring& filePath, const std::wstring& url, int64_t totalBytes);
    void updateProgress(uint64_t id, int64_t chunkBytes, int64_t totalBytes);
    void markCompleted(uint64_t id);
    void markFailed(uint64_t id);
    void cancelDownload(uint64_t id);
    void pauseDownload(uint64_t id);
    void resumeDownload(uint64_t id);
    void removeDownload(uint64_t id);
    void clearCompleted();

    bool hasActiveDownloads() const;
    double overallProgress() const;

    void setChangeListener(std::function<void()> listener) { m_listener = std::move(listener); }

private:
    DownloadsManager() = default;
    std::vector<DownloadItem> m_items;
    uint64_t m_nextId = 1;
    std::function<void()> m_listener;

    void notify();
};

} // namespace safari
