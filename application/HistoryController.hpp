#pragma once

#include "infrastructure/storage/SqliteStore.hpp"

#include <QObject>

#include <memory>
#include <optional>

namespace revaplayer::infrastructure::storage {
class SqliteStore;
struct ResumeStateRecord;
}

namespace revaplayer::application {

class HistoryController final : public QObject {
    Q_OBJECT

public:
    explicit HistoryController(std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
                               QObject *parent = nullptr);
    ~HistoryController() override;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString lastError() const;

    [[nodiscard]] std::optional<revaplayer::infrastructure::storage::ResumeStateRecord>
    resumeStateFor(const QString &source) const;
    [[nodiscard]] QVector<revaplayer::infrastructure::storage::PlaybackHistoryRecord>
    recentHistory(int limit = 100) const;

    void recordMediaOpened(const QString &source,
                           const QString &title,
                           double durationSeconds,
                           bool historyEnabled = true);
    void savePlaybackProgress(const QString &source,
                              const QString &title,
                              double positionSeconds,
                              double durationSeconds,
                              bool historyEnabled = true,
                              bool resumeEnabled = true);
    void markPlaybackCompleted(const QString &source,
                               const QString &title,
                               double durationSeconds,
                               bool historyEnabled = true,
                               bool resumeEnabled = true);
    bool clearHistory();
    bool removeHistoryEntry(const QString &source);
    bool trimHistory(int limit);

private:
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store_;
};

}  // namespace revaplayer::application
