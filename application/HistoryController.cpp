#include "application/HistoryController.hpp"

#include "infrastructure/storage/SqliteStore.hpp"

#include <algorithm>

namespace revaplayer::application {

HistoryController::HistoryController(
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
    QObject *parent)
    : QObject(parent)
    , store_(std::move(store))
{
}

HistoryController::~HistoryController() = default;

bool HistoryController::initialize()
{
    return store_ != nullptr && store_->initialize();
}

bool HistoryController::isReady() const
{
    return store_ != nullptr && store_->isInitialized();
}

QString HistoryController::lastError() const
{
    return store_ != nullptr ? store_->lastError() : QStringLiteral("History store is not configured.");
}

std::optional<revaplayer::infrastructure::storage::ResumeStateRecord>
HistoryController::resumeStateFor(const QString &source) const
{
    return store_ != nullptr ? store_->loadResumeState(source) : std::nullopt;
}

QVector<revaplayer::infrastructure::storage::PlaybackHistoryRecord>
HistoryController::recentHistory(const int limit) const
{
    return store_ != nullptr
        ? store_->loadPlaybackHistory(limit)
        : QVector<revaplayer::infrastructure::storage::PlaybackHistoryRecord> {};
}

void HistoryController::recordMediaOpened(const QString &source,
                                          const QString &title,
                                          const double durationSeconds,
                                          const bool historyEnabled)
{
    if (store_ == nullptr || source.isEmpty() || !historyEnabled) {
        return;
    }

    store_->savePlaybackHistory(source, title.trimmed(), 0.0, std::max(0.0, durationSeconds), false);
}

void HistoryController::savePlaybackProgress(const QString &source,
                                             const QString &title,
                                             const double positionSeconds,
                                             const double durationSeconds,
                                             const bool historyEnabled,
                                             const bool resumeEnabled)
{
    if (store_ == nullptr || source.isEmpty()) {
        return;
    }

    const double safePosition = std::max(0.0, positionSeconds);
    const double safeDuration = std::max(0.0, durationSeconds);

    if (resumeEnabled) {
        store_->saveResumeState(source, title.trimmed(), safePosition, safeDuration);
    } else {
        store_->clearResumeState(source);
    }

    if (historyEnabled) {
        store_->savePlaybackHistory(source, title.trimmed(), safePosition, safeDuration, false);
    }
}

void HistoryController::markPlaybackCompleted(const QString &source,
                                              const QString &title,
                                              const double durationSeconds,
                                              const bool historyEnabled,
                                              const bool resumeEnabled)
{
    if (store_ == nullptr || source.isEmpty()) {
        return;
    }

    const double safeDuration = std::max(0.0, durationSeconds);
    store_->clearResumeState(source);
    if (historyEnabled) {
        store_->savePlaybackHistory(source, title.trimmed(), safeDuration, safeDuration, true);
    } else if (!resumeEnabled) {
        store_->clearResumeState(source);
    }
}

bool HistoryController::clearHistory()
{
    return store_ != nullptr && store_->clearPlaybackHistory();
}

bool HistoryController::removeHistoryEntry(const QString &source)
{
    return store_ != nullptr && store_->removePlaybackHistoryEntry(source);
}

bool HistoryController::trimHistory(const int limit)
{
    return store_ != nullptr && store_->prunePlaybackHistory(limit);
}

}  // namespace revaplayer::application
