#include "application/PlaylistController.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QStandardItem>
#include <QStyle>
#include <QUrl>

#include <algorithm>

namespace revaplayer::application {
namespace {

constexpr auto kFolderBrowserSourceScheme = "reva-folder";
constexpr auto kFolderBrowserBackSourceScheme = "reva-folder-back";

QString normalizedSourceKey(const QString &source)
{
    return source;
}

bool sourceUsesScheme(const QString &source, const QString &scheme)
{
    const QString trimmedSource = source.trimmed();
    const QUrl url(trimmedSource);
    if (url.isValid() && url.scheme() == scheme) {
        return true;
    }

    // Folder-browser sources percent-encode absolute paths after "://"; QUrl can
    // reject encoded slashes in the host, so keep scheme checks prefix-based.
    if (scheme == QString::fromLatin1(kFolderBrowserSourceScheme)
        || scheme == QString::fromLatin1(kFolderBrowserBackSourceScheme)) {
        return trimmedSource.startsWith(scheme + QStringLiteral("://"));
    }
    return url.isValid() && url.scheme() == scheme;
}

bool sourceIsReorderable(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    return !trimmedSource.isEmpty()
        && !sourceUsesScheme(trimmedSource, QString::fromLatin1(kFolderBrowserSourceScheme))
        && !sourceUsesScheme(trimmedSource, QString::fromLatin1(kFolderBrowserBackSourceScheme));
}

QString decodedFolderPathFromSource(const QString &source, const QString &scheme)
{
    if (!sourceUsesScheme(source, scheme)) {
        return {};
    }

    const QString prefix = scheme + QStringLiteral("://");
    const QString encodedPath = source.trimmed().startsWith(prefix)
        ? source.trimmed().mid(prefix.size())
        : QString {};
    return QUrl::fromPercentEncoding(encodedPath.toUtf8());
}

QString sourceLabelFor(const QString &source)
{
    if (sourceUsesScheme(source, QString::fromLatin1(kFolderBrowserSourceScheme))
        || sourceUsesScheme(source, QString::fromLatin1(kFolderBrowserBackSourceScheme))) {
        return QDir::toNativeSeparators(decodedFolderPathFromSource(
            source,
            sourceUsesScheme(source, QString::fromLatin1(kFolderBrowserBackSourceScheme))
                ? QString::fromLatin1(kFolderBrowserBackSourceScheme)
                : QString::fromLatin1(kFolderBrowserSourceScheme)));
    }

    const QUrl url(source);
    if (url.isLocalFile()) {
        return QDir::toNativeSeparators(url.toLocalFile());
    }

    return QDir::toNativeSeparators(source.trimmed());
}

QString displayTextForEntry(const revaplayer::domain::PlaylistEntry &entry,
                           const bool showFullPaths,
                           const bool showIndexPrefixes,
                           const int currentIndex,
                           const QString &displayTitle)
{
    const QString preferredDisplayTitle = displayTitle.trimmed().isEmpty() ? entry.title.trimmed() : displayTitle.trimmed();
    const QString baseText = showFullPaths
        ? sourceLabelFor(entry.source)
        : (preferredDisplayTitle.isEmpty() ? sourceLabelFor(entry.source) : preferredDisplayTitle);

    if (!showIndexPrefixes) {
        return baseText;
    }

    const QString prefix = (entry.index == currentIndex || entry.isCurrent)
        ? QStringLiteral("▶ %1").arg(entry.index + 1, 2, 10, QChar('0'))
        : QStringLiteral("%1").arg(entry.index + 1, 2, 10, QChar('0'));
    return QStringLiteral("%1  •  %2").arg(prefix, baseText);
}

QString normalizedSortMode(QString sortMode)
{
    sortMode = sortMode.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("natural"),
        QStringLiteral("title"),
        QStringLiteral("duration"),
        QStringLiteral("progress"),
    };
    return allowed.contains(sortMode) ? sortMode : QStringLiteral("natural");
}

bool sortModeDependsOnPresentationData(const QString &sortMode)
{
    return sortMode == QStringLiteral("duration")
        || sortMode == QStringLiteral("progress");
}

QStringList badgeListForPresentation(const PlaylistPresentationData &presentation)
{
    QStringList badges;
    if (presentation.favorite) {
        badges.push_back(QStringLiteral("Fav"));
    }
    if (presentation.hasSubtitleSibling) {
        badges.push_back(QStringLiteral("Sub"));
    }
    if (presentation.width >= 3840 || presentation.height >= 2160) {
        badges.push_back(QStringLiteral("4K"));
    } else if (presentation.width >= 2560 || presentation.height >= 1440) {
        badges.push_back(QStringLiteral("QHD"));
    }
    if (presentation.completed) {
        badges.push_back(QStringLiteral("Done"));
    } else if (presentation.watchedPercent > 0) {
        badges.push_back(QStringLiteral("%1%").arg(std::clamp(presentation.watchedPercent, 0, 100)));
    }
    if (presentation.metadataPending) {
        badges.push_back(QStringLiteral("Scan"));
    }
    return badges;
}

}  // namespace

