#include "application/BookmarkController.hpp"

#include "infrastructure/storage/SqliteStore.hpp"

#include <algorithm>

namespace revaplayer::application {

BookmarkController::BookmarkController(
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
    QObject *parent)
    : QObject(parent)
    , store_(std::move(store))
{
}

BookmarkController::~BookmarkController() = default;

bool BookmarkController::initialize()
{
    return store_ != nullptr && store_->initialize();
}

bool BookmarkController::isReady() const
{
    return store_ != nullptr && store_->isInitialized();
}

QString BookmarkController::lastError() const
{
    return store_ != nullptr ? store_->lastError() : QStringLiteral("Bookmark store is not configured.");
}

QVector<revaplayer::domain::Bookmark> BookmarkController::bookmarksFor(const QString &source) const
{
    return store_ != nullptr ? store_->loadBookmarks(source) : QVector<revaplayer::domain::Bookmark> {};
}

std::optional<revaplayer::domain::Bookmark> BookmarkController::createBookmark(const QString &source,
                                                                                 const QString &title,
                                                                                 const double positionSeconds,
                                                                                 const QString &note,
                                                                                 const QString &category) const
{
    const QString safeSource = source.trimmed();
    const QString safeTitle = title.isNull() ? QStringLiteral("") : title;
    const QString safeNote = note.isNull() ? QStringLiteral("") : note;
    const QString safeCategory = category.isNull() ? QStringLiteral("") : category;

    if (store_ == nullptr || safeSource.isEmpty()) {
        return std::nullopt;
    }

    return store_->createBookmark(
        safeSource,
        safeTitle,
        std::max(0.0, positionSeconds),
        safeNote,
        safeCategory);
}

bool BookmarkController::deleteBookmark(const qint64 bookmarkId) const
{
    return store_ != nullptr && store_->deleteBookmark(bookmarkId);
}

}  // namespace revaplayer::application
