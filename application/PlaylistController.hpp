#pragma once

#include "domain/MediaItem.hpp"

#include <QAbstractItemModel>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QStandardItemModel>
#include <QStringList>
#include <QVector>

namespace revaplayer::application {

namespace PlaylistRoles {

constexpr int IndexRole = Qt::UserRole;
constexpr int SourceRole = Qt::UserRole + 1;
constexpr int SearchRole = Qt::UserRole + 2;
constexpr int ProgressRole = Qt::UserRole + 10;
constexpr int CompletedRole = Qt::UserRole + 11;
constexpr int BadgeListRole = Qt::UserRole + 12;
constexpr int SecondaryTextRole = Qt::UserRole + 13;
constexpr int DurationSecondsRole = Qt::UserRole + 14;
constexpr int LastPositionSecondsRole = Qt::UserRole + 15;
constexpr int TagsRole = Qt::UserRole + 16;
constexpr int NotesRole = Qt::UserRole + 17;
constexpr int DifficultyRole = Qt::UserRole + 18;
constexpr int SubtitleStateRole = Qt::UserRole + 19;
constexpr int TitleRole = Qt::UserRole + 20;
constexpr int ResolutionRole = Qt::UserRole + 21;
constexpr int FileFormatRole = Qt::UserRole + 22;
constexpr int MediaKindRole = Qt::UserRole + 23;
constexpr int MetadataPendingRole = Qt::UserRole + 24;
constexpr int CurrentRole = Qt::UserRole + 25;
constexpr int SecondaryBadgeListRole = Qt::UserRole + 26;
constexpr int ReorderableRole = Qt::UserRole + 27;

}  // namespace PlaylistRoles

struct PlaylistPresentationData final {
    QString displayTitle;
    int watchedPercent {-1};
    bool completed {false};
    bool favorite {false};
    QString savedListBadge;
    bool hasSubtitleSibling {false};
    double durationSeconds {0.0};
    double lastPositionSeconds {0.0};
    QStringList tags;
    QString notesPreview;
    QString difficulty;
    QString subtitleState;
    QString secondaryText;
    QStringList secondaryBadges;
    int width {0};
    int height {0};
    QString fileFormat;
    QString mediaKind;
    bool metadataPending {false};
    QImage thumbnail;
};

class PlaylistController final : public QObject {
    Q_OBJECT

public:
    explicit PlaylistController(QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *model();
    [[nodiscard]] int playlistIndexFor(const QModelIndex &index) const;
    [[nodiscard]] QVector<revaplayer::domain::PlaylistEntry> entries() const;
    void setDisplayOptions(bool showFullPaths, bool showIndexPrefixes);
    void setSortMode(const QString &sortMode);
    void setPresentationData(QHash<QString, PlaylistPresentationData> presentationData);

signals:
    void playlistReorderRequested(const QVector<int> &orderedIndices);

public slots:
    void setEntries(const QVector<revaplayer::domain::PlaylistEntry> &entries, int currentIndex);

private:
    [[nodiscard]] const revaplayer::domain::PlaylistEntry *entryForPlaylistIndex(int playlistIndex) const;
    void synchronizeEntriesOrder(const QVector<int> &orderedIndices);
    [[nodiscard]] QVector<int> orderedPlaylistIndices() const;
    [[nodiscard]] bool canUpdatePresentationDataInPlace() const;
    void populateItem(QStandardItem *item,
                      const revaplayer::domain::PlaylistEntry &entry,
                      const PlaylistPresentationData &presentation) const;
    void updatePresentationDataInPlace();
    void rebuildModel();

    QStandardItemModel model_;
    QVector<revaplayer::domain::PlaylistEntry> entries_;
    QHash<QString, PlaylistPresentationData> presentationData_;
    int currentIndex_ {-1};
    bool showFullPaths_ {false};
    bool showIndexPrefixes_ {true};
    QString sortMode_ {QStringLiteral("natural")};
    bool rebuildingModel_ {false};
};

}  // namespace revaplayer::application