PlaylistController::PlaylistController(QObject *parent)
    : QObject(parent)
{
    model_.setColumnCount(1);
    connect(&model_, &QAbstractItemModel::rowsMoved, this, [this]() {
        if (rebuildingModel_ || entries_.isEmpty()) {
            return;
        }

        const QVector<int> orderedIndices = orderedPlaylistIndices();
        if (orderedIndices.size() == entries_.size()) {
            synchronizeEntriesOrder(orderedIndices);
            emit playlistReorderRequested(orderedIndices);
        }
    });
}

QAbstractItemModel *PlaylistController::model()
{
    return &model_;
}

int PlaylistController::playlistIndexFor(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return -1;
    }

    return index.data(PlaylistRoles::IndexRole).toInt();
}

QVector<revaplayer::domain::PlaylistEntry> PlaylistController::entries() const
{
    return entries_;
}

void PlaylistController::setEntries(const QVector<revaplayer::domain::PlaylistEntry> &entries, const int currentIndex)
{
    entries_ = entries;
    currentIndex_ = currentIndex;
    rebuildModel();
}

void PlaylistController::setDisplayOptions(const bool showFullPaths, const bool showIndexPrefixes)
{
    if (showFullPaths_ == showFullPaths && showIndexPrefixes_ == showIndexPrefixes) {
        return;
    }

    showFullPaths_ = showFullPaths;
    showIndexPrefixes_ = showIndexPrefixes;
    rebuildModel();
}

void PlaylistController::setSortMode(const QString &sortMode)
{
    const QString normalized = normalizedSortMode(sortMode);
    if (sortMode_ == normalized) {
        return;
    }

    sortMode_ = normalized;
    rebuildModel();
}

void PlaylistController::setPresentationData(QHash<QString, PlaylistPresentationData> presentationData)
{
    presentationData_ = std::move(presentationData);
    if (canUpdatePresentationDataInPlace()) {
        updatePresentationDataInPlace();
        return;
    }

    rebuildModel();
}

const revaplayer::domain::PlaylistEntry *PlaylistController::entryForPlaylistIndex(const int playlistIndex) const
{
    const auto it = std::find_if(entries_.cbegin(), entries_.cend(), [playlistIndex](const auto &entry) {
        return entry.index == playlistIndex;
    });
    return it != entries_.cend() ? &(*it) : nullptr;
}

void PlaylistController::synchronizeEntriesOrder(const QVector<int> &orderedIndices)
{
    if (orderedIndices.size() != entries_.size()) {
        return;
    }

    QVector<revaplayer::domain::PlaylistEntry> reorderedEntries;
    reorderedEntries.reserve(entries_.size());
    for (const int playlistIndex : orderedIndices) {
        if (const auto *entry = entryForPlaylistIndex(playlistIndex); entry != nullptr) {
            reorderedEntries.push_back(*entry);
        }
    }

    if (reorderedEntries.size() == entries_.size()) {
        entries_ = std::move(reorderedEntries);
    }
}

