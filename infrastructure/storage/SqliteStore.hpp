#pragma once

#include "domain/Bookmark.hpp"
#include "domain/CustomCommand.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace revaplayer::infrastructure::storage {

struct WindowStateRecord {
    QByteArray geometry;
    QByteArray state;
    bool maximized {false};
    bool fullscreen {false};
};

struct ResumeStateRecord {
    QString source;
    QString title;
    double positionSeconds {0.0};
    double durationSeconds {0.0};
    QString updatedAt;
};

struct PlaybackHistoryRecord {
    QString source;
    QString title;
    double positionSeconds {0.0};
    double durationSeconds {0.0};
    bool completed {false};
    QString lastOpenedAt;
    QString updatedAt;
};

class SqliteStore final {
public:
    explicit SqliteStore(QString databasePath = {});
    ~SqliteStore();

    [[nodiscard]] static QString defaultDatabasePath();

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool isInitialized() const;
    [[nodiscard]] const QString &lastError() const;
    [[nodiscard]] const QString &databasePath() const;
    [[nodiscard]] bool containsValue(const QString &key) const;
    [[nodiscard]] QString stringValue(const QString &key, const QString &defaultValue = {}) const;
    [[nodiscard]] QStringList keysWithPrefix(const QString &prefix) const;
    [[nodiscard]] bool boolValue(const QString &key, bool defaultValue = false) const;
    bool setStringValue(const QString &key, const QString &value);
    bool setBoolValue(const QString &key, bool value);
    bool setDefaultStringValue(const QString &key, const QString &value);
    bool setDefaultBoolValue(const QString &key, bool value);
    bool removeValue(const QString &key) const;
    [[nodiscard]] std::optional<WindowStateRecord> loadWindowState(const QString &name) const;
    bool saveWindowState(const QString &name,
                         const QByteArray &geometry,
                         const QByteArray &state,
                         bool maximized,
                         bool fullscreen);
    bool clearWindowState(const QString &name);
    [[nodiscard]] std::optional<ResumeStateRecord> loadResumeState(const QString &source) const;
    bool saveResumeState(const QString &source,
                         const QString &title,
                         double positionSeconds,
                         double durationSeconds);
    bool clearResumeState(const QString &source);
    bool savePlaybackHistory(const QString &source,
                             const QString &title,
                             double positionSeconds,
                             double durationSeconds,
                             bool completed);
    [[nodiscard]] QVector<PlaybackHistoryRecord> loadPlaybackHistory(int limit = 100) const;
    bool removePlaybackHistoryEntry(const QString &source) const;
    bool clearPlaybackHistory() const;
    bool prunePlaybackHistory(int maxEntries) const;
    [[nodiscard]] QVector<revaplayer::domain::Bookmark> loadBookmarks(const QString &source) const;
    [[nodiscard]] std::optional<revaplayer::domain::Bookmark> createBookmark(const QString &source,
                                                                               const QString &title,
                                                                               double positionSeconds,
                                                                               const QString &note,
                                                                               const QString &category) const;
    bool deleteBookmark(qint64 bookmarkId) const;
    [[nodiscard]] QVector<revaplayer::domain::CustomCommand> loadCustomCommands() const;
    bool replaceCustomCommands(const QVector<revaplayer::domain::CustomCommand> &commands);
    bool resetApplicationData() const;

private:
    bool ensureReady();
    bool openDatabase();
    bool ensureDatabaseDirectory();
    bool applyPragmas() const;
    bool migrateDatabase();
    [[nodiscard]] int schemaVersion() const;
    bool setSchemaVersion(int version) const;
    [[nodiscard]] bool tableColumnExists(const QString &tableName, const QString &columnName) const;
    bool createTables() const;
    bool executeStatement(const QString &sql) const;
    void setLastError(QString message);
    [[nodiscard]] static QString normalizeMediaKey(const QString &source);

    QString databasePath_;
    QString lastError_;
    QString connectionName_;
    bool initialized_ {false};
};

}  // namespace revaplayer::infrastructure::storage
