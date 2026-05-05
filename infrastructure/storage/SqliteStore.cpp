#include "infrastructure/storage/SqliteStore.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <utility>

namespace revaplayer::infrastructure::storage {
namespace {

constexpr int kCurrentSchemaVersion = 3;
constexpr auto kDatabaseFileName = "revaplayer.sqlite";

QString normalizedPlayableStoredSource(const QString &source)
{
    if (source.isEmpty()) {
        return {};
    }

    const QUrl url(source);
    if (url.isValid() && url.isLocalFile()) {
        const QFileInfo fileInfo(url.toLocalFile());
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return {};
        }
        return QDir::cleanPath(fileInfo.absoluteFilePath());
    }

    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        return {};
    }

    const QUrl userInputUrl = QUrl::fromUserInput(trimmedSource);
    if (userInputUrl.isLocalFile()) {
        const QFileInfo fileInfo(userInputUrl.toLocalFile());
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return {};
        }
        return QDir::cleanPath(fileInfo.absoluteFilePath());
    }

    if (userInputUrl.isValid() && !userInputUrl.scheme().isEmpty()) {
        QUrl normalizedUrl = userInputUrl.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment);
        normalizedUrl.setPassword(QString {});
        return normalizedUrl.toString(QUrl::RemovePassword | QUrl::NormalizePathSegments);
    }

    return trimmedSource;
}

}  // namespace

SqliteStore::SqliteStore(QString databasePath)
    : databasePath_(std::move(databasePath))
{
}

SqliteStore::~SqliteStore()
{
    if (connectionName_.isEmpty()) {
        return;
    }

    {
        QSqlDatabase database = QSqlDatabase::database(connectionName_, false);
        if (database.isValid()) {
            database.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName_);
}

QString SqliteStore::defaultDatabasePath()
{
    const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataLocation.isEmpty()) {
        return {};
    }

    return QDir(appDataLocation).filePath(QString::fromLatin1(kDatabaseFileName));
}

bool SqliteStore::initialize()
{
    if (initialized_) {
        return true;
    }

    lastError_.clear();
    return ensureReady();
}

bool SqliteStore::isInitialized() const
{
    return initialized_;
}

const QString &SqliteStore::lastError() const
{
    return lastError_;
}

const QString &SqliteStore::databasePath() const
{
    return databasePath_;
}

bool SqliteStore::containsValue(const QString &key) const
{
    if (!initialized_ || key.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("SELECT 1 FROM settings WHERE key = ? LIMIT 1"));
    query.addBindValue(key.trimmed());
    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return query.next();
}

QString SqliteStore::stringValue(const QString &key, const QString &defaultValue) const
{
    if (!initialized_) {
        return defaultValue;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);

    if (!query.exec() || !query.next()) {
        return defaultValue;
    }

    return query.value(0).toString();
}

QStringList SqliteStore::keysWithPrefix(const QString &prefix) const
{
    QStringList keys;
    if (!initialized_ || prefix.trimmed().isEmpty()) {
        return keys;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "SELECT key FROM settings WHERE key LIKE ? ESCAPE '\\' ORDER BY key ASC"));
    QString pattern = prefix;
    pattern.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    pattern.replace(QStringLiteral("%"), QStringLiteral("\\%"));
    pattern.replace(QStringLiteral("_"), QStringLiteral("\\_"));
    query.addBindValue(QStringLiteral("%1%").arg(pattern));

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return keys;
    }

    while (query.next()) {
        const QString key = query.value(0).toString().trimmed();
        if (!key.isEmpty()) {
            keys.push_back(key);
        }
    }

    return keys;
}

bool SqliteStore::boolValue(const QString &key, const bool defaultValue) const
{
    const QString rawValue = stringValue(key, defaultValue ? QStringLiteral("1") : QStringLiteral("0")).trimmed().toLower();
    return rawValue == QStringLiteral("1") || rawValue == QStringLiteral("true") || rawValue == QStringLiteral("yes");
}