QVector<int> PlaylistController::orderedPlaylistIndices() const
{
    QVector<int> orderedIndices;
    orderedIndices.reserve(model_.rowCount());
    for (int row = 0; row < model_.rowCount(); ++row) {
        const QModelIndex index = model_.index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        orderedIndices.push_back(index.data(PlaylistRoles::IndexRole).toInt());
    }

    return orderedIndices;
}

bool PlaylistController::canUpdatePresentationDataInPlace() const
{
    if (rebuildingModel_
        || sortModeDependsOnPresentationData(sortMode_)
        || model_.rowCount() != entries_.size()) {
        return false;
    }

    for (int row = 0; row < model_.rowCount(); ++row) {
        const QStandardItem *item = model_.item(row, 0);
        if (item == nullptr) {
            return false;
        }

        const int playlistIndex = item->data(PlaylistRoles::IndexRole).toInt();
        const auto *entry = entryForPlaylistIndex(playlistIndex);
        if (entry == nullptr) {
            return false;
        }

        if (normalizedSourceKey(item->data(PlaylistRoles::SourceRole).toString())
            != normalizedSourceKey(entry->source)) {
            return false;
        }
    }

    return true;
}

void PlaylistController::populateItem(QStandardItem *item,
                                      const revaplayer::domain::PlaylistEntry &entry,
                                      const PlaylistPresentationData &presentation) const
{
    if (item == nullptr) {
        return;
    }

    const bool current = entry.index == currentIndex_ || entry.isCurrent;
    const QString preferredDisplayTitle = presentation.displayTitle.trimmed().isEmpty()
        ? entry.title.trimmed()
        : presentation.displayTitle.trimmed();
    item->setText(displayTextForEntry(entry, showFullPaths_, showIndexPrefixes_, currentIndex_, preferredDisplayTitle));
    item->setEditable(false);
    const bool reorderable = sourceIsReorderable(entry.source);
    Qt::ItemFlags itemFlags = (item->flags() | Qt::ItemIsDropEnabled) & ~Qt::ItemIsEditable;
    if (reorderable) {
        itemFlags |= Qt::ItemIsDragEnabled;
    } else {
        itemFlags &= ~Qt::ItemIsDragEnabled;
    }
    item->setFlags(itemFlags);
    item->setToolTip(QString {});
    item->setData(entry.index, PlaylistRoles::IndexRole);
    item->setData(entry.source, PlaylistRoles::SourceRole);
    item->setData(reorderable, PlaylistRoles::ReorderableRole);
    item->setData(
        QStringLiteral("%1 %2 %3")
            .arg(preferredDisplayTitle, sourceLabelFor(entry.source), QString::number(entry.index + 1)),
        PlaylistRoles::SearchRole);
    item->setData(std::clamp(presentation.watchedPercent, -1, 100), PlaylistRoles::ProgressRole);
    item->setData(presentation.completed, PlaylistRoles::CompletedRole);
    item->setData(badgeListForPresentation(presentation), PlaylistRoles::BadgeListRole);
    item->setData(presentation.secondaryText, PlaylistRoles::SecondaryTextRole);
    item->setData(presentation.secondaryBadges, PlaylistRoles::SecondaryBadgeListRole);
    item->setData(presentation.durationSeconds, PlaylistRoles::DurationSecondsRole);
    item->setData(presentation.lastPositionSeconds, PlaylistRoles::LastPositionSecondsRole);
    item->setData(presentation.tags, PlaylistRoles::TagsRole);
    item->setData(presentation.notesPreview, PlaylistRoles::NotesRole);
    item->setData(presentation.difficulty, PlaylistRoles::DifficultyRole);
    item->setData(presentation.subtitleState, PlaylistRoles::SubtitleStateRole);
    item->setData(preferredDisplayTitle.isEmpty() ? sourceLabelFor(entry.source) : preferredDisplayTitle, PlaylistRoles::TitleRole);
    item->setData(
        presentation.width > 0 && presentation.height > 0
            ? QStringLiteral("%1x%2").arg(presentation.width).arg(presentation.height)
            : QString {},
        PlaylistRoles::ResolutionRole);
    item->setData(presentation.fileFormat, PlaylistRoles::FileFormatRole);
    item->setData(presentation.mediaKind, PlaylistRoles::MediaKindRole);
    item->setData(presentation.metadataPending, PlaylistRoles::MetadataPendingRole);
    item->setData(current, PlaylistRoles::CurrentRole);
    if (!presentation.thumbnail.isNull()) {
        item->setData(QIcon(QPixmap::fromImage(presentation.thumbnail)), Qt::DecorationRole);
    } else if (sourceUsesScheme(entry.source, QString::fromLatin1(kFolderBrowserBackSourceScheme))) {
        item->setData(QApplication::style()->standardIcon(QStyle::SP_ArrowBack), Qt::DecorationRole);
    } else if (sourceUsesScheme(entry.source, QString::fromLatin1(kFolderBrowserSourceScheme))) {
        item->setData(QApplication::style()->standardIcon(QStyle::SP_DirIcon), Qt::DecorationRole);
    } else {
        item->setData(QVariant {}, Qt::DecorationRole);
    }

    QFont font = item->font();
    font.setBold(current);
    item->setFont(font);
}

