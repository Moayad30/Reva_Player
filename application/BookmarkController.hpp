#pragma once

#include "domain/Bookmark.hpp"

#include <QObject>
#include <QString>
#include <QVector>

#include <memory>
#include <optional>

namespace revaplayer::infrastructure::storage {
class SqliteStore;
}

namespace revaplayer::application {

class BookmarkController final : public QObject {
    Q_OBJECT

public:
    explicit BookmarkController(std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store,
                                QObject *parent = nullptr);
    ~BookmarkController() override;

    [[nodiscard]] bool initialize();
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] QVector<revaplayer::domain::Bookmark> bookmarksFor(const QString &source) const;
    [[nodiscard]] std::optional<revaplayer::domain::Bookmark> createBookmark(const QString &source,
                                                                               const QString &title,
                                                                               double positionSeconds,
                                                                               const QString &note = {},
                                                                               const QString &category = {}) const;
    bool deleteBookmark(qint64 bookmarkId) const;

private:
    std::unique_ptr<revaplayer::infrastructure::storage::SqliteStore> store_;
};

}  // namespace revaplayer::application