bool SqliteStore::setStringValue(const QString &key, const QString &value)
{
    if (!ensureReady()) {
        return false;
    }

    const QString safeKey = key.trimmed();
    if (safeKey.isEmpty()) {
        setLastError(QStringLiteral("SQLite settings key cannot be empty."));
        return false;
    }

    const QString safeValue = value.isNull() ? QStringLiteral("") : value;

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value, updated_at) VALUES (?, ?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = excluded.updated_at"));
    query.addBindValue(safeKey);
    query.addBindValue(safeValue);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::setBoolValue(const QString &key, const bool value)
{
    return setStringValue(key, value ? QStringLiteral("1") : QStringLiteral("0"));
}

bool SqliteStore::setDefaultStringValue(const QString &key, const QString &value)
{
    if (!ensureReady()) {
        return false;
    }

    const QString safeKey = key.trimmed();
    if (safeKey.isEmpty()) {
        setLastError(QStringLiteral("SQLite settings key cannot be empty."));
        return false;
    }

    if (containsValue(safeKey)) {
        return true;
    }

    return setStringValue(safeKey, value.isNull() ? QStringLiteral("") : value);
}

bool SqliteStore::setDefaultBoolValue(const QString &key, const bool value)
{
    return setDefaultStringValue(key, value ? QStringLiteral("1") : QStringLiteral("0"));
}

bool SqliteStore::removeValue(const QString &key) const
{
    if (!initialized_ || key.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("DELETE FROM settings WHERE key = ?"));
    query.addBindValue(key.trimmed());
    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return true;
}

std::optional<WindowStateRecord> SqliteStore::loadWindowState(const QString &name) const
{
    if (!initialized_) {
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "SELECT geometry, state, is_maximized, is_fullscreen "
        "FROM window_state WHERE name = ?"));
    query.addBindValue(name);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    WindowStateRecord record;
    record.geometry = query.value(0).toByteArray();
    record.state = query.value(1).toByteArray();
    record.maximized = query.value(2).toInt() != 0;
    record.fullscreen = query.value(3).toInt() != 0;
    return record;
}

bool SqliteStore::saveWindowState(const QString &name,
                                  const QByteArray &geometry,
                                  const QByteArray &state,
                                  const bool maximized,
                                  const bool fullscreen)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "INSERT INTO window_state(name, geometry, state, is_maximized, is_fullscreen, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(name) DO UPDATE SET "
        "geometry = excluded.geometry, "
        "state = excluded.state, "
        "is_maximized = excluded.is_maximized, "
        "is_fullscreen = excluded.is_fullscreen, "
        "updated_at = excluded.updated_at"));
    query.addBindValue(name);
    query.addBindValue(geometry);
    query.addBindValue(state);
    query.addBindValue(maximized ? 1 : 0);
    query.addBindValue(fullscreen ? 1 : 0);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::clearWindowState(const QString &name)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("DELETE FROM window_state WHERE name = ?"));
    query.addBindValue(name);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

std::optional<ResumeStateRecord> SqliteStore::loadResumeState(const QString &source) const
{
    const QString safeSource = normalizeMediaKey(source);
    if (!initialized_ || safeSource.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "SELECT source, title, last_position_seconds, duration_seconds, updated_at "
        "FROM resume_state WHERE source = ?"));
    query.addBindValue(safeSource);

    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    ResumeStateRecord record;
    record.source = normalizedPlayableStoredSource(query.value(0).toString());
    if (record.source.isEmpty()) {
        const_cast<SqliteStore *>(this)->clearResumeState(safeSource);
        return std::nullopt;
    }
    record.title = query.value(1).toString();
    record.positionSeconds = query.value(2).toDouble();
    record.durationSeconds = query.value(3).toDouble();
    record.updatedAt = query.value(4).toString();
    return record;
}