void PlaylistController::updatePresentationDataInPlace()
{
    for (int row = 0; row < model_.rowCount(); ++row) {
        QStandardItem *item = model_.item(row, 0);
        if (item == nullptr) {
            continue;
        }

        const int playlistIndex = item->data(PlaylistRoles::IndexRole).toInt();
        const auto *entry = entryForPlaylistIndex(playlistIndex);
        if (entry == nullptr) {
            continue;
        }

        populateItem(item, *entry, presentationData_.value(normalizedSourceKey(entry->source)));
    }
}

void PlaylistController::rebuildModel()
{
    rebuildingModel_ = true;
    model_.clear();
    model_.setColumnCount(1);

    QVector<revaplayer::domain::PlaylistEntry> entries = entries_;
    if (sortMode_ != QStringLiteral("natural")) {
        std::stable_sort(entries.begin(), entries.end(), [this](const auto &left, const auto &right) {
            const PlaylistPresentationData leftPresentation = presentationData_.value(normalizedSourceKey(left.source));
            const PlaylistPresentationData rightPresentation = presentationData_.value(normalizedSourceKey(right.source));

            if (sortMode_ == QStringLiteral("title")) {
                const QString leftTitle = leftPresentation.displayTitle.trimmed().isEmpty()
                    ? (left.title.trimmed().isEmpty() ? sourceLabelFor(left.source) : left.title.trimmed())
                    : leftPresentation.displayTitle.trimmed();
                const QString rightTitle = rightPresentation.displayTitle.trimmed().isEmpty()
                    ? (right.title.trimmed().isEmpty() ? sourceLabelFor(right.source) : right.title.trimmed())
                    : rightPresentation.displayTitle.trimmed();
                return leftTitle.localeAwareCompare(rightTitle) < 0;
            }
            if (sortMode_ == QStringLiteral("duration")) {
                if (!qFuzzyCompare(leftPresentation.durationSeconds + 1.0, rightPresentation.durationSeconds + 1.0)) {
                    return leftPresentation.durationSeconds > rightPresentation.durationSeconds;
                }
            }
            if (sortMode_ == QStringLiteral("progress")) {
                if (leftPresentation.completed != rightPresentation.completed) {
                    return leftPresentation.completed > rightPresentation.completed;
                }
                if (leftPresentation.watchedPercent != rightPresentation.watchedPercent) {
                    return leftPresentation.watchedPercent > rightPresentation.watchedPercent;
                }
            }

            return left.index < right.index;
        });
    }

    for (const auto &entry : entries) {
        const PlaylistPresentationData presentation = presentationData_.value(normalizedSourceKey(entry.source));
        auto *item = new QStandardItem();
        populateItem(item, entry, presentation);
        model_.appendRow(item);
    }
    rebuildingModel_ = false;
}

}  // namespace revaplayer::application
