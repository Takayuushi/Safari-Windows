#include "downloads.h"
#include <algorithm>

namespace safari {

DownloadsManager& DownloadsManager::instance() {
    static DownloadsManager s_mgr;
    return s_mgr;
}

void DownloadsManager::notify() {
    if (m_listener) {
        m_listener();
    }
}

uint64_t DownloadsManager::addDownload(const std::wstring& filename, const std::wstring& filePath, const std::wstring& url, int64_t totalBytes) {
    DownloadItem item;
    item.id = m_nextId++;
    item.filename = filename;
    item.filePath = filePath;
    item.url = url;
    item.totalBytes = totalBytes;
    item.receivedBytes = 0;
    item.state = DownloadState::InProgress;

    m_items.insert(m_items.begin(), item);
    notify();
    return item.id;
}

void DownloadsManager::updateProgress(uint64_t id, int64_t receivedBytes, int64_t totalBytes) {
    for (auto& item : m_items) {
        if (item.id == id) {
            item.receivedBytes = receivedBytes;
            if (totalBytes > 0) item.totalBytes = totalBytes;
            notify();
            break;
        }
    }
}

void DownloadsManager::markCompleted(uint64_t id) {
    for (auto& item : m_items) {
        if (item.id == id) {
            item.state = DownloadState::Completed;
            item.receivedBytes = item.totalBytes;
            notify();
            break;
        }
    }
}

void DownloadsManager::markFailed(uint64_t id) {
    for (auto& item : m_items) {
        if (item.id == id) {
            item.state = DownloadState::Failed;
            notify();
            break;
        }
    }
}

void DownloadsManager::clearCompleted() {
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(), [](const DownloadItem& item) {
        return item.state != DownloadState::InProgress;
    }), m_items.end());
    notify();
}

bool DownloadsManager::hasActiveDownloads() const {
    for (const auto& item : m_items) {
        if (item.state == DownloadState::InProgress) return true;
    }
    return false;
}

double DownloadsManager::overallProgress() const {
    int64_t total = 0;
    int64_t received = 0;
    for (const auto& item : m_items) {
        if (item.state == DownloadState::InProgress && item.totalBytes > 0) {
            total += item.totalBytes;
            received += item.receivedBytes;
        }
    }
    if (total <= 0) return 0.0;
    return (double)received / (double)total;
}

} // namespace safari