bool SqliteStore::saveResumeState(const QString &source,
                                  const QString &title,
                                  const double positionSeconds,
                                  const double durationSeconds)
{
    const QString safeSource = normalizeMediaKey(source);
    if (!ensureReady() || safeSource.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "INSERT INTO resume_state(source, title, last_position_seconds, duration_seconds, updated_at) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(source) DO UPDATE SET "
        "title = excluded.title, "
        "last_position_seconds = excluded.last_position_seconds, "
        "duration_seconds = excluded.duration_seconds, "
        "updated_at = excluded.updated_at"));
    query.addBindValue(safeSource);
    query.addBindValue(title.trimmed());
    query.addBindValue(positionSeconds);
    query.addBindValue(durationSeconds);
    query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::clearResumeState(const QString &source)
{
    const QString safeSource = normalizeMediaKey(source);
    if (!ensureReady() || safeSource.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("DELETE FROM resume_state WHERE source = ?"));
    query.addBindValue(safeSource);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::savePlaybackHistory(const QString &source,
                                      const QString &title,
                                      const double positionSeconds,
                                      const double durationSeconds,
                                      const bool completed)
{
    const QString safeSource = normalizeMediaKey(source);
    if (!ensureReady() || safeSource.isEmpty()) {
        return false;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "INSERT INTO playback_history("
        "source, title, last_position_seconds, duration_seconds, completed, last_opened_at, updated_at"
        ") VALUES (?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(source) DO UPDATE SET "
        "title = excluded.title, "
        "last_position_seconds = CASE "
        "WHEN excluded.completed = 0 "
        "AND excluded.last_position_seconds <= 0 "
        "AND playback_history.last_position_seconds > 0 "
        "THEN playback_history.last_position_seconds "
        "ELSE excluded.last_position_seconds END, "
        "duration_seconds = CASE "
        "WHEN excluded.completed = 0 "
        "AND excluded.duration_seconds <= 0 "
        "AND playback_history.duration_seconds > 0 "
        "THEN playback_history.duration_seconds "
        "ELSE excluded.duration_seconds END, "
        "completed = CASE "
        "WHEN excluded.completed = 0 "
        "AND excluded.last_position_seconds <= 0 "
        "AND playback_history.last_position_seconds > 0 "
        "THEN playback_history.completed "
        "ELSE excluded.completed END, "
        "last_opened_at = excluded.last_opened_at, "
        "updated_at = excluded.updated_at"));
    query.addBindValue(safeSource);
    query.addBindValue(title.trimmed());
    query.addBindValue(positionSeconds);
    query.addBindValue(durationSeconds);
    query.addBindValue(completed ? 1 : 0);
    query.addBindValue(timestamp);
    query.addBindValue(timestamp);

    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }

    return true;
}

QVector<PlaybackHistoryRecord> SqliteStore::loadPlaybackHistory(const int limit) const
{
    QVector<PlaybackHistoryRecord> history;
    if (!initialized_) {
        return history;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    const int safeLimit = std::clamp(limit, 1, 500);
    query.prepare(QStringLiteral(
        "SELECT source, title, last_position_seconds, duration_seconds, completed, last_opened_at, updated_at "
        "FROM playback_history "
        "ORDER BY last_opened_at DESC, updated_at DESC "
        "LIMIT ?"));
    query.addBindValue(safeLimit);

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return history;
    }

    QStringList invalidSources;
    while (query.next()) {
        PlaybackHistoryRecord entry;
        entry.source = normalizedPlayableStoredSource(query.value(0).toString());
        if (entry.source.isEmpty()) {
            const QString rawSource = query.value(0).toString().trimmed();
            if (!rawSource.isEmpty()) {
                invalidSources.push_back(rawSource);
            }
            continue;
        }
        entry.title = query.value(1).toString();
        entry.positionSeconds = std::max(0.0, query.value(2).toDouble());
        entry.durationSeconds = std::max(0.0, query.value(3).toDouble());
        entry.completed = query.value(4).toInt() != 0;
        entry.lastOpenedAt = query.value(5).toString();
        entry.updatedAt = query.value(6).toString();
        history.push_back(std::move(entry));
    }

    for (const QString &invalidSource : std::as_const(invalidSources)) {
        QSqlQuery deleteQuery(QSqlDatabase::database(connectionName_, false));
        deleteQuery.prepare(QStringLiteral("DELETE FROM playback_history WHERE source = ?"));
        deleteQuery.addBindValue(invalidSource);
        deleteQuery.exec();
    }

    return history;
}

bool SqliteStore::clearPlaybackHistory() const
{
    if (!initialized_) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    if (!query.exec(QStringLiteral("DELETE FROM playback_history"))) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::removePlaybackHistoryEntry(const QString &source) const
{
    const QString safeSource = normalizeMediaKey(source);
    if (!initialized_ || safeSource.isEmpty()) {
        return false;
    }

    QSqlQuery historyQuery(QSqlDatabase::database(connectionName_, false));
    historyQuery.prepare(QStringLiteral("DELETE FROM playback_history WHERE source = ?"));
    historyQuery.addBindValue(safeSource);
    if (!historyQuery.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(historyQuery.lastError().text());
        return false;
    }

    QSqlQuery resumeQuery(QSqlDatabase::database(connectionName_, false));
    resumeQuery.prepare(QStringLiteral("DELETE FROM resume_state WHERE source = ?"));
    resumeQuery.addBindValue(safeSource);
    if (!resumeQuery.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(resumeQuery.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::prunePlaybackHistory(const int maxEntries) const
{
    if (!initialized_) {
        return false;
    }

    const int safeLimit = std::clamp(maxEntries, 1, 500);
    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "DELETE FROM playback_history "
        "WHERE source NOT IN ("
        "    SELECT source FROM playback_history "
        "    ORDER BY last_opened_at DESC, updated_at DESC "
        "    LIMIT ?"
        ")"));
    query.addBindValue(safeLimit);

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return true;
}

QVector<revaplayer::domain::Bookmark> SqliteStore::loadBookmarks(const QString &source) const
{
    QVector<revaplayer::domain::Bookmark> bookmarks;
    const QString safeSource = normalizeMediaKey(source);
    if (!initialized_ || safeSource.isEmpty()) {
        return bookmarks;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "SELECT id, source, title, category, note, position_seconds, created_at, updated_at "
        "FROM bookmarks WHERE source = ? ORDER BY position_seconds ASC, id ASC"));
    query.addBindValue(safeSource);

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return bookmarks;
    }

    while (query.next()) {
        revaplayer::domain::Bookmark bookmark;
        bookmark.id = query.value(0).toLongLong();
        bookmark.source = query.value(1).toString();
        bookmark.title = query.value(2).toString();
        bookmark.category = query.value(3).toString();
        bookmark.note = query.value(4).toString();
        bookmark.positionSeconds = query.value(5).toDouble();
        bookmark.createdAt = query.value(6).toString();
        bookmark.updatedAt = query.value(7).toString();
        bookmarks.push_back(std::move(bookmark));
    }

    return bookmarks;
}

std::optional<revaplayer::domain::Bookmark> SqliteStore::createBookmark(const QString &source,
                                                                          const QString &title,
                                                                          const double positionSeconds,
                                                                          const QString &note,
                                                                          const QString &category) const
{
    const QString safeSource = normalizeMediaKey(source);
    QString safeTitle = title.isNull() ? QStringLiteral("") : title.trimmed();
    QString safeNote = note.isNull() ? QStringLiteral("") : note.trimmed();
    QString safeCategory = category.isNull() ? QStringLiteral("") : category.trimmed();

    if (safeTitle.isNull()) {
        safeTitle = QStringLiteral("");
    }
    if (safeNote.isNull()) {
        safeNote = QStringLiteral("");
    }
    if (safeCategory.isNull()) {
        safeCategory = QStringLiteral("");
    }

    if (!initialized_ || safeSource.isEmpty()) {
        return std::nullopt;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "INSERT INTO bookmarks(source, title, category, note, position_seconds, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(QVariant::fromValue(safeSource));
    query.addBindValue(QVariant::fromValue(safeTitle));
    query.addBindValue(QVariant::fromValue(safeCategory));
    query.addBindValue(QVariant::fromValue(safeNote));
    query.addBindValue(positionSeconds);
    query.addBindValue(timestamp);
    query.addBindValue(timestamp);

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return std::nullopt;
    }

    revaplayer::domain::Bookmark bookmark;
    bookmark.id = query.lastInsertId().toLongLong();
    bookmark.source = safeSource;
    bookmark.title = safeTitle;
    bookmark.category = safeCategory;
    bookmark.note = safeNote;
    bookmark.positionSeconds = positionSeconds;
    bookmark.createdAt = timestamp;
    bookmark.updatedAt = timestamp;
    return bookmark;
}

bool SqliteStore::deleteBookmark(const qint64 bookmarkId) const
{
    if (!initialized_ || bookmarkId < 0) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral("DELETE FROM bookmarks WHERE id = ?"));
    query.addBindValue(bookmarkId);
    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    return true;
}

QVector<revaplayer::domain::CustomCommand> SqliteStore::loadCustomCommands() const
{
    QVector<revaplayer::domain::CustomCommand> commands;
    if (!initialized_) {
        return commands;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    query.prepare(QStringLiteral(
        "SELECT id, name, script "
        "FROM custom_commands "
        "ORDER BY position ASC, id ASC"));

    if (!query.exec()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return commands;
    }

    while (query.next()) {
        revaplayer::domain::CustomCommand command;
        command.id = query.value(0).toLongLong();
        command.name = query.value(1).toString();
        command.script = query.value(2).toString();
        commands.push_back(std::move(command));
    }

    return commands;
}

bool SqliteStore::replaceCustomCommands(const QVector<revaplayer::domain::CustomCommand> &commands)
{
    if (!ensureReady()) {
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(connectionName_, false);
    if (!database.transaction()) {
        setLastError(database.lastError().text());
        return false;
    }

    QSqlQuery clearQuery(database);
    if (!clearQuery.exec(QStringLiteral("DELETE FROM custom_commands"))) {
        setLastError(clearQuery.lastError().text());
        database.rollback();
        return false;
    }

    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QSqlQuery insertQuery(database);
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO custom_commands(position, name, script, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"));

    for (qsizetype index = 0; index < commands.size(); ++index) {
        const auto &command = commands.at(index);
        insertQuery.addBindValue(static_cast<int>(index));
        insertQuery.addBindValue(command.name.trimmed());
        insertQuery.addBindValue(command.script);
        insertQuery.addBindValue(timestamp);
        insertQuery.addBindValue(timestamp);

        if (!insertQuery.exec()) {
            setLastError(insertQuery.lastError().text());
            database.rollback();
            return false;
        }

        insertQuery.finish();
    }

    if (!database.commit()) {
        setLastError(database.lastError().text());
        database.rollback();
        return false;
    }

    return true;
}

bool SqliteStore::resetApplicationData() const
{
    if (!const_cast<SqliteStore *>(this)->ensureReady()) {
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(connectionName_, false);
    if (!database.transaction()) {
        const_cast<SqliteStore *>(this)->setLastError(database.lastError().text());
        return false;
    }

    static const QStringList statements {
        QStringLiteral("DELETE FROM settings"),
        QStringLiteral("DELETE FROM window_state"),
        QStringLiteral("DELETE FROM resume_state"),
        QStringLiteral("DELETE FROM playback_history"),
        QStringLiteral("DELETE FROM bookmarks"),
        QStringLiteral("DELETE FROM custom_commands"),
    };

    for (const QString &statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        const_cast<SqliteStore *>(this)->setLastError(database.lastError().text());
        database.rollback();
        return false;
    }

    return true;
}

bool SqliteStore::ensureReady()
{
    if (initialized_) {
        return true;
    }

    if (databasePath_.isEmpty()) {
        databasePath_ = SqliteStore::defaultDatabasePath();
    }

    if (databasePath_.isEmpty()) {
        setLastError(QStringLiteral("Could not determine an application data path for SQLite."));
        return false;
    }

    if (!ensureDatabaseDirectory() || !openDatabase() || !applyPragmas() || !migrateDatabase()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool SqliteStore::openDatabase()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        setLastError(QStringLiteral("Qt SQLite driver is not available."));
        return false;
    }

    if (connectionName_.isEmpty()) {
        connectionName_ = QStringLiteral("revaplayer-sqlite-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    database.setDatabaseName(databasePath_);

    if (!database.open()) {
        setLastError(database.lastError().text());
        return false;
    }

    return true;
}

bool SqliteStore::ensureDatabaseDirectory()
{
    const QFileInfo fileInfo(databasePath_);
    QDir directory = fileInfo.dir();
    if (directory.exists()) {
        return true;
    }

    if (directory.mkpath(QStringLiteral("."))) {
        return true;
    }

    setLastError(QStringLiteral("Failed to create database directory: %1").arg(directory.absolutePath()));
    return false;
}

bool SqliteStore::applyPragmas() const
{
    return executeStatement(QStringLiteral("PRAGMA journal_mode = WAL"))
        && executeStatement(QStringLiteral("PRAGMA synchronous = NORMAL"))
        && executeStatement(QStringLiteral("PRAGMA foreign_keys = ON"));
}

bool SqliteStore::migrateDatabase()
{
    int version = schemaVersion();
    if (version < 0) {
        return false;
    }

    if (version > kCurrentSchemaVersion) {
        setLastError(QStringLiteral("Database schema version %1 is newer than this build supports.")
                         .arg(version));
        return false;
    }

    if (version == 0) {
        if (!createTables()) {
            return false;
        }
        if (!setSchemaVersion(kCurrentSchemaVersion)) {
            return false;
        }
        version = kCurrentSchemaVersion;
    }

    if (version < 2) {
        if (!executeStatement(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS custom_commands ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "position INTEGER NOT NULL DEFAULT 0,"
                "name TEXT NOT NULL,"
                "script TEXT NOT NULL,"
                "created_at TEXT NOT NULL,"
                "updated_at TEXT NOT NULL"
                ")"))
            || !executeStatement(QStringLiteral(
                "CREATE INDEX IF NOT EXISTS idx_custom_commands_position "
                "ON custom_commands(position, id)"))) {
            return false;
        }
        version = 2;
    }

    if (version < 3) {
        if (!tableColumnExists(QStringLiteral("bookmarks"), QStringLiteral("category"))) {
            if (!executeStatement(QStringLiteral(
                    "ALTER TABLE bookmarks ADD COLUMN category TEXT NOT NULL DEFAULT ''"))) {
                return false;
            }
        }
        version = 3;
    }

    if (version != kCurrentSchemaVersion && !setSchemaVersion(kCurrentSchemaVersion)) {
        return false;
    }

    return true;
}

int SqliteStore::schemaVersion() const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return -1;
    }

    return query.value(0).toInt();
}

bool SqliteStore::setSchemaVersion(const int version) const
{
    return executeStatement(QStringLiteral("PRAGMA user_version = %1").arg(version));
}

bool SqliteStore::tableColumnExists(const QString &tableName, const QString &columnName) const
{
    if (tableName.trimmed().isEmpty() || columnName.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
        return false;
    }

    while (query.next()) {
        if (query.value(1).toString().compare(columnName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

bool SqliteStore::createTables() const
{
    return executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS settings ("
               "key TEXT PRIMARY KEY,"
               "value TEXT NOT NULL,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS window_state ("
               "name TEXT PRIMARY KEY,"
               "geometry BLOB NOT NULL,"
               "state BLOB NOT NULL,"
               "is_maximized INTEGER NOT NULL DEFAULT 0,"
               "is_fullscreen INTEGER NOT NULL DEFAULT 0,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS resume_state ("
               "source TEXT PRIMARY KEY,"
               "title TEXT NOT NULL DEFAULT '',"
               "last_position_seconds REAL NOT NULL DEFAULT 0,"
               "duration_seconds REAL NOT NULL DEFAULT 0,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS playback_history ("
               "source TEXT PRIMARY KEY,"
               "title TEXT NOT NULL DEFAULT '',"
               "last_position_seconds REAL NOT NULL DEFAULT 0,"
               "duration_seconds REAL NOT NULL DEFAULT 0,"
               "completed INTEGER NOT NULL DEFAULT 0,"
               "last_opened_at TEXT NOT NULL,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS bookmarks ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "source TEXT NOT NULL,"
               "title TEXT NOT NULL DEFAULT '',"
               "category TEXT NOT NULL DEFAULT '',"
               "note TEXT NOT NULL DEFAULT '',"
               "position_seconds REAL NOT NULL DEFAULT 0,"
               "created_at TEXT NOT NULL,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE INDEX IF NOT EXISTS idx_bookmarks_source_position "
               "ON bookmarks(source, position_seconds)"))
        && executeStatement(QStringLiteral(
               "CREATE TABLE IF NOT EXISTS custom_commands ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "position INTEGER NOT NULL DEFAULT 0,"
               "name TEXT NOT NULL,"
               "script TEXT NOT NULL,"
               "created_at TEXT NOT NULL,"
               "updated_at TEXT NOT NULL"
               ")"))
        && executeStatement(QStringLiteral(
               "CREATE INDEX IF NOT EXISTS idx_custom_commands_position "
               "ON custom_commands(position, id)"));
}

bool SqliteStore::executeStatement(const QString &sql) const
{
    QSqlQuery query(QSqlDatabase::database(connectionName_, false));
    if (query.exec(sql)) {
        return true;
    }

    const_cast<SqliteStore *>(this)->setLastError(query.lastError().text());
    return false;
}

void SqliteStore::setLastError(QString message)
{
    lastError_ = std::move(message);
}

QString SqliteStore::normalizeMediaKey(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        return {};
    }

    const QUrl userInputUrl = QUrl::fromUserInput(trimmedSource);
    if (userInputUrl.isLocalFile()) {
        return QDir::cleanPath(QFileInfo(userInputUrl.toLocalFile()).absoluteFilePath());
    }

    if (userInputUrl.isValid() && !userInputUrl.scheme().isEmpty()) {
        QUrl normalizedUrl = userInputUrl.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment);
        normalizedUrl.setPassword(QString {});
        return normalizedUrl.toString(QUrl::RemovePassword | QUrl::NormalizePathSegments);
    }

    return QDir::cleanPath(QFileInfo(trimmedSource).absoluteFilePath());
}

}  // namespace revaplayer::infrastructure::storage
