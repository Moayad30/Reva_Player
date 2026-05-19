#include "ui/MainWindow.hpp"

#include "application/BookmarkController.hpp"
#include "application/HistoryController.hpp"
#include "application/PlaybackDiagnosticsFormatter.hpp"
#include "application/PlaybackController.hpp"
#include "application/PlaybackTuning.hpp"
#include "application/PlaylistController.hpp"
#include "application/SettingsController.hpp"
#include "application/SnapshotController.hpp"
#include "application/SnapshotPath.hpp"
#include "application/SubtitleStyleOptions.hpp"
#include "application/ThemeStyle.hpp"
#include "application/UiLanguage.hpp"
#include "domain/TrackInfo.hpp"
#include "infrastructure/storage/SqliteStore.hpp"
#include "services/media/ThumbnailService.hpp"
#include "ui/BookmarkDialog.hpp"
#include "ui/ControlBar.hpp"
#include "ui/FileDialogUtils.hpp"
#include "ui/FirstRunDialog.hpp"
#include "ui/HoverPreviewPopup.hpp"
#include "ui/MediaInfoDialog.hpp"
#include "ui/SettingsDialog.hpp"
#include "ui/MediaInformationOverlay.hpp"
#include "ui/VideoViewport.hpp"

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QAbstractSpinBox>
#include <QAbstractAnimation>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QClipboard>
#include <QCollator>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QCursor>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QEasingCurve>
#include <QFormLayout>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPainterPath>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QShowEvent>
#include <QScreen>
#include <QTabWidget>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantAnimation>
#include <QWidget>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>

namespace revaplayer::ui {
namespace {

QString uiText(const char *sourceText)
{
    return revaplayer::application::translateUiText(QString::fromUtf8(sourceText));
}

QPoint mouseEventLocalPoint(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event != nullptr ? event->position().toPoint() : QPoint {};
#else
    return event != nullptr ? event->pos() : QPoint {};
#endif
}

QPoint mouseEventGlobalPoint(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event != nullptr ? event->globalPosition().toPoint() : QPoint {};
#else
    return event != nullptr ? event->globalPos() : QPoint {};
#endif
}

std::optional<double> promptManualDelay(QWidget *parent,
                                        const QString &title,
                                        const QString &label,
                                        const double currentSeconds)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto *labelWidget = new QLabel(label, &dialog);
    labelWidget->setWordWrap(true);
    layout->addWidget(labelWidget);

    auto *slider = new QSlider(Qt::Horizontal, &dialog);
    slider->setRange(-10000, 10000);
    slider->setSingleStep(5);
    slider->setPageStep(50);
    layout->addWidget(slider);

    auto *controlRow = new QWidget(&dialog);
    auto *controlLayout = new QHBoxLayout(controlRow);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);

    auto *minusButton = new QPushButton(QStringLiteral("-"), controlRow);
    auto *spinBox = new QDoubleSpinBox(controlRow);
    auto *plusButton = new QPushButton(QStringLiteral("+"), controlRow);
    auto *resetButton = new QPushButton(uiText("Reset"), controlRow);
    spinBox->setRange(-3600.0, 3600.0);
    spinBox->setDecimals(2);
    spinBox->setSingleStep(0.05);
    spinBox->setSuffix(QStringLiteral(" s"));
    spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spinBox->setValue(std::clamp(currentSeconds, -3600.0, 3600.0));
    minusButton->setFixedWidth(38);
    plusButton->setFixedWidth(38);
    controlLayout->addWidget(minusButton, 0);
    controlLayout->addWidget(spinBox, 1);
    controlLayout->addWidget(plusButton, 0);
    controlLayout->addWidget(resetButton, 0);
    layout->addWidget(controlRow);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("Apply"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    layout->addWidget(buttonBox);

    const auto syncSliderFromSpin = [slider, spinBox]() {
        const int sliderValue = std::clamp(static_cast<int>(std::lround(spinBox->value() * 100.0)), slider->minimum(), slider->maximum());
        const QSignalBlocker blocker(slider);
        slider->setValue(sliderValue);
    };
    syncSliderFromSpin();

    QObject::connect(slider, &QSlider::valueChanged, spinBox, [spinBox](const int value) {
        const QSignalBlocker blocker(spinBox);
        spinBox->setValue(value / 100.0);
    });
    QObject::connect(spinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), slider, [syncSliderFromSpin](const double) {
        syncSliderFromSpin();
    });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() {
        spinBox->setValue(spinBox->value() - spinBox->singleStep());
    });
    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() {
        spinBox->setValue(spinBox->value() + spinBox->singleStep());
    });
    QObject::connect(resetButton, &QPushButton::clicked, spinBox, [spinBox]() {
        spinBox->setValue(0.0);
    });
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return spinBox->value();
}

class PlaylistFilterProxyModel final : public QSortFilterProxyModel {
public:
    explicit PlaylistFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        setDynamicSortFilter(false);
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        Qt::ItemFlags baseFlags = QSortFilterProxyModel::flags(index);
        if (!index.isValid()) {
            return baseFlags | Qt::ItemIsDropEnabled;
        }

        baseFlags |= Qt::ItemIsDropEnabled;
        if (index.data(revaplayer::application::PlaylistRoles::ReorderableRole).toBool()) {
            baseFlags |= Qt::ItemIsDragEnabled;
        } else {
            baseFlags &= ~Qt::ItemIsDragEnabled;
        }
        return baseFlags;
    }

    [[nodiscard]] QStringList mimeTypes() const override
    {
        return sourceModel() != nullptr ? sourceModel()->mimeTypes() : QSortFilterProxyModel::mimeTypes();
    }

    [[nodiscard]] QMimeData *mimeData(const QModelIndexList &indexes) const override
    {
        if (sourceModel() == nullptr) {
            return QSortFilterProxyModel::mimeData(indexes);
        }

        QModelIndexList sourceIndexes;
        sourceIndexes.reserve(indexes.size());
        for (const QModelIndex &index : indexes) {
            if (index.isValid() && index.data(revaplayer::application::PlaylistRoles::ReorderableRole).toBool()) {
                sourceIndexes.push_back(mapToSource(index));
            }
        }
        return sourceModel()->mimeData(sourceIndexes);
    }

    [[nodiscard]] bool canDropMimeData(const QMimeData *data,
                                       Qt::DropAction action,
                                       int row,
                                       int column,
                                       const QModelIndex &parent) const override
    {
        if (sourceModel() == nullptr) {
            return QSortFilterProxyModel::canDropMimeData(data, action, row, column, parent);
        }

        return sourceModel()->canDropMimeData(data, action, row, column, mapToSource(parent));
    }

    [[nodiscard]] Qt::DropActions supportedDropActions() const override
    {
        return Qt::MoveAction;
    }

    [[nodiscard]] Qt::DropActions supportedDragActions() const override
    {
        return Qt::MoveAction;
    }

    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override
    {
        if (action != Qt::MoveAction || sourceModel() == nullptr) {
            return QSortFilterProxyModel::dropMimeData(data, action, row, column, parent);
        }

        return sourceModel()->dropMimeData(data, action, row, column, mapToSource(parent));
    }

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override
    {
        if (sourceModel() == nullptr) {
            return false;
        }

        return sourceModel()->moveRows(mapToSource(sourceParent), sourceRow, count,
                                      mapToSource(destinationParent), destinationChild);
    }
};

bool isFolderBrowserSource(const QString &source);
bool isFolderBrowserBackSource(const QString &source);

class DeferredDragListView final : public QListView {
public:
    explicit DeferredDragListView(QWidget *parent = nullptr)
        : QListView(parent)
    {
        setSelectionRectVisible(false);
        setDropIndicatorShown(false);
        reorderAnimation_ = new QVariantAnimation(this);
        reorderAnimation_->setEasingCurve(QEasingCurve::OutCubic);
        reorderAnimation_->setDuration(180);
        reorderAnimation_->setStartValue(0.0);
        reorderAnimation_->setEndValue(1.0);
        connect(reorderAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            reorderAnimationProgress_ = std::clamp(value.toDouble(), 0.0, 1.0);
            if (viewport() != nullptr) {
                viewport()->update();
            }
        });
        connect(reorderAnimation_, &QVariantAnimation::finished, this, [this]() {
            reorderAnimationStartOffsets_.clear();
            reorderAnimationProgress_ = 1.0;
            if (viewport() != nullptr) {
                viewport()->update();
            }
        });

        liveReorderPreviewAnimation_ = new QVariantAnimation(this);
        liveReorderPreviewAnimation_->setEasingCurve(QEasingCurve::OutCubic);
        liveReorderPreviewAnimation_->setDuration(150);
        liveReorderPreviewAnimation_->setStartValue(0.0);
        liveReorderPreviewAnimation_->setEndValue(1.0);
        connect(liveReorderPreviewAnimation_, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            liveReorderPreviewProgress_ = std::clamp(value.toDouble(), 0.0, 1.0);
            if (viewport() != nullptr) {
                viewport()->update();
            }
        });
        connect(liveReorderPreviewAnimation_, &QVariantAnimation::finished, this, [this]() {
            liveReorderPreviewCurrentOffsets_ = liveReorderPreviewTargetOffsets_;
            liveReorderPreviewStartOffsets_.clear();
            liveReorderPreviewProgress_ = 1.0;
            if (viewport() != nullptr) {
                viewport()->update();
            }
        });
    }

    void setActivationHandler(std::function<void(const QModelIndex &)> handler)
    {
        activationHandler_ = std::move(handler);
    }

    void setManualReorderHandler(std::function<void(const QVector<int> &)> handler)
    {
        manualReorderHandler_ = std::move(handler);
    }

    int animatedYOffsetForPlaylistIndex(const int playlistIndex) const
    {
        int offset = currentLivePreviewOffsetForPlaylistIndex(playlistIndex);
        const auto it = reorderAnimationStartOffsets_.constFind(playlistIndex);
        if (it != reorderAnimationStartOffsets_.constEnd()) {
            offset += static_cast<int>(std::lround(static_cast<double>(it.value()) * (1.0 - reorderAnimationProgress_)));
        }
        return offset;
    }

    bool isPlaylistIndexBeingDragged(const int playlistIndex) const
    {
        return draggedPlaylistIndices_.contains(playlistIndex);
    }

    bool reorderDragActive() const
    {
        return !draggedPlaylistIndices_.isEmpty();
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (selectionGestureActive_) {
            event->accept();
            return;
        }

        pressedIndex_ = QModelIndex {};
        pressPosition_ = QPoint {};
        dragStarted_ = false;
        allowDragStart_ = false;
        if (event != nullptr
            && event->button() == Qt::LeftButton
            && (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == Qt::NoModifier) {
            pressedIndex_ = indexAt(mouseEventLocalPoint(event));
            pressPosition_ = mouseEventLocalPoint(event);
            allowDragStart_ = isReorderableIndex(pressedIndex_);
        }

        QListView::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event == nullptr) {
            QListView::mouseDoubleClickEvent(event);
            return;
        }

        if (event->button() != Qt::LeftButton
            || (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) != Qt::NoModifier) {
            QListView::mouseDoubleClickEvent(event);
            return;
        }

        const QModelIndex anchorIndex = indexAt(mouseEventLocalPoint(event));
        if (!anchorIndex.isValid() || selectionModel() == nullptr) {
            QListView::mouseDoubleClickEvent(event);
            return;
        }

        selectionGestureActive_ = true;
        selectionGestureAnchor_ = anchorIndex;
        selectionModel()->setCurrentIndex(anchorIndex, QItemSelectionModel::Current);
        event->accept();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (selectionGestureActive_ && event != nullptr && selectionModel() != nullptr) {
            const QRect viewportRect = viewport() != nullptr ? viewport()->rect() : rect();
            QPoint localPoint = mouseEventLocalPoint(event);
            if (auto *scrollBar = verticalScrollBar(); scrollBar != nullptr) {
                if (localPoint.y() < viewportRect.top() + 18) {
                    scrollBar->setValue(scrollBar->value() - std::max(8, scrollBar->singleStep()));
                } else if (localPoint.y() > viewportRect.bottom() - 18) {
                    scrollBar->setValue(scrollBar->value() + std::max(8, scrollBar->singleStep()));
                }
            }

            const QPoint clampedPoint(
                std::clamp(localPoint.x(), viewportRect.left() + 1, std::max(viewportRect.left() + 1, viewportRect.right() - 1)),
                std::clamp(localPoint.y(), viewportRect.top() + 1, std::max(viewportRect.top() + 1, viewportRect.bottom() - 1)));
            QModelIndex targetIndex = indexAt(clampedPoint);
            if (!targetIndex.isValid()) {
                targetIndex = selectionGestureAnchor_;
            }

            const int anchorRow = selectionGestureAnchor_.row();
            const int targetRow = targetIndex.row();
            if (anchorRow >= 0 && targetRow >= 0) {
                const QModelIndex rangeStart = model()->index(std::min(anchorRow, targetRow), 0, rootIndex());
                const QModelIndex rangeEnd = model()->index(std::max(anchorRow, targetRow), 0, rootIndex());
                selectionModel()->select(QItemSelection(rangeStart, rangeEnd),
                                         QItemSelectionModel::ClearAndSelect);
                selectionModel()->setCurrentIndex(targetIndex,
                                                  QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
                scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
            }

            event->accept();
            return;
        }

        if (!dragStarted_
            && event != nullptr
            && allowDragStart_
            && pressedIndex_.isValid()
            && (mouseEventLocalPoint(event) - pressPosition_).manhattanLength() >= QApplication::startDragDistance()) {
            dragStarted_ = true;
            if (dragEnabled() && model() != nullptr) {
                startDrag(model()->supportedDragActions());
            }
            event->accept();
            return;
        }

        QListView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (selectionGestureActive_) {
            selectionGestureActive_ = false;
            selectionGestureAnchor_ = QModelIndex {};
            allowDragStart_ = false;
            if (event != nullptr) {
                event->accept();
            }
            return;
        }

        QListView::mouseReleaseEvent(event);
        if (event != nullptr
            && event->button() == Qt::LeftButton
            && !dragStarted_
            && activationHandler_
            && pressedIndex_.isValid()
            && (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == Qt::NoModifier) {
            const QModelIndex releasedIndex = indexAt(mouseEventLocalPoint(event));
            if (releasedIndex.isValid() && releasedIndex == pressedIndex_) {
                activationHandler_(releasedIndex);
            }
        }
        allowDragStart_ = false;
        dragStarted_ = false;
        pressPosition_ = QPoint {};
        pressedIndex_ = QModelIndex {};
    }

    void startDrag(Qt::DropActions supportedActions) override
    {
        if (!allowDragStart_ || !isReorderableIndex(pressedIndex_)) {
            return;
        }

        dragStarted_ = true;
        draggedPlaylistIndices_.clear();
        QModelIndexList selectedRows = selectionModel() != nullptr ? selectionModel()->selectedRows() : QModelIndexList {};
        if (selectedRows.isEmpty()
            || std::none_of(selectedRows.cbegin(), selectedRows.cend(), [this](const QModelIndex &index) {
                   return index == pressedIndex_;
               })) {
            selectedRows.clear();
            selectedRows.push_back(pressedIndex_);
        }
        std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex &left, const QModelIndex &right) {
            return left.row() < right.row();
        });
        for (const QModelIndex &index : selectedRows) {
            if (!isReorderableIndex(index)) {
                continue;
            }
            draggedPlaylistIndices_.push_back(index.data(revaplayer::application::PlaylistRoles::IndexRole).toInt());
        }
        if (draggedPlaylistIndices_.isEmpty()) {
            dragStarted_ = false;
            return;
        }
        QListView::startDrag(supportedActions);
        draggedPlaylistIndices_.clear();
        clearDropIndicatorState();
        clearLiveReorderPreview();
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        updateDropIndicatorState(event);
        QListView::dragMoveEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent *event) override
    {
        clearDropIndicatorState();
        clearLiveReorderPreview();
        QListView::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (event != nullptr
            && manualReorderHandler_
            && !draggedPlaylistIndices_.isEmpty()
            && model() != nullptr) {
            const QVector<int> currentOrder = currentPlaylistOrder();
            if (!currentOrder.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const QPoint dropPoint = event->position().toPoint();
#else
                const QPoint dropPoint = event->pos();
#endif
                const QVector<int> reordered = reorderedPlaylistOrderForTarget(reorderTargetPositionForPoint(dropPoint));

                if (reordered != currentOrder) {
                    prepareReorderAnimation(currentOrder);
                    manualReorderHandler_(reordered);
                    clearLiveReorderPreview();
                    startPreparedReorderAnimation();
                }
                draggedPlaylistIndices_.clear();
                clearDropIndicatorState();
                clearLiveReorderPreview();
                event->acceptProposedAction();
                return;
            }
        }

        draggedPlaylistIndices_.clear();
        clearDropIndicatorState();
        clearLiveReorderPreview();
        QListView::dropEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QListView::paintEvent(event);

        if (!showDropIndicatorLine_ || viewport() == nullptr) {
            return;
        }

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        const QColor accentColor = palette().highlight().color();
        painter.setPen(QPen(accentColor.lighter(112), 2.0));
        const int left = 12;
        const int right = std::max(left + 24, viewport()->width() - 12);
        painter.drawLine(left, dropIndicatorLineY_, right, dropIndicatorLineY_);
    }

private:
    template <typename DragEvent>
    void updateDropIndicatorState(DragEvent *event)
    {
        setDropIndicatorShown(false);
        if (event == nullptr || model() == nullptr || viewport() == nullptr || draggedPlaylistIndices_.isEmpty()) {
            clearDropIndicatorState();
            clearLiveReorderPreview();
            return;
        }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QPoint dropPoint = event->position().toPoint();
#else
        const QPoint dropPoint = event->pos();
#endif

        const int targetPosition = reorderTargetPositionForPoint(dropPoint);
        int indicatorY = dropIndicatorYForReorderTarget(targetPosition);
        if (indicatorY < 0) {
            clearDropIndicatorState();
            clearLiveReorderPreview();
            return;
        }

        indicatorY = std::clamp(indicatorY, 2, std::max(2, viewport()->height() - 2));
        if (!showDropIndicatorLine_ || dropIndicatorLineY_ != indicatorY) {
            showDropIndicatorLine_ = true;
            dropIndicatorLineY_ = indicatorY;
            viewport()->update();
        }
        updateLiveReorderPreview(targetPosition);
    }

    void clearDropIndicatorState()
    {
        if (!showDropIndicatorLine_) {
            return;
        }

        showDropIndicatorLine_ = false;
        dropIndicatorLineY_ = -1;
        if (viewport() != nullptr) {
            viewport()->update();
        }
    }

    bool isReorderableIndex(const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return false;
        }

        const QVariant reorderableData = index.data(revaplayer::application::PlaylistRoles::ReorderableRole);
        if (reorderableData.isValid()) {
            return reorderableData.toBool();
        }

        const QString source = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        return !source.isEmpty() && !isFolderBrowserSource(source) && !isFolderBrowserBackSource(source);
    }

    QVector<int> reorderableRows() const
    {
        QVector<int> rows;
        if (model() == nullptr) {
            return rows;
        }

        rows.reserve(model()->rowCount());
        for (int row = 0; row < model()->rowCount(); ++row) {
            const QModelIndex modelIndex = model()->index(row, 0, rootIndex());
            if (isReorderableIndex(modelIndex)) {
                rows.push_back(row);
            }
        }
        return rows;
    }

    QVector<int> currentPlaylistOrder() const
    {
        QVector<int> order;
        if (model() == nullptr) {
            return order;
        }

        order.reserve(model()->rowCount());
        for (int row = 0; row < model()->rowCount(); ++row) {
            const QModelIndex modelIndex = model()->index(row, 0, rootIndex());
            if (modelIndex.isValid()) {
                order.push_back(modelIndex.data(revaplayer::application::PlaylistRoles::IndexRole).toInt());
            }
        }
        return order;
    }

    int reorderTargetPositionForPoint(const QPoint &dropPoint) const
    {
        if (model() == nullptr) {
            return 0;
        }

        const QVector<int> rows = reorderableRows();
        if (rows.isEmpty()) {
            return 0;
        }

        QModelIndex dropIndex = indexAt(dropPoint);
        int insertionRow = model()->rowCount();
        if (dropIndex.isValid()) {
            insertionRow = dropIndex.row();
            const QRect dropRect = visualRect(dropIndex);
            if (dropPoint.y() >= dropRect.center().y()) {
                ++insertionRow;
            }
        } else {
            const QModelIndex firstReorderable = model()->index(rows.first(), 0, rootIndex());
            if (firstReorderable.isValid()) {
                const QRect firstRect = visualRect(firstReorderable);
                insertionRow = dropPoint.y() <= firstRect.top() ? rows.first() : rows.last() + 1;
            }
        }

        int targetPosition = 0;
        for (const int row : rows) {
            if (row < insertionRow) {
                ++targetPosition;
            }
        }
        return std::clamp(targetPosition, 0, static_cast<int>(rows.size()));
    }

    int dropIndicatorYForReorderTarget(const int targetPosition) const
    {
        if (model() == nullptr || viewport() == nullptr) {
            return -1;
        }

        const QVector<int> rows = reorderableRows();
        if (rows.isEmpty()) {
            return -1;
        }

        const int clampedTarget = std::clamp(targetPosition, 0, static_cast<int>(rows.size()));
        if (clampedTarget <= 0) {
            const QModelIndex firstIndex = model()->index(rows.first(), 0, rootIndex());
            const QRect firstRect = visualRect(firstIndex);
            return firstRect.isValid() ? firstRect.top() : -1;
        }

        if (clampedTarget >= rows.size()) {
            const QModelIndex lastIndex = model()->index(rows.last(), 0, rootIndex());
            const QRect lastRect = visualRect(lastIndex);
            return lastRect.isValid() ? lastRect.bottom() : -1;
        }

        const QModelIndex targetIndex = model()->index(rows.at(clampedTarget), 0, rootIndex());
        const QRect targetRect = visualRect(targetIndex);
        return targetRect.isValid() ? targetRect.top() : -1;
    }

    QVector<int> reorderedPlaylistOrderForTarget(const int targetPosition) const
    {
        QVector<int> fullOrder = currentPlaylistOrder();
        const QVector<int> rows = reorderableRows();
        if (fullOrder.isEmpty() || rows.isEmpty() || draggedPlaylistIndices_.isEmpty()) {
            return fullOrder;
        }

        QVector<int> reorderableOrder;
        reorderableOrder.reserve(rows.size());
        for (const int row : rows) {
            if (row >= 0 && row < fullOrder.size()) {
                reorderableOrder.push_back(fullOrder.at(row));
            }
        }

        QVector<int> draggedIndices;
        draggedIndices.reserve(draggedPlaylistIndices_.size());
        for (const int playlistIndex : std::as_const(draggedPlaylistIndices_)) {
            if (reorderableOrder.contains(playlistIndex) && !draggedIndices.contains(playlistIndex)) {
                draggedIndices.push_back(playlistIndex);
            }
        }
        if (draggedIndices.isEmpty()) {
            return fullOrder;
        }

        QVector<int> reorderedMediaOrder;
        reorderedMediaOrder.reserve(reorderableOrder.size());
        for (const int playlistIndex : std::as_const(reorderableOrder)) {
            if (!draggedIndices.contains(playlistIndex)) {
                reorderedMediaOrder.push_back(playlistIndex);
            }
        }

        int adjustedTargetPosition = std::clamp(targetPosition, 0, static_cast<int>(reorderableOrder.size()));
        for (int position = 0; position < targetPosition && position < reorderableOrder.size(); ++position) {
            if (draggedIndices.contains(reorderableOrder.at(position))) {
                --adjustedTargetPosition;
            }
        }
        adjustedTargetPosition = std::clamp(adjustedTargetPosition, 0, static_cast<int>(reorderedMediaOrder.size()));
        for (int index = 0; index < draggedIndices.size(); ++index) {
            reorderedMediaOrder.insert(adjustedTargetPosition + index, draggedIndices.at(index));
        }

        if (reorderedMediaOrder.size() != rows.size()) {
            return fullOrder;
        }
        for (int index = 0; index < rows.size(); ++index) {
            const int row = rows.at(index);
            if (row >= 0 && row < fullOrder.size()) {
                fullOrder[row] = reorderedMediaOrder.at(index);
            }
        }
        return fullOrder;
    }

    int currentLivePreviewOffsetForPlaylistIndex(const int playlistIndex) const
    {
        const int startOffset = liveReorderPreviewStartOffsets_.value(
            playlistIndex,
            liveReorderPreviewCurrentOffsets_.value(playlistIndex, 0));
        const int targetOffset = liveReorderPreviewTargetOffsets_.value(playlistIndex, 0);
        return static_cast<int>(std::lround(
            static_cast<double>(startOffset)
            + static_cast<double>(targetOffset - startOffset) * liveReorderPreviewProgress_));
    }

    void updateLiveReorderPreview(const int targetPosition)
    {
        if (model() == nullptr || viewport() == nullptr || draggedPlaylistIndices_.isEmpty()) {
            clearLiveReorderPreview();
            return;
        }

        const QVector<int> currentOrder = currentPlaylistOrder();
        const QVector<int> rows = reorderableRows();
        if (currentOrder.isEmpty() || rows.isEmpty()) {
            clearLiveReorderPreview();
            return;
        }

        QHash<int, int> topByPlaylistIndex;
        topByPlaylistIndex.reserve(rows.size());
        for (const int row : rows) {
            const QModelIndex modelIndex = model()->index(row, 0, rootIndex());
            if (!modelIndex.isValid()) {
                continue;
            }

            const int playlistIndex = modelIndex.data(revaplayer::application::PlaylistRoles::IndexRole).toInt();
            topByPlaylistIndex.insert(playlistIndex, visualRect(modelIndex).top());
        }

        const QVector<int> reordered = reorderedPlaylistOrderForTarget(targetPosition);
        if (reordered.size() != currentOrder.size()) {
            clearLiveReorderPreview();
            return;
        }

        QHash<int, int> targetOffsets;
        targetOffsets.reserve(rows.size());
        for (int position = 0; position < rows.size(); ++position) {
            const int row = rows.at(position);
            if (row < 0 || row >= reordered.size()) {
                continue;
            }
            const int playlistIndex = reordered.at(row);
            if (draggedPlaylistIndices_.contains(playlistIndex)) {
                continue;
            }

            const auto oldTopIt = topByPlaylistIndex.constFind(playlistIndex);
            if (oldTopIt == topByPlaylistIndex.constEnd()) {
                continue;
            }

            const QModelIndex newRowIndex = model()->index(row, 0, rootIndex());
            if (!newRowIndex.isValid()) {
                continue;
            }

            const int offset = visualRect(newRowIndex).top() - oldTopIt.value();
            if (std::abs(offset) > 1) {
                targetOffsets.insert(playlistIndex, offset);
            }
        }

        if (targetOffsets == liveReorderPreviewTargetOffsets_) {
            return;
        }

        beginLiveReorderPreviewAnimation(targetOffsets);
    }

    void beginLiveReorderPreviewAnimation(const QHash<int, int> &targetOffsets)
    {
        QHash<int, int> currentOffsets;
        QSet<int> keys;
        for (auto it = liveReorderPreviewCurrentOffsets_.keyBegin(); it != liveReorderPreviewCurrentOffsets_.keyEnd(); ++it) {
            keys.insert(*it);
        }
        for (auto it = liveReorderPreviewTargetOffsets_.keyBegin(); it != liveReorderPreviewTargetOffsets_.keyEnd(); ++it) {
            keys.insert(*it);
        }
        for (auto it = targetOffsets.keyBegin(); it != targetOffsets.keyEnd(); ++it) {
            keys.insert(*it);
        }
        for (const int playlistIndex : keys) {
            const int currentOffset = currentLivePreviewOffsetForPlaylistIndex(playlistIndex);
            if (currentOffset != 0 || targetOffsets.value(playlistIndex, 0) != 0) {
                currentOffsets.insert(playlistIndex, currentOffset);
            }
        }

        liveReorderPreviewStartOffsets_ = currentOffsets;
        liveReorderPreviewTargetOffsets_ = targetOffsets;
        liveReorderPreviewProgress_ = 0.0;
        if (liveReorderPreviewAnimation_ != nullptr) {
            if (liveReorderPreviewAnimation_->state() != QAbstractAnimation::Stopped) {
                liveReorderPreviewAnimation_->stop();
            }
            liveReorderPreviewAnimation_->start();
        } else {
            liveReorderPreviewCurrentOffsets_ = targetOffsets;
            liveReorderPreviewProgress_ = 1.0;
        }
        if (viewport() != nullptr) {
            viewport()->update();
        }
    }

    void clearLiveReorderPreview()
    {
        if (liveReorderPreviewAnimation_ != nullptr && liveReorderPreviewAnimation_->state() != QAbstractAnimation::Stopped) {
            liveReorderPreviewAnimation_->stop();
        }
        if (liveReorderPreviewCurrentOffsets_.isEmpty()
            && liveReorderPreviewStartOffsets_.isEmpty()
            && liveReorderPreviewTargetOffsets_.isEmpty()) {
            liveReorderPreviewProgress_ = 1.0;
            return;
        }

        liveReorderPreviewCurrentOffsets_.clear();
        liveReorderPreviewStartOffsets_.clear();
        liveReorderPreviewTargetOffsets_.clear();
        liveReorderPreviewProgress_ = 1.0;
        if (viewport() != nullptr) {
            viewport()->update();
        }
    }

    void prepareReorderAnimation(const QVector<int> &currentOrder)
    {
        pendingReorderOldTops_.clear();
        if (model() == nullptr || viewport() == nullptr || currentOrder.isEmpty()) {
            return;
        }

        const QRect captureRect = viewport()->rect().adjusted(0, -viewport()->height(), 0, viewport()->height());
        for (int row = 0; row < model()->rowCount(); ++row) {
            const QModelIndex modelIndex = model()->index(row, 0, rootIndex());
            if (!modelIndex.isValid()) {
                continue;
            }

            const QRect rowRect = visualRect(modelIndex);
            if (!rowRect.isValid() || !rowRect.intersects(captureRect)) {
                continue;
            }

            const int playlistIndex = modelIndex.data(revaplayer::application::PlaylistRoles::IndexRole).toInt();
            pendingReorderOldTops_.insert(
                playlistIndex,
                rowRect.top() + currentLivePreviewOffsetForPlaylistIndex(playlistIndex));
        }
    }

    void startPreparedReorderAnimation()
    {
        reorderAnimationStartOffsets_.clear();
        if (model() == nullptr || viewport() == nullptr || pendingReorderOldTops_.isEmpty()) {
            pendingReorderOldTops_.clear();
            return;
        }

        doItemsLayout();
        for (int row = 0; row < model()->rowCount(); ++row) {
            const QModelIndex modelIndex = model()->index(row, 0, rootIndex());
            if (!modelIndex.isValid()) {
                continue;
            }

            const int playlistIndex = modelIndex.data(revaplayer::application::PlaylistRoles::IndexRole).toInt();
            const auto oldTopIt = pendingReorderOldTops_.constFind(playlistIndex);
            if (oldTopIt == pendingReorderOldTops_.constEnd()) {
                continue;
            }

            const QRect newRect = visualRect(modelIndex);
            if (!newRect.isValid()) {
                continue;
            }

            const int offset = oldTopIt.value() - newRect.top();
            if (std::abs(offset) > 1) {
                reorderAnimationStartOffsets_.insert(playlistIndex, offset);
            }
        }
        pendingReorderOldTops_.clear();

        if (reorderAnimationStartOffsets_.isEmpty() || reorderAnimation_ == nullptr) {
            return;
        }

        if (reorderAnimation_->state() != QAbstractAnimation::Stopped) {
            reorderAnimation_->stop();
        }
        reorderAnimationProgress_ = 0.0;
        reorderAnimation_->start();
    }

    bool allowDragStart_ {false};
    bool dragStarted_ {false};
    bool selectionGestureActive_ {false};
    bool showDropIndicatorLine_ {false};
    int dropIndicatorLineY_ {-1};
    double reorderAnimationProgress_ {1.0};
    double liveReorderPreviewProgress_ {1.0};
    QModelIndex selectionGestureAnchor_;
    QModelIndex pressedIndex_;
    QPoint pressPosition_;
    QVector<int> draggedPlaylistIndices_;
    QHash<int, int> pendingReorderOldTops_;
    QHash<int, int> reorderAnimationStartOffsets_;
    QHash<int, int> liveReorderPreviewCurrentOffsets_;
    QHash<int, int> liveReorderPreviewStartOffsets_;
    QHash<int, int> liveReorderPreviewTargetOffsets_;
    QVariantAnimation *reorderAnimation_ {nullptr};
    QVariantAnimation *liveReorderPreviewAnimation_ {nullptr};
    std::function<void(const QModelIndex &)> activationHandler_;
    std::function<void(const QVector<int> &)> manualReorderHandler_;
};

class DeferredDragListWidget final : public QListWidget {
public:
    explicit DeferredDragListWidget(QWidget *parent = nullptr)
        : QListWidget(parent)
    {
        setSelectionRectVisible(false);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        allowDragStart_ = false;
        if (event != nullptr
            && event->button() == Qt::LeftButton
            && (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == Qt::NoModifier) {
            const QModelIndex pressedIndex = indexAt(mouseEventLocalPoint(event));
            allowDragStart_ = pressedIndex.isValid();
        }

        QListWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QListWidget::mouseReleaseEvent(event);
        allowDragStart_ = false;
    }

    void startDrag(Qt::DropActions supportedActions) override
    {
        if (!allowDragStart_) {
            return;
        }
        QListWidget::startDrag(supportedActions);
    }

private:
    bool allowDragStart_ {false};
};

QString formatPlaybackTime(double timeSeconds);
QString localMediaPathForSource(const QString &source);
QStringList supportedMediaFilesInDirectory(const QDir &directory, bool naturalSort);

bool sourceUsesStreamingCache(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        return false;
    }

    if (!localMediaPathForSource(trimmedSource).isEmpty()) {
        return false;
    }

    const QUrl url = QUrl::fromUserInput(trimmedSource);
    return url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile();
}

bool storedMediaSourceIsUsable(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        return false;
    }

    const QString localPath = localMediaPathForSource(trimmedSource);
    if (!localPath.isEmpty()) {
        const QFileInfo fileInfo(localPath);
        return fileInfo.exists() && fileInfo.isFile();
    }

    const QUrl url = QUrl::fromUserInput(trimmedSource);
    return url.isValid() && !url.scheme().isEmpty();
}

QModelIndexList playlistContextActionIndexes(const QListView *playlistView, const QModelIndex &contextIndex)
{
    if (playlistView == nullptr || !contextIndex.isValid()) {
        return {};
    }

    const QItemSelectionModel *selectionModel = playlistView->selectionModel();
    if (selectionModel == nullptr || !selectionModel->isSelected(contextIndex)) {
        return QModelIndexList {contextIndex};
    }

    const QModelIndexList selectedRows = selectionModel->selectedRows();
    return selectedRows.isEmpty() ? QModelIndexList {contextIndex} : selectedRows;
}

bool sourcesReferToSameMedia(const QString &leftSource, const QString &rightSource)
{
    const QString left = leftSource;
    const QString right = rightSource;
    if (left.isEmpty() || right.isEmpty()) {
        return false;
    }

    if (left == right) {
        return true;
    }

    const QString leftLocalPath = localMediaPathForSource(left);
    const QString rightLocalPath = localMediaPathForSource(right);
    return !leftLocalPath.isEmpty()
        && !rightLocalPath.isEmpty()
        && leftLocalPath == rightLocalPath;
}

struct PlaylistProgressContribution final {
    int itemProgress {0};
    bool knownDuration {false};
    double durationSeconds {0.0};
    double watchedSeconds {0.0};
};

PlaylistProgressContribution playlistProgressContributionFor(const int watchedPercent,
                                                             const bool completed,
                                                             const double durationSeconds,
                                                             const double lastPositionSeconds)
{
    PlaylistProgressContribution contribution;
    contribution.itemProgress = completed ? 100 : std::max(0, watchedPercent);
    contribution.durationSeconds = std::max(0.0, durationSeconds);
    contribution.knownDuration = contribution.durationSeconds > 0.0;
    if (contribution.knownDuration) {
        contribution.watchedSeconds = completed
            ? contribution.durationSeconds
            : std::clamp(std::max(0.0, lastPositionSeconds), 0.0, contribution.durationSeconds);
    }
    return contribution;
}

bool playlistBadgeIsProgressBadge(const QString &badge)
{
    static const QRegularExpression progressBadgePattern(QStringLiteral("^\\d{1,3}%$"));
    const QString trimmed = badge.trimmed();
    return trimmed == QStringLiteral("Done") || progressBadgePattern.match(trimmed).hasMatch();
}

bool playlistBadgeIsPlaybackTimeBadge(const QString &badge)
{
    static const QRegularExpression timeBadgePattern(QStringLiteral("^\\d{2}:\\d{2}(:\\d{2})?$"));
    return timeBadgePattern.match(badge.trimmed()).hasMatch();
}

QStringList playlistBadgesWithPlaybackProgress(QStringList badges, const int watchedPercent, const bool completed)
{
    badges.erase(
        std::remove_if(badges.begin(), badges.end(), [](const QString &badge) {
            return playlistBadgeIsProgressBadge(badge);
        }),
        badges.end());

    const QString progressBadge = completed
        ? QStringLiteral("Done")
        : (watchedPercent > 0 ? QStringLiteral("%1%").arg(std::clamp(watchedPercent, 0, 100)) : QString {});
    if (!progressBadge.isEmpty()) {
        const int scanIndex = badges.indexOf(QStringLiteral("Scan"));
        if (scanIndex >= 0) {
            badges.insert(scanIndex, progressBadge);
        } else {
            badges.push_back(progressBadge);
        }
    }
    badges.removeDuplicates();
    return badges;
}

QStringList playlistSecondaryBadgesWithPlaybackProgress(QStringList badges,
                                                        const QStringList &visibleColumns,
                                                        const int watchedPercent,
                                                        const double lastPositionSeconds)
{
    const bool showWatched = visibleColumns.contains(QStringLiteral("watched"));
    const bool showLastPosition = visibleColumns.contains(QStringLiteral("last_position"));
    if (!showWatched && !showLastPosition) {
        return badges;
    }

    badges.erase(
        std::remove_if(badges.begin(), badges.end(), [showWatched, showLastPosition](const QString &badge) {
            return (showWatched && playlistBadgeIsProgressBadge(badge))
                || (showLastPosition && playlistBadgeIsPlaybackTimeBadge(badge));
        }),
        badges.end());

    if (showWatched && watchedPercent >= 0) {
        badges.push_back(QStringLiteral("%1%").arg(std::clamp(watchedPercent, 0, 100)));
    }
    if (showLastPosition && lastPositionSeconds > 0.0) {
        badges.push_back(formatPlaybackTime(lastPositionSeconds));
    }
    badges.removeDuplicates();
    return badges;
}

bool sessionServiceRegistered(const QString &service)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        return false;
    }

    QDBusConnectionInterface *busInterface = QDBusConnection::sessionBus().interface();
    if (busInterface == nullptr) {
        return false;
    }

    const QDBusReply<bool> registered = busInterface->isServiceRegistered(service);
    return registered.isValid() && registered.value();
}

bool callSessionInhibit(const QString &service,
                        const QString &path,
                        const QString &interface,
                        const QString &method,
                        const QVariantList &arguments,
                        uint *cookie)
{
    if (cookie == nullptr || !sessionServiceRegistered(service)) {
        return false;
    }

    QDBusInterface dbusInterface(service, path, interface, QDBusConnection::sessionBus());
    if (!dbusInterface.isValid()) {
        return false;
    }

    QDBusMessage reply = dbusInterface.callWithArgumentList(QDBus::Block, method, arguments);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        return false;
    }

    bool ok = false;
    const uint value = reply.arguments().first().toUInt(&ok);
    if (!ok || value == 0) {
        return false;
    }

    *cookie = value;
    return true;
}

void callSessionUninhibit(const QString &service,
                          const QString &path,
                          const QString &interface,
                          const QString &method,
                          const uint cookie)
{
    if (cookie == 0 || !sessionServiceRegistered(service)) {
        return;
    }

    QDBusInterface dbusInterface(service, path, interface, QDBusConnection::sessionBus());
    if (dbusInterface.isValid()) {
        dbusInterface.call(QDBus::NoBlock, method, cookie);
    }
}

constexpr int kPendingPlaylistSelectionRetryDelayMs = 90;
constexpr int kPendingPlaylistSelectionMaxRetries = 32;

bool textPrefersRightToLeft(const QString &text)
{
    for (const QChar character : text) {
        switch (character.direction()) {
        case QChar::DirR:
        case QChar::DirAL:
        case QChar::DirRLE:
        case QChar::DirRLO:
        case QChar::DirRLI:
            return true;
        case QChar::DirL:
        case QChar::DirLRE:
        case QChar::DirLRO:
        case QChar::DirLRI:
            return false;
        default:
            break;
        }
    }

    return false;
}

void refreshWidgetStyle(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }

    if (QStyle *styleEngine = widget->style(); styleEngine != nullptr) {
        styleEngine->unpolish(widget);
        styleEngine->polish(widget);
    }
    widget->update();
}

QString cssColor(const QColor &color, const QString &fallback = QStringLiteral("transparent"))
{
    return color.isValid() ? color.name(QColor::HexArgb) : fallback;
}

QColor withAlphaPercent(QColor color, const int opacityPercent)
{
    if (!color.isValid()) {
        return color;
    }

    color.setAlpha(static_cast<int>(std::lround(255.0 * std::clamp(opacityPercent, 0, 100) / 100.0)));
    return color;
}

double clampConfiguredVideoViewportAlignment(const double alignment, const bool constrainToFrameBounds)
{
    return std::clamp(alignment, constrainToFrameBounds ? -1.0 : -2.0, constrainToFrameBounds ? 1.0 : 2.0);
}

double keyboardVideoPanStep(const double sensitivity)
{
    return std::clamp(0.18 * std::max(0.25, sensitivity), 0.05, 0.90);
}

bool zoomWheelModifierPressed(const Qt::KeyboardModifiers modifiers)
{
    return modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
}

QString normalizedStartupCanvasStyleId(QString value)
{
    value = value.trimmed().toLower();
    if (value == QStringLiteral("graphite")) {
        return QStringLiteral("black");
    }
    return value == QStringLiteral("black")
            || value == QStringLiteral("warm")
        ? value
        : QStringLiteral("theme");
}

void setScopedBackgroundStyle(QWidget *widget, const QString &background)
{
    if (widget == nullptr) {
        return;
    }

    const QString objectName = widget->objectName().trimmed();
    if (objectName.isEmpty()) {
        widget->setStyleSheet(QStringLiteral("background: %1;").arg(background));
    } else {
        widget->setStyleSheet(QStringLiteral("QWidget#%1 { background: %2; }").arg(objectName, background));
    }
    widget->update();
}

QColor widgetThemeColor(const QWidget *widget, const char *propertyName, const QColor &fallback = QColor {})
{
    if (widget == nullptr || propertyName == nullptr || *propertyName == '\0') {
        return fallback;
    }

    const QVariant property = widget->property(propertyName);
    QColor resolved;
    if (property.canConvert<QColor>()) {
        resolved = property.value<QColor>();
    }
    if (!resolved.isValid()) {
        resolved = QColor(property.toString().trimmed());
    }
    return resolved.isValid() ? resolved : fallback;
}

void setWidgetThemeColor(QWidget *widget, const char *propertyName, const QColor &color)
{
    if (widget == nullptr || propertyName == nullptr || *propertyName == '\0') {
        return;
    }

    widget->setProperty(propertyName, color);
}

void applyPlaylistThemeProperties(QWidget *widget, const QString &themeId, const QString &accentId)
{
    if (widget == nullptr) {
        return;
    }

    setWidgetThemeColor(
        widget,
        "playlistSurfaceColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("SURFACE_BG")));
    setWidgetThemeColor(
        widget,
        "playlistSurfaceAltColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("SURFACE_BG_ALT")));
    setWidgetThemeColor(
        widget,
        "playlistSurfaceSoftColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("SURFACE_BG_SOFT")));
    setWidgetThemeColor(
        widget,
        "playlistTextPrimaryColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("TEXT_PRIMARY")));
    setWidgetThemeColor(
        widget,
        "playlistTextMutedColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("TEXT_MUTED")));
    setWidgetThemeColor(
        widget,
        "playlistBorderStrongColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("BORDER_STRONG")));
    setWidgetThemeColor(
        widget,
        "playlistBorderSoftColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("BORDER_SOFT")));
    setWidgetThemeColor(
        widget,
        "playlistAccentColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("ACCENT")));
    setWidgetThemeColor(
        widget,
        "playlistSelectBorderColor",
        revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("SELECT_BORDER")));
    refreshWidgetStyle(widget);
}

QColor blendColors(const QColor &baseColor, const QColor &overlayColor, const int overlayAlpha)
{
    const int alpha = std::clamp(overlayAlpha, 0, 255);
    if (alpha <= 0) {
        return baseColor;
    }
    if (alpha >= 255) {
        return overlayColor;
    }

    return QColor(
        ((baseColor.red() * (255 - alpha)) + (overlayColor.red() * alpha)) / 255,
        ((baseColor.green() * (255 - alpha)) + (overlayColor.green() * alpha)) / 255,
        ((baseColor.blue() * (255 - alpha)) + (overlayColor.blue() * alpha)) / 255);
}

QColor playlistCardSurfaceColor(const QColor &baseColor,
                                const QColor &accentColor,
                                const bool hovered,
                                const bool current,
                                const bool pendingActivation)
{
    QColor surfaceColor = baseColor;
    if (surfaceColor.lightness() < 112) {
        surfaceColor = surfaceColor.lighter(pendingActivation ? 152 : (hovered ? 138 : (current ? 132 : 126)));
        const int minimumLightness = pendingActivation ? 40 : (hovered ? 36 : (current ? 33 : 30));
        if (surfaceColor.lightness() < minimumLightness) {
            const QColor accentHsl = accentColor.toHsl();
            int hue = accentHsl.hslHue();
            if (hue < 0) {
                hue = 214;
            }
            surfaceColor = QColor::fromHsl(
                hue,
                std::clamp(accentHsl.hslSaturation() / 5, 14, 46),
                minimumLightness);
        }

        surfaceColor = blendColors(
            surfaceColor,
            accentColor,
            pendingActivation ? 14 : (hovered ? 11 : (current ? 9 : 6)));
        return surfaceColor;
    }

    return surfaceColor.darker(pendingActivation ? 100 : (hovered ? 102 : 104));
}

QString normalizePlaylistViewMode(QString mode);
QString normalizePlaylistViewDensity(QString density);
QString normalizePlaylistLayoutFit(QString fitId);
QString normalizePlaylistCardZoom(QString zoomId);
QString normalizePlaylistThumbnailShape(QString shapeId);
QStringList defaultPlaylistVisibleColumns();
QStringList normalizePlaylistColumns(const QStringList &columns);
bool playlistColumnsProduceSecondaryText(const QStringList &columns);

struct PlaylistDelegateLayout final {
    int minimumHeight {96};
    int leadingVisualWidth {82};
    int coverMinWidth {56};
    int coverDivisor {5};
    int badgeHeight {20};
    int badgeBudgetPercent {22};
    int badgeMaxWidth {92};
    int lineGap {5};
    int titlePointDelta {1};
    int secondaryPointDelta {0};
    bool hideThumbnail {false};
    bool squareThumbnail {false};
};

int playlistCardZoomPercent(const QString &zoomId)
{
    return normalizePlaylistCardZoom(zoomId).toInt();
}

double playlistCardZoomVisualScale(const QString &zoomId)
{
    const int zoomPercent = playlistCardZoomPercent(zoomId);
    if (zoomPercent <= 85) {
        return 0.82;
    }
    if (zoomPercent >= 145) {
        return 1.92;
    }
    if (zoomPercent >= 130) {
        return 1.58;
    }
    if (zoomPercent >= 115) {
        return 1.28;
    }
    return 1.0;
}

double playlistCardZoomScale(const QString &zoomId)
{
    return playlistCardZoomVisualScale(zoomId);
}

QString playlistCardZoomFromLegacySettings(const QString &modeId,
                                           const QString &densityId,
                                           const QString &layoutFitId)
{
    const QString mode = normalizePlaylistViewMode(modeId);
    const QString density = normalizePlaylistViewDensity(densityId);
    const QString layoutFit = normalizePlaylistLayoutFit(layoutFitId);
    if (mode == QStringLiteral("compact") || density == QStringLiteral("compact")) {
        return QStringLiteral("85");
    }
    if (mode == QStringLiteral("covers")) {
        return QStringLiteral("130");
    }
    if (mode == QStringLiteral("cards") || mode == QStringLiteral("study")) {
        return QStringLiteral("115");
    }
    if (density == QStringLiteral("comfortable") || layoutFit == QStringLiteral("preview_first")) {
        return QStringLiteral("115");
    }
    return QStringLiteral("100");
}

QString playlistThumbnailShapeFromLegacySettings(const QString &modeId,
                                                 const QString &layoutFitId)
{
    const QString mode = normalizePlaylistViewMode(modeId);
    const QString layoutFit = normalizePlaylistLayoutFit(layoutFitId);
    if (mode == QStringLiteral("compact")) {
        return QStringLiteral("square");
    }
    if (layoutFit == QStringLiteral("text_first")) {
        return QStringLiteral("square");
    }
    return QStringLiteral("rectangle");
}

QString playlistCardZoomForWidget(const QWidget *widget)
{
    if (widget == nullptr) {
        return QStringLiteral("100");
    }

    const QString directValue = widget->property("playlistCardZoom").toString().trimmed();
    if (!directValue.isEmpty()) {
        return normalizePlaylistCardZoom(directValue);
    }

    return playlistCardZoomFromLegacySettings(
        widget->property("playlistViewMode").toString(),
        widget->property("playlistViewDensity").toString(),
        widget->property("playlistLayoutFit").toString());
}

QString playlistThumbnailShapeForWidget(const QWidget *widget)
{
    if (widget == nullptr) {
        return QStringLiteral("rectangle");
    }

    const QString directValue = widget->property("playlistThumbnailShape").toString().trimmed();
    if (!directValue.isEmpty()) {
        return normalizePlaylistThumbnailShape(directValue);
    }

    return playlistThumbnailShapeFromLegacySettings(
        widget->property("playlistViewMode").toString(),
        widget->property("playlistLayoutFit").toString());
}

QString adaptedPlaylistCardZoomForViewport(const QString &zoomId, const int availableWidth)
{
    const QString normalized = normalizePlaylistCardZoom(zoomId);
    if (availableWidth < 420) {
        if (normalized == QStringLiteral("145")) {
            return QStringLiteral("115");
        }
        if (normalized == QStringLiteral("130")) {
            return QStringLiteral("100");
        }
    } else if (availableWidth < 540) {
        if (normalized == QStringLiteral("145")) {
            return QStringLiteral("130");
        }
    }
    return normalized;
}

PlaylistDelegateLayout playlistDelegateLayoutFor(const QString &zoomId,
                                                 const QString &thumbnailShapeId)
{
    const QString shape = normalizePlaylistThumbnailShape(thumbnailShapeId);
    const double zoomScale = playlistCardZoomScale(zoomId);
    const int zoomPercent = playlistCardZoomPercent(zoomId);
    PlaylistDelegateLayout layout;
    layout.minimumHeight = std::max(72, static_cast<int>(std::round(102.0 * zoomScale)));
    layout.leadingVisualWidth = std::max(0, static_cast<int>(std::round((shape == QStringLiteral("rectangle") ? 108.0 : 72.0) * zoomScale)));
    layout.coverMinWidth = std::max(0, static_cast<int>(std::round((shape == QStringLiteral("rectangle") ? 92.0 : 60.0) * zoomScale)));
    layout.coverDivisor = shape == QStringLiteral("rectangle") ? 4 : 5;
    layout.badgeHeight = std::clamp(static_cast<int>(std::round(20.0 * zoomScale)), 18, 32);
    layout.badgeBudgetPercent = shape == QStringLiteral("hidden") ? 18 : (shape == QStringLiteral("rectangle") ? 24 : 20);
    layout.badgeMaxWidth = std::clamp(static_cast<int>(std::round(102.0 * zoomScale)), 80, 138);
    layout.lineGap = std::clamp(static_cast<int>(std::round(5.0 * zoomScale)), 4, 11);
    layout.titlePointDelta = zoomPercent >= 145 ? 3 : (zoomPercent >= 130 ? 2 : (zoomPercent >= 115 ? 1 : (zoomPercent <= 85 ? -1 : 0)));
    layout.secondaryPointDelta = zoomPercent >= 130 ? 2 : (zoomPercent >= 115 ? 1 : (zoomPercent <= 85 ? -1 : 0));
    layout.hideThumbnail = shape == QStringLiteral("hidden");
    layout.squareThumbnail = shape == QStringLiteral("square");

    if (layout.hideThumbnail) {
        layout.leadingVisualWidth = 0;
        layout.coverMinWidth = 0;
        layout.coverDivisor = 7;
    }

    return layout;
}

int playlistMaxTitleLines(const QString &zoomId)
{
    const int zoomPercent = playlistCardZoomPercent(zoomId);
    if (zoomPercent >= 145) {
        return 4;
    }
    if (zoomPercent >= 130) {
        return 3;
    }
    return zoomPercent <= 85 ? 1 : 2;
}

int playlistMaxSecondaryLines(const QString &zoomId, const QString &thumbnailShapeId, const int availableWidth)
{
    const int zoomPercent = playlistCardZoomPercent(zoomId);
    if (zoomPercent <= 85) {
        return 1;
    }

    const QString shape = normalizePlaylistThumbnailShape(thumbnailShapeId);
    const int effectiveWidth = availableWidth + (shape == QStringLiteral("hidden") ? 80 : (shape == QStringLiteral("square") ? 32 : 0));
    if (zoomPercent >= 145) {
        return effectiveWidth >= 430 ? 3 : 2;
    }
    if (zoomPercent >= 130) {
        return effectiveWidth >= 430 ? 2 : 1;
    }
    return effectiveWidth >= 520 ? 2 : 1;
}

QStringList playlistWrappedTextLines(const QString &text,
                                    const QFontMetrics &metrics,
                                    const int width,
                                    const int maxLines)
{
    if (width <= 0 || maxLines <= 0) {
        return {};
    }

    const QString normalized = text.simplified();
    if (normalized.isEmpty()) {
        return {};
    }

    const QStringList words = normalized.split(QChar(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return {metrics.elidedText(normalized, Qt::ElideRight, width)};
    }

    QStringList lines;
    QString currentLine;
    int wordIndex = 0;
    while (wordIndex < words.size() && lines.size() < maxLines) {
        const QString &word = words.at(wordIndex);
        const QString candidate = currentLine.isEmpty()
            ? word
            : QStringLiteral("%1 %2").arg(currentLine, word);

        if (metrics.horizontalAdvance(candidate) <= width) {
            currentLine = candidate;
            ++wordIndex;
            continue;
        }

        const bool lastAllowedLine = lines.size() == maxLines - 1;
        if (lastAllowedLine) {
            QString remainder = currentLine.isEmpty() ? word : currentLine;
            for (int remainderIndex = currentLine.isEmpty() ? wordIndex + 1 : wordIndex;
                 remainderIndex < words.size();
                 ++remainderIndex) {
                remainder = QStringLiteral("%1 %2").arg(remainder, words.at(remainderIndex));
            }
            lines.push_back(metrics.elidedText(remainder.trimmed(), Qt::ElideRight, width));
            return lines;
        }

        if (!currentLine.isEmpty()) {
            lines.push_back(currentLine);
            currentLine.clear();
            continue;
        }

        lines.push_back(metrics.elidedText(word, Qt::ElideRight, width));
        ++wordIndex;
    }

    if (!currentLine.isEmpty() && lines.size() < maxLines) {
        lines.push_back(metrics.elidedText(currentLine, Qt::ElideRight, width));
    }

    return lines;
}

class PlaylistItemDelegate final : public QStyledItemDelegate {
public:
    explicit PlaylistItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize baseSize = QStyledItemDelegate::sizeHint(option, index);
        const QString zoomId = adaptedPlaylistCardZoomForViewport(
            playlistCardZoomForWidget(option.widget),
            option.widget != nullptr ? option.widget->width() : baseSize.width());
        const QString thumbnailShapeId = playlistThumbnailShapeForWidget(option.widget);
        const QStringList visibleColumns = option.widget != nullptr
            ? normalizePlaylistColumns(option.widget->property("playlistVisibleColumns").toStringList())
            : defaultPlaylistVisibleColumns();
        const bool showProgressBar = option.widget != nullptr
            && option.widget->property("playlistProgressModeEnabled").toBool();
        const bool showDuration = visibleColumns.contains(QStringLiteral("duration"));
        const PlaylistDelegateLayout layout = playlistDelegateLayoutFor(zoomId, thumbnailShapeId);

        QFont titleFont = option.font;
        titleFont.setBold(true);
        if (layout.titlePointDelta != 0) {
            titleFont.setPointSize(std::max(8, titleFont.pointSize() + layout.titlePointDelta));
        }
        QFont secondaryFont = option.font;
        if (layout.secondaryPointDelta != 0) {
            secondaryFont.setPointSize(std::max(7, secondaryFont.pointSize() + layout.secondaryPointDelta));
        }

        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics secondaryMetrics(secondaryFont);
        const int availableWidth = option.widget != nullptr ? option.widget->width() : baseSize.width();
        const int reservedTitleHeight = titleMetrics.lineSpacing() * playlistMaxTitleLines(zoomId);
        const QString secondaryText = index.data(revaplayer::application::PlaylistRoles::SecondaryTextRole).toString().trimmed();
        const QStringList secondaryBadges = index.data(revaplayer::application::PlaylistRoles::SecondaryBadgeListRole).toStringList();
        const bool showSecondaryBadges = !secondaryBadges.isEmpty() && playlistColumnsProduceSecondaryText(visibleColumns);
        const int reservedSecondaryHeight = (!secondaryText.isEmpty() && playlistColumnsProduceSecondaryText(visibleColumns))
            ? secondaryMetrics.lineSpacing() * playlistMaxSecondaryLines(zoomId, thumbnailShapeId, availableWidth)
            : 0;
        int contentHeight = reservedTitleHeight + 6;
        if (reservedSecondaryHeight > 0) {
            contentHeight += layout.lineGap + reservedSecondaryHeight;
        }
        if (showDuration || showSecondaryBadges) {
            contentHeight += layout.lineGap + layout.badgeHeight;
        }
        if (showProgressBar) {
            contentHeight += 12;
        }

        const int preferredHeight = contentHeight + 24;
        baseSize.setHeight(std::max(baseSize.height(), std::max(layout.minimumHeight, preferredHeight)));
        return baseSize;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (painter == nullptr) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const auto *playlistView = dynamic_cast<const DeferredDragListView *>(option.widget);
        if (playlistView != nullptr) {
            const int playlistIndex = index.data(revaplayer::application::PlaylistRoles::IndexRole).toInt();
            const int animatedOffset = playlistView->animatedYOffsetForPlaylistIndex(playlistIndex);
            if (animatedOffset != 0) {
                painter->translate(0, animatedOffset);
            }
            if (playlistView->isPlaylistIndexBeingDragged(playlistIndex)) {
                painter->setOpacity(0.36);
            }
        }

        const QRect outerRect = option.rect.adjusted(6, 4, -6, -4);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver)
            && (playlistView == nullptr || !playlistView->reorderDragActive());
        const bool rtl = (option.direction == Qt::RightToLeft)
            || (option.widget != nullptr && option.widget->layoutDirection() == Qt::RightToLeft);
        const QColor accentColor = widgetThemeColor(option.widget, "playlistAccentColor", option.palette.highlight().color());
        const QColor baseSurfaceColor = widgetThemeColor(option.widget, "playlistSurfaceColor", option.palette.base().color());
        const QColor borderStrongColor = widgetThemeColor(option.widget, "playlistBorderStrongColor", option.palette.mid().color());
        const QColor primaryTextBaseColor = widgetThemeColor(option.widget, "playlistTextPrimaryColor", option.palette.text().color());
        const QColor selectionBorderColor = widgetThemeColor(
            option.widget,
            "playlistSelectBorderColor",
            QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 132));
        const bool current = index.data(revaplayer::application::PlaylistRoles::CurrentRole).toBool();
        const QString activationSource = option.widget != nullptr
            ? option.widget->property("playlistActivationSource").toString().trimmed()
            : QString {};
        const bool pendingActivation = sourcesReferToSameMedia(
            index.data(revaplayer::application::PlaylistRoles::SourceRole).toString(),
            activationSource);
        QColor surfaceColor = selected
            ? QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 42)
            : playlistCardSurfaceColor(baseSurfaceColor, accentColor, hovered, current, pendingActivation);
        QColor borderColor = pendingActivation
            ? accentColor.lighter(selected ? 136 : 126)
            : (current
            ? accentColor.lighter(selected ? 126 : 118)
            : (selected ? selectionBorderColor : borderStrongColor));
        if (!selected && !current && borderColor.lightness() < 144) {
            borderColor = borderColor.lighter(148);
        }

        painter->setPen(QPen(borderColor, 1.0));
        painter->setBrush(surfaceColor);
        painter->drawRoundedRect(outerRect, 14, 14);

        if (current) {
            QRect accentRect = rtl
                ? QRect(outerRect.right() - 4, outerRect.top() + 8, 4, outerRect.height() - 16)
                : QRect(outerRect.left(), outerRect.top() + 8, 4, outerRect.height() - 16);
            painter->setPen(Qt::NoPen);
            painter->setBrush(accentColor);
            painter->drawRoundedRect(accentRect, 2, 2);
        }

        const QRect contentRect = outerRect.adjusted(12, 8, -12, -8);
        const QString text = index.data(Qt::DisplayRole).toString();
        const QStringList badges = index.data(revaplayer::application::PlaylistRoles::BadgeListRole).toStringList();
        const int progress = index.data(revaplayer::application::PlaylistRoles::ProgressRole).toInt();
        const bool completed = index.data(revaplayer::application::PlaylistRoles::CompletedRole).toBool();
        const double durationSeconds = index.data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble();
        const QString durationText = durationSeconds > 0.0 ? formatPlaybackTime(durationSeconds) : QString {};
        const QString secondaryText = index.data(revaplayer::application::PlaylistRoles::SecondaryTextRole).toString().trimmed();
        const QStringList secondaryBadges = index.data(revaplayer::application::PlaylistRoles::SecondaryBadgeListRole).toStringList();
        const bool textRtl = rtl || textPrefersRightToLeft(text);
        const bool showProgressBar = option.widget != nullptr
            && option.widget->property("playlistProgressModeEnabled").toBool();
        const QString zoomId = adaptedPlaylistCardZoomForViewport(
            playlistCardZoomForWidget(option.widget),
            option.widget != nullptr ? option.widget->width() : outerRect.width());
        const QString thumbnailShapeId = playlistThumbnailShapeForWidget(option.widget);
        const QStringList visibleColumns = option.widget != nullptr
            ? normalizePlaylistColumns(option.widget->property("playlistVisibleColumns").toStringList())
            : defaultPlaylistVisibleColumns();
        const bool showDuration = visibleColumns.contains(QStringLiteral("duration")) && !durationText.isEmpty();
        const QIcon thumbnailIcon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const QString resolutionText = index.data(revaplayer::application::PlaylistRoles::ResolutionRole).toString().trimmed();
        const QString fileFormatText = index.data(revaplayer::application::PlaylistRoles::FileFormatRole).toString().trimmed().toUpper();
        const PlaylistDelegateLayout layout = playlistDelegateLayoutFor(zoomId, thumbnailShapeId);
        const double zoomScale = playlistCardZoomScale(zoomId);

        const int minimumTextWidth = outerRect.width() < 380
            ? (thumbnailShapeId == QStringLiteral("hidden") ? 210 : 170)
            : (thumbnailShapeId == QStringLiteral("hidden") ? 270 : (thumbnailShapeId == QStringLiteral("square") ? 232 : 208));
        int leadingVisualWidth = 0;
        if (!layout.hideThumbnail) {
            const int maxCoverWidth = std::max(layout.coverMinWidth, contentRect.width() - minimumTextWidth);
            const int coverHeight = std::max(
                std::max(42, static_cast<int>(std::round(48.0 * zoomScale))),
                contentRect.height() - (showProgressBar ? 12 : 0) - 4);
            const int preferredWidth = layout.squareThumbnail
                ? coverHeight
                : std::max(
                      layout.coverMinWidth,
                      static_cast<int>(std::round(coverHeight * (16.0 / 9.0))));
            leadingVisualWidth = std::clamp(preferredWidth, layout.coverMinWidth, maxCoverWidth);
            const QRect coverRect(
                rtl ? contentRect.right() - leadingVisualWidth : contentRect.left(),
                contentRect.top(),
                leadingVisualWidth,
                coverHeight);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(255, 255, 255, 28) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 24));
            painter->drawRoundedRect(coverRect, 12, 12);

            if (!thumbnailIcon.isNull()) {
                const int thumbPadding = 3;
                const QRect thumbRect = coverRect.adjusted(thumbPadding, thumbPadding, -thumbPadding, -thumbPadding);
                const QPixmap pixmap = thumbnailIcon.pixmap(thumbRect.size());
                if (!pixmap.isNull()) {
                    const QPixmap scaledPixmap = pixmap.scaled(
                        thumbRect.size(),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
                    const QPoint thumbTopLeft(
                        thumbRect.left() + ((thumbRect.width() - scaledPixmap.width()) / 2),
                        thumbRect.top() + ((thumbRect.height() - scaledPixmap.height()) / 2));
                    QPainterPath clipPath;
                    clipPath.addRoundedRect(thumbRect, 10, 10);
                    painter->setClipPath(clipPath);
                    painter->drawPixmap(thumbTopLeft, scaledPixmap);
                    painter->setClipping(false);
                }
            } else {
                const QRect placeholderRect = coverRect.adjusted(4, 4, -4, -4);
                painter->setBrush(selected ? QColor(255, 255, 255, 22) : QColor(255, 255, 255, 12));
                painter->drawRoundedRect(placeholderRect, 10, 10);

                QFont coverFont = option.font;
                coverFont.setBold(true);
                coverFont.setPointSize(std::max(8, static_cast<int>(std::round(9.0 * zoomScale))));
                painter->setFont(coverFont);
                painter->setPen(primaryTextBaseColor);
                const QString previewHint = !resolutionText.isEmpty()
                    ? resolutionText
                    : (!fileFormatText.isEmpty() ? fileFormatText : uiText("Video"));
                painter->drawText(placeholderRect.adjusted(6, 8, -6, -22), Qt::AlignCenter, QStringLiteral("▶"));
                painter->drawText(placeholderRect.adjusted(6, placeholderRect.height() - 24, -6, -6), Qt::AlignCenter, previewHint);
            }
        }

        const int badgeHeight = layout.badgeHeight;
        const int badgeGap = outerRect.width() < 360 ? 5 : 7;
        const int badgeMaxWidth = std::clamp(
            (contentRect.width() * 22) / 100,
            54,
            layout.badgeMaxWidth);
        const int badgeStripBudget = std::clamp(
            (contentRect.width() * layout.badgeBudgetPercent) / 100,
            64,
            132);
        QFont badgeFont = option.font;
        if (outerRect.width() < 360) {
            badgeFont.setPointSize(std::max(7, badgeFont.pointSize() - 1));
        }
        const QFontMetrics badgeMetrics(badgeFont);
        QStringList orderedBadges = badges;
        if (!rtl) {
            std::reverse(orderedBadges.begin(), orderedBadges.end());
        }

        QStringList visibleBadges;
        const int maxVisibleBadges = outerRect.width() < 430 ? 2 : 3;
        int consumedBadgeWidth = 0;
        for (int badgeIndex = 0; badgeIndex < orderedBadges.size(); ++badgeIndex) {
            const QString badgeText = orderedBadges.at(badgeIndex).trimmed();
            if (badgeText.isEmpty()) {
                continue;
            }

            const QString elidedBadgeText = badgeMetrics.elidedText(badgeText, Qt::ElideRight, badgeMaxWidth - 18);
            const int badgeWidth = std::max(38, badgeMetrics.horizontalAdvance(elidedBadgeText) + 18);
            const int requiredWidth = visibleBadges.isEmpty() ? badgeWidth : badgeWidth + badgeGap;
            if ((!visibleBadges.isEmpty() && consumedBadgeWidth + requiredWidth > badgeStripBudget)
                || visibleBadges.size() >= maxVisibleBadges) {
                const int hiddenCount = orderedBadges.size() - badgeIndex;
                if (hiddenCount > 0) {
                    visibleBadges.push_back(QStringLiteral("+%1").arg(hiddenCount));
                }
                break;
            }
            visibleBadges.push_back(elidedBadgeText);
            consumedBadgeWidth += requiredWidth;
        }

        int badgeTrailingInset = 0;
        int badgeCursor = rtl ? contentRect.left() : contentRect.right();
        const auto drawBadge = [&](const QString &badgeText) {
            const QString badge = badgeText.trimmed();
            if (badge.isEmpty()) {
                return;
            }

            const int badgeWidth = std::min(
                badgeMaxWidth,
                std::max(38, badgeMetrics.horizontalAdvance(badge) + 18));
            QRect badgeRect;
            if (rtl) {
                badgeRect = QRect(badgeCursor, contentRect.top(), badgeWidth, badgeHeight);
                badgeCursor = badgeRect.right() + badgeGap;
                badgeTrailingInset = badgeCursor - contentRect.left();
            } else {
                badgeRect = QRect(badgeCursor - badgeWidth, contentRect.top(), badgeWidth, badgeHeight);
                badgeCursor = badgeRect.left() - badgeGap;
                badgeTrailingInset = contentRect.right() - badgeCursor;
            }
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(255, 255, 255, 52) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 72));
            painter->drawRoundedRect(badgeRect, 10, 10);
            painter->setFont(badgeFont);
            painter->setPen(primaryTextBaseColor);
            painter->drawText(badgeRect, Qt::AlignCenter, badge);
        };

        for (const QString &badge : visibleBadges) {
            drawBadge(badge);
        }

        int durationWidth = 0;
        if (showDuration) {
            QFont durationFont = option.font;
            durationFont.setBold(true);
            QFontMetrics durationMetrics(durationFont);
            durationWidth = std::max(54, durationMetrics.horizontalAdvance(durationText) + 18);
        }

        QRect textRect = contentRect;
        if (rtl) {
            textRect.adjust(0, 0, -(leadingVisualWidth + 12), 0);
        } else {
            textRect.adjust(leadingVisualWidth + 12, 0, 0, 0);
        }
        if (rtl) {
            textRect.adjust(std::max(badgeTrailingInset, 0), 0, 0, 0);
        } else {
            textRect.adjust(0, 0, -std::max(badgeTrailingInset, 0), 0);
        }
        textRect.setBottom(showProgressBar ? textRect.bottom() - 12 : textRect.bottom());
        const bool showSecondaryBadges = !secondaryBadges.isEmpty() && playlistColumnsProduceSecondaryText(visibleColumns);
        const bool showMetaRow = showDuration || showSecondaryBadges;
        QRect metaRowRect;
        QRect durationRect;
        if (showMetaRow) {
            metaRowRect = QRect(
                textRect.left(),
                std::max(textRect.top(), textRect.bottom() - badgeHeight + 1),
                textRect.width(),
                badgeHeight);
            textRect.setBottom(metaRowRect.top() - 4);
            if (durationWidth > 0) {
                const int durationLeft = rtl ? metaRowRect.left() : std::max(metaRowRect.left(), metaRowRect.right() - durationWidth);
                durationRect = QRect(durationLeft, metaRowRect.top(), durationWidth, badgeHeight);
            }
        }
        QFont titleFont = option.font;
        titleFont.setBold(true);
        if (layout.titlePointDelta != 0) {
            titleFont.setPointSize(std::max(8, titleFont.pointSize() + layout.titlePointDelta));
        }
        QFont secondaryFont = option.font;
        if (layout.secondaryPointDelta != 0) {
            secondaryFont.setPointSize(std::max(7, secondaryFont.pointSize() + layout.secondaryPointDelta));
        }
        const QColor primaryTextColor = primaryTextBaseColor;
        painter->setPen(primaryTextColor);
        painter->setFont(titleFont);
        const int horizontalAlignment = textRtl ? Qt::AlignRight : Qt::AlignLeft;
        const QFontMetrics titleMetrics(titleFont);
        const int maxTitleLines = playlistMaxTitleLines(zoomId);
        const QStringList titleLines = playlistWrappedTextLines(text, titleMetrics, textRect.width(), maxTitleLines);
        const int titleLineCount = std::max(1, static_cast<int>(titleLines.size()));
        const int titleHeight = titleMetrics.lineSpacing() * titleLineCount;
        QRect titleRect(textRect.left(), textRect.top(), textRect.width(), titleHeight);

        int titleTop = titleRect.top();
        if (titleLines.isEmpty()) {
            painter->drawText(
                QRect(textRect.left(), titleTop, textRect.width(), titleMetrics.lineSpacing()),
                Qt::AlignVCenter | horizontalAlignment,
                titleMetrics.elidedText(text, Qt::ElideRight, textRect.width()));
            titleTop += titleMetrics.lineSpacing();
        } else {
            for (const QString &titleLine : titleLines) {
                painter->drawText(
                    QRect(textRect.left(), titleTop, textRect.width(), titleMetrics.lineSpacing()),
                    Qt::AlignVCenter | horizontalAlignment,
                    titleLine);
                titleTop += titleMetrics.lineSpacing();
            }
        }

        const QFontMetrics secondaryMetrics(secondaryFont);
        const int maxSecondaryLines = (!secondaryText.isEmpty() && playlistColumnsProduceSecondaryText(visibleColumns))
            ? playlistMaxSecondaryLines(zoomId, thumbnailShapeId, textRect.width())
            : 0;
        const QStringList secondaryLines = (!secondaryText.isEmpty() && playlistColumnsProduceSecondaryText(visibleColumns))
            ? playlistWrappedTextLines(
                  secondaryText,
                  secondaryMetrics,
                  textRect.width(),
                  maxSecondaryLines)
            : QStringList {};
        const int secondaryLineCount = std::max(1, static_cast<int>(secondaryLines.size()));
        const int secondaryHeight = secondaryMetrics.lineSpacing() * secondaryLineCount;
        const bool canShowSecondary = !secondaryText.isEmpty()
            && (textRect.bottom() - titleRect.bottom()) >= secondaryHeight + layout.lineGap;
        if (canShowSecondary) {
            QRect secondaryRect(
                textRect.left(),
                titleRect.bottom() + 1 + layout.lineGap,
                textRect.width(),
                secondaryHeight);
            QColor secondaryColor = primaryTextColor;
            secondaryColor.setAlpha(selected ? 232 : (playlistCardZoomPercent(zoomId) >= 130 ? 214 : 198));
            painter->setFont(secondaryFont);
            painter->setPen(secondaryColor);
            int secondaryTop = secondaryRect.top();
            if (secondaryLines.isEmpty()) {
                painter->drawText(
                    QRect(secondaryRect.left(), secondaryTop, secondaryRect.width(), secondaryMetrics.lineSpacing()),
                    Qt::AlignTop | horizontalAlignment,
                    secondaryMetrics.elidedText(secondaryText, Qt::ElideRight, secondaryRect.width()));
            } else {
                for (const QString &secondaryLine : secondaryLines) {
                    painter->drawText(
                        QRect(secondaryRect.left(), secondaryTop, secondaryRect.width(), secondaryMetrics.lineSpacing()),
                        Qt::AlignTop | horizontalAlignment,
                        secondaryLine);
                    secondaryTop += secondaryMetrics.lineSpacing();
                }
            }
            painter->setFont(titleFont);
            painter->setPen(primaryTextColor);
        }

        if (!metaRowRect.isNull() && showSecondaryBadges) {
            QRect secondaryBadgesRect = metaRowRect;
            if (!durationRect.isNull()) {
                if (rtl) {
                    secondaryBadgesRect.setLeft(durationRect.right() + 6);
                } else {
                    secondaryBadgesRect.setRight(durationRect.left() - 6);
                }
            }

            if (secondaryBadgesRect.width() > 32) {
                QFont secondaryBadgeFont = secondaryFont;
                secondaryBadgeFont.setBold(false);
                QFontMetrics secondaryBadgeMetrics(secondaryBadgeFont);
                const int secondaryBadgeMaxWidth = std::clamp(
                    static_cast<int>(std::round(126.0 * zoomScale)),
                    54,
                    std::max(72, secondaryBadgesRect.width()));
                const int secondaryBadgeGap = badgeGap;
                const int secondaryBadgeBudget = secondaryBadgesRect.width();
                const int maxSecondaryBadgeCount = secondaryBadgesRect.width() < 280 ? 3 : 4;

                QStringList visibleSecondaryBadges;
                int consumedSecondaryBadgeWidth = 0;
                for (int badgeIndex = 0; badgeIndex < secondaryBadges.size(); ++badgeIndex) {
                    const QString badgeText = secondaryBadges.at(badgeIndex).trimmed();
                    if (badgeText.isEmpty()) {
                        continue;
                    }

                    const QString elidedBadgeText = secondaryBadgeMetrics.elidedText(
                        badgeText,
                        Qt::ElideRight,
                        secondaryBadgeMaxWidth - 18);
                    const int badgeWidth = std::max(38, secondaryBadgeMetrics.horizontalAdvance(elidedBadgeText) + 18);
                    const int requiredWidth = visibleSecondaryBadges.isEmpty()
                        ? badgeWidth
                        : badgeWidth + secondaryBadgeGap;
                    if ((!visibleSecondaryBadges.isEmpty() && consumedSecondaryBadgeWidth + requiredWidth > secondaryBadgeBudget)
                        || visibleSecondaryBadges.size() >= maxSecondaryBadgeCount) {
                        const int hiddenCount = secondaryBadges.size() - badgeIndex;
                        if (hiddenCount > 0) {
                            visibleSecondaryBadges.push_back(QStringLiteral("+%1").arg(hiddenCount));
                        }
                        break;
                    }
                    visibleSecondaryBadges.push_back(elidedBadgeText);
                    consumedSecondaryBadgeWidth += requiredWidth;
                }

                const int totalSecondaryBadgeWidth = [&]() {
                    int width = 0;
                    for (int badgeIndex = 0; badgeIndex < visibleSecondaryBadges.size(); ++badgeIndex) {
                        const QString badgeText = visibleSecondaryBadges.at(badgeIndex);
                        width += std::max(38, secondaryBadgeMetrics.horizontalAdvance(badgeText) + 18);
                        if (badgeIndex > 0) {
                            width += secondaryBadgeGap;
                        }
                    }
                    return width;
                }();

                int secondaryBadgeCursor = rtl
                    ? std::max(secondaryBadgesRect.left(), secondaryBadgesRect.right() - totalSecondaryBadgeWidth + 1)
                    : secondaryBadgesRect.left();
                for (const QString &badgeText : visibleSecondaryBadges) {
                    const int chipWidth = std::max(38, secondaryBadgeMetrics.horizontalAdvance(badgeText) + 18);
                    QRect chipRect(secondaryBadgeCursor, secondaryBadgesRect.top(), chipWidth, secondaryBadgesRect.height());
                    secondaryBadgeCursor += chipWidth + secondaryBadgeGap;

                    const QColor secondaryBadgeFillColor = selected
                        ? blendColors(surfaceColor, accentColor.darker(180), 62)
                        : blendColors(surfaceColor, accentColor.darker(155), 42);
                    QColor secondaryBadgeBorderColor = selected
                        ? blendColors(surfaceColor.lighter(122), accentColor.lighter(156), 132)
                        : blendColors(borderStrongColor.lighter(132), accentColor.lighter(146), 92);
                    secondaryBadgeBorderColor.setAlpha(selected ? 214 : 178);
                    painter->setPen(QPen(secondaryBadgeBorderColor, 1.15));
                    painter->setBrush(secondaryBadgeFillColor);
                    painter->drawRoundedRect(chipRect, 10, 10);
                    painter->setFont(secondaryBadgeFont);
                    QColor secondaryBadgeTextColor = primaryTextBaseColor;
                    secondaryBadgeTextColor.setAlpha(selected ? 236 : 226);
                    painter->setPen(secondaryBadgeTextColor);
                    painter->drawText(chipRect, Qt::AlignCenter, badgeText);
                }

                painter->setFont(titleFont);
                painter->setPen(primaryTextColor);
            }
        }

        if (!durationRect.isNull()) {
            QFont durationFont = option.font;
            durationFont.setBold(true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(255, 255, 255, 42) : QColor(255, 255, 255, 22));
            painter->drawRoundedRect(durationRect, 10, 10);
            painter->setFont(durationFont);
            QColor durationTextColor = primaryTextBaseColor;
            durationTextColor.setAlpha(selected ? 245 : 196);
            painter->setPen(durationTextColor);
            painter->drawText(durationRect, Qt::AlignCenter, durationText);
            painter->setFont(titleFont);
        }

        if (showProgressBar && progress >= 0) {
            const QRect progressRect(contentRect.left(), outerRect.bottom() - 11, contentRect.width(), 5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 255, 255, selected ? 68 : 38));
            painter->drawRoundedRect(progressRect, 2.5, 2.5);
            const int fillWidth = std::clamp(static_cast<int>(std::round(progressRect.width() * (std::clamp(progress, 0, 100) / 100.0))), 0, progressRect.width());
            if (fillWidth > 0) {
                QRect fillRect = progressRect;
                fillRect.setWidth(fillWidth);
                painter->setBrush(completed ? QColor(QStringLiteral("#4dd28a")) : accentColor);
                painter->drawRoundedRect(fillRect, 2.5, 2.5);
            }
        }

        painter->restore();
    }
};

class FavoriteItemDelegate final : public QStyledItemDelegate {
public:
    explicit FavoriteItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize baseSize = QStyledItemDelegate::sizeHint(option, index);
        Q_UNUSED(index);
        baseSize.setHeight(std::max(baseSize.height(), 112));
        return baseSize;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (painter == nullptr) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect outerRect = option.rect.adjusted(6, 4, -6, -4);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver);
        const bool rtl = (option.direction == Qt::RightToLeft)
            || (option.widget != nullptr && option.widget->layoutDirection() == Qt::RightToLeft);
        const QColor accentColor = widgetThemeColor(option.widget, "playlistAccentColor", option.palette.highlight().color());
        const QColor baseSurfaceColor = widgetThemeColor(option.widget, "playlistSurfaceColor", option.palette.base().color());
        const QColor borderStrongColor = widgetThemeColor(option.widget, "playlistBorderStrongColor", option.palette.mid().color());
        const QColor primaryTextColor = widgetThemeColor(option.widget, "playlistTextPrimaryColor", option.palette.text().color());
        const QColor selectionBorderColor = widgetThemeColor(
            option.widget,
            "playlistSelectBorderColor",
            QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 132));
        const QColor surfaceColor = selected
            ? QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 42)
            : playlistCardSurfaceColor(baseSurfaceColor, accentColor, hovered, false, false);
        QColor borderColor = selected
            ? selectionBorderColor
            : borderStrongColor;
        if (!selected && borderColor.lightness() < 144) {
            borderColor = borderColor.lighter(148);
        }

        painter->setPen(QPen(borderColor, 1.0));
        painter->setBrush(surfaceColor);
        painter->drawRoundedRect(outerRect, 14, 14);

        const QRect contentRect = outerRect.adjusted(12, 8, -12, -8);
        const QString title = index.data(Qt::DisplayRole).toString().trimmed();
        const QString resolutionText = index.data(revaplayer::application::PlaylistRoles::ResolutionRole).toString().trimmed();
        const QString fileFormatText = index.data(revaplayer::application::PlaylistRoles::FileFormatRole).toString().trimmed().toUpper();
        const double durationSeconds = index.data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble();
        const QString durationText = durationSeconds > 0.0 ? formatPlaybackTime(durationSeconds) : QString {};
        const QIcon thumbnailIcon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const bool textRtl = rtl || textPrefersRightToLeft(title);

        const int thumbnailHeight = std::min(76, std::max(62, contentRect.height() - 10));
        const int thumbnailWidth = std::min(132, std::max(108, contentRect.width() / 4));
        const QRect thumbnailRect(
            rtl ? contentRect.right() - thumbnailWidth : contentRect.left(),
            contentRect.top() + ((contentRect.height() - thumbnailHeight) / 2),
            thumbnailWidth,
            thumbnailHeight);

        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor(255, 255, 255, 28) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 24));
        painter->drawRoundedRect(thumbnailRect, 12, 12);

        if (!thumbnailIcon.isNull()) {
            const QRect thumbRect = thumbnailRect.adjusted(3, 3, -3, -3);
            const QPixmap pixmap = thumbnailIcon.pixmap(thumbRect.size());
            if (!pixmap.isNull()) {
                const QPixmap scaledPixmap = pixmap.scaled(
                    thumbRect.size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation);
                const QPoint thumbTopLeft(
                    thumbRect.left() + ((thumbRect.width() - scaledPixmap.width()) / 2),
                    thumbRect.top() + ((thumbRect.height() - scaledPixmap.height()) / 2));
                QPainterPath clipPath;
                clipPath.addRoundedRect(thumbRect, 10, 10);
                painter->setClipPath(clipPath);
                painter->drawPixmap(thumbTopLeft, scaledPixmap);
                painter->setClipping(false);
            }
        } else {
            const QRect placeholderRect = thumbnailRect.adjusted(4, 4, -4, -4);
            painter->setBrush(selected ? QColor(255, 255, 255, 22) : QColor(255, 255, 255, 12));
            painter->drawRoundedRect(placeholderRect, 10, 10);

            QFont coverFont = option.font;
            coverFont.setBold(true);
            painter->setFont(coverFont);
            painter->setPen(primaryTextColor);
            const QString previewHint = !resolutionText.isEmpty()
                ? resolutionText
                : (!fileFormatText.isEmpty() ? fileFormatText : uiText("Video"));
            painter->drawText(placeholderRect.adjusted(6, 8, -6, -22), Qt::AlignCenter, QStringLiteral("▶"));
            painter->drawText(placeholderRect.adjusted(6, placeholderRect.height() - 24, -6, -6), Qt::AlignCenter, previewHint);
        }

        QRect textRect = contentRect;
        if (rtl) {
            textRect.setRight(thumbnailRect.left() - 12);
        } else {
            textRect.setLeft(thumbnailRect.right() + 12);
        }

        QFont titleFont = option.font;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(primaryTextColor);
        const QFontMetrics titleMetrics(titleFont);
        const QStringList titleLines = playlistWrappedTextLines(title, titleMetrics, textRect.width(), 2);
        const int horizontalAlignment = textRtl ? Qt::AlignRight : Qt::AlignLeft;
        const int titleLineHeight = titleMetrics.lineSpacing();
        const int titleHeight = titleLineHeight * std::max(1, static_cast<int>(titleLines.size()));
        int titleTop = std::max(textRect.top(), textRect.center().y() - (titleHeight / 2) - (durationText.isEmpty() ? 0 : 10));

        if (titleLines.isEmpty()) {
            painter->drawText(
                QRect(textRect.left(), titleTop, textRect.width(), titleLineHeight),
                Qt::AlignVCenter | horizontalAlignment,
                titleMetrics.elidedText(title, Qt::ElideRight, textRect.width()));
        } else {
            for (const QString &line : titleLines) {
                painter->drawText(
                    QRect(textRect.left(), titleTop, textRect.width(), titleLineHeight),
                    Qt::AlignVCenter | horizontalAlignment,
                    line);
                titleTop += titleLineHeight;
            }
        }

        if (!durationText.isEmpty()) {
            QFont badgeFont = option.font;
            badgeFont.setBold(true);
            painter->setFont(badgeFont);
            const QFontMetrics badgeMetrics(badgeFont);
            const int badgeWidth = std::max(54, badgeMetrics.horizontalAdvance(durationText) + 18);
            const QRect badgeRect(
                rtl ? textRect.left() : textRect.right() - badgeWidth,
                contentRect.bottom() - 24,
                badgeWidth,
                22);
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? QColor(255, 255, 255, 52) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 72));
            painter->drawRoundedRect(badgeRect, 10, 10);
            painter->setPen(primaryTextColor);
            painter->drawText(badgeRect, Qt::AlignCenter, durationText);
        }

        painter->restore();
    }
};

class BookmarkItemDelegate final : public QStyledItemDelegate {
public:
    explicit BookmarkItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize baseSize = QStyledItemDelegate::sizeHint(option, index);
        Q_UNUSED(index);
        baseSize.setHeight(std::max(baseSize.height(), 118));
        return baseSize;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (painter == nullptr) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRect outerRect = option.rect.adjusted(6, 4, -6, -4);
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered = (option.state & QStyle::State_MouseOver);
        const bool rtl = (option.direction == Qt::RightToLeft)
            || (option.widget != nullptr && option.widget->layoutDirection() == Qt::RightToLeft);
        const QColor accentColor = widgetThemeColor(option.widget, "playlistAccentColor", option.palette.highlight().color());
        const QColor baseSurfaceColor = widgetThemeColor(option.widget, "playlistSurfaceColor", option.palette.base().color());
        const QColor borderStrongColor = widgetThemeColor(option.widget, "playlistBorderStrongColor", option.palette.mid().color());
        const QColor primaryTextColor = widgetThemeColor(option.widget, "playlistTextPrimaryColor", option.palette.text().color());
        const QColor mutedTextColor = widgetThemeColor(option.widget, "playlistTextMutedColor", option.palette.mid().color().lighter(130));
        const QColor selectionBorderColor = widgetThemeColor(
            option.widget,
            "playlistSelectBorderColor",
            QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 132));
        const QColor surfaceColor = selected
            ? QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 42)
            : playlistCardSurfaceColor(baseSurfaceColor, accentColor, hovered, false, false);
        QColor borderColor = selected
            ? selectionBorderColor
            : borderStrongColor;
        if (!selected && borderColor.lightness() < 144) {
            borderColor = borderColor.lighter(148);
        }

        painter->setPen(QPen(borderColor, 1.0));
        painter->setBrush(surfaceColor);
        painter->drawRoundedRect(outerRect, 14, 14);

        const QRect contentRect = outerRect.adjusted(12, 8, -12, -8);
        const QString title = index.data(Qt::UserRole + 2).toString().trimmed();
        const QString note = index.data(Qt::UserRole + 3).toString().trimmed();
        const QString category = index.data(Qt::UserRole + 4).toString().trimmed();
        const double positionSeconds = index.data(Qt::UserRole + 1).toDouble();
        const QString badgeText = formatPlaybackTime(positionSeconds);
        const QString secondaryText = !note.isEmpty()
            ? (category.isEmpty() ? note : QStringLiteral("%1  •  %2").arg(category, note))
            : (category.isEmpty() ? uiText("Jump to this bookmark instantly from here.") : category);
        const QIcon thumbnailIcon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const bool textRtl = rtl || textPrefersRightToLeft(title) || textPrefersRightToLeft(secondaryText);

        const int thumbnailHeight = std::min(76, std::max(62, contentRect.height() - 10));
        const int thumbnailWidth = std::min(132, std::max(108, contentRect.width() / 4));
        const QRect thumbnailRect(
            rtl ? contentRect.right() - thumbnailWidth : contentRect.left(),
            contentRect.top() + ((contentRect.height() - thumbnailHeight) / 2),
            thumbnailWidth,
            thumbnailHeight);

        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor(255, 255, 255, 28) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 24));
        painter->drawRoundedRect(thumbnailRect, 12, 12);

        if (!thumbnailIcon.isNull()) {
            const QRect thumbRect = thumbnailRect.adjusted(3, 3, -3, -3);
            const QPixmap pixmap = thumbnailIcon.pixmap(thumbRect.size());
            if (!pixmap.isNull()) {
                const QPixmap scaledPixmap = pixmap.scaled(
                    thumbRect.size(),
                    Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
                const QPoint thumbTopLeft(
                    thumbRect.left() + ((thumbRect.width() - scaledPixmap.width()) / 2),
                    thumbRect.top() + ((thumbRect.height() - scaledPixmap.height()) / 2));
                QPainterPath clipPath;
                clipPath.addRoundedRect(thumbRect, 10, 10);
                painter->setClipPath(clipPath);
                painter->drawPixmap(thumbTopLeft, scaledPixmap);
                painter->setClipping(false);
            }
        } else {
            const QRect placeholderRect = thumbnailRect.adjusted(4, 4, -4, -4);
            painter->setBrush(selected ? QColor(255, 255, 255, 22) : QColor(255, 255, 255, 12));
            painter->drawRoundedRect(placeholderRect, 10, 10);

            QFont coverFont = option.font;
            coverFont.setBold(true);
            painter->setFont(coverFont);
            painter->setPen(primaryTextColor);
            painter->drawText(placeholderRect.adjusted(6, 8, -6, -22), Qt::AlignCenter, QStringLiteral("▶"));
            painter->drawText(placeholderRect.adjusted(6, placeholderRect.height() - 24, -6, -6), Qt::AlignCenter, badgeText);
        }

        QRect textRect = contentRect;
        if (rtl) {
            textRect.setRight(thumbnailRect.left() - 12);
        } else {
            textRect.setLeft(thumbnailRect.right() + 12);
        }

        const int horizontalAlignment = textRtl ? Qt::AlignRight : Qt::AlignLeft;
        const int badgeWidth = 70;
        const QRect badgeRect(
            rtl ? textRect.left() : textRect.right() - badgeWidth,
            contentRect.bottom() - 24,
            badgeWidth,
            22);

        QRect titleRect = textRect;
        if (rtl) {
            titleRect.setLeft(badgeRect.right() + 10);
        } else {
            titleRect.setRight(badgeRect.left() - 10);
        }

        QFont titleFont = option.font;
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(primaryTextColor);
        const QFontMetrics titleMetrics(titleFont);
        const QStringList titleLines = playlistWrappedTextLines(
            title.isEmpty() ? uiText("Bookmark") : title,
            titleMetrics,
            titleRect.width(),
            2);
        int titleTop = titleRect.top() + 2;
        for (const QString &line : titleLines) {
            painter->drawText(
                QRect(titleRect.left(), titleTop, titleRect.width(), titleMetrics.lineSpacing()),
                Qt::AlignVCenter | horizontalAlignment,
                line);
            titleTop += titleMetrics.lineSpacing();
        }

        QFont secondaryFont = option.font;
        secondaryFont.setPointSizeF(std::max(secondaryFont.pointSizeF() - 1.0, 9.0));
        painter->setFont(secondaryFont);
        painter->setPen(mutedTextColor);
        const QFontMetrics secondaryMetrics(secondaryFont);
        painter->drawText(
            QRect(titleRect.left(), titleTop + 4, titleRect.width(), secondaryMetrics.lineSpacing() * 2),
            Qt::TextWordWrap | horizontalAlignment,
            secondaryMetrics.elidedText(secondaryText, Qt::ElideRight, titleRect.width() * 2));

        QFont badgeFont = option.font;
        badgeFont.setBold(true);
        painter->setFont(badgeFont);
        painter->setPen(Qt::NoPen);
        painter->setBrush(selected ? QColor(255, 255, 255, 52) : QColor(accentColor.red(), accentColor.green(), accentColor.blue(), 72));
        painter->drawRoundedRect(badgeRect, 10, 10);
        painter->setPen(primaryTextColor);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);

        painter->restore();
    }
};

void forceMenuLeftToRight(QMenu *menu)
{
    if (menu == nullptr) {
        return;
    }

    menu->setLayoutDirection(Qt::LeftToRight);
    const QList<QAction *> menuActions = menu->actions();
    for (QAction *action : menuActions) {
        if (action == nullptr) {
            continue;
        }

        if (QMenu *submenu = action->menu(); submenu != nullptr) {
            forceMenuLeftToRight(submenu);
        }
    }
}

constexpr double kLoopComparisonEpsilon = 0.01;
constexpr int kDefaultThumbnailPopupWidth = 352;
constexpr int kDefaultSceneStepSeconds = 30;
constexpr int kMaxSceneItems = 24;
constexpr int kSceneThumbnailImageRole = Qt::UserRole + 8;
constexpr auto kPlaylistSnapshotPrefix = "playlist_snapshot/";
constexpr auto kFileProfilePrefix = "media_profile/file/";
constexpr auto kTypeProfilePrefix = "media_profile/type/";
constexpr auto kSmartPlaylistRulePrefix = "smart_playlist_rule/";
constexpr auto kFavoriteMediaPrefix = "favorite_media/";
constexpr auto kPinnedCoursePrefix = "playlist_course/";
constexpr auto kPinnedCourseMediaOrderPrefix = "playlist_course_media_order/";
constexpr auto kPlaylistViewPresetPrefix = "playlist_view_preset/";
constexpr auto kPlaylistViewActiveSetting = "playlist/view_active_preset";
constexpr auto kPlaylistViewModeSetting = "playlist/view_mode";
constexpr auto kPlaylistViewDensitySetting = "playlist/view_density";
constexpr auto kPlaylistLayoutFitSetting = "playlist/layout_fit";
constexpr auto kPlaylistCardZoomSetting = "playlist/card_zoom";
constexpr auto kPlaylistThumbnailShapeSetting = "playlist/thumbnail_shape";
constexpr auto kPlaylistViewColumnsSetting = "playlist/view_columns";
constexpr auto kPlaylistDetailsVisibleTabsSetting = "playlist/details_visible_tabs";
constexpr auto kPlaylistDetailsCurrentTabSetting = "playlist/details_current_tab";
constexpr auto kPlaylistSortModeSetting = "playlist/sort_mode";
constexpr auto kMediaMetadataPrefix = "media_meta/";
constexpr auto kMediaScanCachePrefix = "media_scan/";
constexpr auto kLayoutPresetPrefix = "layout_preset/";
constexpr auto kGestureEnabledSetting = "input/gestures/enabled";
constexpr auto kGestureThresholdSetting = "input/gestures/threshold";
constexpr auto kScreenshotFormatSetting = "capture/screenshot_format";
constexpr auto kScreenshotTemplateSetting = "capture/screenshot_template";
constexpr auto kPointerRightEdgeActionFullscreenSetting = "input/pointer/right_edge_action_fullscreen";
constexpr auto kPointerRightEdgeActionWindowedSetting = "input/pointer/right_edge_action_windowed";
constexpr auto kPointerRightEdgeWindowedEnabledSetting = "input/pointer/right_edge_windowed_enabled";
constexpr auto kPointerRightEdgeLeaveActionSetting = "input/pointer/right_edge_leave_action";
constexpr auto kPointerRightEdgeMarginSetting = "input/pointer/right_edge_margin";
constexpr auto kPointerLeaveDelaySetting = "input/pointer/leave_delay_ms";
constexpr auto kPointerKeepControlsVisibleSetting = "input/pointer/keep_controls_visible";
constexpr auto kUiDensitySetting = "ui/density";
constexpr auto kUiAccentSetting = "ui/accent";
constexpr auto kDashboardEnabledSetting = "ui/dashboard_enabled";
constexpr auto kDashboardShowOnIdleSetting = "ui/dashboard_show_on_idle";
constexpr auto kDashboardContinueSectionSetting = "ui/dashboard_section_continue";
constexpr auto kDashboardRecentSectionSetting = "ui/dashboard_section_recent";
constexpr auto kDashboardFavoritesSectionSetting = "ui/dashboard_section_favorites";
constexpr auto kDashboardSavedListsSectionSetting = "ui/dashboard_section_saved_lists";
constexpr auto kFirstRunCompletedSetting = "ui/first_run_completed";
constexpr auto kUiRadiusSetting = "ui/radius_px";
constexpr auto kUiSpacingSetting = "ui/spacing_px";
constexpr auto kUiFontScaleSetting = "ui/font_scale_percent";
constexpr auto kUiFontWeightSetting = "ui/font_weight_value";
constexpr auto kUiLetterSpacingSetting = "ui/letter_spacing_px";
constexpr auto kUiBorderContrastSetting = "ui/border_contrast_percent";
constexpr auto kUiShadowStrengthSetting = "ui/shadow_strength_percent";
constexpr auto kUiBlurStrengthSetting = "ui/blur_strength_percent";
constexpr auto kUiAnimationSpeedSetting = "ui/animation_speed_percent";
constexpr auto kUiAnimationEasingSetting = "ui/animation_easing";
constexpr auto kUiOverlayOpacitySetting = "ui/overlay_opacity_percent";
constexpr auto kStartupCanvasStyleSetting = "ui/startup_canvas_style";
constexpr auto kControlBarShowOpenButtonSetting = "ui/control_bar/show_open_button";
constexpr auto kControlBarShowStopButtonSetting = "ui/control_bar/show_stop_button";
constexpr auto kControlBarShowPlaylistButtonSetting = "ui/control_bar/show_playlist_button";
constexpr auto kControlBarShowDetailsButtonSetting = "ui/control_bar/show_details_button";
constexpr auto kControlBarShowTimeLabelSetting = "ui/control_bar/show_time_label";
constexpr auto kControlBarShowSpeedButtonSetting = "ui/control_bar/show_speed_button";
constexpr auto kControlBarShowRepeatLoopButtonsSetting = "ui/control_bar/show_repeat_loop_buttons";
constexpr auto kControlBarShowTrackMenusSetting = "ui/control_bar/show_track_menus";
constexpr auto kControlBarShowVolumeControlsSetting = "ui/control_bar/show_volume_controls";
constexpr auto kControlBarShowFullscreenButtonSetting = "ui/control_bar/show_fullscreen_button";
constexpr auto kControlBarTimelineThicknessSetting = "ui/control_bar/timeline_thickness";
constexpr auto kControlBarTimelineHandleSizeSetting = "ui/control_bar/timeline_handle_size";
constexpr auto kControlBarVolumeSliderThicknessSetting = "ui/control_bar/volume_slider_thickness";
constexpr auto kControlBarVolumeSliderWidthSetting = "ui/control_bar/volume_slider_width";
constexpr auto kSessionWidePlaybackSpeedSetting = "playback/session_wide_speed";
constexpr int kMediaInfoDialogRefreshIntervalMs = 1000;
constexpr int kDefaultControlBarVolumeSliderThickness = 6;
constexpr int kDefaultControlBarVolumeSliderWidth = 88;
constexpr int kPreviousDefaultControlBarVolumeSliderWidth = 132;
constexpr int kLegacyControlBarVolumeSliderWidth = 176;
constexpr int kMinimumOverlayPanelWidth = 220;
constexpr int kMaximumStoredOverlayPanelWidth = 2400;
constexpr auto kUiAdaptiveEnabledSetting = "ui/adaptive_enabled";
constexpr auto kUiAdaptiveBreakpointSetting = "ui/adaptive_breakpoint_px";
constexpr auto kProgressTrackingModeSetting = "playlist/progress_mode_enabled";
constexpr auto kProgressCompletionThresholdSetting = "playlist/progress_completion_threshold";
constexpr auto kProgressShowBadgesSetting = "playlist/progress_show_badges";
constexpr auto kMouseZoneTopActionSetting = "input/mouse_zone/top";
constexpr auto kMouseZoneBottomActionSetting = "input/mouse_zone/bottom";
constexpr auto kMouseZoneLeftActionSetting = "input/mouse_zone/left";
constexpr auto kMouseZoneRightActionSetting = "input/mouse_zone/right";
constexpr auto kMouseZoneCenterActionSetting = "input/mouse_zone/center";
constexpr auto kMouseZoneRightUserDefinedSetting = "input/mouse_zone/right_user_defined";
constexpr auto kShowPlaylistPanelOnFolderLoadUserDefinedSetting = "playlist/show_panel_on_folder_load_user_defined";
constexpr auto kWindowChromeMigrationV1Setting = "ui/window_chrome_migration_v1";

struct PlaybackPowerPolicy final {
    int playbackUiRefreshIntervalMs {350};
    int playlistPlaybackRefreshIntervalMs {1000};
    int progressSaveDeltaSeconds {12};
    int deferredStartupRefreshMs {1000};
    int playlistThumbnailInitialRows {10};
    int playlistThumbnailLookaheadRows {8};
};

PlaybackPowerPolicy playbackPowerPolicyForProfile(const revaplayer::domain::PlayerProfile profile)
{
    switch (profile) {
    case revaplayer::domain::PlayerProfile::Battery:
        return PlaybackPowerPolicy {
            .playbackUiRefreshIntervalMs = 500,
            .playlistPlaybackRefreshIntervalMs = 2000,
            .progressSaveDeltaSeconds = 15,
            .deferredStartupRefreshMs = 1800,
            .playlistThumbnailInitialRows = 4,
            .playlistThumbnailLookaheadRows = 2,
        };
    case revaplayer::domain::PlayerProfile::Balanced:
        return PlaybackPowerPolicy {
            .playbackUiRefreshIntervalMs = 350,
            .playlistPlaybackRefreshIntervalMs = 1000,
            .progressSaveDeltaSeconds = 12,
            .deferredStartupRefreshMs = 1000,
            .playlistThumbnailInitialRows = 8,
            .playlistThumbnailLookaheadRows = 6,
        };
    case revaplayer::domain::PlayerProfile::Quality:
        return PlaybackPowerPolicy {
            .playbackUiRefreshIntervalMs = 250,
            .playlistPlaybackRefreshIntervalMs = 750,
            .progressSaveDeltaSeconds = 10,
            .deferredStartupRefreshMs = 700,
            .playlistThumbnailInitialRows = 10,
            .playlistThumbnailLookaheadRows = 8,
        };
    }

    return PlaybackPowerPolicy {};
}
constexpr auto kWindowChromeUserDefinedSetting = "ui/window_chrome_user_defined_v1";
constexpr auto kLegacyControlBarShowPanelButtonsSetting = "ui/control_bar/show_panel_buttons";
constexpr auto kVideoZoomStatePrefix = "video/zoom_state/";
constexpr auto kSubtitleRememberedTrackPrefix = "subtitle/remembered_track/";
constexpr auto kSubtitleRememberedDelayPrefix = "subtitle/remembered_delay/";
constexpr int kPointerPanelSuppressionDelayMs = 480;
constexpr double kVideoTransformEpsilon = 0.001;

constexpr std::array<int, 10> kEqualizerFrequencies {
    31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};

struct ShortcutDefinition {
    const char *id;
    const char *label;
    const char *defaultSequence;
};

QString shortcutCategoryForId(const QString &shortcutId)
{
    const QString normalizedId = shortcutId.endsWith(QStringLiteral("_alt"))
        ? shortcutId.left(shortcutId.size() - 4)
        : shortcutId;

    if (normalizedId.startsWith(QStringLiteral("open_"))
        || normalizedId == QStringLiteral("close_application")) {
        return uiText("File / Tools");
    }

    if (normalizedId.startsWith(QStringLiteral("subtitle_"))
        || normalizedId == QStringLiteral("toggle_subtitle_visibility")
        || normalizedId == QStringLiteral("load_subtitle")) {
        return uiText("Subtitles");
    }

    if (normalizedId.startsWith(QStringLiteral("audio_"))
        || normalizedId == QStringLiteral("volume_down")
        || normalizedId == QStringLiteral("volume_up")
        || normalizedId == QStringLiteral("toggle_mute")) {
        return uiText("Audio");
    }

    if (normalizedId.startsWith(QStringLiteral("aspect_"))
        || normalizedId.startsWith(QStringLiteral("crop_"))
        || normalizedId.startsWith(QStringLiteral("rotate_"))
        || normalizedId.startsWith(QStringLiteral("video_zoom_"))
        || normalizedId.startsWith(QStringLiteral("video_pan_"))
        || normalizedId == QStringLiteral("toggle_deinterlace")) {
        return uiText("Video");
    }

    if (normalizedId.startsWith(QStringLiteral("toggle_"))
        || normalizedId.startsWith(QStringLiteral("previous_"))
        || normalizedId.startsWith(QStringLiteral("next_"))
        || normalizedId == QStringLiteral("favorite_current_media")
        || normalizedId.startsWith(QStringLiteral("seek_"))
        || normalizedId == QStringLiteral("play_pause")
        || normalizedId == QStringLiteral("stop")
        || normalizedId.startsWith(QStringLiteral("speed_"))
        || normalizedId == QStringLiteral("take_screenshot")
        || normalizedId == QStringLiteral("add_bookmark")
        || normalizedId == QStringLiteral("delete_bookmark")
        || normalizedId.startsWith(QStringLiteral("set_loop_"))
        || normalizedId == QStringLiteral("clear_loop")
        || normalizedId.startsWith(QStringLiteral("frame_step_"))) {
        return uiText("Playback");
    }

    return uiText("Interface");
}

constexpr ShortcutDefinition kShortcutDefinitions[] {
    {"open_file", "Open File", "Ctrl+O"},
    {"open_folder", "Open Folder", "Ctrl+Shift+O"},
    {"open_url", "Open URL", "Ctrl+L"},
    {"load_subtitle", "Load Subtitle File", "Ctrl+Shift+L"},
    {"toggle_subtitle_visibility", "Toggle Subtitle Visibility", "S"},
    {"subtitle_scale_down", "Decrease Subtitle Scale", "Ctrl+Alt+Down"},
    {"subtitle_scale_up", "Increase Subtitle Scale", "Ctrl+Alt+Up"},
    {"subtitle_scale_reset", "Reset Subtitle Scale", "Ctrl+Alt+0"},
    {"subtitle_position_up", "Move Subtitles Up", "Alt+PageUp"},
    {"subtitle_position_down", "Move Subtitles Down", "Alt+PageDown"},
    {"subtitle_position_reset", "Reset Subtitle Position", "Alt+Home"},
    {"subtitle_style_cycle", "Cycle Subtitle Style Override", "Alt+S"},
    {"show_media_info", "Show Media Info", "Ctrl+I"},
    {"open_preferences", "Open Preferences", "Ctrl+,"},
    {"close_application", "Exit", "Ctrl+Q"},
    {"play_pause", "Play / Pause", "Space"},
    {"stop", "Stop", "Ctrl+."},
    {"take_screenshot", "Take Screenshot", "Ctrl+Shift+S"},
    {"add_bookmark", "Add Bookmark", "Ctrl+B"},
    {"delete_bookmark", "Delete Selected Bookmark", "Del"},
    {"speed_down", "Decrease Playback Speed", "["},
    {"speed_up", "Increase Playback Speed", "]"},
    {"speed_reset", "Reset Playback Speed", "\\"},
    {"subtitle_delay_down", "Subtitle Delay Decrease", "Ctrl+["},
    {"subtitle_delay_up", "Subtitle Delay Increase", "Ctrl+]"},
    {"subtitle_delay_reset", "Reset Subtitle Delay", "Ctrl+\\"},
    {"audio_delay_down", "Audio Delay Down", "Alt+["},
    {"audio_delay_up", "Audio Delay Up", "Alt+]"},
    {"audio_delay_reset", "Reset Audio Delay", "Alt+\\"},
    {"set_loop_start", "Set Loop Start", "A"},
    {"set_loop_end", "Set Loop End", "B"},
    {"clear_loop", "Clear A-B Loop", "Shift+B"},
    {"frame_step_backward", "Previous Frame", ","},
    {"frame_step_forward", "Next Frame", "."},
    {"toggle_always_on_top", "Toggle Always On Top", "Ctrl+Shift+A"},
    {"show_media_information_overlay", "Show Media Information", "Ctrl+Shift+I"},
    {"toggle_playlist", "Toggle Playlist Panel", "Ctrl+P"},
    {"toggle_details", "Toggle Details Panel", "Ctrl+T"},
    {"toggle_fullscreen", "Toggle Fullscreen", "F"},
    {"previous_playlist", "Previous Playlist Item", "PageUp"},
    {"next_playlist", "Next Playlist Item", "PageDown"},
    {"previous_chapter", "Previous Chapter", "Alt+Left"},
    {"next_chapter", "Next Chapter", "Alt+Right"},
    {"seek_backward_short", "Seek Backward (Short Step)", "Left"},
    {"seek_forward_short", "Seek Forward (Short Step)", "Right"},
    {"seek_backward_long", "Seek Backward (Long Step)", "Shift+Left"},
    {"seek_forward_long", "Seek Forward (Long Step)", "Shift+Right"},
    {"volume_down", "Volume Down", "Down"},
    {"volume_up", "Volume Up", "Up"},
    {"toggle_mute", "Toggle Mute", "M"},
    {"video_zoom_out", "Zoom Out", "Ctrl+-"},
    {"video_zoom_in", "Zoom In", "Ctrl+="},
    {"video_zoom_reset", "Reset Zoom", "Ctrl+0"},
    {"video_pan_left", "Pan Left", "Ctrl+Shift+Left"},
    {"video_pan_right", "Pan Right", "Ctrl+Shift+Right"},
    {"video_pan_up", "Pan Up", "Ctrl+Shift+Up"},
    {"video_pan_down", "Pan Down", "Ctrl+Shift+Down"},
    {"toggle_deinterlace", "Toggle Deinterlace", "Ctrl+D"},
    {"aspect_default", "Aspect Ratio Default", "Ctrl+Alt+1"},
    {"aspect_16_9", "Aspect Ratio 16:9", "Ctrl+Alt+2"},
    {"aspect_4_3", "Aspect Ratio 4:3", "Ctrl+Alt+3"},
    {"aspect_1_85", "Aspect Ratio 1.85:1", "Ctrl+Alt+4"},
    {"aspect_2_35", "Aspect Ratio 2.35:1", "Ctrl+Alt+5"},
    {"crop_default", "Crop Default / Container", "Ctrl+Shift+1"},
    {"crop_16_9", "Crop 16:9", "Ctrl+Shift+2"},
    {"crop_1_85", "Crop 1.85:1", "Ctrl+Shift+3"},
    {"crop_2_35", "Crop 2.35:1", "Ctrl+Shift+4"},
    {"crop_disable", "Disable Crop", "Ctrl+Shift+0"},
    {"rotate_default", "Rotation 0°", "Ctrl+R"},
    {"rotate_90", "Rotation 90°", "Ctrl+Alt+R"},
    {"rotate_180", "Rotation 180°", "Ctrl+Alt+Shift+R"},
    {"rotate_270", "Rotation 270°", "Ctrl+Alt+G"},
    {"favorite_current_media", "Toggle Favorite Current Media", "Ctrl+Shift+F"},
    {"play_pause_alt", "Play / Pause (Alt Binding)", ""},
    {"stop_alt", "Stop (Alt Binding)", ""},
    {"toggle_fullscreen_alt", "Toggle Fullscreen (Alt Binding)", ""},
    {"toggle_playlist_alt", "Toggle Playlist Panel (Alt Binding)", ""},
    {"toggle_details_alt", "Toggle Details Panel (Alt Binding)", ""},
    {"seek_backward_short_alt", "Seek Backward Short (Alt Binding)", ""},
    {"seek_forward_short_alt", "Seek Forward Short (Alt Binding)", ""},
    {"seek_backward_long_alt", "Seek Backward Long (Alt Binding)", ""},
    {"seek_forward_long_alt", "Seek Forward Long (Alt Binding)", ""},
    {"volume_down_alt", "Volume Down (Alt Binding)", ""},
    {"volume_up_alt", "Volume Up (Alt Binding)", ""},
    {"toggle_mute_alt", "Toggle Mute (Alt Binding)", ""},
    {"speed_down_alt", "Decrease Speed (Alt Binding)", ""},
    {"speed_up_alt", "Increase Speed (Alt Binding)", ""},
    {"speed_reset_alt", "Reset Speed (Alt Binding)", ""},
    {"subtitle_delay_down_alt", "Subtitle Delay Down (Alt Binding)", ""},
    {"subtitle_delay_up_alt", "Subtitle Delay Up (Alt Binding)", ""},
    {"subtitle_delay_reset_alt", "Subtitle Delay Reset (Alt Binding)", ""},
    {"audio_delay_down_alt", "Audio Delay Down (Alt Binding)", ""},
    {"audio_delay_up_alt", "Audio Delay Up (Alt Binding)", ""},
    {"audio_delay_reset_alt", "Audio Delay Reset (Alt Binding)", ""},
    {"subtitle_scale_down_alt", "Subtitle Scale Down (Alt Binding)", ""},
    {"subtitle_scale_up_alt", "Subtitle Scale Up (Alt Binding)", ""},
    {"subtitle_position_up_alt", "Subtitle Position Up (Alt Binding)", ""},
    {"subtitle_position_down_alt", "Subtitle Position Down (Alt Binding)", ""},
    {"video_zoom_out_alt", "Video Zoom Out (Alt Binding)", ""},
    {"video_zoom_in_alt", "Video Zoom In (Alt Binding)", ""},
    {"video_zoom_reset_alt", "Video Zoom Reset (Alt Binding)", ""},
    {"previous_playlist_alt", "Previous Playlist Item (Alt Binding)", ""},
    {"next_playlist_alt", "Next Playlist Item (Alt Binding)", ""},
    {"previous_chapter_alt", "Previous Chapter (Alt Binding)", ""},
    {"next_chapter_alt", "Next Chapter (Alt Binding)", ""},
    {"add_bookmark_alt", "Add Bookmark (Alt Binding)", ""},
    {"take_screenshot_alt", "Take Screenshot (Alt Binding)", ""},
    {"open_preferences_alt", "Open Preferences (Alt Binding)", ""},
    {"show_media_info_alt", "Show Media Info (Alt Binding)", ""},
    {"open_file_alt", "Open File (Alt Binding)", ""},
    {"open_folder_alt", "Open Folder (Alt Binding)", ""},
    {"open_url_alt", "Open URL (Alt Binding)", ""},
};

QKeySequence portableShortcut(const char *portableText)
{
    return portableText == nullptr || *portableText == '\0'
        ? QKeySequence {}
        : QKeySequence(QString::fromLatin1(portableText), QKeySequence::PortableText);
}

QKeySequence keySequenceForKeyEvent(const QKeyEvent *event)
{
    if (event == nullptr || event->key() == Qt::Key_unknown) {
        return {};
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QKeySequence(event->keyCombination());
#else
    const int modifiers = static_cast<int>(
        event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    return QKeySequence(modifiers | event->key());
#endif
}

bool isTextInputLikeWidget(const QWidget *widget)
{
    return widget != nullptr
        && (widget->inherits("QLineEdit")
            || widget->inherits("QPlainTextEdit")
            || widget->inherits("QTextEdit")
            || widget->inherits("QAbstractSpinBox")
            || widget->inherits("QComboBox")
            || widget->inherits("QKeySequenceEdit"));
}

bool isSupportedMediaFile(const QFileInfo &fileInfo)
{
    static const QSet<QString> kExtensions {
        QStringLiteral("3gp"),
        QStringLiteral("aac"),
        QStringLiteral("avi"),
        QStringLiteral("flac"),
        QStringLiteral("m4a"),
        QStringLiteral("m4v"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("mp3"),
        QStringLiteral("mp4"),
        QStringLiteral("mpeg"),
        QStringLiteral("mpg"),
        QStringLiteral("ogg"),
        QStringLiteral("opus"),
        QStringLiteral("wav"),
        QStringLiteral("webm"),
        QStringLiteral("wmv"),
    };

    return fileInfo.exists()
        && fileInfo.isFile()
        && kExtensions.contains(fileInfo.suffix().trimmed().toLower());
}

QString normalizedVideoCodecLabel(QString codec)
{
    codec = codec.trimmed().toUpper();
    if (codec == QStringLiteral("AV01")) {
        return QStringLiteral("AV1");
    }
    if (codec.startsWith(QStringLiteral("VP09"))) {
        return QStringLiteral("VP9");
    }
    if (codec.startsWith(QStringLiteral("AVC"))) {
        return QStringLiteral("H.264");
    }
    if (codec.startsWith(QStringLiteral("HEV")) || codec.startsWith(QStringLiteral("HVC"))) {
        return QStringLiteral("H.265");
    }
    return codec;
}

QString buildSimpleVideoQualityLabel(const revaplayer::domain::TrackInfo &track)
{
    if (track.height > 0) {
        return QStringLiteral("%1p").arg(track.height);
    }
    if (track.width > 0) {
        return QStringLiteral("%1p").arg(track.width);
    }
    if (!track.title.trimmed().isEmpty()) {
        return track.title.trimmed();
    }
    return uiText("Quality");
}

QString buildSimpleSubtitleLabel(const revaplayer::domain::TrackInfo &track)
{
    QStringList parts;
    if (!track.title.trimmed().isEmpty()) {
        parts.push_back(track.title.trimmed());
    }
    if (!track.language.trimmed().isEmpty()) {
        parts.push_back(track.language.trimmed().toUpper());
    }
    if (track.external) {
        parts.push_back(uiText("External"));
    }
    if (parts.isEmpty()) {
        parts.push_back(QStringLiteral("Subtitle %1").arg(track.id));
    }
    return parts.join(QStringLiteral(" | "));
}

int videoTrackCodecPreference(const revaplayer::domain::TrackInfo &track)
{
    const QString codec = normalizedVideoCodecLabel(track.codec);
    if (codec == QStringLiteral("H.264")) {
        return 4;
    }
    if (codec == QStringLiteral("VP9")) {
        return 3;
    }
    if (codec == QStringLiteral("AV1")) {
        return 2;
    }
    if (codec == QStringLiteral("H.265")) {
        return 1;
    }
    return 0;
}

bool shouldPreferVideoQualityTrack(const revaplayer::domain::TrackInfo &candidate,
                                   const revaplayer::domain::TrackInfo &current)
{
    if (candidate.selected != current.selected) {
        return candidate.selected;
    }

    const int candidateCodecPreference = videoTrackCodecPreference(candidate);
    const int currentCodecPreference = videoTrackCodecPreference(current);
    if (candidateCodecPreference != currentCodecPreference) {
        return candidateCodecPreference > currentCodecPreference;
    }

    const int candidateRoundedFps = static_cast<int>(std::lround(candidate.fps));
    const int currentRoundedFps = static_cast<int>(std::lround(current.fps));
    if (candidateRoundedFps != currentRoundedFps) {
        return candidateRoundedFps > currentRoundedFps;
    }

    if (candidate.width != current.width) {
        return candidate.width > current.width;
    }

    return candidate.id < current.id;
}

QVector<revaplayer::domain::TrackInfo> collapsedVideoQualityTracks(
    const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    QVector<revaplayer::domain::TrackInfo> collapsedTracks;
    QHash<int, int> indexByQuality;

    for (const auto &track : tracks) {
        if (track.type != revaplayer::domain::TrackType::Video) {
            continue;
        }

        const int qualityKey = track.height > 0 ? track.height : (track.width > 0 ? track.width : -std::max(1, track.id));
        const auto existingIt = indexByQuality.constFind(qualityKey);
        if (existingIt == indexByQuality.cend()) {
            indexByQuality.insert(qualityKey, collapsedTracks.size());
            collapsedTracks.push_back(track);
            continue;
        }

        if (shouldPreferVideoQualityTrack(track, collapsedTracks[existingIt.value()])) {
            collapsedTracks[existingIt.value()] = track;
        }
    }

    std::sort(collapsedTracks.begin(), collapsedTracks.end(), [](const auto &left, const auto &right) {
        const int leftQuality = left.height > 0 ? left.height : left.width;
        const int rightQuality = right.height > 0 ? right.height : right.width;
        if (leftQuality != rightQuality) {
            return leftQuality > rightQuality;
        }
        if (left.width != right.width) {
            return left.width > right.width;
        }
        if (left.selected != right.selected) {
            return left.selected;
        }
        return left.id < right.id;
    });

    return collapsedTracks;
}

QString buildTrackTitle(const revaplayer::domain::TrackInfo &track)
{
    const auto buildVideoTrackLabel = [&](const bool includeTrackId) {
        QStringList parts;
        if (track.height > 0) {
            QString qualityLabel = QStringLiteral("%1p").arg(track.height);
            if (track.fps > 30.5) {
                qualityLabel += QString::number(std::max(1, static_cast<int>(std::lround(track.fps))));
            }
            parts.push_back(qualityLabel);
        }
        if (track.width > 0 && track.height > 0) {
            parts.push_back(QStringLiteral("%1x%2").arg(track.width).arg(track.height));
        }
        const QString codecLabel = normalizedVideoCodecLabel(track.codec);
        if (!codecLabel.isEmpty()) {
            parts.push_back(codecLabel);
        }
        if (parts.isEmpty() && !track.title.trimmed().isEmpty()) {
            parts.push_back(track.title.trimmed());
        }
        if (parts.isEmpty()) {
            parts.push_back(QStringLiteral("Video"));
        }
        if (includeTrackId) {
            parts.push_back(QStringLiteral("#%1").arg(track.id));
        }
        return parts.join(QStringLiteral(" | "));
    };

    if (track.type == revaplayer::domain::TrackType::Video) {
        return buildVideoTrackLabel(true);
    }

    QStringList parts;
    parts << QStringLiteral("#%1").arg(track.id);

    if (!track.title.isEmpty()) {
        parts << track.title;
    }
    if (!track.language.isEmpty()) {
        parts << track.language.toUpper();
    }
    if (track.external) {
        parts << QStringLiteral("External");
    }

    return parts.join(QStringLiteral(" | "));
}

QString formatPlaybackTime(const double timeSeconds)
{
    const int totalSeconds = std::max(0, static_cast<int>(std::lround(timeSeconds)));
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds % 3600) / 60;
    const int seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

bool isFolderBrowserSource(const QString &source);
bool isFolderBrowserBackSource(const QString &source);
QString folderBrowserPathFromSource(const QString &source);
QString folderDisplayName(const QString &folderPath);

QString fileNameFromPathLikeSource(const QString &source)
{
    QString pathLike = source.trimmed();
    if (pathLike.isEmpty()) {
        return {};
    }

    const QUrl url(pathLike);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (!url.isLocalFile()) {
            return url.toDisplayString(QUrl::RemovePassword);
        }
        pathLike = url.toLocalFile();
    }

    pathLike.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (pathLike.size() > 1 && pathLike.endsWith(QLatin1Char('/'))) {
        pathLike.chop(1);
    }

    const qsizetype slashIndex = pathLike.lastIndexOf(QLatin1Char('/'));
    return slashIndex >= 0 ? pathLike.mid(slashIndex + 1).trimmed() : pathLike.trimmed();
}

QString cleanAbsolutePathWithoutFilesystemLookup(const QString &path)
{
    QString pathLike = path.trimmed();
    if (pathLike.isEmpty()) {
        return {};
    }

    const QUrl url(pathLike);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (!url.isLocalFile()) {
            return {};
        }
        pathLike = url.toLocalFile();
    }

    pathLike = QDir::fromNativeSeparators(pathLike);
    if (QDir::isAbsolutePath(pathLike)) {
        return QDir::cleanPath(pathLike);
    }

    return QDir::cleanPath(QDir::current().absoluteFilePath(pathLike));
}

QString displayTitleForHistory(const QString &source, const QString &currentTitle)
{
    if (!currentTitle.trimmed().isEmpty()) {
        return currentTitle.trimmed();
    }

    if (isFolderBrowserSource(source) || isFolderBrowserBackSource(source)) {
        const QString folderPath = folderBrowserPathFromSource(source);
        return folderDisplayName(folderPath);
    }

    const QUrl url(source);
    if (url.isValid() && !url.scheme().isEmpty() && !url.isLocalFile()) {
        return url.toDisplayString(QUrl::RemovePassword);
    }

    const QString fileName = fileNameFromPathLikeSource(source);
    return fileName.isEmpty() ? source.trimmed() : fileName;
}

QString formatHistoryTimestamp(const QString &isoTimestamp)
{
    const QDateTime parsed = QDateTime::fromString(isoTimestamp.trimmed(), Qt::ISODate);
    if (!parsed.isValid()) {
        return QStringLiteral("Unknown date");
    }

    return parsed.toLocalTime().toString(QStringLiteral("MMM d • HH:mm"));
}

QString formatHistorySummary(const revaplayer::infrastructure::storage::PlaybackHistoryRecord &entry)
{
    if (entry.durationSeconds > 0.0) {
        const QString progressText = entry.completed
            ? QStringLiteral("Completed")
            : QStringLiteral("%1 / %2")
                  .arg(formatPlaybackTime(entry.positionSeconds), formatPlaybackTime(entry.durationSeconds));
        return QStringLiteral("%1 • %2").arg(progressText, formatHistoryTimestamp(entry.lastOpenedAt));
    }

    return formatHistoryTimestamp(entry.lastOpenedAt);
}

QIcon buildScenePlaceholderIcon(const QSize &size, const QString &timeText)
{
    const QPalette palette = QApplication::palette();
    const QColor baseColor = palette.color(QPalette::Base);
    const QColor altColor = palette.color(QPalette::AlternateBase);
    const QColor midColor = palette.color(QPalette::Mid);
    const QColor textColor = palette.color(QPalette::Text);

    QPixmap pixmap(size);
    pixmap.fill(baseColor.darker(105));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(altColor);
    painter.setPen(QPen(midColor, 1.0));
    painter.drawRoundedRect(pixmap.rect().adjusted(1, 1, -2, -2), 10, 10);

    painter.setPen(textColor);
    painter.setFont(QFont(QStringLiteral("Noto Sans"), 11, QFont::DemiBold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, timeText);
    return QIcon(pixmap);
}

QString defaultBookmarkTitle(const QString &mediaTitle, const double positionSeconds)
{
    if (!mediaTitle.trimmed().isEmpty()) {
        return QStringLiteral("%1 @ %2").arg(mediaTitle.trimmed(), formatPlaybackTime(positionSeconds));
    }

    return uiText("Bookmark at %1").arg(formatPlaybackTime(positionSeconds));
}

struct DroppedMediaPayload {
    QStringList files;
    QStringList remoteUrls;

    [[nodiscard]] bool isEmpty() const
    {
        return files.isEmpty() && remoteUrls.isEmpty();
    }
};

struct PinnedCourseFolder final {
    QString key;
    QString label;
    QString path;
    QString category;
    QString description;
    QString colorId;
    QString iconId;
    int order {0};
};

QVector<PinnedCourseFolder> loadPinnedCourses(const revaplayer::application::SettingsController *settingsController);

struct MediaMetadata final {
    QStringList tags;
    QString notes;
    QString difficulty;
};

constexpr auto kFolderBrowserSourceScheme = "reva-folder";
constexpr auto kFolderBrowserBackSourceScheme = "reva-folder-back";

struct PlaylistViewPreset final {
    QString key;
    QString name;
    QString iconId;
    QString colorId;
    QString cardZoomId;
    QString thumbnailShapeId;
    bool secondaryTextVisible {false};
    QString sortModeId;
    QString filterText;
    QString currentDetailsTabId;
    QStringList columns;
    QStringList visibleDetailsTabs;
    bool showInTabs {true};
    int order {0};
};

struct PlaylistPanelSettingsState final {
    QString cardZoomId;
    QString thumbnailShapeId;
    bool secondaryTextVisible {false};
    QString sortModeId;
    bool showFullPaths {false};
    bool showIndexPrefixes {false};
    QStringList columns;
};

DroppedMediaPayload parseDroppedMedia(const QMimeData *mimeData)
{
    DroppedMediaPayload payload;
    if (mimeData == nullptr) {
        return payload;
    }

    const auto urls = mimeData->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            const QFileInfo fileInfo(url.toLocalFile());
            if (fileInfo.exists()) {
                payload.files << fileInfo.absoluteFilePath();
            }
        } else if (url.isValid() && !url.scheme().isEmpty()) {
            payload.remoteUrls << url.toString();
        }
    }

    if (!payload.isEmpty() || !mimeData->hasText()) {
        payload.files.removeDuplicates();
        payload.remoteUrls.removeDuplicates();
        return payload;
    }

    const QString droppedText = mimeData->text().trimmed();
    if (droppedText.isEmpty()) {
        return payload;
    }

    const QUrl normalized = QUrl::fromUserInput(droppedText);
    if (normalized.isLocalFile()) {
        const QFileInfo fileInfo(normalized.toLocalFile());
        if (fileInfo.exists()) {
            payload.files << fileInfo.absoluteFilePath();
        }
    } else if (normalized.isValid() && !normalized.scheme().isEmpty()) {
        payload.remoteUrls << normalized.toString();
    }

    payload.files.removeDuplicates();
    payload.remoteUrls.removeDuplicates();
    return payload;
}

QString encodeSettingKeySegment(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QString decodeSettingKeySegment(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}

QString videoZoomStateStorageKeyForSource(const QString &source)
{
    const QString trimmed = source.trimmed();
    return trimmed.isEmpty()
        ? QString {}
        : QStringLiteral("%1%2").arg(QString::fromLatin1(kVideoZoomStatePrefix), encodeSettingKeySegment(trimmed));
}

QString rememberedSubtitleTrackStorageKeyForSource(const QString &source)
{
    const QString trimmed = source.trimmed();
    return trimmed.isEmpty()
        ? QString {}
        : QStringLiteral("%1%2").arg(QString::fromLatin1(kSubtitleRememberedTrackPrefix), encodeSettingKeySegment(trimmed));
}

QString rememberedSubtitleDelayStorageKeyForSource(const QString &source)
{
    const QString trimmed = source.trimmed();
    return trimmed.isEmpty()
        ? QString {}
        : QStringLiteral("%1%2").arg(QString::fromLatin1(kSubtitleRememberedDelayPrefix), encodeSettingKeySegment(trimmed));
}

QString subtitleDelayDisplayText(const double delaySeconds)
{
    QString signedValue = QString::number(delaySeconds, 'f', 2);
    if (std::abs(delaySeconds) < 0.0005) {
        signedValue = QStringLiteral("0");
    } else if (delaySeconds > 0.0) {
        signedValue.prepend(QChar('+'));
    }
    while (signedValue.endsWith(QStringLiteral("0"))) {
        signedValue.chop(1);
    }
    if (signedValue.endsWith(QStringLiteral("."))) {
        signedValue.chop(1);
    }
    if (signedValue == QStringLiteral("-0")) {
        signedValue = QStringLiteral("0");
    }
    const bool arabicUi = revaplayer::application::currentUiLanguage() == QStringLiteral("ar");
    return signedValue + (arabicUi ? QStringLiteral("ث") : QStringLiteral("s"));
}

QString subtitleTrackChoiceSignature(const revaplayer::domain::TrackInfo &track)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(track.language.trimmed().toLower(),
             track.title.trimmed().toLower(),
             track.external ? QStringLiteral("1") : QStringLiteral("0"),
             track.codec.trimmed().toLower());
}

QString folderBrowserSourceForPath(const QString &folderPath, const bool backEntry = false)
{
    const QString absolutePath = QFileInfo(folderPath).absoluteFilePath();
    if (absolutePath.isEmpty()) {
        return {};
    }

    return QStringLiteral("%1://%2")
        .arg(backEntry ? QString::fromLatin1(kFolderBrowserBackSourceScheme) : QString::fromLatin1(kFolderBrowserSourceScheme),
             QString::fromUtf8(QUrl::toPercentEncoding(absolutePath)));
}

bool sourceUsesCustomScheme(const QString &source, const char *scheme)
{
    const QString trimmedSource = source.trimmed();
    const QString schemeText = QString::fromLatin1(scheme);
    const QUrl url(trimmedSource);
    return (url.isValid() && url.scheme() == schemeText)
        || trimmedSource.startsWith(schemeText + QStringLiteral("://"));
}

bool isFolderBrowserSource(const QString &source)
{
    return sourceUsesCustomScheme(source, kFolderBrowserSourceScheme);
}

bool isFolderBrowserBackSource(const QString &source)
{
    return sourceUsesCustomScheme(source, kFolderBrowserBackSourceScheme);
}

QString folderBrowserPathFromSource(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    const bool folderSource = sourceUsesCustomScheme(trimmedSource, kFolderBrowserSourceScheme);
    const bool backSource = sourceUsesCustomScheme(trimmedSource, kFolderBrowserBackSourceScheme);
    if (!folderSource && !backSource) {
        return {};
    }

    const QString schemeText = folderSource
        ? QString::fromLatin1(kFolderBrowserSourceScheme)
        : QString::fromLatin1(kFolderBrowserBackSourceScheme);
    const QString prefix = schemeText + QStringLiteral("://");
    const QString encodedPath = trimmedSource.startsWith(prefix)
        ? trimmedSource.mid(prefix.size())
        : QString {};
    return QUrl::fromPercentEncoding(encodedPath.toUtf8());
}

QString folderDisplayName(const QString &folderPath)
{
    const QFileInfo folderInfo(folderPath);
    const QString absolutePath = folderInfo.absoluteFilePath();
    if (absolutePath.trimmed().isEmpty()) {
        return {};
    }

    const QString name = folderInfo.fileName().trimmed();
    return name.isEmpty() ? QDir::toNativeSeparators(absolutePath) : name;
}

QString customSettingValue(const revaplayer::application::SettingsController *settingsController,
                           const char *key,
                           const QString &defaultValue = {})
{
    return settingsController != nullptr
        ? settingsController->customValue(QString::fromLatin1(key), defaultValue)
        : defaultValue;
}

double persistedPlaybackPosition(const double positionSeconds,
                                 const double,
                                 const bool)
{
    return std::max(0.0, positionSeconds);
}

bool customSettingFlag(const revaplayer::application::SettingsController *settingsController,
                       const char *key,
                       const bool defaultValue)
{
    const QString rawValue = customSettingValue(
        settingsController,
        key,
        defaultValue ? QStringLiteral("1") : QStringLiteral("0")).trimmed().toLower();
    if (rawValue.isEmpty()) {
        return defaultValue;
    }

    return rawValue != QStringLiteral("0")
        && rawValue != QStringLiteral("false")
        && rawValue != QStringLiteral("no")
        && rawValue != QStringLiteral("off");
}

bool rawCustomFlagEnabled(QString value)
{
    value = value.trimmed().toLower();
    if (value.isEmpty()) {
        return false;
    }

    return value != QStringLiteral("0")
        && value != QStringLiteral("false")
        && value != QStringLiteral("no")
        && value != QStringLiteral("off");
}

struct ControlBarPanelButtonSettings final {
    bool showPlaylistButton {true};
    bool showDetailsButton {true};
};

ControlBarPanelButtonSettings resolvedControlBarPanelButtonSettings(
    const revaplayer::application::SettingsController *settingsController)
{
    const QString playlistRaw = customSettingValue(
        settingsController,
        kControlBarShowPlaylistButtonSetting).trimmed();
    const QString detailsRaw = customSettingValue(
        settingsController,
        kControlBarShowDetailsButtonSetting).trimmed();
    const QString legacyPanelRaw = customSettingValue(
        settingsController,
        kLegacyControlBarShowPanelButtonsSetting).trimmed();

    ControlBarPanelButtonSettings resolved;
    if (!playlistRaw.isEmpty()) {
        resolved.showPlaylistButton = rawCustomFlagEnabled(playlistRaw);
        resolved.showDetailsButton = detailsRaw.isEmpty() ? true : rawCustomFlagEnabled(detailsRaw);
        return resolved;
    }

    const bool legacyPanelVisible = legacyPanelRaw.isEmpty() ? true : rawCustomFlagEnabled(legacyPanelRaw);
    resolved.showPlaylistButton = legacyPanelVisible;
    resolved.showDetailsButton = legacyPanelVisible
        && (detailsRaw.isEmpty() ? true : rawCustomFlagEnabled(detailsRaw));
    return resolved;
}

void applyLegacySidePanelBehaviorMigration(revaplayer::application::SettingsController *settingsController)
{
    if (settingsController == nullptr) {
        return;
    }

    constexpr auto kSidePanelBehaviorMigrationSetting = "ui/side_panel_behavior_migration_v1";
    if (!customSettingFlag(settingsController, kSidePanelBehaviorMigrationSetting, false)) {
        const QString overlayRaw = settingsController->customValue(QStringLiteral("ui/overlay_panels_on_video")).trimmed();
        const QString windowedRevealRaw = settingsController->customValue(QString::fromLatin1(kPointerRightEdgeWindowedEnabledSetting)).trimmed();
        const QString windowedActionRaw = settingsController->customValue(QString::fromLatin1(kPointerRightEdgeActionWindowedSetting)).trimmed().toLower();
        const bool legacyOverlayDisabled = !overlayRaw.isEmpty() && !rawCustomFlagEnabled(overlayRaw);
        const bool legacyWindowedRevealDisabled = !windowedRevealRaw.isEmpty() && !rawCustomFlagEnabled(windowedRevealRaw);
        const bool legacyWindowedActionDisabled = windowedActionRaw == QStringLiteral("none");

        if (legacyOverlayDisabled && legacyWindowedRevealDisabled && legacyWindowedActionDisabled) {
            settingsController->setOverlayPanelsOnVideo(true);
            settingsController->setFullscreenSideSelectorEnabled(true);
            settingsController->setCustomValue(QString::fromLatin1(kSidePanelBehaviorMigrationSetting), QStringLiteral("1"));
        }
    }

    constexpr auto kSidePanelBehaviorMigrationV2Setting = "ui/side_panel_behavior_migration_v2";
    if (!customSettingFlag(settingsController, kSidePanelBehaviorMigrationV2Setting, false)) {
        const QString rightEdgeLeaveRaw = settingsController->customValue(QString::fromLatin1(kPointerRightEdgeLeaveActionSetting)).trimmed().toLower();
        settingsController->setCustomValue(QString::fromLatin1(kPointerRightEdgeWindowedEnabledSetting), QStringLiteral("0"));
        settingsController->setCustomValue(QString::fromLatin1(kPointerRightEdgeActionWindowedSetting), QStringLiteral("none"));
        if (rightEdgeLeaveRaw.isEmpty() || rightEdgeLeaveRaw == QStringLiteral("keep")) {
            settingsController->setCustomValue(QString::fromLatin1(kPointerRightEdgeLeaveActionSetting), QStringLiteral("hide_panel"));
        }
        settingsController->setCustomValue(QString::fromLatin1(kSidePanelBehaviorMigrationV2Setting), QStringLiteral("1"));
    }

    constexpr auto kInputBehaviorMigrationV1Setting = "ui/input_behavior_migration_v1";
    if (!customSettingFlag(settingsController, kInputBehaviorMigrationV1Setting, false)) {
        settingsController->setCustomValue(QString::fromLatin1(kInputBehaviorMigrationV1Setting), QStringLiteral("1"));
    }

    constexpr auto kInputBehaviorMigrationV2Setting = "ui/input_behavior_migration_v2";
    if (!customSettingFlag(settingsController, kInputBehaviorMigrationV2Setting, false)) {
        const QString doubleClickActionRaw = settingsController->customValue(QStringLiteral("input/double_click_action")).trimmed().toLower();
        const bool looksLikeAccidentalPlayPauseMigration =
            customSettingFlag(settingsController, kInputBehaviorMigrationV1Setting, false)
            && (doubleClickActionRaw.isEmpty() || doubleClickActionRaw == QStringLiteral("play_pause"))
            && !settingsController->doubleClickFullscreenEnabled();
        if (looksLikeAccidentalPlayPauseMigration) {
            settingsController->setDoubleClickAction(QStringLiteral("fullscreen"));
            settingsController->setDoubleClickFullscreenEnabled(true);
        }
        settingsController->setCustomValue(QString::fromLatin1(kInputBehaviorMigrationV2Setting), QStringLiteral("1"));
    }

    constexpr auto kInputBehaviorMigrationV3Setting = "ui/input_behavior_migration_v3";
    if (!customSettingFlag(settingsController, kInputBehaviorMigrationV3Setting, false)) {
        const QString effectiveDoubleClickActionRaw =
            settingsController->customValue(QStringLiteral("input/double_click_action")).trimmed().toLower();
        if (effectiveDoubleClickActionRaw.isEmpty()) {
            settingsController->setDoubleClickAction(QStringLiteral("play_pause"));
            settingsController->setDoubleClickFullscreenEnabled(false);
        }
        settingsController->setCustomValue(QString::fromLatin1(kInputBehaviorMigrationV3Setting), QStringLiteral("1"));
    }

    constexpr auto kInputBehaviorMigrationV4Setting = "ui/input_behavior_migration_v4";
    if (!customSettingFlag(settingsController, kInputBehaviorMigrationV4Setting, false)) {
        if (!customSettingFlag(settingsController, kMouseZoneRightUserDefinedSetting, false)) {
            settingsController->setCustomValue(QString::fromLatin1(kMouseZoneRightActionSetting), QStringLiteral("none"));
        }
        if (!customSettingFlag(settingsController, kShowPlaylistPanelOnFolderLoadUserDefinedSetting, false)) {
            settingsController->setShowPlaylistPanelOnFolderLoad(false);
        }
        settingsController->setCustomValue(QString::fromLatin1(kInputBehaviorMigrationV4Setting), QStringLiteral("1"));
    }
}

void applyLegacyWindowChromeMigration(revaplayer::application::SettingsController *settingsController)
{
    if (settingsController == nullptr) {
        return;
    }

    if (customSettingFlag(settingsController, kWindowChromeMigrationV1Setting, false)) {
        return;
    }

    settingsController->setCustomValue(QString::fromLatin1(kWindowChromeMigrationV1Setting), QStringLiteral("1"));
}

int customSettingInt(const revaplayer::application::SettingsController *settingsController,
                     const char *key,
                     const int defaultValue,
                     const int minimumValue,
                     const int maximumValue)
{
    bool ok = false;
    const int parsed = customSettingValue(
        settingsController,
        key,
        QString::number(defaultValue)).toInt(&ok);
    return std::clamp(ok ? parsed : defaultValue, minimumValue, maximumValue);
}

int resolvedControlBarVolumeSliderWidth(const revaplayer::application::SettingsController *settingsController)
{
    const QString rawValue = customSettingValue(settingsController, kControlBarVolumeSliderWidthSetting).trimmed();
    if (rawValue.isEmpty()) {
        return kDefaultControlBarVolumeSliderWidth;
    }

    bool ok = false;
    const int parsed = rawValue.toInt(&ok);
    if (!ok) {
        return kDefaultControlBarVolumeSliderWidth;
    }
    if (parsed == kPreviousDefaultControlBarVolumeSliderWidth || parsed == kLegacyControlBarVolumeSliderWidth) {
        return kDefaultControlBarVolumeSliderWidth;
    }
    return std::clamp(parsed, 0, 96);
}

int clampPersistedOverlayPanelWidth(const int width)
{
    return std::clamp(width, 0, kMaximumStoredOverlayPanelWidth);
}

double customSettingDouble(const revaplayer::application::SettingsController *settingsController,
                           const char *key,
                           const double defaultValue,
                           const double minimumValue,
                           const double maximumValue)
{
    bool ok = false;
    const double parsed = customSettingValue(
        settingsController,
        key,
        QString::number(defaultValue, 'f', 2)).toDouble(&ok);
    return std::clamp(ok ? parsed : defaultValue, minimumValue, maximumValue);
}

bool videoViewportTransformActive(const double zoomFactor, const double alignX, const double alignY)
{
    return std::abs(zoomFactor - 1.0) > kVideoTransformEpsilon
        || std::abs(alignX) > kVideoTransformEpsilon
        || std::abs(alignY) > kVideoTransformEpsilon;
}

QString gestureActionSettingKey(const QString &directionId)
{
    return QStringLiteral("input/gestures/%1").arg(directionId.trimmed().toLower());
}

QString defaultGestureAction(const QString &directionId)
{
    const QString normalized = directionId.trimmed().toLower();
    if (normalized == QStringLiteral("left")) {
        return QStringLiteral("seek_backward_short");
    }
    if (normalized == QStringLiteral("right")) {
        return QStringLiteral("seek_forward_short");
    }
    if (normalized == QStringLiteral("up")) {
        return QStringLiteral("volume_up");
    }
    if (normalized == QStringLiteral("down")) {
        return QStringLiteral("volume_down");
    }
    return QStringLiteral("none");
}

QString normalizeGestureAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("seek_backward_short"),
        QStringLiteral("seek_forward_short"),
        QStringLiteral("volume_up"),
        QStringLiteral("volume_down"),
        QStringLiteral("speed_up"),
        QStringLiteral("speed_down"),
        QStringLiteral("playlist"),
        QStringLiteral("details"),
        QStringLiteral("play_pause"),
        QStringLiteral("subtitle_delay_up"),
        QStringLiteral("subtitle_delay_down"),
        QStringLiteral("none"),
    };
    return allowed.contains(actionId) ? actionId : QStringLiteral("none");
}

QString gestureActionLabel(const QString &actionId)
{
    const QString normalized = normalizeGestureAction(actionId);
    if (normalized == QStringLiteral("seek_backward_short")) {
        return uiText("Seek Backward");
    }
    if (normalized == QStringLiteral("seek_forward_short")) {
        return uiText("Seek Forward");
    }
    if (normalized == QStringLiteral("volume_up")) {
        return uiText("Volume Up");
    }
    if (normalized == QStringLiteral("volume_down")) {
        return uiText("Volume Down");
    }
    if (normalized == QStringLiteral("speed_up")) {
        return uiText("Speed Up");
    }
    if (normalized == QStringLiteral("speed_down")) {
        return uiText("Speed Down");
    }
    if (normalized == QStringLiteral("playlist")) {
        return uiText("Show Playlist");
    }
    if (normalized == QStringLiteral("details")) {
        return uiText("Show Details");
    }
    if (normalized == QStringLiteral("play_pause")) {
        return uiText("Play / Pause");
    }
    if (normalized == QStringLiteral("subtitle_delay_up")) {
        return uiText("Subtitle Delay +");
    }
    if (normalized == QStringLiteral("subtitle_delay_down")) {
        return uiText("Subtitle Delay -");
    }
    return uiText("Disabled");
}

QString mouseZoneSettingKey(const QString &zoneId)
{
    const QString normalized = zoneId.trimmed().toLower();
    if (normalized == QStringLiteral("top")) {
        return QString::fromLatin1(kMouseZoneTopActionSetting);
    }
    if (normalized == QStringLiteral("bottom")) {
        return QString::fromLatin1(kMouseZoneBottomActionSetting);
    }
    if (normalized == QStringLiteral("left")) {
        return QString::fromLatin1(kMouseZoneLeftActionSetting);
    }
    if (normalized == QStringLiteral("right")) {
        return QString::fromLatin1(kMouseZoneRightActionSetting);
    }
    if (normalized == QStringLiteral("center")) {
        return QString::fromLatin1(kMouseZoneCenterActionSetting);
    }
    return {};
}

QString defaultMouseZoneAction(const QString &zoneId)
{
    const QString normalized = zoneId.trimmed().toLower();
    if (normalized == QStringLiteral("top")) {
        return QStringLiteral("top_bar");
    }
    if (normalized == QStringLiteral("bottom")) {
        return QStringLiteral("controls");
    }
    return QStringLiteral("none");
}

QString normalizeMouseZoneAction(QString actionId)
{
    actionId = actionId.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("none"),
        QStringLiteral("playlist"),
        QStringLiteral("details"),
        QStringLiteral("controls"),
        QStringLiteral("top_bar"),
        QStringLiteral("dashboard"),
    };
    return allowed.contains(actionId) ? actionId : QStringLiteral("none");
}

QString smartPlaylistRuleStorageKey(const QString &name)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kSmartPlaylistRulePrefix), encodeSettingKeySegment(name));
}

QJsonObject smartPlaylistRuleObject(const QString &name, const QString &kind, const QString &value = {})
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), name.trimmed());
    object.insert(QStringLiteral("kind"), kind.trimmed().toLower());
    object.insert(QStringLiteral("value"), value.trimmed());
    object.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return object;
}

QString favoriteStorageKey(const QString &source)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kFavoriteMediaPrefix), encodeSettingKeySegment(source));
}

QString mediaMetadataStorageKey(const QString &source)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kMediaMetadataPrefix), encodeSettingKeySegment(source));
}

QString mediaScanSourceKey(const QString &source)
{
    if (isFolderBrowserSource(source) || isFolderBrowserBackSource(source)) {
        return {};
    }

    const QString localPath = localMediaPathForSource(source);
    if (!localPath.isEmpty()) {
        return cleanAbsolutePathWithoutFilesystemLookup(localPath);
    }

    return source.trimmed();
}

QString mediaScanStorageKey(const QString &source)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kMediaScanCachePrefix), encodeSettingKeySegment(mediaScanSourceKey(source)));
}

QJsonObject mediaMetadataObject(const MediaMetadata &metadata)
{
    QJsonObject object;
    object.insert(QStringLiteral("tags"), QJsonArray::fromStringList(metadata.tags));
    object.insert(QStringLiteral("notes"), metadata.notes.trimmed());
    object.insert(QStringLiteral("difficulty"), metadata.difficulty.trimmed());
    object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return object;
}

MediaMetadata loadMediaMetadata(const revaplayer::application::SettingsController *settingsController, const QString &source)
{
    MediaMetadata metadata;
    if (settingsController == nullptr || source.trimmed().isEmpty()) {
        return metadata;
    }

    const QJsonObject object = QJsonDocument::fromJson(
        settingsController->customValue(mediaMetadataStorageKey(source)).toUtf8()).object();
    for (const QJsonValue &value : object.value(QStringLiteral("tags")).toArray()) {
        const QString tag = value.toString().trimmed();
        if (!tag.isEmpty() && !metadata.tags.contains(tag)) {
            metadata.tags.push_back(tag);
        }
    }
    metadata.notes = object.value(QStringLiteral("notes")).toString().trimmed();
    metadata.difficulty = object.value(QStringLiteral("difficulty")).toString().trimmed();
    return metadata;
}

QJsonObject mediaScanResultObject(const revaplayer::services::media::MediaScanResult &result)
{
    QJsonObject object;
    object.insert(QStringLiteral("source"), mediaScanSourceKey(result.source));
    object.insert(QStringLiteral("media_title"), result.mediaTitle.trimmed());
    object.insert(QStringLiteral("artist"), result.artist.trimmed());
    object.insert(QStringLiteral("album"), result.album.trimmed());
    object.insert(QStringLiteral("file_format"), result.fileFormat.trimmed());
    object.insert(QStringLiteral("duration_seconds"), result.durationSeconds);
    object.insert(QStringLiteral("width"), result.width);
    object.insert(QStringLiteral("height"), result.height);
    object.insert(QStringLiteral("has_video"), result.hasVideoTrack);
    object.insert(QStringLiteral("has_audio"), result.hasAudioTrack);
    object.insert(QStringLiteral("subtitle_tracks"), result.subtitleTrackCount);

    const QString localPath = localMediaPathForSource(result.source);
    if (localPath.isEmpty()) {
        object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        return object;
    }

    const QFileInfo fileInfo(localPath);
    if (fileInfo.exists() && fileInfo.isFile()) {
        object.insert(QStringLiteral("file_size"), static_cast<double>(fileInfo.size()));
        object.insert(
            QStringLiteral("last_modified_ms"),
            static_cast<double>(fileInfo.lastModified().toMSecsSinceEpoch()));
    }

    object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return object;
}

std::optional<revaplayer::services::media::MediaScanResult> loadCachedMediaScanResult(
    const revaplayer::application::SettingsController *settingsController,
    const QString &source)
{
    if (settingsController == nullptr || source.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const QString localPath = localMediaPathForSource(source);
    if (localPath.isEmpty()) {
        return std::nullopt;
    }

    const QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return std::nullopt;
    }

    const QJsonObject object = QJsonDocument::fromJson(
        settingsController->customValue(mediaScanStorageKey(source)).toUtf8()).object();
    if (object.isEmpty()) {
        return std::nullopt;
    }

    const qint64 cachedSize = static_cast<qint64>(object.value(QStringLiteral("file_size")).toDouble(-1.0));
    const qint64 cachedModifiedMs = static_cast<qint64>(object.value(QStringLiteral("last_modified_ms")).toDouble(-1.0));
    if (cachedSize != fileInfo.size() || cachedModifiedMs != fileInfo.lastModified().toMSecsSinceEpoch()) {
        return std::nullopt;
    }

    revaplayer::services::media::MediaScanResult result;
    result.source = mediaScanSourceKey(source);
    result.mediaTitle = object.value(QStringLiteral("media_title")).toString().trimmed();
    result.artist = object.value(QStringLiteral("artist")).toString().trimmed();
    result.album = object.value(QStringLiteral("album")).toString().trimmed();
    result.fileFormat = object.value(QStringLiteral("file_format")).toString().trimmed();
    result.durationSeconds = std::max(0.0, object.value(QStringLiteral("duration_seconds")).toDouble(0.0));
    result.width = std::max(0, object.value(QStringLiteral("width")).toInt(0));
    result.height = std::max(0, object.value(QStringLiteral("height")).toInt(0));
    result.hasVideoTrack = object.value(QStringLiteral("has_video")).toBool(false);
    result.hasAudioTrack = object.value(QStringLiteral("has_audio")).toBool(false);
    result.subtitleTrackCount = std::max(0, object.value(QStringLiteral("subtitle_tracks")).toInt(0));
    return result;
}

QString resolutionLabelForScan(const revaplayer::services::media::MediaScanResult &result)
{
    if (result.width <= 0 || result.height <= 0) {
        return {};
    }

    return QStringLiteral("%1x%2").arg(result.width).arg(result.height);
}

QString approximateResolutionLabelForScan(const revaplayer::services::media::MediaScanResult &result)
{
    if (result.width <= 0 || result.height <= 0) {
        return {};
    }

    static const std::array<int, 8> standardHeights {240, 360, 480, 540, 720, 1080, 1440, 2160};
    const int measuredHeight = std::min(result.width, result.height);
    int bestHeight = standardHeights.front();
    int bestDistance = std::abs(measuredHeight - bestHeight);
    for (const int candidate : standardHeights) {
        const int distance = std::abs(measuredHeight - candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestHeight = candidate;
        }
    }
    return QStringLiteral("%1p").arg(bestHeight);
}

QString mediaKindLabelForScan(const revaplayer::services::media::MediaScanResult &result)
{
    if (result.hasVideoTrack) {
        return uiText("Video");
    }
    if (result.hasAudioTrack) {
        return uiText("Audio");
    }
    return uiText("Media");
}

QString normalizePlaylistViewMode(QString mode)
{
    mode = mode.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("list"),
        QStringLiteral("cards"),
        QStringLiteral("covers"),
        QStringLiteral("compact"),
        QStringLiteral("study"),
    };
    return allowed.contains(mode) ? mode : QStringLiteral("list");
}

QString normalizePlaylistViewDensity(QString density)
{
    density = density.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("compact"),
        QStringLiteral("normal"),
        QStringLiteral("comfortable"),
    };
    return allowed.contains(density) ? density : QStringLiteral("normal");
}

QString normalizePlaylistLayoutFit(QString fitId)
{
    fitId = fitId.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("text_first"),
        QStringLiteral("balanced"),
        QStringLiteral("preview_first"),
    };
    return allowed.contains(fitId) ? fitId : QStringLiteral("balanced");
}

QString normalizePlaylistCardZoom(QString zoomId)
{
    zoomId = zoomId.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("85"),
        QStringLiteral("100"),
        QStringLiteral("115"),
        QStringLiteral("130"),
        QStringLiteral("145"),
    };
    return allowed.contains(zoomId) ? zoomId : QStringLiteral("100");
}

QString normalizePlaylistThumbnailShape(QString shapeId)
{
    shapeId = shapeId.trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("hidden"),
        QStringLiteral("square"),
        QStringLiteral("rectangle"),
    };
    return allowed.contains(shapeId) ? shapeId : QStringLiteral("rectangle");
}

QString normalizePlaylistSortMode(QString sortMode)
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

QStringList normalizePlaylistColumns(const QStringList &columns)
{
    static const QSet<QString> allowed {
        QStringLiteral("watched"),
        QStringLiteral("last_position"),
        QStringLiteral("duration"),
        QStringLiteral("file_extension"),
        QStringLiteral("resolution_approx"),
        QStringLiteral("resolution_exact"),
        QStringLiteral("tags"),
        QStringLiteral("notes"),
        QStringLiteral("subtitle_state"),
        QStringLiteral("difficulty"),
    };

    QStringList normalized;
    for (QString column : columns) {
        column = column.trimmed().toLower();
        if (!column.isEmpty() && allowed.contains(column) && !normalized.contains(column)) {
            normalized.push_back(column);
        }
    }
    return normalized;
}

QStringList defaultPlaylistVisibleColumns()
{
    return {
        QStringLiteral("duration"),
        QStringLiteral("resolution_approx"),
        QStringLiteral("resolution_exact"),
    };
}

bool playlistColumnsProduceSecondaryText(const QStringList &columns)
{
    return std::any_of(columns.cbegin(), columns.cend(), [](const QString &columnId) {
        return columnId != QStringLiteral("duration");
    });
}

QStringList normalizeVisibleDetailsTabs(const QStringList &tabIds,
                                        const QVector<QPair<QString, QString>> &detailsTabs)
{
    QStringList availableTabIds;
    availableTabIds.reserve(detailsTabs.size());
    for (const auto &tab : detailsTabs) {
        const QString tabId = tab.first.trimmed();
        if (!tabId.isEmpty() && !availableTabIds.contains(tabId)) {
            availableTabIds.push_back(tabId);
        }
    }

    QStringList normalized;
    normalized.reserve(tabIds.size());
    for (QString tabId : tabIds) {
        tabId = tabId.trimmed();
        if (!tabId.isEmpty() && availableTabIds.contains(tabId) && !normalized.contains(tabId)) {
            normalized.push_back(tabId);
        }
    }

    return normalized.isEmpty() ? availableTabIds : normalized;
}

double playlistThumbnailTimeSeconds(const double durationSeconds)
{
    if (durationSeconds <= 0.0) {
        return 1.0;
    }

    const double safeDuration = std::max(1.0, durationSeconds);
    const double target = std::clamp(safeDuration * 0.18, 1.0, 75.0);
    return std::min(target, std::max(0.0, safeDuration - 0.5));
}

constexpr int kPlaylistThumbnailPreviewWidth = 156;
constexpr int kPlaylistInitialMetadataScanBatch = 15;
constexpr int kPlaylistMetadataScanLookahead = 8;
constexpr int kPlaylistMetadataScanLookbehind = 3;
constexpr int kPlaylistMetadataFailureRetryLimit = 3;

bool isLikelyVideoMediaSuffix(const QString &suffix)
{
    static const QSet<QString> kVideoExtensions {
        QStringLiteral("3gp"),
        QStringLiteral("avi"),
        QStringLiteral("m4v"),
        QStringLiteral("mkv"),
        QStringLiteral("mov"),
        QStringLiteral("mp4"),
        QStringLiteral("mpeg"),
        QStringLiteral("mpg"),
        QStringLiteral("webm"),
        QStringLiteral("wmv"),
    };
    return kVideoExtensions.contains(suffix.trimmed().toLower());
}

bool isLikelyVideoMediaFile(const QFileInfo &fileInfo)
{
    return isLikelyVideoMediaSuffix(fileInfo.suffix());
}

QString playlistThumbnailLocalPath(const QString &source)
{
    const QString localPath = localMediaPathForSource(source);
    if (localPath.isEmpty()) {
        return {};
    }

    const QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return {};
    }
    return fileInfo.absoluteFilePath();
}

QString playlistThumbnailCacheDirectoryPath()
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.trimmed().isEmpty()) {
        return {};
    }

    const QString directoryPath = QDir(cacheRoot).filePath(QStringLiteral("playlist-thumbnails"));
    QDir().mkpath(directoryPath);
    return directoryPath;
}

QString playlistThumbnailRequestKey(const QString &source, const double durationSeconds)
{
    const QString localPath = playlistThumbnailLocalPath(source);
    if (localPath.isEmpty()) {
        return {};
    }

    const QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return {};
    }

    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(fileInfo.absoluteFilePath())
        .arg(fileInfo.lastModified().toMSecsSinceEpoch())
        .arg(fileInfo.size())
        .arg(revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(
            playlistThumbnailTimeSeconds(durationSeconds)))
        .arg(kPlaylistThumbnailPreviewWidth);
}

QString playlistThumbnailRequestKeyForFileMetadata(const QString &absoluteFilePath,
                                                  const qint64 lastModifiedMs,
                                                  const qint64 fileSize,
                                                  const double durationSeconds)
{
    const QString normalizedPath = QDir::cleanPath(absoluteFilePath);
    if (normalizedPath.isEmpty() || lastModifiedMs < 0 || fileSize < 0) {
        return {};
    }

    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(normalizedPath)
        .arg(lastModifiedMs)
        .arg(fileSize)
        .arg(revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(
            playlistThumbnailTimeSeconds(durationSeconds)))
        .arg(kPlaylistThumbnailPreviewWidth);
}

QString playlistThumbnailCachePathForRequestKey(const QString &requestKey)
{
    const QString cacheDirectory = playlistThumbnailCacheDirectoryPath();
    if (requestKey.isEmpty() || cacheDirectory.isEmpty()) {
        return {};
    }

    const QByteArray digest = QCryptographicHash::hash(requestKey.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QDir(cacheDirectory).filePath(QString::fromLatin1(digest) + QStringLiteral(".jpg"));
}

bool loadPlaylistThumbnailDiskCacheForFileMetadata(const QString &absoluteFilePath,
                                                   const qint64 lastModifiedMs,
                                                   const qint64 fileSize,
                                                   const double durationSeconds,
                                                   QImage *image)
{
    if (image == nullptr) {
        return false;
    }

    const QString cachePath = playlistThumbnailCachePathForRequestKey(
        playlistThumbnailRequestKeyForFileMetadata(absoluteFilePath, lastModifiedMs, fileSize, durationSeconds));
    if (cachePath.isEmpty()) {
        return false;
    }

    if (!image->load(cachePath)) {
        return false;
    }

    return !image->isNull();
}

int playlistVisibleRowFromTop(QListView *view)
{
    if (view == nullptr || view->model() == nullptr) {
        return -1;
    }

    const QRect viewportRect = view->viewport()->rect();
    const int probeX = std::max(6, viewportRect.left() + 12);
    for (int y = viewportRect.top() + 4; y <= viewportRect.bottom(); y += 12) {
        const QModelIndex index = view->indexAt(QPoint(probeX, y));
        if (index.isValid()) {
            return index.row();
        }
    }

    return view->model()->rowCount() > 0 ? 0 : -1;
}

int playlistVisibleRowFromBottom(QListView *view)
{
    if (view == nullptr || view->model() == nullptr) {
        return -1;
    }

    const QRect viewportRect = view->viewport()->rect();
    const int probeX = std::max(6, viewportRect.left() + 12);
    for (int y = viewportRect.bottom() - 4; y >= viewportRect.top(); y -= 12) {
        const QModelIndex index = view->indexAt(QPoint(probeX, y));
        if (index.isValid()) {
            return index.row();
        }
    }

    const int rowCount = view->model()->rowCount();
    return rowCount > 0 ? rowCount - 1 : -1;
}

QString playlistViewPresetStorageKey(const QString &name)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kPlaylistViewPresetPrefix), encodeSettingKeySegment(name));
}

QJsonObject playlistViewPresetObject(const PlaylistViewPreset &preset)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), preset.name.trimmed());
    object.insert(QStringLiteral("icon"), preset.iconId.trimmed());
    object.insert(QStringLiteral("color"), preset.colorId.trimmed());
    object.insert(QStringLiteral("card_zoom"), normalizePlaylistCardZoom(preset.cardZoomId));
    object.insert(QStringLiteral("thumbnail_shape"), normalizePlaylistThumbnailShape(preset.thumbnailShapeId));
    object.insert(QStringLiteral("secondary_text_visible"), preset.secondaryTextVisible);
    object.insert(QStringLiteral("sort_mode"), normalizePlaylistSortMode(preset.sortModeId));
    object.insert(QStringLiteral("filter"), preset.filterText.trimmed());
    object.insert(QStringLiteral("current_details_tab"), preset.currentDetailsTabId.trimmed());
    object.insert(QStringLiteral("columns"), QJsonArray::fromStringList(normalizePlaylistColumns(preset.columns)));
    object.insert(QStringLiteral("visible_details_tabs"), QJsonArray::fromStringList(preset.visibleDetailsTabs));
    object.insert(QStringLiteral("show_in_tabs"), preset.showInTabs);
    object.insert(QStringLiteral("order"), preset.order);
    object.insert(QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return object;
}

QVector<PlaylistViewPreset> loadPlaylistViewPresets(const revaplayer::application::SettingsController *settingsController)
{
    QVector<PlaylistViewPreset> presets;
    if (settingsController == nullptr) {
        return presets;
    }

    const QStringList keys = settingsController->customKeys(QString::fromLatin1(kPlaylistViewPresetPrefix));
    presets.reserve(keys.size());
    for (const QString &key : keys) {
        const QJsonObject object = QJsonDocument::fromJson(settingsController->customValue(key).toUtf8()).object();
        PlaylistViewPreset preset;
        preset.key = key;
        preset.name = object.value(QStringLiteral("name")).toString().trimmed();
        if (preset.name.isEmpty()) {
            preset.name = decodeSettingKeySegment(key.mid(QString::fromLatin1(kPlaylistViewPresetPrefix).size()));
        }
        preset.iconId = object.value(QStringLiteral("icon")).toString().trimmed();
        preset.colorId = object.value(QStringLiteral("color")).toString().trimmed();
        const QString rawCardZoom = object.value(QStringLiteral("card_zoom")).toString().trimmed();
        const QString rawThumbnailShape = object.value(QStringLiteral("thumbnail_shape")).toString().trimmed();
        preset.cardZoomId = rawCardZoom.isEmpty()
            ? playlistCardZoomFromLegacySettings(
                  object.value(QStringLiteral("mode")).toString(),
                  object.value(QStringLiteral("density")).toString(),
                  object.value(QStringLiteral("layout_fit")).toString())
            : normalizePlaylistCardZoom(rawCardZoom);
        preset.thumbnailShapeId = rawThumbnailShape.isEmpty()
            ? playlistThumbnailShapeFromLegacySettings(
                  object.value(QStringLiteral("mode")).toString(),
                  object.value(QStringLiteral("layout_fit")).toString())
            : normalizePlaylistThumbnailShape(rawThumbnailShape);
        preset.secondaryTextVisible = object.contains(QStringLiteral("secondary_text_visible"))
            ? object.value(QStringLiteral("secondary_text_visible")).toBool(false)
            : false;
        preset.sortModeId = normalizePlaylistSortMode(object.value(QStringLiteral("sort_mode")).toString());
        preset.filterText = object.value(QStringLiteral("filter")).toString().trimmed();
        preset.currentDetailsTabId = object.value(QStringLiteral("current_details_tab")).toString().trimmed();
        QStringList columns;
        for (const QJsonValue &value : object.value(QStringLiteral("columns")).toArray()) {
            columns.push_back(value.toString());
        }
        preset.columns = object.contains(QStringLiteral("columns"))
            ? normalizePlaylistColumns(columns)
            : defaultPlaylistVisibleColumns();
        for (const QJsonValue &value : object.value(QStringLiteral("visible_details_tabs")).toArray()) {
            const QString tabId = value.toString().trimmed();
            if (!tabId.isEmpty() && !preset.visibleDetailsTabs.contains(tabId)) {
                preset.visibleDetailsTabs.push_back(tabId);
            }
        }
        preset.showInTabs = !object.contains(QStringLiteral("show_in_tabs")) || object.value(QStringLiteral("show_in_tabs")).toBool(true);
        preset.order = object.value(QStringLiteral("order")).toInt(presets.size());
        presets.push_back(std::move(preset));
    }

    std::sort(presets.begin(), presets.end(), [](const PlaylistViewPreset &left, const PlaylistViewPreset &right) {
        if (left.order != right.order) {
            return left.order < right.order;
        }
        return left.name.localeAwareCompare(right.name) < 0;
    });
    return presets;
}

QColor accentColorForId(const QString &accentId)
{
    const QString normalized = accentId.trimmed().toLower();
    if (normalized == QStringLiteral("emerald")) {
        return QColor(QStringLiteral("#33c38b"));
    }
    if (normalized == QStringLiteral("amber")) {
        return QColor(QStringLiteral("#f0b246"));
    }
    if (normalized == QStringLiteral("rose")) {
        return QColor(QStringLiteral("#ef6f92"));
    }
    if (normalized == QStringLiteral("red")) {
        return QColor(QStringLiteral("#ef4444"));
    }
    if (normalized == QStringLiteral("purple")) {
        return QColor(QStringLiteral("#a78bfa"));
    }
    if (normalized == QStringLiteral("orange")) {
        return QColor(QStringLiteral("#ff7a1a"));
    }
    if (normalized == QStringLiteral("graphite")) {
        return QColor(QStringLiteral("#9aa7bc"));
    }
    return QColor(QStringLiteral("#6a92ff"));
}

QIcon playlistViewIcon(QWidget *widget, const QString &iconId)
{
    if (widget == nullptr) {
        return {};
    }

    const QString normalized = iconId.trimmed().toLower();
    QStyle *style = widget->style();
    if (style == nullptr) {
        return {};
    }
    if (normalized == QStringLiteral("folder")) {
        return style->standardIcon(QStyle::SP_DirIcon);
    }
    if (normalized == QStringLiteral("details")) {
        return style->standardIcon(QStyle::SP_FileDialogDetailedView);
    }
    if (normalized == QStringLiteral("cards")) {
        return style->standardIcon(QStyle::SP_FileDialogContentsView);
    }
    if (normalized == QStringLiteral("study")) {
        return style->standardIcon(QStyle::SP_FileDialogInfoView);
    }
    if (normalized == QStringLiteral("favorites")) {
        return style->standardIcon(QStyle::SP_DialogYesButton);
    }
    return style->standardIcon(QStyle::SP_MediaPlay);
}

QString playlistCardZoomLabel(const QString &zoomId)
{
    const QString normalized = normalizePlaylistCardZoom(zoomId);
    if (normalized == QStringLiteral("85")) {
        return uiText("85% Compact");
    }
    if (normalized == QStringLiteral("115")) {
        return uiText("115% Comfortable");
    }
    if (normalized == QStringLiteral("130")) {
        return uiText("130% Large");
    }
    if (normalized == QStringLiteral("145")) {
        return uiText("145% Extra Large");
    }
    return uiText("100% Default");
}

QString playlistThumbnailShapeLabel(const QString &shapeId)
{
    const QString normalized = normalizePlaylistThumbnailShape(shapeId);
    if (normalized == QStringLiteral("square")) {
        return uiText("Square");
    }
    if (normalized == QStringLiteral("hidden")) {
        return uiText("Hidden");
    }
    return uiText("Rectangle");
}

QString playlistColumnLabel(const QString &columnId)
{
    const QString normalized = columnId.trimmed().toLower();
    if (normalized == QStringLiteral("watched")) {
        return uiText("Watched %");
    }
    if (normalized == QStringLiteral("last_position")) {
        return uiText("Last Position");
    }
    if (normalized == QStringLiteral("duration")) {
        return uiText("Duration");
    }
    if (normalized == QStringLiteral("file_extension")) {
        return uiText("File extension");
    }
    if (normalized == QStringLiteral("resolution_approx")) {
        return uiText("Approx. resolution");
    }
    if (normalized == QStringLiteral("resolution_exact")) {
        return uiText("Original resolution");
    }
    if (normalized == QStringLiteral("tags")) {
        return uiText("Tags");
    }
    if (normalized == QStringLiteral("notes")) {
        return uiText("Notes");
    }
    if (normalized == QStringLiteral("subtitle_state")) {
        return uiText("Subtitle State");
    }
    if (normalized == QStringLiteral("difficulty")) {
        return uiText("Difficulty");
    }
    return normalized;
}

QStringList defaultVisibleDetailsTabIds()
{
    return {
        QStringLiteral("bookmarks"),
        QStringLiteral("chapters"),
        QStringLiteral("tracks"),
        QStringLiteral("history"),
        QStringLiteral("favorites"),
        QStringLiteral("scenes"),
        QStringLiteral("media_lab"),
    };
}

bool editPlaylistPanelSettingsDialog(QWidget *parent,
                                     const PlaylistPanelSettingsState &initialState,
                                     PlaylistPanelSettingsState *outState)
{
    if (outState == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(uiText("Playlist Panel"));
    dialog.setModal(true);
    dialog.resize(520, 620);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    auto *zoomCombo = new QComboBox(&dialog);
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("85")), QStringLiteral("85"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("100")), QStringLiteral("100"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("115")), QStringLiteral("115"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("130")), QStringLiteral("130"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("145")), QStringLiteral("145"));

    auto *thumbnailShapeCombo = new QComboBox(&dialog);
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("rectangle")), QStringLiteral("rectangle"));
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("square")), QStringLiteral("square"));
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("hidden")), QStringLiteral("hidden"));

    auto *sortCombo = new QComboBox(&dialog);
    sortCombo->addItem(uiText("Natural"), QStringLiteral("natural"));
    sortCombo->addItem(uiText("Title"), QStringLiteral("title"));
    sortCombo->addItem(uiText("Duration"), QStringLiteral("duration"));
    sortCombo->addItem(uiText("Progress"), QStringLiteral("progress"));
    auto *showPathsCheck = new QCheckBox(uiText("Show full file paths in the playlist"), &dialog);
    auto *showIndexCheck = new QCheckBox(uiText("Show numeric prefixes and current-item markers in the playlist"), &dialog);

    formLayout->addRow(uiText("Playlist zoom"), zoomCombo);
    formLayout->addRow(uiText("Thumbnail shape"), thumbnailShapeCombo);
    formLayout->addRow(uiText("Sort by"), sortCombo);
    layout->addLayout(formLayout, 0);
    layout->addWidget(showPathsCheck, 0);
    layout->addWidget(showIndexCheck, 0);

    auto *columnsGroup = new QGroupBox(uiText("Visible Columns"), &dialog);
    auto *columnsLayout = new QVBoxLayout(columnsGroup);
    columnsLayout->setContentsMargins(10, 10, 10, 10);
    columnsLayout->setSpacing(6);
    const QStringList columnIds {
        QStringLiteral("watched"),
        QStringLiteral("last_position"),
        QStringLiteral("duration"),
        QStringLiteral("file_extension"),
        QStringLiteral("resolution_approx"),
        QStringLiteral("resolution_exact"),
        QStringLiteral("tags"),
        QStringLiteral("notes"),
        QStringLiteral("subtitle_state"),
        QStringLiteral("difficulty"),
    };
    QVector<QCheckBox *> columnChecks;
    columnChecks.reserve(columnIds.size());
    for (const QString &columnId : columnIds) {
        auto *check = new QCheckBox(playlistColumnLabel(columnId), columnsGroup);
        check->setChecked(initialState.columns.contains(columnId));
        columnChecks.push_back(check);
        columnsLayout->addWidget(check, 0);
    }
    layout->addWidget(columnsGroup, 0);

    zoomCombo->setCurrentIndex(std::max(0, zoomCombo->findData(normalizePlaylistCardZoom(initialState.cardZoomId))));
    thumbnailShapeCombo->setCurrentIndex(std::max(0, thumbnailShapeCombo->findData(normalizePlaylistThumbnailShape(initialState.thumbnailShapeId))));
    sortCombo->setCurrentIndex(std::max(0, sortCombo->findData(normalizePlaylistSortMode(initialState.sortModeId))));
    showPathsCheck->setChecked(initialState.showFullPaths);
    showIndexCheck->setChecked(initialState.showIndexPrefixes);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("Apply"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    PlaylistPanelSettingsState state = initialState;
    state.cardZoomId = zoomCombo->currentData().toString();
    state.thumbnailShapeId = thumbnailShapeCombo->currentData().toString();
    state.sortModeId = sortCombo->currentData().toString();
    state.showFullPaths = showPathsCheck->isChecked();
    state.showIndexPrefixes = showIndexCheck->isChecked();
    state.columns.clear();
    for (qsizetype index = 0; index < columnChecks.size() && index < columnIds.size(); ++index) {
        if (columnChecks.at(index) != nullptr && columnChecks.at(index)->isChecked()) {
            state.columns.push_back(columnIds.at(index));
        }
    }
    state.columns = normalizePlaylistColumns(state.columns);
    *outState = state;
    return true;
}

bool editDetailsPanelSettingsDialog(QWidget *parent,
                                    const QVector<QPair<QString, QString>> &detailsTabs,
                                    const QStringList &initialVisibleTabs,
                                    const QString &initialCurrentTabId,
                                    QStringList *outVisibleTabs,
                                    QString *outCurrentTabId)
{
    if (outVisibleTabs == nullptr || outCurrentTabId == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(uiText("Details Panel"));
    dialog.setModal(true);
    dialog.resize(420, 480);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    auto *currentDetailsCombo = new QComboBox(&dialog);
    currentDetailsCombo->addItem(uiText("Keep current tab"), QString {});
    for (const auto &tab : detailsTabs) {
        currentDetailsCombo->addItem(tab.second, tab.first);
    }
    currentDetailsCombo->setCurrentIndex(std::max(0, currentDetailsCombo->findData(initialCurrentTabId)));
    formLayout->addRow(uiText("Open Details tab"), currentDetailsCombo);
    layout->addLayout(formLayout, 0);

    auto *listGroup = new QGroupBox(uiText("Visible Details Tabs"), &dialog);
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(10, 10, 10, 10);
    listLayout->setSpacing(6);

    auto *detailsList = new QListWidget(listGroup);
    const QStringList normalizedVisibleTabs = normalizeVisibleDetailsTabs(initialVisibleTabs, detailsTabs);
    for (const auto &tab : detailsTabs) {
        auto *item = new QListWidgetItem(tab.second, detailsList);
        item->setData(Qt::UserRole, tab.first);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(normalizedVisibleTabs.isEmpty() || normalizedVisibleTabs.contains(tab.first) ? Qt::Checked : Qt::Unchecked);
    }
    listLayout->addWidget(detailsList, 1);
    layout->addWidget(listGroup, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("Apply"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    QStringList visibleTabs;
    for (int row = 0; row < detailsList->count(); ++row) {
        QListWidgetItem *item = detailsList->item(row);
        if (item != nullptr && item->checkState() == Qt::Checked) {
            visibleTabs.push_back(item->data(Qt::UserRole).toString());
        }
    }

    const QString normalizedCurrentTabId = currentDetailsCombo->currentData().toString().trimmed();
    const QStringList normalizedVisibleTabsResult = normalizeVisibleDetailsTabs(visibleTabs, detailsTabs);
    *outVisibleTabs = normalizedVisibleTabsResult;
    *outCurrentTabId = normalizedVisibleTabsResult.contains(normalizedCurrentTabId) ? normalizedCurrentTabId : QString {};
    return true;
}

bool editPlaylistViewPresetDialog(QWidget *parent,
                                  const QString &windowTitle,
                                  const QString &headline,
                                  const PlaylistViewPreset &initialPreset,
                                  const QVector<QPair<QString, QString>> &detailsTabs,
                                  PlaylistViewPreset *outPreset)
{
    if (outPreset == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(windowTitle);
    dialog.setModal(true);
    dialog.resize(560, 640);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *headlineLabel = new QLabel(headline, &dialog);
    headlineLabel->setWordWrap(true);
    layout->addWidget(headlineLabel, 0);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    auto *nameEdit = new QLineEdit(initialPreset.name, &dialog);
    auto *iconCombo = new QComboBox(&dialog);
    iconCombo->addItem(uiText("Play"), QStringLiteral("play"));
    iconCombo->addItem(uiText("Folder"), QStringLiteral("folder"));
    iconCombo->addItem(uiText("Details"), QStringLiteral("details"));
    iconCombo->addItem(uiText("Cards"), QStringLiteral("cards"));
    iconCombo->addItem(uiText("Study"), QStringLiteral("study"));
    iconCombo->addItem(uiText("Favorites"), QStringLiteral("favorites"));
    auto *colorCombo = new QComboBox(&dialog);
    for (const auto &accent : revaplayer::application::availableAccents()) {
        colorCombo->addItem(uiText(accent.label.toUtf8().constData()), accent.id);
    }
    auto *zoomCombo = new QComboBox(&dialog);
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("85")), QStringLiteral("85"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("100")), QStringLiteral("100"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("115")), QStringLiteral("115"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("130")), QStringLiteral("130"));
    zoomCombo->addItem(playlistCardZoomLabel(QStringLiteral("145")), QStringLiteral("145"));
    auto *thumbnailShapeCombo = new QComboBox(&dialog);
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("rectangle")), QStringLiteral("rectangle"));
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("square")), QStringLiteral("square"));
    thumbnailShapeCombo->addItem(playlistThumbnailShapeLabel(QStringLiteral("hidden")), QStringLiteral("hidden"));
    auto *sortCombo = new QComboBox(&dialog);
    sortCombo->addItem(uiText("Natural"), QStringLiteral("natural"));
    sortCombo->addItem(uiText("Title"), QStringLiteral("title"));
    sortCombo->addItem(uiText("Duration"), QStringLiteral("duration"));
    sortCombo->addItem(uiText("Progress"), QStringLiteral("progress"));
    auto *filterEdit = new QLineEdit(initialPreset.filterText, &dialog);
    auto *showInTabsCheck = new QCheckBox(uiText("Show this view as a visible tab"), &dialog);
    showInTabsCheck->setChecked(initialPreset.showInTabs);
    auto *currentDetailsCombo = new QComboBox(&dialog);
    currentDetailsCombo->addItem(uiText("Keep current tab"), QString {});
    for (const auto &tab : detailsTabs) {
        currentDetailsCombo->addItem(tab.second, tab.first);
    }

    formLayout->addRow(uiText("Name"), nameEdit);
    formLayout->addRow(uiText("Icon"), iconCombo);
    formLayout->addRow(uiText("Color"), colorCombo);
    formLayout->addRow(uiText("Playlist zoom"), zoomCombo);
    formLayout->addRow(uiText("Thumbnail shape"), thumbnailShapeCombo);
    formLayout->addRow(uiText("Sort by"), sortCombo);
    formLayout->addRow(uiText("Default filter"), filterEdit);
    formLayout->addRow(uiText("Open Details tab"), currentDetailsCombo);
    layout->addLayout(formLayout, 0);
    layout->addWidget(showInTabsCheck, 0);

    auto *columnsGroup = new QGroupBox(uiText("Visible Columns"), &dialog);
    auto *columnsLayout = new QVBoxLayout(columnsGroup);
    columnsLayout->setContentsMargins(10, 10, 10, 10);
    columnsLayout->setSpacing(6);
    const QStringList columnIds {
        QStringLiteral("watched"),
        QStringLiteral("last_position"),
        QStringLiteral("duration"),
        QStringLiteral("file_extension"),
        QStringLiteral("resolution_approx"),
        QStringLiteral("resolution_exact"),
        QStringLiteral("tags"),
        QStringLiteral("notes"),
        QStringLiteral("subtitle_state"),
        QStringLiteral("difficulty"),
    };
    QVector<QCheckBox *> columnChecks;
    for (const QString &columnId : columnIds) {
        auto *check = new QCheckBox(playlistColumnLabel(columnId), columnsGroup);
        check->setChecked(initialPreset.columns.contains(columnId));
        columnChecks.push_back(check);
        columnsLayout->addWidget(check, 0);
    }
    layout->addWidget(columnsGroup, 0);

    iconCombo->setCurrentIndex(std::max(0, iconCombo->findData(initialPreset.iconId.trimmed().isEmpty() ? QStringLiteral("play") : initialPreset.iconId)));
    colorCombo->setCurrentIndex(std::max(0, colorCombo->findData(initialPreset.colorId.trimmed().isEmpty() ? QStringLiteral("blue") : initialPreset.colorId)));
    zoomCombo->setCurrentIndex(std::max(0, zoomCombo->findData(normalizePlaylistCardZoom(initialPreset.cardZoomId))));
    thumbnailShapeCombo->setCurrentIndex(std::max(0, thumbnailShapeCombo->findData(normalizePlaylistThumbnailShape(initialPreset.thumbnailShapeId))));
    sortCombo->setCurrentIndex(std::max(0, sortCombo->findData(normalizePlaylistSortMode(initialPreset.sortModeId))));
    currentDetailsCombo->setCurrentIndex(std::max(0, currentDetailsCombo->findData(initialPreset.currentDetailsTabId)));

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog, nameEdit]() {
        if (nameEdit != nullptr && !nameEdit->text().trimmed().isEmpty()) {
            dialog.accept();
        }
    });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    PlaylistViewPreset preset = initialPreset;
    preset.name = nameEdit->text().trimmed();
    preset.iconId = iconCombo->currentData().toString();
    preset.colorId = colorCombo->currentData().toString();
    preset.cardZoomId = zoomCombo->currentData().toString();
    preset.thumbnailShapeId = thumbnailShapeCombo->currentData().toString();
    preset.sortModeId = sortCombo->currentData().toString();
    preset.filterText = filterEdit->text().trimmed();
    preset.currentDetailsTabId = currentDetailsCombo->currentData().toString();
    preset.showInTabs = showInTabsCheck->isChecked();
    preset.columns.clear();
    for (qsizetype index = 0; index < columnChecks.size() && index < columnIds.size(); ++index) {
        if (columnChecks.at(index) != nullptr && columnChecks.at(index)->isChecked()) {
            preset.columns.push_back(columnIds.at(index));
        }
    }
    preset.columns = normalizePlaylistColumns(preset.columns);
    preset.secondaryTextVisible = playlistColumnsProduceSecondaryText(preset.columns);
    *outPreset = preset;
    return !preset.name.isEmpty();
}

QString layoutPresetStorageKey(const QString &name)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kLayoutPresetPrefix), encodeSettingKeySegment(name));
}

QStringList savedLayoutPresetNames(const revaplayer::application::SettingsController *settingsController)
{
    QStringList names;
    if (settingsController == nullptr) {
        return names;
    }

    const QStringList keys = settingsController->customKeys(QString::fromLatin1(kLayoutPresetPrefix));
    names.reserve(keys.size());
    for (const QString &key : keys) {
        const QJsonObject object = QJsonDocument::fromJson(settingsController->customValue(key).toUtf8()).object();
        const QString name = object.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty() && !names.contains(name)) {
            names.push_back(name);
        }
    }
    std::sort(names.begin(), names.end(), [](const QString &left, const QString &right) {
        return left.localeAwareCompare(right) < 0;
    });
    return names;
}

QString pinnedCourseStorageKey(const QString &path)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kPinnedCoursePrefix), encodeSettingKeySegment(path));
}

QString pinnedCourseMediaOrderStorageKey(const QString &path)
{
    return QStringLiteral("%1%2")
        .arg(QString::fromLatin1(kPinnedCourseMediaOrderPrefix), encodeSettingKeySegment(path));
}

QString folderPathFromPinnedCourseMediaOrderStorageKey(const QString &key)
{
    const QString prefix = QString::fromLatin1(kPinnedCourseMediaOrderPrefix);
    if (!key.startsWith(prefix)) {
        return {};
    }

    const QString decodedPath = decodeSettingKeySegment(key.mid(prefix.size())).trimmed();
    return decodedPath.isEmpty() ? QString {} : QFileInfo(decodedPath).absoluteFilePath();
}

bool folderPathIsSameOrDescendantOf(const QString &folderPath, const QString &rootFolderPath)
{
    const QString absoluteFolderPath = QFileInfo(folderPath).absoluteFilePath();
    const QString absoluteRootPath = QFileInfo(rootFolderPath).absoluteFilePath();
    if (absoluteFolderPath.isEmpty() || absoluteRootPath.isEmpty()) {
        return false;
    }
    if (absoluteFolderPath == absoluteRootPath) {
        return true;
    }

    const QString relativePath = QDir(absoluteRootPath).relativeFilePath(absoluteFolderPath);
    return !relativePath.isEmpty()
        && relativePath != QStringLiteral(".")
        && !relativePath.startsWith(QStringLiteral("../"))
        && relativePath != QStringLiteral("..")
        && !QDir::isAbsolutePath(relativePath);
}

bool folderHasSavedManualOrderScope(const revaplayer::application::SettingsController *settingsController,
                                    const QString &folderPath)
{
    if (settingsController == nullptr || folderPath.trimmed().isEmpty()) {
        return false;
    }

    const QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController);
    return std::any_of(courses.cbegin(), courses.cend(), [&folderPath](const PinnedCourseFolder &course) {
        return folderPathIsSameOrDescendantOf(folderPath, course.path);
    });
}

QStringList normalizedMediaOrderSources(const QStringList &sources)
{
    QStringList normalized;
    normalized.reserve(sources.size());
    QSet<QString> seenSources;
    for (const QString &source : sources) {
        const QString localPath = localMediaPathForSource(source).trimmed();
        if (localPath.isEmpty()) {
            continue;
        }

        const QString absolutePath = QFileInfo(localPath).absoluteFilePath();
        if (absolutePath.isEmpty() || seenSources.contains(absolutePath)) {
            continue;
        }

        normalized.push_back(absolutePath);
        seenSources.insert(absolutePath);
    }
    return normalized;
}

QStringList applyPinnedCourseMediaOrder(const QStringList &sources, const QStringList &storedOrder)
{
    const QStringList normalizedSources = normalizedMediaOrderSources(sources);
    if (normalizedSources.isEmpty() || storedOrder.isEmpty()) {
        return normalizedSources;
    }

    QStringList orderedSources;
    orderedSources.reserve(normalizedSources.size());
    QSet<QString> addedSources;
    for (const QString &orderedSource : storedOrder) {
        const QString absolutePath = QFileInfo(orderedSource).absoluteFilePath();
        if (absolutePath.isEmpty() || addedSources.contains(absolutePath) || !normalizedSources.contains(absolutePath)) {
            continue;
        }
        orderedSources.push_back(absolutePath);
        addedSources.insert(absolutePath);
    }

    for (const QString &source : normalizedSources) {
        if (!addedSources.contains(source)) {
            orderedSources.push_back(source);
        }
    }

    return orderedSources;
}

QStringList loadPinnedCourseMediaOrder(const revaplayer::application::SettingsController *settingsController,
                                       const QString &folderPath)
{
    if (settingsController == nullptr || folderPath.trimmed().isEmpty()) {
        return {};
    }

    const QString key = pinnedCourseMediaOrderStorageKey(QFileInfo(folderPath).absoluteFilePath());
    if (key.isEmpty()) {
        return {};
    }

    const QJsonArray orderArray = QJsonDocument::fromJson(settingsController->customValue(key).toUtf8()).array();
    QStringList orderedSources;
    orderedSources.reserve(orderArray.size());
    for (const QJsonValue &value : orderArray) {
        const QString path = QFileInfo(value.toString().trimmed()).absoluteFilePath();
        if (!path.isEmpty()) {
            orderedSources.push_back(path);
        }
    }
    return orderedSources;
}

void persistPinnedCourseMediaOrder(revaplayer::application::SettingsController *settingsController,
                                   const QString &folderPath,
                                   const QStringList &orderedSources)
{
    if (settingsController == nullptr || folderPath.trimmed().isEmpty()) {
        return;
    }

    const QString storageKey = pinnedCourseMediaOrderStorageKey(QFileInfo(folderPath).absoluteFilePath());
    if (storageKey.isEmpty()) {
        return;
    }

    const QStringList normalizedSources = normalizedMediaOrderSources(orderedSources);
    if (normalizedSources.isEmpty()) {
        settingsController->removeCustomValue(storageKey);
        return;
    }

    QJsonArray orderArray;
    for (const QString &source : normalizedSources) {
        orderArray.push_back(source);
    }
    settingsController->setCustomValue(
        storageKey,
        QString::fromUtf8(QJsonDocument(orderArray).toJson(QJsonDocument::Compact)));
}

QStringList playlistSourcesForPinnedCourse(const revaplayer::application::SettingsController *settingsController,
                                           const QString &folderPath,
                                           const bool naturalSort)
{
    return applyPinnedCourseMediaOrder(
        supportedMediaFilesInDirectory(QDir(folderPath), naturalSort),
        loadPinnedCourseMediaOrder(settingsController, folderPath));
}

QString savedListDisplayLabel(const PinnedCourseFolder &course, const bool includeCategory)
{
    const QString trimmedLabel = course.label.trimmed();
    const QString trimmedCategory = course.category.trimmed();
    if (includeCategory && !trimmedCategory.isEmpty()) {
        return QStringLiteral("%1  •  %2").arg(trimmedCategory, trimmedLabel);
    }
    return trimmedLabel;
}

QJsonObject pinnedCourseObject(const QString &path,
                               const QString &label,
                               const QString &category,
                               const QString &description = {},
                               const QString &colorId = {},
                               const QString &iconId = {},
                               const int order = 0)
{
    QJsonObject object;
    object.insert(QStringLiteral("path"), path);
    object.insert(QStringLiteral("label"), label.trimmed());
    object.insert(QStringLiteral("category"), category.trimmed());
    object.insert(QStringLiteral("description"), description.trimmed());
    object.insert(QStringLiteral("color"), colorId.trimmed());
    object.insert(QStringLiteral("icon"), iconId.trimmed());
    object.insert(QStringLiteral("order"), order);
    object.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return object;
}

QVector<PinnedCourseFolder> loadPinnedCourses(const revaplayer::application::SettingsController *settingsController)
{
    QVector<PinnedCourseFolder> courses;
    if (settingsController == nullptr) {
        return courses;
    }

    const QStringList keys = settingsController->customKeys(QString::fromLatin1(kPinnedCoursePrefix));
    courses.reserve(keys.size());
    for (const QString &key : keys) {
        const QJsonObject object = QJsonDocument::fromJson(settingsController->customValue(key).toUtf8()).object();
        const QString path = object.value(QStringLiteral("path")).toString();
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo pathInfo(path);
        if (!pathInfo.exists() || !pathInfo.isDir()) {
            continue;
        }
        PinnedCourseFolder course;
        course.key = key;
        course.path = pathInfo.absoluteFilePath();
        course.label = object.value(QStringLiteral("label")).toString().trimmed();
        if (course.label.isEmpty()) {
            course.label = QFileInfo(course.path).fileName();
        }
        course.category = object.value(QStringLiteral("category")).toString().trimmed();
        course.description = object.value(QStringLiteral("description")).toString().trimmed();
        course.colorId = object.value(QStringLiteral("color")).toString().trimmed();
        course.iconId = object.value(QStringLiteral("icon")).toString().trimmed();
        course.order = object.value(QStringLiteral("order")).toInt(courses.size());
        courses.push_back(std::move(course));
    }
    std::sort(courses.begin(), courses.end(), [](const PinnedCourseFolder &left, const PinnedCourseFolder &right) {
        if (left.order != right.order) {
            return left.order < right.order;
        }
        const int categoryCompare = left.category.localeAwareCompare(right.category);
        if (categoryCompare != 0) {
            return categoryCompare < 0;
        }
        return left.label.localeAwareCompare(right.label) < 0;
    });
    return courses;
}

bool editSavedFolderDetails(QWidget *parent,
                            const QString &windowTitle,
                            const QString &headline,
                            const PinnedCourseFolder &initialCourse,
                            const QString &defaultColorId,
                            PinnedCourseFolder *outCourse)
{
    if (outCourse == nullptr || initialCourse.path.isEmpty()) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(windowTitle);
    dialog.setModal(true);
    dialog.resize(560, 420);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *headlineLabel = new QLabel(headline, &dialog);
    headlineLabel->setWordWrap(true);
    layout->addWidget(headlineLabel, 0);

    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    auto *pathLabel = new QLabel(initialCourse.path, &dialog);
    pathLabel->setWordWrap(true);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *nameEdit = new QLineEdit(initialCourse.label, &dialog);
    auto *categoryEdit = new QLineEdit(initialCourse.category, &dialog);
    auto *iconCombo = new QComboBox(&dialog);
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("folder")), uiText("Folder"), QStringLiteral("folder"));
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("play")), uiText("Play"), QStringLiteral("play"));
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("details")), uiText("Details"), QStringLiteral("details"));
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("cards")), uiText("Cards"), QStringLiteral("cards"));
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("study")), uiText("Study"), QStringLiteral("study"));
    iconCombo->addItem(playlistViewIcon(&dialog, QStringLiteral("favorites")), uiText("Favorites"), QStringLiteral("favorites"));
    auto *colorCombo = new QComboBox(&dialog);
    for (const auto &accent : revaplayer::application::availableAccents()) {
        colorCombo->addItem(uiText(accent.label.toUtf8().constData()), accent.id);
    }
    auto *descriptionEdit = new QPlainTextEdit(initialCourse.description, &dialog);
    descriptionEdit->setPlaceholderText(uiText("Optional note about this folder"));
    descriptionEdit->setTabChangesFocus(true);
    descriptionEdit->setFixedHeight(88);
    nameEdit->setPlaceholderText(uiText("Saved list name"));
    categoryEdit->setPlaceholderText(uiText("Category / group"));
    formLayout->addRow(uiText("Folder path"), pathLabel);
    formLayout->addRow(uiText("Name"), nameEdit);
    formLayout->addRow(uiText("Category"), categoryEdit);
    formLayout->addRow(uiText("Icon"), iconCombo);
    formLayout->addRow(uiText("Color"), colorCombo);
    formLayout->addRow(uiText("Description"), descriptionEdit);
    layout->addLayout(formLayout, 1);

    iconCombo->setCurrentIndex(std::max(
        0,
        iconCombo->findData(initialCourse.iconId.trimmed().isEmpty() ? QStringLiteral("folder") : initialCourse.iconId)));
    colorCombo->setCurrentIndex(std::max(
        0,
        colorCombo->findData(initialCourse.colorId.trimmed().isEmpty() ? defaultColorId : initialCourse.colorId)));

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok); okButton != nullptr) {
        okButton->setText(uiText("Save"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog, nameEdit]() {
        if (nameEdit != nullptr && !nameEdit->text().trimmed().isEmpty()) {
            dialog.accept();
        }
    });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    PinnedCourseFolder course = initialCourse;
    course.label = nameEdit->text().trimmed();
    course.category = categoryEdit->text().trimmed();
    course.description = descriptionEdit->toPlainText().trimmed();
    course.iconId = iconCombo->currentData().toString().trimmed();
    course.colorId = revaplayer::application::normalizeAccentId(colorCombo->currentData().toString());
    if (course.label.isEmpty()) {
        course.label = QFileInfo(course.path).fileName();
    }
    *outCourse = course;
    return !outCourse->label.isEmpty();
}

void normalizePinnedCourses(QVector<PinnedCourseFolder> *courses)
{
    if (courses == nullptr) {
        return;
    }

    QVector<PinnedCourseFolder> normalized;
    normalized.reserve(courses->size());
    QSet<QString> seenPaths;

    for (PinnedCourseFolder course : std::as_const(*courses)) {
        const QString normalizedPath = QFileInfo(course.path).absoluteFilePath();
        if (normalizedPath.isEmpty() || seenPaths.contains(normalizedPath)) {
            continue;
        }

        course.path = normalizedPath;
        if (course.label.trimmed().isEmpty()) {
            course.label = QFileInfo(normalizedPath).fileName();
        }
        if (course.iconId.trimmed().isEmpty()) {
            course.iconId = QStringLiteral("folder");
        }
        course.colorId = revaplayer::application::normalizeAccentId(
            course.colorId.trimmed().isEmpty() ? QStringLiteral("blue") : course.colorId);
        course.order = normalized.size();
        if (course.key.trimmed().isEmpty()) {
            course.key = pinnedCourseStorageKey(course.path);
        }
        normalized.push_back(std::move(course));
        seenPaths.insert(normalizedPath);
    }

    *courses = std::move(normalized);
}

bool persistPinnedCourses(revaplayer::application::SettingsController *settingsController,
                          QVector<PinnedCourseFolder> courses)
{
    if (settingsController == nullptr) {
        return false;
    }

    normalizePinnedCourses(&courses);
    QSet<QString> keepKeys;
    bool savedAll = true;
    for (const PinnedCourseFolder &course : std::as_const(courses)) {
        const QString storageKey = course.key.trimmed().isEmpty() ? pinnedCourseStorageKey(course.path) : course.key.trimmed();
        if (storageKey.isEmpty()) {
            savedAll = false;
            continue;
        }

        keepKeys.insert(storageKey);
        savedAll = settingsController->setCustomValue(
            storageKey,
            QString::fromUtf8(
                QJsonDocument(
                    pinnedCourseObject(
                        course.path,
                        course.label,
                        course.category,
                        course.description,
                        course.colorId,
                        course.iconId,
                        course.order))
                    .toJson(QJsonDocument::Compact)))
            && savedAll;
    }

    const QStringList existingKeys = settingsController->customKeys(QString::fromLatin1(kPinnedCoursePrefix));
    for (const QString &existingKey : existingKeys) {
        if (!keepKeys.contains(existingKey)) {
            savedAll = settingsController->removeCustomValue(existingKey) && savedAll;
        }
    }

    const QStringList existingOrderKeys = settingsController->customKeys(QString::fromLatin1(kPinnedCourseMediaOrderPrefix));
    for (const QString &existingOrderKey : existingOrderKeys) {
        const QString orderFolderPath = folderPathFromPinnedCourseMediaOrderStorageKey(existingOrderKey);
        const bool keepOrder = std::any_of(courses.cbegin(), courses.cend(), [&orderFolderPath](const PinnedCourseFolder &course) {
            return folderPathIsSameOrDescendantOf(orderFolderPath, course.path);
        });
        if (!keepOrder) {
            savedAll = settingsController->removeCustomValue(existingOrderKey) && savedAll;
        }
    }
    return savedAll;
}

bool manageSavedFoldersDialog(QWidget *parent,
                              const QVector<PinnedCourseFolder> &initialCourses,
                              const QString &defaultColorId,
                              QVector<PinnedCourseFolder> *outCourses)
{
    if (outCourses == nullptr) {
        return false;
    }

    QVector<PinnedCourseFolder> courses = initialCourses;
    normalizePinnedCourses(&courses);

    QDialog dialog(parent);
    dialog.setWindowTitle(uiText("Manage Saved Lists"));
    dialog.setModal(true);
    dialog.resize(860, 540);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);

    auto *headlineLabel = new QLabel(
        uiText("Manage every saved folder here. Add, remove, reorder, and customize how it appears across the playlist."),
        &dialog);
    headlineLabel->setWordWrap(true);
    layout->addWidget(headlineLabel, 0);

    auto *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(12);

    auto *listGroup = new QGroupBox(uiText("Saved folders"), &dialog);
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(10, 10, 10, 10);
    listLayout->setSpacing(8);
    auto *coursesList = new DeferredDragListWidget(listGroup);
    coursesList->setSelectionMode(QAbstractItemView::SingleSelection);
    coursesList->setDragEnabled(true);
    coursesList->setAcceptDrops(true);
    coursesList->setDropIndicatorShown(true);
    coursesList->setDefaultDropAction(Qt::MoveAction);
    coursesList->setDragDropMode(QAbstractItemView::InternalMove);
    coursesList->setDragDropOverwriteMode(false);
    listLayout->addWidget(coursesList, 1);

    auto *actionsLayout = new QGridLayout();
    actionsLayout->setHorizontalSpacing(8);
    actionsLayout->setVerticalSpacing(8);
    auto *addButton = new QPushButton(uiText("Add Folder"), listGroup);
    auto *editButton = new QPushButton(uiText("Edit Details"), listGroup);
    auto *removeButton = new QPushButton(uiText("Remove"), listGroup);
    auto *moveUpButton = new QPushButton(uiText("Move Up"), listGroup);
    auto *moveDownButton = new QPushButton(uiText("Move Down"), listGroup);
    actionsLayout->addWidget(addButton, 0, 0);
    actionsLayout->addWidget(editButton, 0, 1);
    actionsLayout->addWidget(removeButton, 1, 0);
    actionsLayout->addWidget(moveUpButton, 1, 1);
    actionsLayout->addWidget(moveDownButton, 2, 0, 1, 2);
    listLayout->addLayout(actionsLayout, 0);
    contentLayout->addWidget(listGroup, 1);

    auto *detailsGroup = new QGroupBox(uiText("Selected folder"), &dialog);
    auto *detailsLayout = new QFormLayout(detailsGroup);
    detailsLayout->setSpacing(10);
    auto *nameValue = new QLabel(detailsGroup);
    auto *categoryValue = new QLabel(detailsGroup);
    auto *appearanceValue = new QLabel(detailsGroup);
    auto *pathValue = new QLabel(detailsGroup);
    auto *descriptionValue = new QLabel(detailsGroup);
    for (QLabel *label : {nameValue, categoryValue, appearanceValue, pathValue, descriptionValue}) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
    detailsLayout->addRow(uiText("Name"), nameValue);
    detailsLayout->addRow(uiText("Category"), categoryValue);
    detailsLayout->addRow(uiText("Appearance"), appearanceValue);
    detailsLayout->addRow(uiText("Folder path"), pathValue);
    detailsLayout->addRow(uiText("Description"), descriptionValue);
    contentLayout->addWidget(detailsGroup, 1);

    layout->addLayout(contentLayout, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    if (QPushButton *saveButton = buttonBox->button(QDialogButtonBox::Save); saveButton != nullptr) {
        saveButton->setText(uiText("Save"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel); cancelButton != nullptr) {
        cancelButton->setText(uiText("Cancel"));
    }
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);

    auto refreshList = [&]() {
        const int previousRow = coursesList->currentRow();
        const int courseCount = static_cast<int>(courses.size());
        coursesList->clear();
        for (const PinnedCourseFolder &course : std::as_const(courses)) {
            QStringList detailParts;
            if (!course.category.trimmed().isEmpty()) {
                detailParts.push_back(course.category.trimmed());
            }
            if (!course.description.trimmed().isEmpty()) {
                detailParts.push_back(course.description.trimmed());
            } else {
                detailParts.push_back(QFileInfo(course.path).fileName());
            }
            auto *item = new QListWidgetItem(
                playlistViewIcon(coursesList, course.iconId.trimmed().isEmpty() ? QStringLiteral("folder") : course.iconId),
                QStringLiteral("%1\n%2").arg(course.label, detailParts.join(QStringLiteral("  •  "))),
                coursesList);
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable);
            item->setData(Qt::UserRole, course.path);
            item->setForeground(accentColorForId(course.colorId));
            item->setToolTip(QStringLiteral("%1\n%2").arg(course.path, detailParts.join(QStringLiteral("  •  "))));
        }

        if (courses.isEmpty()) {
            coursesList->addItem(uiText("No saved lists yet"));
            QListWidgetItem *placeholder = coursesList->item(0);
            if (placeholder != nullptr) {
                placeholder->setFlags(Qt::NoItemFlags);
                placeholder->setForeground(QColor(QStringLiteral("#8e99ab")));
            }
            coursesList->setCurrentRow(-1);
            return;
        }

        const int targetRow = std::clamp(previousRow, 0, courseCount - 1);
        coursesList->setCurrentRow(targetRow);
    };

    auto refreshDetails = [&]() {
        const int row = coursesList->currentRow();
        const bool validRow = row >= 0 && row < courses.size();
        editButton->setEnabled(validRow);
        removeButton->setEnabled(validRow);
        moveUpButton->setEnabled(validRow && row > 0);
        moveDownButton->setEnabled(validRow && row < courses.size() - 1);

        if (!validRow) {
            nameValue->setText(uiText("Nothing selected"));
            categoryValue->setText(QStringLiteral("—"));
            appearanceValue->setText(QStringLiteral("—"));
            pathValue->setText(QStringLiteral("—"));
            descriptionValue->setText(QStringLiteral("—"));
            return;
        }

        const PinnedCourseFolder &course = courses.at(row);
        nameValue->setText(course.label);
        categoryValue->setText(course.category.trimmed().isEmpty() ? uiText("Unset") : course.category.trimmed());
        appearanceValue->setText(uiText("%1 icon • %2 accent").arg(course.iconId, course.colorId));
        pathValue->setText(course.path);
        descriptionValue->setText(course.description.trimmed().isEmpty() ? uiText("No description") : course.description.trimmed());
    };

    auto editRow = [&](const int row) {
        if (row < 0 || row >= courses.size()) {
            return;
        }

        PinnedCourseFolder updated = courses.at(row);
        if (!editSavedFolderDetails(
                &dialog,
                uiText("Edit Saved List"),
                uiText("Update this saved folder and how it appears inside the playlist."),
                updated,
                defaultColorId,
                &updated)) {
            return;
        }

        courses[row] = updated;
        normalizePinnedCourses(&courses);
        refreshList();
        const int refreshedRow = std::clamp(row, 0, std::max(0, static_cast<int>(courses.size()) - 1));
        if (!courses.isEmpty()) {
            coursesList->setCurrentRow(refreshedRow);
        }
        refreshDetails();
    };

    QObject::connect(coursesList, &QListWidget::currentRowChanged, &dialog, [refreshDetails](const int) {
        refreshDetails();
    });
    QObject::connect(coursesList->model(), &QAbstractItemModel::rowsMoved, &dialog, [&]() {
        QVector<PinnedCourseFolder> reorderedCourses;
        reorderedCourses.reserve(courses.size());
        for (int row = 0; row < coursesList->count(); ++row) {
            QListWidgetItem *item = coursesList->item(row);
            if (item == nullptr) {
                continue;
            }

            const QString path = QFileInfo(item->data(Qt::UserRole).toString()).absoluteFilePath();
            const auto courseIt = std::find_if(courses.cbegin(), courses.cend(), [&path](const PinnedCourseFolder &course) {
                return QFileInfo(course.path).absoluteFilePath() == path;
            });
            if (courseIt != courses.cend()) {
                reorderedCourses.push_back(*courseIt);
            }
        }

        if (reorderedCourses.size() == courses.size()) {
            courses = reorderedCourses;
            normalizePinnedCourses(&courses);
        }
        refreshDetails();
    });
    QObject::connect(coursesList, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem *) {
        editRow(coursesList->currentRow());
    });

    QObject::connect(addButton, &QPushButton::clicked, &dialog, [&]() {
        const QString selectedPath = filedialog::getExistingDirectory(
            &dialog,
            uiText("Choose Folder"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (selectedPath.trimmed().isEmpty()) {
            return;
        }

        const QString normalizedPath = QFileInfo(selectedPath).absoluteFilePath();
        const auto existingIt = std::find_if(courses.cbegin(), courses.cend(), [&normalizedPath](const PinnedCourseFolder &course) {
            return QFileInfo(course.path).absoluteFilePath() == normalizedPath;
        });

        PinnedCourseFolder course;
        if (existingIt != courses.cend()) {
            course = *existingIt;
        } else {
            course.path = normalizedPath;
            course.label = QFileInfo(normalizedPath).fileName();
            course.iconId = QStringLiteral("folder");
            course.colorId = defaultColorId;
            course.order = courses.size();
        }

        if (!editSavedFolderDetails(
                &dialog,
                existingIt == courses.cend() ? uiText("Save Folder as List") : uiText("Edit Saved List"),
                uiText("Choose how this saved folder should appear across the playlist."),
                course,
                defaultColorId,
                &course)) {
            return;
        }

        if (existingIt != courses.cend()) {
            courses[std::distance(courses.cbegin(), existingIt)] = course;
        } else {
            courses.push_back(course);
        }
        normalizePinnedCourses(&courses);
        refreshList();
        const int row = static_cast<int>(std::distance(
            courses.cbegin(),
            std::find_if(courses.cbegin(), courses.cend(), [&normalizedPath](const PinnedCourseFolder &candidate) {
                return QFileInfo(candidate.path).absoluteFilePath() == normalizedPath;
            })));
        if (!courses.isEmpty()) {
            coursesList->setCurrentRow(std::max(0, row));
        }
        refreshDetails();
    });

    QObject::connect(editButton, &QPushButton::clicked, &dialog, [&]() {
        editRow(coursesList->currentRow());
    });

    QObject::connect(removeButton, &QPushButton::clicked, &dialog, [&]() {
        const int row = coursesList->currentRow();
        if (row < 0 || row >= courses.size()) {
            return;
        }

        const QString label = courses.at(row).label;
        if (QMessageBox::question(
                &dialog,
                uiText("Remove Saved List"),
                uiText("Remove \"%1\" from the saved folders?").arg(label))
            != QMessageBox::Yes) {
            return;
        }

        courses.removeAt(row);
        normalizePinnedCourses(&courses);
        refreshList();
        if (!courses.isEmpty()) {
            coursesList->setCurrentRow(std::clamp(row, 0, static_cast<int>(courses.size()) - 1));
        }
        refreshDetails();
    });

    QObject::connect(moveUpButton, &QPushButton::clicked, &dialog, [&]() {
        const int row = coursesList->currentRow();
        if (row <= 0 || row >= courses.size()) {
            return;
        }

        courses.swapItemsAt(row, row - 1);
        normalizePinnedCourses(&courses);
        refreshList();
        coursesList->setCurrentRow(row - 1);
        refreshDetails();
    });

    QObject::connect(moveDownButton, &QPushButton::clicked, &dialog, [&]() {
        const int row = coursesList->currentRow();
        if (row < 0 || row >= courses.size() - 1) {
            return;
        }

        courses.swapItemsAt(row, row + 1);
        normalizePinnedCourses(&courses);
        refreshList();
        coursesList->setCurrentRow(row + 1);
        refreshDetails();
    });

    refreshList();
    refreshDetails();

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    normalizePinnedCourses(&courses);
    *outCourses = courses;
    return true;
}

QString screenshotFormat(const revaplayer::application::SettingsController *settingsController)
{
    const QString raw = customSettingValue(settingsController, kScreenshotFormatSetting, QStringLiteral("png")).trimmed().toLower();
    return raw == QStringLiteral("jpg") || raw == QStringLiteral("jpeg")
        ? QStringLiteral("jpg")
        : QStringLiteral("png");
}

QString screenshotTemplate(const revaplayer::application::SettingsController *settingsController)
{
    const QString raw = customSettingValue(
        settingsController,
        kScreenshotTemplateSetting,
        QStringLiteral("{timestamp}-{title}-{index}")).trimmed();
    return raw.isEmpty() ? QStringLiteral("{timestamp}-{title}-{index}") : raw;
}

QString defaultIdleOverlayText()
{
    return uiText("Open a file, drop media here, or open a URL");
}

QString defaultIdleOverlayMarkup()
{
    return QStringLiteral(
               "<div style='text-align:center;'>"
               "<div style='font-size:28px; font-weight:700; letter-spacing:0.4px;'>%1</div>"
               "<div style='margin-top:10px; font-size:14px; color:#94a8c1;'>%2</div>"
               "<div style='margin-top:18px; font-size:12px; color:#cfd9e5;'>%3 &nbsp;&nbsp;•&nbsp;&nbsp; %4 &nbsp;&nbsp;•&nbsp;&nbsp; %5</div>"
               "</div>")
        .arg(
            uiText("Ready to play"),
            defaultIdleOverlayText(),
            uiText("Ctrl+O Open File"),
            uiText("Ctrl+L Open URL"),
            uiText("Drag and Drop"));
}

QString buildScreenshotOutputPath(const QString &directoryPath,
                                  const QString &mediaLabel,
                                  const QString &format,
                                  const QString &namingTemplate,
                                  const int index)
{
    QString fileName = namingTemplate;
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString safeTitle = revaplayer::application::sanitizeSnapshotFileStem(mediaLabel);
    fileName.replace(QStringLiteral("{timestamp}"), timestamp);
    fileName.replace(QStringLiteral("{title}"), safeTitle);
    fileName.replace(QStringLiteral("{index}"), QStringLiteral("%1").arg(std::max(1, index), 2, 10, QChar('0')));
    fileName = revaplayer::application::sanitizeSnapshotFileStem(fileName);
    const QString suffix = format.trimmed().toLower() == QStringLiteral("jpeg")
        ? QStringLiteral("jpg")
        : (format.trimmed().toLower().isEmpty() ? QStringLiteral("png") : format.trimmed().toLower());
    return QDir(directoryPath).filePath(QStringLiteral("%1.%2").arg(fileName, suffix == QStringLiteral("jpeg") ? QStringLiteral("jpg") : suffix));
}

bool isSupportedSubtitleFile(const QFileInfo &fileInfo)
{
    static const QSet<QString> kSubtitleExtensions {
        QStringLiteral("ass"),
        QStringLiteral("dfxp"),
        QStringLiteral("idx"),
        QStringLiteral("lrc"),
        QStringLiteral("microdvd"),
        QStringLiteral("mpl"),
        QStringLiteral("mpl2"),
        QStringLiteral("pgs"),
        QStringLiteral("rt"),
        QStringLiteral("sami"),
        QStringLiteral("sbv"),
        QStringLiteral("scc"),
        QStringLiteral("smi"),
        QStringLiteral("srt"),
        QStringLiteral("ssa"),
        QStringLiteral("sub"),
        QStringLiteral("sup"),
        QStringLiteral("ttml"),
        QStringLiteral("txt"),
        QStringLiteral("usf"),
        QStringLiteral("vtt"),
        QStringLiteral("webvtt"),
    };

    return fileInfo.exists()
        && fileInfo.isFile()
        && kSubtitleExtensions.contains(fileInfo.suffix().trimmed().toLower());
}

QStringList supportedMediaFilesInDirectory(const QDir &directory, const bool naturalSort)
{
    QFileInfoList siblings = directory.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [](const QFileInfo &entry) {
                      return !isSupportedMediaFile(entry);
                  }),
                   siblings.end());

    if (siblings.isEmpty()) {
        return {};
    }

    if (naturalSort) {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::sort(siblings.begin(), siblings.end(), [&collator](const QFileInfo &left, const QFileInfo &right) {
            return collator.compare(left.fileName(), right.fileName()) < 0;
        });
    }

    QStringList playlist;
    playlist.reserve(siblings.size());
    for (const QFileInfo &entry : std::as_const(siblings)) {
        playlist.push_back(entry.canonicalFilePath().isEmpty() ? entry.absoluteFilePath() : entry.canonicalFilePath());
    }
    return playlist;
}

QFileInfoList supportedSubdirectoriesInDirectory(const QDir &directory, const bool naturalSort)
{
    QFileInfoList directories = directory.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    directories.erase(std::remove_if(directories.begin(), directories.end(), [](const QFileInfo &entry) {
                          return !entry.exists() || !entry.isDir();
                      }),
                      directories.end());

    if (directories.isEmpty()) {
        return {};
    }

    if (naturalSort) {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::sort(directories.begin(), directories.end(), [&collator](const QFileInfo &left, const QFileInfo &right) {
            return collator.compare(left.fileName(), right.fileName()) < 0;
        });
    }

    return directories;
}

QVector<revaplayer::domain::PlaylistEntry> playlistEntriesForSources(const QStringList &sources, const QString &currentSource = {})
{
    QVector<revaplayer::domain::PlaylistEntry> entries;
    entries.reserve(sources.size());
    const QString normalizedCurrentSource = currentSource.trimmed();
    for (qsizetype index = 0; index < sources.size(); ++index) {
        const QString source = sources.at(index).trimmed();
        if (source.isEmpty()) {
            continue;
        }

        revaplayer::domain::PlaylistEntry entry;
        entry.index = entries.size();
        entry.source = source;
        const QString localPath = localMediaPathForSource(source);
        entry.title = localPath.isEmpty() ? source : QFileInfo(localPath).fileName();
        entry.isCurrent = !normalizedCurrentSource.isEmpty() && source == normalizedCurrentSource;
        entries.push_back(std::move(entry));
    }

    return entries;
}

QVector<revaplayer::domain::PlaylistEntry> folderBrowserEntriesForDirectory(const QString &folderPath,
                                                                              const QString &rootFolderPath,
                                                                              const bool naturalSort,
                                                                              const QString &currentSource = {},
                                                                              const QStringList &orderedMediaSources = {})
{
    QVector<revaplayer::domain::PlaylistEntry> entries;
    const QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        return entries;
    }

    const QString absoluteFolderPath = folderInfo.absoluteFilePath();
    const QString absoluteRootFolderPath = QFileInfo(rootFolderPath).absoluteFilePath();
    const QString absoluteParentPath = folderInfo.dir().absolutePath();
    const bool canGoBack = !absoluteRootFolderPath.isEmpty()
        && absoluteFolderPath != absoluteRootFolderPath
        && !absoluteParentPath.isEmpty()
        && absoluteParentPath != absoluteFolderPath;

    if (canGoBack) {
        revaplayer::domain::PlaylistEntry backEntry;
        backEntry.index = entries.size();
        backEntry.source = folderBrowserSourceForPath(absoluteParentPath, true);
        backEntry.title = uiText("Back to %1").arg(folderDisplayName(absoluteParentPath));
        entries.push_back(std::move(backEntry));
    }

    for (const QFileInfo &directoryEntry : supportedSubdirectoriesInDirectory(QDir(absoluteFolderPath), naturalSort)) {
        revaplayer::domain::PlaylistEntry entry;
        entry.index = entries.size();
        entry.source = folderBrowserSourceForPath(directoryEntry.absoluteFilePath());
        entry.title = directoryEntry.fileName().trimmed();
        entries.push_back(std::move(entry));
    }

    const QStringList mediaSources = orderedMediaSources.isEmpty()
        ? supportedMediaFilesInDirectory(QDir(absoluteFolderPath), naturalSort)
        : orderedMediaSources;
    const QVector<revaplayer::domain::PlaylistEntry> mediaEntries = playlistEntriesForSources(mediaSources, currentSource);
    for (const auto &mediaEntry : mediaEntries) {
        revaplayer::domain::PlaylistEntry entry = mediaEntry;
        entry.index = entries.size();
        entries.push_back(std::move(entry));
    }

    return entries;
}

QString localMediaPathForSource(const QString &source)
{
    if (isFolderBrowserSource(source) || isFolderBrowserBackSource(source)) {
        return {};
    }

    const QString trimmedSource = source.trimmed();
    if (trimmedSource.isEmpty()) {
        return {};
    }

    const QUrl url(trimmedSource);
    if (url.isValid() && !url.scheme().isEmpty()) {
        if (url.isLocalFile()) {
            return url.toLocalFile();
        }
        return {};
    }

    if (url.isLocalFile()) {
        return url.toLocalFile();
    }

    return trimmedSource;
}

QString playlistFolderPathForEntries(const QVector<revaplayer::domain::PlaylistEntry> &entries)
{
    QString folderPath;
    for (const auto &entry : entries) {
        const QString localPath = localMediaPathForSource(entry.source).trimmed();
        if (localPath.isEmpty()) {
            return {};
        }

        const QFileInfo fileInfo(localPath);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            return {};
        }

        const QString candidateFolder = fileInfo.dir().absolutePath();
        if (candidateFolder.isEmpty()) {
            return {};
        }
        if (folderPath.isEmpty()) {
            folderPath = candidateFolder;
        } else if (folderPath != candidateFolder) {
            return {};
        }
    }

    return folderPath;
}

QStringList orderedLocalSourcesForEntries(const QVector<revaplayer::domain::PlaylistEntry> &entries,
                                          const QString &folderPath = {})
{
    const QString expectedFolderPath = folderPath.trimmed().isEmpty()
        ? playlistFolderPathForEntries(entries)
        : QFileInfo(folderPath).absoluteFilePath();
    if (expectedFolderPath.isEmpty()) {
        return {};
    }

    QStringList orderedSources;
    orderedSources.reserve(entries.size());
    for (const auto &entry : entries) {
        const QString sourcePath = localMediaPathForSource(entry.source);
        if (sourcePath.isEmpty()) {
            return {};
        }

        const QString localPath = QFileInfo(sourcePath).absoluteFilePath();
        if (localPath.isEmpty() || QFileInfo(localPath).dir().absolutePath() != expectedFolderPath) {
            return {};
        }
        orderedSources.push_back(localPath);
    }
    return normalizedMediaOrderSources(orderedSources);
}

QStringList orderedLocalMediaSourcesForFolderBrowserEntries(const QVector<revaplayer::domain::PlaylistEntry> &entries,
                                                            const QString &folderPath)
{
    const QString expectedFolderPath = QFileInfo(folderPath).absoluteFilePath();
    if (expectedFolderPath.isEmpty()) {
        return {};
    }

    QStringList orderedSources;
    orderedSources.reserve(entries.size());
    for (const auto &entry : entries) {
        const QString sourcePath = localMediaPathForSource(entry.source);
        if (sourcePath.isEmpty()) {
            continue;
        }

        const QString localPath = QFileInfo(sourcePath).absoluteFilePath();
        if (localPath.isEmpty()) {
            continue;
        }

        const QFileInfo localInfo(localPath);
        if (localInfo.exists() && localInfo.isFile() && localInfo.dir().absolutePath() == expectedFolderPath) {
            orderedSources.push_back(localPath);
        }
    }
    return normalizedMediaOrderSources(orderedSources);
}

QStringList bookmarkCategorySuggestions(const QVector<revaplayer::domain::Bookmark> &bookmarks)
{
    QStringList categories {
        QStringLiteral("Favorite"),
        QStringLiteral("Interesting"),
        QStringLiteral("Scene"),
        QStringLiteral("Review"),
    };

    for (const auto &bookmark : bookmarks) {
        const QString category = bookmark.category.trimmed();
        if (!category.isEmpty() && !categories.contains(category, Qt::CaseInsensitive)) {
            categories.push_back(category);
        }
    }

    return categories;
}

int subtitleLanguageScore(const revaplayer::domain::TrackInfo &track,
                          const QStringList &preferredLanguages,
                          const bool preferExternal)
{
    if (track.type != revaplayer::domain::TrackType::Subtitle || track.id < 0) {
        return std::numeric_limits<int>::min();
    }

    int score = 0;
    if (track.external) {
        score += preferExternal ? 140 : 20;
    }

    const QString language = track.language.trimmed().toLower();
    const QString title = track.title.trimmed().toLower();
    for (int index = 0; index < preferredLanguages.size(); ++index) {
        const QString token = preferredLanguages.at(index).trimmed().toLower();
        if (token.isEmpty()) {
            continue;
        }

        if (language == token) {
            score += 120 - (index * 10);
            break;
        }
        if (language.startsWith(token) || title.contains(token)) {
            score += 80 - (index * 8);
            break;
        }
    }

    if (title.contains(QStringLiteral("forced"))) {
        score += 18;
    }
    if (title.contains(QStringLiteral("full")) || title.contains(QStringLiteral("dialog"))) {
        score += 8;
    }

    return score;
}

int subtitleFileScore(const QFileInfo &fileInfo,
                      const QString &baseName,
                      const QStringList &preferredLanguages)
{
    const QString lowerName = fileInfo.completeBaseName().trimmed().toLower();
    const QString base = baseName.trimmed().toLower();
    int score = 0;

    if (lowerName == base) {
        score += 160;
    } else if (lowerName.startsWith(base + QStringLiteral(".")) || lowerName.startsWith(base + QStringLiteral("_"))) {
        score += 120;
    } else if (lowerName.contains(base)) {
        score += 80;
    }

    for (int index = 0; index < preferredLanguages.size(); ++index) {
        const QString token = preferredLanguages.at(index).trimmed().toLower();
        if (token.isEmpty()) {
            continue;
        }
        if (lowerName.contains(token)) {
            score += 60 - (index * 8);
            break;
        }
    }

    if (lowerName.contains(QStringLiteral("forced"))) {
        score += 18;
    }

    return score;
}

bool subtitleMatchesSameNameOnly(const QFileInfo &fileInfo, const QString &baseName)
{
    return fileInfo.completeBaseName().trimmed().compare(baseName.trimmed(), Qt::CaseInsensitive) == 0;
}

bool subtitleMatchesSameNameWithLanguageSuffix(const QFileInfo &fileInfo, const QString &baseName)
{
    const QString lowerName = fileInfo.completeBaseName().trimmed().toLower();
    const QString base = baseName.trimmed().toLower();
    if (lowerName == base) {
        return true;
    }

    auto startsWithDelimiter = [&lowerName, &base](const QChar delimiter) {
        return lowerName.startsWith(base + delimiter);
    };
    if (!startsWithDelimiter(QChar('.')) && !startsWithDelimiter(QChar('_'))) {
        return false;
    }

    const QString suffix = lowerName.mid(base.size() + 1);
    if (suffix.isEmpty()) {
        return false;
    }

    const QString firstToken = suffix.split(QRegularExpression(QStringLiteral("[._-]")), Qt::SkipEmptyParts).value(0).trimmed();
    if (firstToken.size() < 2 || firstToken.size() > 8) {
        return false;
    }
    for (const QChar character : firstToken) {
        if (!character.isLetterOrNumber()) {
            return false;
        }
    }
    return true;
}

QString jsonString(const QJsonObject &object, const QString &key, const QString &fallback = {})
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

}  // namespace

MainWindow::MainWindow(revaplayer::application::BookmarkController *bookmarkController,
                       revaplayer::application::HistoryController *historyController,
                       revaplayer::application::PlaybackController *playbackController,
                       revaplayer::application::PlaylistController *playlistController,
                       revaplayer::application::SnapshotController *snapshotController,
                       revaplayer::application::SettingsController *settingsController,
                       QWidget *parent)
    : QMainWindow(parent)
    , bookmarkController_(bookmarkController)
    , historyController_(historyController)
    , playbackController_(playbackController)
    , playlistController_(playlistController)
    , snapshotController_(snapshotController)
    , settingsController_(settingsController)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAcceptDrops(true);
    const QIcon applicationIcon = QApplication::windowIcon();
    if (!applicationIcon.isNull()) {
        setWindowIcon(applicationIcon);
    }
    installEventFilter(this);
    if (qApp != nullptr) {
        qApp->installEventFilter(this);
    }
    resize(1440, 860);
    setMinimumSize(200, 120);
    applyLegacyWindowChromeMigration(settingsController_);

    setupUi();
    thumbnailService_ = new revaplayer::services::media::ThumbnailService(this);
    metadataScanService_ = new revaplayer::services::media::MetadataScanService(this);
    hoverPreviewPopup_ = new HoverPreviewPopup(this);
    previewRequestTimer_ = new QTimer(this);
    previewRequestTimer_->setSingleShot(true);
    previewStatusTimer_ = new QTimer(this);
    previewStatusTimer_->setSingleShot(true);
    metadataRefreshTimer_ = new QTimer(this);
    metadataRefreshTimer_->setSingleShot(true);
    pendingPlaylistSelectionTimer_ = new QTimer(this);
    pendingPlaylistSelectionTimer_->setSingleShot(true);
    mediaInformationOverlayTimer_ = new QTimer(this);
    mediaInformationOverlayTimer_->setSingleShot(true);
    fullscreenChromeTimer_ = new QTimer(this);
    fullscreenChromeTimer_->setSingleShot(true);
    pointerLeaveTimer_ = new QTimer(this);
    pointerLeaveTimer_->setSingleShot(true);
    burstScreenshotTimer_ = new QTimer(this);
    burstScreenshotTimer_->setSingleShot(true);
    playlistThumbnailProcess_ = new QProcess(this);
    playlistThumbnailProcess_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(pointerLeaveTimer_, &QTimer::timeout, this, &MainWindow::evaluateManagedPointerLeave);
    connect(mediaInformationOverlayTimer_, &QTimer::timeout, this, &MainWindow::hideMediaInformationOverlay);
    connect(
        playlistThumbnailProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](const int exitCode, const QProcess::ExitStatus exitStatus) {
            handlePlaylistThumbnailProcessFinished(exitCode, exitStatus == QProcess::NormalExit);
        });
    connect(playlistThumbnailProcess_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError) {
        if (activePlaylistThumbnailRequestKey_.isEmpty()) {
            return;
        }

        playlistThumbnailFailedRequestKeys_.insert(activePlaylistThumbnailRequestKey_);
        if (!activePlaylistThumbnailOutputPath_.trimmed().isEmpty()) {
            QFile::remove(activePlaylistThumbnailOutputPath_);
        }
        activePlaylistThumbnailSource_.clear();
        activePlaylistThumbnailRequestKey_.clear();
        activePlaylistThumbnailOutputPath_.clear();
        startNextPlaylistThumbnailRequest();
    });
    connect(metadataRefreshTimer_, &QTimer::timeout, this, [this]() {
        refreshPlaylistPresentationData();
        refreshPlaylistSummary();
        refreshPlaylistInspector();
    });
    connect(pendingPlaylistSelectionTimer_, &QTimer::timeout, this, &MainWindow::attemptPendingPlaylistSelection);
    setupMenuBar();
    setupDockWidgets();
    setupPlaybackActions();
    applyShortcutPreferences();
    applyPlaybackProfile();
    setRepeatMode(configuredRepeatMode(), false);
    applyRuntimePreferences();
    defaultWindowState_ = saveState();
    restorePersistentState();
    connectUi();
    applyUiPreferences();
    (void)ensureBookmarkStorageReady(false);
    populateSecondarySubtitleOptions();
    clearSceneBrowser(uiText("Load a local video to browse scenes"));
    updateHomeDashboardVisibility();
    QTimer::singleShot(0, this, [this]() {
        reloadHistoryPanel();
        populateSecondarySubtitleOptions();
        updateHomeDashboardVisibility();
    });
    QTimer::singleShot(deferredStartupRefreshMs_, this, [this]() {
        if (metadataScanService_ != nullptr && metadataScanService_->initialize()) {
            updatePlaylistMetadataScanButtonState();
        }
        rebuildPinnedCourseTabs();
        reloadHomeDashboard();
        updateHomeDashboardVisibility();
    });
    updateWindowTitle();
    statusBar()->showMessage(uiText("Ready"), 2500);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
    persistPlaybackProgress(false, true);
    releaseDisplaySleepInhibition();
    if (clearHistoryOnExitEnabled() && historyController_ != nullptr && historyController_->isReady()) {
        historyController_->clearHistory();
    }
    persistWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event != nullptr && event->type() == QEvent::WindowStateChange) {
        const bool enteringFullscreen = isFullScreen();
        fullscreenTransitionActive_ = true;
        suppressVideoOverlayUpdates_ = true;

        const auto finishTransition = [this]() {
            updateFullscreenChromeMode();
            fullscreenTransitionActive_ = false;
            suppressVideoOverlayUpdates_ = false;
            updateVideoOverlayGeometry();
            if (isFullScreen()) {
                QTimer::singleShot(90, this, [this]() {
                    if (isFullScreen()) {
                        updateVideoOverlayGeometry();
                    }
                });
                QTimer::singleShot(180, this, [this]() {
                    if (isFullScreen()) {
                        updateVideoOverlayGeometry();
                    }
                });
            }
            if (pendingPlaylistMetadataRefresh_) {
                pendingPlaylistMetadataRefresh_ = false;
                schedulePlaylistMetadataRefresh(120);
            }
        };

        if (enteringFullscreen) {
            QTimer::singleShot(0, this, finishTransition);
        } else {
            // Give the window manager one frame to settle before reattaching docked chrome.
            QTimer::singleShot(85, this, finishTransition);
        }
    }
}

revaplayer::infrastructure::mpv::MpvRenderHost *MainWindow::renderHost() const
{
    return videoViewport_->renderHost();
}

void MainWindow::openStartupRequest(const QString &startupUrl, const QStringList &startupFiles)
{
    if (!startupUrl.trimmed().isEmpty()) {
        openMediaSource(startupUrl);
        return;
    }

    if (!startupFiles.isEmpty()) {
        openLocalMediaFiles(startupFiles, false);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (!parseDroppedMedia(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const DroppedMediaPayload payload = parseDroppedMedia(event->mimeData());
    if (payload.isEmpty()) {
        statusBar()->showMessage(uiText("Drop a local media file or a valid media URL."), 4000);
        event->ignore();
        return;
    }

    if (!payload.files.isEmpty()) {
        if (!openLocalMediaFiles(payload.files)) {
            event->ignore();
            return;
        }
    } else if (!payload.remoteUrls.isEmpty()) {
        beginLoadFeedback(payload.remoteUrls.first());
        enforceHiddenSidePanelsAfterMediaOpen(1100, true);
        playbackController_->openUrl(payload.remoteUrls.first());
    }

    event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event == nullptr) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (triggerConfiguredShortcut(keyEvent)) {
            keyEvent->accept();
            return true;
        }
    }

    if ((watched == playlistResizeHandle_ || watched == detailsResizeHandle_) && panelOverlayModeActive_) {
        const SidePanel resizedPanel = watched == detailsResizeHandle_ ? SidePanel::Details : SidePanel::Playlist;
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && videoViewport_ != nullptr) {
                overlayPanelResizeActive_ = true;
                overlayPanelResizeStartGlobalX_ = mouseEventGlobalPoint(mouseEvent).x();
                overlayPanelResizeStartWidth_ = overlayPanelWidthFor(resizedPanel) > 0
                    ? overlayPanelWidthFor(resizedPanel)
                    : std::max(380, (videoViewport_->width() * 36) / 100);
                if (fullscreenChromeTimer_ != nullptr) {
                    fullscreenChromeTimer_->stop();
                }
                mouseEvent->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (overlayPanelResizeActive_ && (mouseEvent->buttons() & Qt::LeftButton) && videoViewport_ != nullptr) {
                const int delta = overlayPanelResizeStartGlobalX_ - mouseEventGlobalPoint(mouseEvent).x();
                const int maxWidth = std::max(380, videoViewport_->width() - 12);
                const int resizedWidth = std::clamp(
                    overlayPanelResizeStartWidth_ + delta,
                    kMinimumOverlayPanelWidth,
                    maxWidth);
                setOverlayPanelWidthFor(resizedPanel, resizedWidth, false);
                updateVideoOverlayGeometry();
                mouseEvent->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease:
            if (overlayPanelResizeActive_) {
                setOverlayPanelWidthFor(resizedPanel, overlayPanelWidth_, true);
            }
            overlayPanelResizeActive_ = false;
            return true;
        case QEvent::Hide:
            if (overlayPanelResizeActive_) {
                setOverlayPanelWidthFor(resizedPanel, overlayPanelWidth_, true);
            }
            overlayPanelResizeActive_ = false;
            break;
        default:
            break;
        }
    }

    if (watched == videoViewport_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateVideoOverlayGeometry();
    }
    if ((watched == this || watched == playlistDock_ || watched == detailsDock_)
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateAdaptiveUiLayout();
    }
    if (playlistView_ != nullptr
        && watched == playlistView_->viewport()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        requestPlaylistThumbnailBatch(true);
    }
    if (watched == pinnedCoursesTabBar_ && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            showPinnedCourseContextMenu(mouseEventLocalPoint(mouseEvent));
            mouseEvent->accept();
            return true;
        }
    }

    const bool managedChromeWidget = watched == controlBar_
        || watched == fullscreenTopBar_
        || watched == sidePanelSelector_
        || watched == playlistDock_
        || watched == detailsDock_
        || watched == playlistResizeHandle_
        || watched == detailsResizeHandle_
        || watched == (playlistDock_ != nullptr ? playlistDock_->widget() : nullptr)
        || watched == (detailsDock_ != nullptr ? detailsDock_->widget() : nullptr);
    if (managedChromeWidget) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::MouseMove:
            if (pointerLeaveTimer_ != nullptr) {
                pointerLeaveTimer_->stop();
            }
            if (isFullScreen() && fullscreenAutoHideEnabled() && fullscreenChromeTimer_ != nullptr) {
                fullscreenChromeTimer_->stop();
            }
            break;
        case QEvent::Leave:
            scheduleManagedPointerLeaveCheck();
            break;
        default:
            break;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);

    if (startupWindowModeApplied_) {
        return;
    }

    startupWindowModeApplied_ = true;
    if (applyFullscreenOnShow_) {
        showFullScreen();
        updateFullscreenChromeMode();
        return;
    }

    if (applyMaximizedOnShow_) {
        showMaximized();
    }

    QTimer::singleShot(0, this, &MainWindow::showFirstRunWizardIfNeeded);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event != nullptr
        && event->modifiers() == Qt::NoModifier
        && event->key() == Qt::Key_Escape
        && isFullScreen()) {
        toggleFullscreen();
        event->accept();
        return;
    }

    if (event != nullptr
        && event->modifiers() == Qt::NoModifier
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        if (!isTextInputLikeWidget(QApplication::focusWidget())) {
            toggleFullscreen();
            event->accept();
            return;
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::openFiles()
{
    QString initialDirectory = QDir::homePath();
    if (settingsController_ != nullptr && settingsController_->rememberLastOpenDirectory()) {
        const QString storedDirectory = settingsController_->lastOpenDirectory();
        if (!storedDirectory.isEmpty()) {
            initialDirectory = storedDirectory;
        }
    }

    const QStringList files = filedialog::getOpenMediaFileNames(
        this,
        uiText("Open Media"),
        initialDirectory,
        uiText("Media Files (*.mkv *.mp4 *.webm *.avi *.mov *.mp3 *.flac *.wav *.m4a *.ogg);;All Files (*)"));

    if (!files.isEmpty()) {
        openLocalMediaFiles(files);
    }
}

void MainWindow::openFolder()
{
    QString initialDirectory = QDir::homePath();
    if (settingsController_ != nullptr && settingsController_->rememberLastOpenDirectory()) {
        const QString storedDirectory = settingsController_->lastOpenDirectory();
        if (!storedDirectory.isEmpty()) {
            initialDirectory = storedDirectory;
        }
    }

    const QString selectedPath = filedialog::getExistingDirectory(
        this,
        uiText("Open Folder"),
        initialDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selectedPath.isEmpty()) {
        return;
    }

    const QFileInfo selectedFolderInfo(selectedPath);
    if (settingsController_ != nullptr
        && settingsController_->rememberLastOpenDirectory()
        && selectedFolderInfo.exists()
        && selectedFolderInfo.isDir()) {
        settingsController_->setLastOpenDirectory(selectedFolderInfo.absoluteFilePath());
    }

    browseFolderPath(selectedFolderInfo.absoluteFilePath(), true);
}

void MainWindow::openUrl()
{
    clearPendingCurrentMediaRestore();
    bool accepted = false;
    const QString input = QInputDialog::getText(
        this,
        uiText("Open URL"),
        uiText("Media URL or local path"),
        QLineEdit::Normal,
        QStringLiteral("https://"),
        &accepted);

    if (!accepted || input.trimmed().isEmpty()) {
        return;
    }
    openMediaSource(input);
}

void MainWindow::loadSubtitleFile()
{
    if (!mediaLoaded_ || playbackController_ == nullptr) {
        statusBar()->showMessage(uiText("Load media before attaching an external subtitle."), 4000);
        return;
    }

    QString initialDirectory = QDir::homePath();
    const QString currentLocalPath = localMediaPathForSource(currentMediaSource_);
    if (!currentLocalPath.isEmpty()) {
        const QFileInfo currentFileInfo(currentLocalPath);
        if (currentFileInfo.dir().exists()) {
            initialDirectory = currentFileInfo.absolutePath();
        }
    }

    if (settingsController_ != nullptr && settingsController_->rememberLastOpenDirectory()) {
        const QString storedDirectory = settingsController_->lastOpenDirectory();
        if (!storedDirectory.isEmpty() && initialDirectory == QDir::homePath()) {
            initialDirectory = storedDirectory;
        }
    }

    const QString subtitlePath = filedialog::getOpenFileName(
        this,
        uiText("Load Subtitle File"),
        initialDirectory,
        uiText("Subtitle Files (*.srt *.ass *.ssa *.sub *.idx *.vtt *.webvtt *.ttml *.dfxp *.smi *.sami *.mpl *.mpl2 *.txt *.lrc *.pgs *.sup *.usf *.rt *.sbv *.scc);;All Files (*)"));

    if (subtitlePath.trimmed().isEmpty()) {
        return;
    }

    const QFileInfo subtitleFileInfo(subtitlePath);
    if (!subtitleFileInfo.exists() || !subtitleFileInfo.isFile()) {
        statusBar()->showMessage(uiText("The selected subtitle file does not exist."), 5000);
        return;
    }

    if (settingsController_ != nullptr
        && settingsController_->rememberLastOpenDirectory()
        && subtitleFileInfo.dir().exists()) {
        settingsController_->setLastOpenDirectory(subtitleFileInfo.absolutePath());
    }

    playbackController_->loadSubtitleFile(subtitleFileInfo.absoluteFilePath());
    setSidePanelVisible(SidePanel::Details, true, true);
    if (detailsTabs_ != nullptr && tracksTree_ != nullptr) {
        detailsTabs_->setCurrentWidget(tracksTree_);
    }
    statusBar()->showMessage(uiText("Loading subtitle: %1").arg(subtitleFileInfo.fileName()), 3000);
}

void MainWindow::showMediaInfoDialog()
{
    if (!mediaLoaded_) {
        statusBar()->showMessage(uiText("Load media before opening Media Info."), 4000);
        return;
    }

    if (mediaInfoDialog_ == nullptr) {
        mediaInfoDialog_ = new MediaInfoDialog(this);
    }

    updateMediaInfoDialog(true);
    mediaInfoDialog_->show();
    mediaInfoDialog_->raise();
    mediaInfoDialog_->activateWindow();
}

void MainWindow::takeScreenshot()
{
    if (snapshotController_ == nullptr) {
        statusBar()->showMessage(uiText("Screenshot capture is not available."), 4000);
        return;
    }

    const QString mediaLabel = effectiveCurrentMediaTitle();
    QString outputPath;
    QString errorMessage;
    if (!snapshotController_->captureScreenshot(mediaLabel, &outputPath, &errorMessage)) {
        statusBar()->showMessage(
            errorMessage.isEmpty() ? uiText("Could not capture a screenshot.") : errorMessage,
            5000);
        return;
    }

    showActionResult(
        uiText("Screenshot requested: %1").arg(outputPath),
        uiText("Screenshot requested: %1").arg(QFileInfo(outputPath).fileName()),
        5000);
}

void MainWindow::addBookmark()
{
    if (!mediaLoaded_ || currentMediaSource_.trimmed().isEmpty()) {
        statusBar()->showMessage(uiText("Load media before creating a bookmark."), 4000);
        return;
    }

    if (!ensureBookmarkStorageReady(true)) {
        return;
    }

    BookmarkDialog dialog(
        formatPlaybackTime(currentPositionSeconds_),
        defaultBookmarkTitle(effectiveCurrentMediaTitle(), currentPositionSeconds_),
        bookmarkCategorySuggestions(bookmarkController_->bookmarksFor(currentMediaSource_)),
        this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString bookmarkTitle = dialog.bookmarkTitle().trimmed().isEmpty()
        ? uiText("Bookmark")
        : dialog.bookmarkTitle().trimmed();
    const auto createdBookmark = bookmarkController_->createBookmark(
        currentMediaSource_,
        bookmarkTitle,
        currentPositionSeconds_,
        dialog.bookmarkNote(),
        dialog.bookmarkCategory());

    if (!createdBookmark.has_value()) {
        const QString failureMessage = bookmarkController_->lastError().trimmed().isEmpty()
            ? uiText("Could not create the bookmark.")
            : uiText("Could not create the bookmark: %1").arg(bookmarkController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }

    reloadBookmarks();
    setSidePanelVisible(SidePanel::Details, true, true);
    if (detailsTabs_ != nullptr && bookmarksPage_ != nullptr) {
        detailsTabs_->setCurrentWidget(bookmarksPage_);
    }
    if (bookmarksList_ != nullptr && bookmarksList_->count() > 0) {
        bookmarksList_->setCurrentRow(bookmarksList_->count() - 1);
        bookmarksList_->scrollToItem(bookmarksList_->currentItem());
    }
    const QString message = uiText("Saved bookmark \"%1\" at %2").arg(bookmarkTitle, formatPlaybackTime(currentPositionSeconds_));
    showActionResult(message, message, 4000);
}

void MainWindow::onBookmarkActivated(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    playbackController_->seekToSeconds(item->data(Qt::UserRole + 1).toDouble());
}

void MainWindow::showBookmarksContextMenu(const QPoint &position)
{
    QMenu menu(this);
    menu.addAction(addBookmarkAction_);
    QAction *importAction = menu.addAction(uiText("Import Bookmarks"));
    QAction *exportAction = menu.addAction(uiText("Export Bookmarks"));
    if (bookmarksList_ != nullptr && bookmarksList_->itemAt(position) != nullptr) {
        menu.addAction(deleteBookmarkAction_);
    }
    forceMenuLeftToRight(&menu);
    QAction *chosenAction = menu.exec(bookmarksList_->viewport()->mapToGlobal(position));
    if (chosenAction == importAction) {
        importBookmarksForCurrentMedia();
    } else if (chosenAction == exportAction) {
        exportBookmarksForCurrentMedia();
    }
}

void MainWindow::resetWindowLayout()
{
    if (settingsController_ != nullptr) {
        settingsController_->clearMainWindowState();
    }

    showNormal();
    restoreState(defaultWindowState_);
    resize(1440, 860);
    setSidePanelVisible(SidePanel::Playlist, true, true);
    if (!panelsOverlayEnabled()) {
        setSidePanelVisible(SidePanel::Details, true, false);
    }
    applyUiPreferences();
        statusBar()->showMessage(uiText("Window layout reset"), 3000);
}

QJsonObject MainWindow::captureLayoutPresetState() const
{
    QJsonObject state;
    const QByteArray geometry = saveGeometry();
    const QByteArray windowState = panelOverlayModeActive_ ? defaultWindowState_ : saveState();
    state.insert(QStringLiteral("geometry"), QString::fromLatin1(geometry.toBase64()));
    state.insert(QStringLiteral("window_state"), QString::fromLatin1(windowState.toBase64()));
    state.insert(QStringLiteral("maximized"), isMaximized());
    state.insert(QStringLiteral("fullscreen"), isFullScreen());
    state.insert(QStringLiteral("playlist_visible"), isSidePanelVisible(SidePanel::Playlist));
    state.insert(QStringLiteral("details_visible"), isSidePanelVisible(SidePanel::Details));
    state.insert(QStringLiteral("details_tab_index"), detailsTabs_ != nullptr ? detailsTabs_->currentIndex() : 0);
    state.insert(QStringLiteral("overlay_width"), overlayPanelWidth_);
    state.insert(QStringLiteral("overlay_playlist_width"), playlistOverlayPanelWidth_);
    state.insert(QStringLiteral("overlay_details_width"), detailsOverlayPanelWidth_);
    state.insert(QStringLiteral("menu_bar_visible"), menuBar() != nullptr && menuBar()->isVisible());
    state.insert(QStringLiteral("captured_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    return state;
}

bool MainWindow::applyLayoutPresetState(const QJsonObject &state, QString *errorMessage)
{
    if (state.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = uiText("The saved layout preset is empty.");
        }
        return false;
    }

    const QByteArray geometry = QByteArray::fromBase64(state.value(QStringLiteral("geometry")).toString().toLatin1());
    const QByteArray windowState = QByteArray::fromBase64(state.value(QStringLiteral("window_state")).toString().toLatin1());
    const bool maximized = state.value(QStringLiteral("maximized")).toBool(false);
    const bool fullscreen = state.value(QStringLiteral("fullscreen")).toBool(false);
    const bool playlistVisible = state.value(QStringLiteral("playlist_visible")).toBool(true);
    const bool detailsVisible = state.value(QStringLiteral("details_visible")).toBool(false);
    const int detailsTabIndex = state.value(QStringLiteral("details_tab_index")).toInt(0);
    const int legacyOverlayWidth = clampPersistedOverlayPanelWidth(
        state.value(QStringLiteral("overlay_width")).toInt(overlayPanelWidth_));
    const int overlayPlaylistWidth = clampPersistedOverlayPanelWidth(
        state.contains(QStringLiteral("overlay_playlist_width"))
            ? state.value(QStringLiteral("overlay_playlist_width")).toInt(legacyOverlayWidth)
            : legacyOverlayWidth);
    const int overlayDetailsWidth = clampPersistedOverlayPanelWidth(
        state.contains(QStringLiteral("overlay_details_width"))
            ? state.value(QStringLiteral("overlay_details_width")).toInt(legacyOverlayWidth)
            : legacyOverlayWidth);
    const bool menuBarVisible = state.value(QStringLiteral("menu_bar_visible")).toBool(true);

    showNormal();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    if (!windowState.isEmpty()) {
        restoreState(windowState);
    }

    playlistOverlayPanelWidth_ = overlayPlaylistWidth;
    detailsOverlayPanelWidth_ = overlayDetailsWidth;
    syncOverlayPanelWidthForActivePanel();
    if (!fullscreen && menuBar() != nullptr) {
        menuBar()->setVisible(menuBarVisible);
    }
    setSidePanelVisible(SidePanel::Playlist, playlistVisible, playlistVisible);
    setSidePanelVisible(SidePanel::Details, detailsVisible, detailsVisible && !playlistVisible);
    if (detailsTabs_ != nullptr && detailsTabIndex >= 0 && detailsTabIndex < detailsTabs_->count()) {
        detailsTabs_->setCurrentIndex(detailsTabIndex);
    }
    syncOverlayPanelWidthForActivePanel();
    if (settingsController_ != nullptr) {
        settingsController_->setPlaylistOverlayPanelWidth(playlistOverlayPanelWidth_);
        settingsController_->setDetailsOverlayPanelWidth(detailsOverlayPanelWidth_);
    }

    if (fullscreen) {
        showFullScreen();
        updateFullscreenChromeMode();
    } else if (maximized) {
        showMaximized();
    }

    updateStatusBarVisibility();
    updateVideoOverlayGeometry();
    syncPanelToggleActions();
    refreshPlaylistSummary();
    focusCurrentPlaylistItem(false);
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void MainWindow::saveCurrentLayoutPreset()
{
    if (settingsController_ == nullptr) {
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        uiText("Save Layout Preset"),
        uiText("Preset name"),
        QLineEdit::Normal,
        QString {},
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    QJsonObject state = captureLayoutPresetState();
    state.insert(QStringLiteral("name"), name);
    settingsController_->setCustomValue(
        layoutPresetStorageKey(name),
        QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));
    statusBar()->showMessage(uiText("Saved layout preset: %1").arg(name), 3000);
}

void MainWindow::loadLayoutPreset()
{
    if (settingsController_ == nullptr) {
        return;
    }

    const QStringList names = savedLayoutPresetNames(settingsController_);
    if (names.isEmpty()) {
        statusBar()->showMessage(uiText("No saved layout presets yet"), 2500);
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getItem(
        this,
        uiText("Load Layout Preset"),
        uiText("Choose a layout preset"),
        names,
        0,
        false,
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    const QJsonObject state = QJsonDocument::fromJson(settingsController_->customValue(layoutPresetStorageKey(name)).toUtf8()).object();
    QString errorMessage;
    if (!applyLayoutPresetState(state, &errorMessage)) {
        statusBar()->showMessage(errorMessage.isEmpty() ? uiText("Could not load the layout preset.") : errorMessage, 4000);
        return;
    }
    statusBar()->showMessage(uiText("Loaded layout preset: %1").arg(name), 3000);
}

void MainWindow::deleteLayoutPreset()
{
    if (settingsController_ == nullptr) {
        return;
    }

    const QStringList names = savedLayoutPresetNames(settingsController_);
    if (names.isEmpty()) {
        statusBar()->showMessage(uiText("No saved layout presets yet"), 2500);
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getItem(
        this,
        uiText("Delete Layout Preset"),
        uiText("Choose a layout preset"),
        names,
        0,
        false,
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    settingsController_->removeCustomValue(layoutPresetStorageKey(name));
    statusBar()->showMessage(uiText("Deleted layout preset: %1").arg(name), 3000);
}

void MainWindow::showSettingsDialog()
{
    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    const bool originalSubtitleVisible = subtitleVisible_;
    const double originalSubtitleScale = currentSubtitleScale_;
    const int originalSubtitlePosition = currentSubtitlePosition_;
    const QString originalSubtitleFontFamily = currentSubtitleFontFamily_;
    const int originalSubtitleFontSize = currentSubtitleFontSize_;
    const QString originalSubtitleAssOverride = currentSubtitleAssOverride_;
    SettingsDialog dialog(settingsController_, historyController_, buildShortcutBindings(), this);
    connect(&dialog, &SettingsDialog::subtitlePreviewRequested, this, [this](const bool visible,
                                                                             const double scale,
                                                                             const int position,
                                                                             const QString &fontFamily,
                                                                             const int fontSize,
                                                                             const QString &assOverride) {
        applySubtitlePreviewState(visible, scale, position, fontFamily, fontSize, assOverride, false);
    });
    connect(&dialog, &SettingsDialog::loadSubtitleFileRequested, this, [this]() {
        loadSubtitleFile();
    });
    connect(&dialog, &SettingsDialog::clearCacheRequested, this, [this]() {
        clearApplicationCache();
    });
    connect(&dialog, &SettingsDialog::factoryResetRequested, this, [this, &dialog]() {
        dialog.done(QDialog::Rejected);
        QTimer::singleShot(0, this, &MainWindow::factoryResetApplication);
    });
    connect(&dialog, &SettingsDialog::settingsApplied, this, [this, previousLanguage]() {
        const QString appliedLanguage = settingsController_ != nullptr
            ? settingsController_->interfaceLanguage()
            : revaplayer::application::defaultUiLanguageId();
        revaplayer::application::setCurrentUiLanguage(appliedLanguage);
        QApplication::setLayoutDirection(revaplayer::application::currentUiLanguageDirection());
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        applySelectedTheme(false);
        applyShortcutPreferences();
        if (settingsController_ != nullptr && !settingsController_->rememberWindowState()) {
            settingsController_->clearMainWindowState();
        }

        applyPlaybackProfile(false);
        setRepeatMode(configuredRepeatMode(), false);
        trimHistoryToLimit();
        reloadHistoryPanel();
        applyRuntimePreferences();
        applyUiPreferences();
        if (sessionWidePlaybackSpeedEnabled()) {
            sessionPlaybackSpeed_ = currentSpeed_;
        }
        if (settingsController_ == nullptr || !settingsController_->rememberLastVolume()) {
            playbackController_->setVolume(
                settingsController_ != nullptr
                    ? settingsController_->startupVolume()
                    : revaplayer::application::kDefaultPlaybackVolume);
        }
        const QString profileLabel = settingsController_ != nullptr
            ? revaplayer::domain::playerProfileLabel(settingsController_->playbackProfile())
            : QStringLiteral("Balanced");
        QString statusMessage = uiText("Preferences applied (%1 profile, %2 theme)")
            .arg(revaplayer::application::translateUiText(profileLabel),
                 revaplayer::application::translateUiText(revaplayer::application::themeLabel(selectedThemeId())));
        if (previousLanguage != appliedLanguage) {
            statusMessage.append(QStringLiteral("  •  %1").arg(uiText("Restart the application to fully apply the selected interface language.")));
        }
        statusBar()->showMessage(statusMessage, 4500);
    });
    if (dialog.exec() != QDialog::Accepted) {
        applySubtitlePreviewState(
            originalSubtitleVisible,
            originalSubtitleScale,
            originalSubtitlePosition,
            originalSubtitleFontFamily,
            originalSubtitleFontSize,
            originalSubtitleAssOverride,
            true);
        return;
    }
}

void MainWindow::showViewportContextMenu(const QPoint &position)
{
    QMenu menu(this);
    auto *openMenu = menu.addMenu(uiText("Open"));
    openMenu->addAction(openFileAction_);
    openMenu->addAction(openFolderAction_);
    openMenu->addAction(openUrlAction_);

    auto *playbackMenu = menu.addMenu(uiText("Playback"));
    playbackMenu->addAction(playPauseAction_);
    playbackMenu->addAction(stopAction_);
    playbackMenu->addSeparator();
    playbackMenu->addAction(takeScreenshotAction_);
    playbackMenu->addAction(addBookmarkAction_);
    auto *speedMenu = playbackMenu->addMenu(uiText("Speed"));
    speedMenu->addAction(speedDownAction_);
    speedMenu->addAction(speedUpAction_);
    speedMenu->addAction(speedResetAction_);

    auto *navigationMenu = menu.addMenu(uiText("Navigate"));
    navigationMenu->addAction(previousPlaylistAction_);
    navigationMenu->addAction(nextPlaylistAction_);
    navigationMenu->addSeparator();
    navigationMenu->addAction(previousChapterAction_);
    navigationMenu->addAction(nextChapterAction_);

    auto *loopMenu = playbackMenu->addMenu(uiText("A-B Loop"));
    loopMenu->addAction(setLoopStartAction_);
    loopMenu->addAction(setLoopEndAction_);
    loopMenu->addAction(clearLoopAction_);
    auto *repeatMenu = playbackMenu->addMenu(uiText("Repeat"));
    repeatMenu->addAction(repeatOffAction_);
    repeatMenu->addAction(repeatFileAction_);
    repeatMenu->addAction(repeatPlaylistAction_);
    playbackMenu->addSeparator();
    playbackMenu->addAction(frameStepBackwardAction_);
    playbackMenu->addAction(frameStepForwardAction_);

    auto *subtitleMenu = menu.addMenu(uiText("Subtitles"));
    subtitleMenu->addAction(toggleSubtitleVisibilityAction_);
    subtitleMenu->addAction(loadSubtitleAction_);
    subtitleMenu->addSeparator();
    subtitleMenu->addAction(subtitleDelayDownAction_);
    subtitleMenu->addAction(subtitleDelayUpAction_);
    subtitleMenu->addAction(subtitleDelayManualAction_);
    subtitleMenu->addAction(subtitleDelayResetAction_);
    auto *subtitleScaleMenu = subtitleMenu->addMenu(uiText("Scale"));
    subtitleScaleMenu->addAction(subtitleScaleDownAction_);
    subtitleScaleMenu->addAction(subtitleScaleUpAction_);
    subtitleScaleMenu->addAction(subtitleScaleResetAction_);
    auto *subtitlePositionMenu = subtitleMenu->addMenu(uiText("Position"));
    subtitlePositionMenu->addAction(subtitlePositionUpAction_);
    subtitlePositionMenu->addAction(subtitlePositionDownAction_);
    subtitlePositionMenu->addAction(subtitlePositionResetAction_);
    if (subtitleOverrideActionGroup_ != nullptr) {
        auto *subtitleStyleMenu = subtitleMenu->addMenu(uiText("Style Override"));
        for (QAction *action : subtitleOverrideActionGroup_->actions()) {
            subtitleStyleMenu->addAction(action);
        }
        subtitleStyleMenu->addSeparator();
        subtitleStyleMenu->addAction(cycleSubtitleAssOverrideAction_);
    }

    auto *audioMenu = menu.addMenu(uiText("Audio"));
    audioMenu->addAction(toggleMuteAction_);
    audioMenu->addSeparator();
    audioMenu->addAction(audioDelayDownAction_);
    audioMenu->addAction(audioDelayUpAction_);
    audioMenu->addAction(audioDelayManualAction_);
    audioMenu->addAction(audioDelayResetAction_);

    if (profileMenu_ != nullptr) {
        menu.addMenu(profileMenu_);
    }

    if (aspectMenu_ != nullptr || cropMenu_ != nullptr || rotateMenu_ != nullptr || deinterlaceAction_ != nullptr) {
        auto *videoMenu = menu.addMenu(uiText("Video"));
        if (aspectMenu_ != nullptr) {
            videoMenu->addMenu(aspectMenu_);
        }
        if (cropMenu_ != nullptr) {
            videoMenu->addMenu(cropMenu_);
        }
        if (rotateMenu_ != nullptr) {
            videoMenu->addMenu(rotateMenu_);
        }
        if (videoZoomInAction_ != nullptr || videoZoomOutAction_ != nullptr || videoZoomResetAction_ != nullptr) {
            auto *zoomMenu = videoMenu->addMenu(uiText("Zoom"));
            if (videoZoomOutAction_ != nullptr) {
                zoomMenu->addAction(videoZoomOutAction_);
            }
            if (videoZoomInAction_ != nullptr) {
                zoomMenu->addAction(videoZoomInAction_);
            }
            if (videoZoomResetAction_ != nullptr) {
                zoomMenu->addAction(videoZoomResetAction_);
            }
            if (videoPanLeftAction_ != nullptr || videoPanRightAction_ != nullptr || videoPanUpAction_ != nullptr || videoPanDownAction_ != nullptr) {
                zoomMenu->addSeparator();
                if (videoPanLeftAction_ != nullptr) {
                    zoomMenu->addAction(videoPanLeftAction_);
                }
                if (videoPanRightAction_ != nullptr) {
                    zoomMenu->addAction(videoPanRightAction_);
                }
                if (videoPanUpAction_ != nullptr) {
                    zoomMenu->addAction(videoPanUpAction_);
                }
                if (videoPanDownAction_ != nullptr) {
                    zoomMenu->addAction(videoPanDownAction_);
                }
            }
        }
        if (deinterlaceAction_ != nullptr) {
            videoMenu->addSeparator();
            videoMenu->addAction(deinterlaceAction_);
        }
    }

    auto *windowMenu = menu.addMenu(uiText("Window"));
    windowMenu->addAction(togglePlaylistAction_);
    windowMenu->addAction(toggleDetailsAction_);
    windowMenu->addAction(showMediaInformationOverlayAction_);
    windowMenu->addAction(toggleFullscreenAction_);

    auto *toolsMenu = menu.addMenu(uiText("Tools"));
    toolsMenu->addAction(showMediaInfoAction_);
    toolsMenu->addAction(preferencesAction_);
    forceMenuLeftToRight(&menu);
    menu.exec(videoViewport_->mapToGlobal(position));
}

void MainWindow::showPlaylistContextMenu(const QPoint &position)
{
    if (playlistView_ == nullptr) {
        return;
    }

    const QModelIndex index = playlistView_->indexAt(position);
    const QVector<int> selectedIndices = selectedPlaylistIndices();
    QMenu menu(this);
    QAction *playSelectedAction = nullptr;
    QAction *copyPathAction = nullptr;
    QAction *editDetailsAction = nullptr;
    QAction *resetProgressAction = nullptr;
    QAction *markCompletedAction = nullptr;
    QAction *addToFavoritesAction = nullptr;
    QAction *removeSelectedAction = nullptr;
    QAction *keepOnlySelectedAction = nullptr;
    QString contextFavoriteSource;
    QString contextFavoriteTitle;
    if (index.isValid()) {
        playSelectedAction = menu.addAction(uiText("Play Selected"));
        copyPathAction = menu.addAction(uiText("Copy Source Path"));
        editDetailsAction = menu.addAction(uiText("Edit Item Details"));
        contextFavoriteSource = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString();
        contextFavoriteTitle = index.data(revaplayer::application::PlaylistRoles::TitleRole).toString().trimmed();
        if (storedMediaSourceIsUsable(contextFavoriteSource)) {
            resetProgressAction = menu.addAction(uiText("Reset Progress"));
            markCompletedAction = menu.addAction(uiText("Mark Watched Complete"));
            const bool alreadyFavorite = settingsController_ != nullptr
                && !settingsController_->customValue(favoriteStorageKey(contextFavoriteSource)).trimmed().isEmpty();
            addToFavoritesAction = menu.addAction(alreadyFavorite
                ? uiText("Remove from Favorites")
                : uiText("Add to Favorites"));
            addToFavoritesAction->setData(alreadyFavorite);
        }
        menu.addSeparator();
    }
    if (!selectedIndices.isEmpty()) {
        removeSelectedAction = menu.addAction(uiText("Remove from List"));
        keepOnlySelectedAction = menu.addAction(uiText("Keep Only Selected"));
    }

    QMenu *loadSnapshotMenu = nullptr;
    QMenu *deleteSnapshotMenu = nullptr;
    const QStringList snapshotKeys = pinnedCourseBrowserActive_ ? QStringList {} : playlistSnapshotKeys();
    if (!snapshotKeys.isEmpty()) {
        loadSnapshotMenu = menu.addMenu(uiText("Load Snapshot"));
        deleteSnapshotMenu = menu.addMenu(uiText("Delete Snapshot"));
        for (const QString &snapshotKey : snapshotKeys) {
            QAction *loadAction = loadSnapshotMenu->addAction(playlistSnapshotLabel(snapshotKey));
            loadAction->setData(snapshotKey);
            QAction *deleteAction = deleteSnapshotMenu->addAction(playlistSnapshotLabel(snapshotKey));
            deleteAction->setData(snapshotKey);
        }
    }

    QMenu *smartPlaylistMenu = pinnedCourseBrowserActive_ ? nullptr : menu.addMenu(uiText("Smart Playlist"));
    QAction *sameFolderAction = smartPlaylistMenu != nullptr ? smartPlaylistMenu->addAction(uiText("Current Folder")) : nullptr;
    QAction *recentHistoryAction = smartPlaylistMenu != nullptr ? smartPlaylistMenu->addAction(uiText("Recent History")) : nullptr;
    QAction *favoritesAction = smartPlaylistMenu != nullptr ? smartPlaylistMenu->addAction(uiText("Favorites")) : nullptr;
    QMenu *savedRulesMenu = nullptr;
    QMenu *deleteRulesMenu = nullptr;
    if (smartPlaylistMenu != nullptr && settingsController_ != nullptr) {
        const QStringList ruleKeys = settingsController_->customKeys(QString::fromLatin1(kSmartPlaylistRulePrefix));
        if (!ruleKeys.isEmpty()) {
            savedRulesMenu = smartPlaylistMenu->addMenu(uiText("Saved Rules"));
            deleteRulesMenu = smartPlaylistMenu->addMenu(uiText("Delete Saved Rule"));
            for (const QString &ruleKey : ruleKeys) {
                const QJsonObject ruleObject = QJsonDocument::fromJson(settingsController_->customValue(ruleKey).toUtf8()).object();
                const QString label = ruleObject.value(QStringLiteral("name")).toString().trimmed().isEmpty()
                    ? decodeSettingKeySegment(ruleKey.mid(QString::fromLatin1(kSmartPlaylistRulePrefix).size()))
                    : ruleObject.value(QStringLiteral("name")).toString().trimmed();
                QAction *loadAction = savedRulesMenu->addAction(label);
                loadAction->setData(ruleKey);
                QAction *deleteAction = deleteRulesMenu->addAction(label);
                deleteAction->setData(ruleKey);
            }
        }
    }
    menu.addSeparator();

    QAction *clearFilterAction = nullptr;
    if (playlistSearchEdit_ != nullptr && !playlistSearchEdit_->text().trimmed().isEmpty()) {
        clearFilterAction = menu.addAction(uiText("Clear Playlist Filter"));
    }

    forceMenuLeftToRight(&menu);
    QAction *chosenAction = menu.exec(playlistView_->viewport()->mapToGlobal(position));
    if (chosenAction == nullptr) {
        return;
    }

    if (chosenAction == playSelectedAction) {
        onPlaylistActivated(index);
        return;
    }

    if (chosenAction == copyPathAction) {
        QGuiApplication::clipboard()->setText(index.data(Qt::UserRole + 1).toString());
        statusBar()->showMessage(uiText("Playlist source copied"), 2500);
        return;
    }

    if (chosenAction == editDetailsAction) {
        if (playlistView_ != nullptr) {
            playlistView_->setCurrentIndex(index);
        }
        editSelectedPlaylistItemMetadata();
        return;
    }

    if (chosenAction == resetProgressAction) {
        resetSelectedPlaylistItemProgress(index);
        return;
    }

    if (chosenAction == markCompletedAction) {
        markSelectedPlaylistItemCompleted(index);
        return;
    }

    if (chosenAction == addToFavoritesAction) {
        if (chosenAction->data().toBool()) {
            settingsController_->removeCustomValue(favoriteStorageKey(contextFavoriteSource));
            if (sourcesReferToSameMedia(contextFavoriteSource, currentMediaSource_)) {
                favoriteCurrentMedia_ = false;
            }
            reloadFavoritesPanel();
            refreshPlaylistPresentationData();
            reloadHomeDashboard();
            updateActionStates();
            statusBar()->showMessage(uiText("Selected item removed from favorites"), 2500);
        } else {
            addMediaSourceToFavorites(contextFavoriteSource,
                                      contextFavoriteTitle,
                                      uiText("Selected item added to favorites"),
                                      uiText("Selected item is already in favorites"));
        }
        return;
    }

    if (chosenAction == removeSelectedAction) {
        removeSelectedPlaylistItems();
        return;
    }

    if (chosenAction == keepOnlySelectedAction) {
        keepOnlySelectedPlaylistItems();
        return;
    }

    if (loadSnapshotMenu != nullptr && chosenAction->parent() == loadSnapshotMenu) {
        loadPlaylistSnapshot(chosenAction->data().toString());
        return;
    }

    if (deleteSnapshotMenu != nullptr && chosenAction->parent() == deleteSnapshotMenu) {
        deletePlaylistSnapshot(chosenAction->data().toString());
        return;
    }

    if (chosenAction == sameFolderAction) {
        buildSmartPlaylistFromSameFolder();
        return;
    }

    if (chosenAction == recentHistoryAction) {
        buildSmartPlaylistFromRecentHistory();
        return;
    }

    if (chosenAction == favoritesAction) {
        buildSmartPlaylistFromFavorites();
        return;
    }

    if (savedRulesMenu != nullptr && chosenAction->parent() == savedRulesMenu) {
        loadSmartPlaylistRule(chosenAction->data().toString());
        return;
    }

    if (deleteRulesMenu != nullptr && chosenAction->parent() == deleteRulesMenu) {
        deleteSmartPlaylistRule(chosenAction->data().toString());
        return;
    }

    if (chosenAction == clearFilterAction && playlistSearchEdit_ != nullptr) {
        playlistSearchEdit_->clear();
    }
}

void MainWindow::onLoadStarted(const QString &displayTarget)
{
    if (homeDashboard_ != nullptr) {
        homeDashboard_->hide();
    }
    if (videoViewport_ != nullptr) {
        videoViewport_->setRenderHostVisible(true);
    }
    enforceHiddenSidePanelsAfterMediaOpen(kPointerPanelSuppressionDelayMs * 2, true);
    beginLoadFeedback(displayTarget);
}

void MainWindow::clearPendingPlaylistSelection()
{
    pendingPlaylistSelectionIndex_ = -1;
    pendingPlaylistSelectionExpectedCount_ = 0;
    pendingPlaylistSelectionRetryCount_ = 0;
    pendingPlaylistSelectionSource_.clear();
    if (pendingPlaylistSelectionTimer_ != nullptr) {
        pendingPlaylistSelectionTimer_->stop();
    }
}

QStringList MainWindow::selectedPlaylistSources() const
{
    QStringList sources;
    if (playlistView_ == nullptr
        || playlistView_->selectionModel() == nullptr
        || playlistView_->model() == nullptr) {
        return sources;
    }

    const QModelIndexList selectedRows = playlistView_->selectionModel()->selectedRows();
    sources.reserve(selectedRows.size());
    for (const QModelIndex &index : selectedRows) {
        const QString source = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        if (!source.isEmpty() && !sources.contains(source)) {
            sources.push_back(source);
        }
    }
    return sources;
}

bool MainWindow::clearStoredProgressForSource(const QString &source)
{
    const QString normalizedSource = source.trimmed();
    if (normalizedSource.isEmpty() || historyController_ == nullptr || !historyController_->isReady()) {
        return false;
    }

    if (!historyController_->removeHistoryEntry(normalizedSource)) {
        return false;
    }

    historyEntries_.erase(
        std::remove_if(historyEntries_.begin(), historyEntries_.end(), [&normalizedSource](const auto &entry) {
            return entry.source == normalizedSource || sourcesReferToSameMedia(entry.source, normalizedSource);
        }),
        historyEntries_.end());

    QStringList historyCacheKeys;
    for (auto it = historyEntriesBySource_.cbegin(); it != historyEntriesBySource_.cend(); ++it) {
        if (it.key() == normalizedSource || sourcesReferToSameMedia(it.key(), normalizedSource)) {
            historyCacheKeys.push_back(it.key());
        }
    }
    for (const QString &key : historyCacheKeys) {
        historyEntriesBySource_.remove(key);
    }

    QStringList resumeCacheKeys;
    for (auto it = resumeStateCache_.cbegin(); it != resumeStateCache_.cend(); ++it) {
        if (it.key() == normalizedSource || sourcesReferToSameMedia(it.key(), normalizedSource)) {
            resumeCacheKeys.push_back(it.key());
        }
    }
    for (const QString &key : resumeCacheKeys) {
        resumeStateCache_.remove(key);
    }

    QStringList lookupKeys;
    for (const QString &key : std::as_const(resumeStateLookupCompleted_)) {
        if (key == normalizedSource || sourcesReferToSameMedia(key, normalizedSource)) {
            lookupKeys.push_back(key);
        }
    }
    for (const QString &key : lookupKeys) {
        resumeStateLookupCompleted_.remove(key);
    }
    resumeStateLookupCompleted_.insert(normalizedSource);

    const QString currentSource = currentMediaSource_.trimmed();
    if (!currentSource.isEmpty() && sourcesReferToSameMedia(normalizedSource, currentSource)) {
        lastPersistedPositionSeconds_ = 0.0;
        progressResetSuppressedSources_.insert(currentSource);
    }

    return true;
}

int MainWindow::clearStoredProgressForSources(const QStringList &sources)
{
    QStringList uniqueSources;
    uniqueSources.reserve(sources.size());
    for (const QString &source : sources) {
        const QString normalizedSource = source.trimmed();
        if (normalizedSource.isEmpty() || !storedMediaSourceIsUsable(normalizedSource)) {
            continue;
        }

        const bool duplicate = std::any_of(uniqueSources.cbegin(), uniqueSources.cend(), [&normalizedSource](const QString &existing) {
            return existing == normalizedSource || sourcesReferToSameMedia(existing, normalizedSource);
        });
        if (!duplicate) {
            uniqueSources.push_back(normalizedSource);
        }
    }

    int resetCount = 0;
    for (const QString &source : std::as_const(uniqueSources)) {
        if (clearStoredProgressForSource(source)) {
            ++resetCount;
        }
    }
    return resetCount;
}

QStringList MainWindow::currentPlaylistProgressSources() const
{
    QStringList sources;

    if (playlistController_ != nullptr) {
        const auto entries = playlistController_->entries();
        sources.reserve(entries.size());
        for (const auto &entry : entries) {
            const QString source = entry.source.trimmed();
            if (storedMediaSourceIsUsable(source)) {
                sources.push_back(source);
            }
        }
    }

    if (sources.isEmpty() && pinnedCourseBrowserActive_) {
        const QString folderPath = pinnedCourseBrowserRootPath_.trimmed().isEmpty()
            ? pinnedCourseBrowserFolderPath_
            : pinnedCourseBrowserRootPath_;
        if (!folderPath.trimmed().isEmpty()) {
            sources = playlistSourcesForPinnedCourse(
                settingsController_,
                QFileInfo(folderPath).absoluteFilePath(),
                naturalSortFolderPlaylistEnabled());
        }
    }

    QStringList uniqueSources;
    uniqueSources.reserve(sources.size());
    for (const QString &source : std::as_const(sources)) {
        const QString normalizedSource = source.trimmed();
        if (normalizedSource.isEmpty() || !storedMediaSourceIsUsable(normalizedSource)) {
            continue;
        }
        const bool duplicate = std::any_of(uniqueSources.cbegin(), uniqueSources.cend(), [&normalizedSource](const QString &existing) {
            return existing == normalizedSource || sourcesReferToSameMedia(existing, normalizedSource);
        });
        if (!duplicate) {
            uniqueSources.push_back(normalizedSource);
        }
    }
    return uniqueSources;
}

void MainWindow::refreshProgressDisplaysAfterReset()
{
    reloadHistoryPanel();
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    if (playlistView_ != nullptr && playlistView_->viewport() != nullptr) {
        playlistView_->viewport()->update();
    }
}

void MainWindow::resetSelectedPlaylistItemProgress(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    const QModelIndexList actionIndexes = playlistContextActionIndexes(playlistView_, index);
    QStringList sources;
    sources.reserve(actionIndexes.size());
    for (const QModelIndex &actionIndex : actionIndexes) {
        const QString source = actionIndex.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        if (!source.isEmpty() && storedMediaSourceIsUsable(source)) {
            sources.push_back(source);
        }
    }

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("No playlist progress to reset"), 2500);
        return;
    }

    const int resetCount = clearStoredProgressForSources(sources);
    if (resetCount <= 0) {
        statusBar()->showMessage(uiText("Could not reset progress"), 3500);
        return;
    }

    refreshProgressDisplaysAfterReset();
    statusBar()->showMessage(
        resetCount == 1 ? uiText("Progress reset for selected item") : uiText("Progress reset for selected items"),
        2500);
}

void MainWindow::markSelectedPlaylistItemCompleted(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    const QModelIndexList actionIndexes = playlistContextActionIndexes(playlistView_, index);
    if (actionIndexes.isEmpty()) {
        statusBar()->showMessage(uiText("No playlist item to mark complete"), 2500);
        return;
    }

    if (historyController_ == nullptr || !historyController_->isReady()) {
        statusBar()->showMessage(uiText("History storage is unavailable"), 3000);
        return;
    }

    if (!historyEnabled()) {
        statusBar()->showMessage(uiText("History is disabled in Preferences"), 3000);
        return;
    }

    int completedCount = 0;
    QStringList completedSources;
    completedSources.reserve(actionIndexes.size());
    const QString nowIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    for (const QModelIndex &actionIndex : actionIndexes) {
        const QString source = actionIndex.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        if (source.isEmpty() || !storedMediaSourceIsUsable(source)) {
            continue;
        }

        const bool duplicate = std::any_of(completedSources.cbegin(), completedSources.cend(), [&source](const QString &existing) {
            return existing == source || sourcesReferToSameMedia(existing, source);
        });
        if (duplicate) {
            continue;
        }
        completedSources.push_back(source);

        double durationSeconds = std::max(
            0.0,
            actionIndex.data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble());
        if (sourcesReferToSameMedia(source, currentMediaSource_) && currentDurationSeconds_ > durationSeconds) {
            durationSeconds = currentDurationSeconds_;
        }

        const QString scanKey = mediaScanSourceKey(source);
        if (durationSeconds <= 0.0 && !scanKey.isEmpty()) {
            const auto scanIt = mediaScanCache_.constFind(scanKey);
            if (scanIt != mediaScanCache_.constEnd() && scanIt->durationSeconds > 0.0) {
                durationSeconds = scanIt->durationSeconds;
            }
        }

        const QString title = displayTitleForHistory(
            source,
            actionIndex.data(revaplayer::application::PlaylistRoles::TitleRole).toString());
        historyController_->markPlaybackCompleted(source, title, durationSeconds, true, resumeEnabled());

        QStringList historyCacheKeys;
        for (auto it = historyEntriesBySource_.cbegin(); it != historyEntriesBySource_.cend(); ++it) {
            if (it.key() == source || sourcesReferToSameMedia(it.key(), source)) {
                historyCacheKeys.push_back(it.key());
            }
        }
        for (const QString &key : std::as_const(historyCacheKeys)) {
            historyEntriesBySource_.remove(key);
        }
        historyEntriesBySource_.insert(
            source,
            revaplayer::infrastructure::storage::PlaybackHistoryRecord {
                source,
                title,
                durationSeconds,
                durationSeconds,
                true,
                nowIso,
                nowIso,
            });

        QStringList resumeCacheKeys;
        for (auto it = resumeStateCache_.cbegin(); it != resumeStateCache_.cend(); ++it) {
            if (it.key() == source || sourcesReferToSameMedia(it.key(), source)) {
                resumeCacheKeys.push_back(it.key());
            }
        }
        for (const QString &key : std::as_const(resumeCacheKeys)) {
            resumeStateCache_.remove(key);
        }
        resumeStateLookupCompleted_.insert(source);
        progressResetSuppressedSources_.remove(source);
        if (sourcesReferToSameMedia(source, currentMediaSource_)) {
            progressResetSuppressedSources_.remove(currentMediaSource_);
            lastPersistedPositionSeconds_ = durationSeconds;
        }

        if (durationSeconds <= 0.0
            && metadataScanService_ != nullptr
            && !scanKey.isEmpty()
            && !localMediaPathForSource(source).isEmpty()
            && !pendingMediaScanSources_.contains(scanKey)
            && !failedMediaScanSources_.contains(scanKey)) {
            pendingMediaScanSources_.insert(scanKey);
            metadataScanService_->enqueueSource(scanKey);
        }

        ++completedCount;
    }

    if (completedCount <= 0) {
        statusBar()->showMessage(uiText("No playlist item to mark complete"), 2500);
        return;
    }

    updatePlaylistMetadataScanButtonState();
    trimHistoryToLimit();
    refreshProgressDisplaysAfterReset();
    statusBar()->showMessage(
        completedCount == 1
            ? uiText("Selected item marked as watched complete")
            : uiText("Selected items marked as watched complete"),
        2500);
}

void MainWindow::resetCurrentPlaylistProgress()
{
    const QStringList sources = currentPlaylistProgressSources();
    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("No playlist progress to reset"), 2500);
        return;
    }

    if (clearStoredProgressForSources(sources) <= 0) {
        statusBar()->showMessage(uiText("Could not reset progress"), 3500);
        return;
    }

    refreshProgressDisplaysAfterReset();
    statusBar()->showMessage(uiText("Progress reset for this list"), 3000);
}

void MainWindow::restorePlaylistSelectionFromSources(const QStringList &sources, const QString &currentSource)
{
    if (sources.isEmpty()
        || playlistView_ == nullptr
        || playlistView_->selectionModel() == nullptr
        || playlistView_->model() == nullptr) {
        return;
    }

    QItemSelectionModel *selectionModel = playlistView_->selectionModel();
    selectionModel->clearSelection();

    QModelIndex currentIndexToRestore;
    for (int row = 0; row < playlistView_->model()->rowCount(); ++row) {
        const QModelIndex index = playlistView_->model()->index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        const QString source = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        if (source.isEmpty()) {
            continue;
        }

        if (sources.contains(source)) {
            selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        if (!currentIndexToRestore.isValid()
            && !currentSource.isEmpty()
            && sourcesReferToSameMedia(source, currentSource)) {
            currentIndexToRestore = index;
        }
    }

    if (currentIndexToRestore.isValid()) {
        playlistView_->setCurrentIndex(currentIndexToRestore);
        if (playlistAutoFollowEnabled() && !preservePlaylistViewportAfterReorder_) {
            playlistView_->scrollTo(currentIndexToRestore, QAbstractItemView::EnsureVisible);
        }
    }
}

void MainWindow::applyRequestedPlaylistOrder(const QVector<int> &orderedIndices)
{
    const auto makeManualOrderVisible = [this]() {
        if (playlistSortModeId() == QStringLiteral("natural")) {
            return;
        }
        if (settingsController_ != nullptr) {
            settingsController_->setCustomValue(QString::fromLatin1(kPlaylistSortModeSetting), QStringLiteral("natural"));
        }
        if (playlistController_ != nullptr) {
            playlistController_->setSortMode(QStringLiteral("natural"));
        }
    };

    if (pinnedCourseBrowserActive_) {
        if (playlistController_ == nullptr || orderedIndices.size() <= 1) {
            return;
        }
        makeManualOrderVisible();

        const QVector<revaplayer::domain::PlaylistEntry> currentEntries = playlistController_->entries();
        if (orderedIndices.size() != currentEntries.size()) {
            return;
        }

        QHash<int, revaplayer::domain::PlaylistEntry> entriesByIndex;
        entriesByIndex.reserve(currentEntries.size());
        for (const auto &entry : currentEntries) {
            entriesByIndex.insert(entry.index, entry);
        }

        QVector<revaplayer::domain::PlaylistEntry> reorderedEntries;
        reorderedEntries.reserve(orderedIndices.size());
        for (const int playlistIndex : orderedIndices) {
            const auto it = entriesByIndex.constFind(playlistIndex);
            if (it == entriesByIndex.constEnd()) {
                return;
            }
            reorderedEntries.push_back(it.value());
        }

        bool changed = false;
        for (int index = 0; index < orderedIndices.size(); ++index) {
            if (orderedIndices.at(index) != index) {
                changed = true;
                break;
            }
        }
        if (!changed) {
            return;
        }

        const QStringList selectedSourcesBeforeReorder = selectedPlaylistSources();
        const QString currentSourceBeforeReorder = playlistView_ != nullptr
            ? playlistView_->currentIndex().data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed()
            : QString {};
        if (playlistView_ != nullptr) {
            preservePlaylistViewportAfterReorder_ = true;
            if (QScrollBar *verticalBar = playlistView_->verticalScrollBar(); verticalBar != nullptr) {
                pendingPlaylistViewportScrollValue_ = verticalBar->value();
            }
            if (QScrollBar *horizontalBar = playlistView_->horizontalScrollBar(); horizontalBar != nullptr) {
                pendingPlaylistViewportHorizontalScrollValue_ = horizontalBar->value();
            }
        }

        int browserCurrentIndex = -1;
        const QString browserFolderPath = QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath();
        QVector<revaplayer::domain::PlaylistEntry> browserNavigationEntries;
        QVector<revaplayer::domain::PlaylistEntry> reorderedMediaEntries;
        browserNavigationEntries.reserve(currentEntries.size());
        reorderedMediaEntries.reserve(reorderedEntries.size());
        for (const auto &entry : std::as_const(currentEntries)) {
            const QString source = entry.source.trimmed();
            if (isFolderBrowserSource(source) || isFolderBrowserBackSource(source)) {
                browserNavigationEntries.push_back(entry);
            }
        }
        for (const auto &entry : std::as_const(reorderedEntries)) {
            const QString sourcePath = localMediaPathForSource(entry.source);
            if (sourcePath.isEmpty()) {
                continue;
            }

            const QString localPath = QFileInfo(sourcePath).absoluteFilePath();
            if (!localPath.isEmpty() && QFileInfo(localPath).dir().absolutePath() == browserFolderPath) {
                reorderedMediaEntries.push_back(entry);
            }
        }

        QVector<revaplayer::domain::PlaylistEntry> browserEntries;
        browserEntries.reserve(browserNavigationEntries.size() + reorderedMediaEntries.size());
        browserEntries += browserNavigationEntries;
        browserEntries += reorderedMediaEntries;

        for (int index = 0; index < browserEntries.size(); ++index) {
            browserEntries[index].index = index;
            browserEntries[index].isCurrent = sourcesReferToSameMedia(browserEntries.at(index).source, currentMediaSource_);
            if (browserEntries.at(index).isCurrent) {
                browserCurrentIndex = index;
            }
        }

        if (folderHasSavedManualOrderScope(settingsController_, browserFolderPath)) {
            persistPinnedCourseMediaOrder(
                settingsController_,
                browserFolderPath,
                orderedLocalMediaSourcesForFolderBrowserEntries(browserEntries, browserFolderPath));
        }

        playlistController_->setEntries(browserEntries, browserCurrentIndex);
        restorePlaylistSelectionFromSources(selectedSourcesBeforeReorder, currentSourceBeforeReorder);
        restorePendingPlaylistViewport();
        refreshPlaylistSummary();
        refreshPlaylistInspector();
        rebuildPinnedCourseTabs();
        statusBar()->showMessage(uiText("Saved folder order updated"), 2500);
        return;
    }

    if (orderedIndices.size() != playlistCount_ || playlistCount_ <= 1 || playbackController_ == nullptr) {
        return;
    }

    makeManualOrderVisible();

    // Manual reordering must win over any deferred "restore natural order"
    // flow that was armed while opening a selected file into a larger playlist.
    pendingPlaylistNaturalOrderSources_.clear();

    const QStringList selectedSourcesBeforeReorder = selectedPlaylistSources();
    const QString currentSourceBeforeReorder = playlistView_ != nullptr
        ? playlistView_->currentIndex().data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed()
        : QString {};
    if (playlistView_ != nullptr) {
        preservePlaylistViewportAfterReorder_ = true;
        if (QScrollBar *verticalBar = playlistView_->verticalScrollBar(); verticalBar != nullptr) {
            pendingPlaylistViewportScrollValue_ = verticalBar->value();
        }
        if (QScrollBar *horizontalBar = playlistView_->horizontalScrollBar(); horizontalBar != nullptr) {
            pendingPlaylistViewportHorizontalScrollValue_ = horizontalBar->value();
        }
    } else {
        clearPendingPlaylistViewportRestore();
    }

    QHash<int, revaplayer::domain::PlaylistEntry> entriesByPlaylistIndex;
    entriesByPlaylistIndex.reserve(playbackPlaylistEntriesCache_.size());
    for (const auto &entry : std::as_const(playbackPlaylistEntriesCache_)) {
        entriesByPlaylistIndex.insert(entry.index, entry);
    }

    QVector<revaplayer::domain::PlaylistEntry> reorderedEntries;
    reorderedEntries.reserve(orderedIndices.size());
    for (const int playlistIndex : orderedIndices) {
        const auto it = entriesByPlaylistIndex.constFind(playlistIndex);
        if (it == entriesByPlaylistIndex.constEnd()) {
            return;
        }
        reorderedEntries.push_back(it.value());
    }

    QVector<int> currentOrder;
    currentOrder.reserve(orderedIndices.size());
    for (int index = 0; index < orderedIndices.size(); ++index) {
        currentOrder.push_back(index);
    }

    bool changed = false;
    for (int targetIndex = 0; targetIndex < orderedIndices.size(); ++targetIndex) {
        const int sourcePlaylistIndex = orderedIndices.at(targetIndex);
        const int currentPosition = currentOrder.indexOf(sourcePlaylistIndex);
        if (currentPosition < 0 || currentPosition == targetIndex) {
            continue;
        }

        playbackController_->executeMpvCommand({
            QStringLiteral("playlist-move"),
            QString::number(currentPosition),
            QString::number(targetIndex),
        });
        currentOrder.move(currentPosition, targetIndex);
        changed = true;
    }

    if (!changed) {
        return;
    }

    int reorderedCurrentIndex = -1;
    for (int index = 0; index < reorderedEntries.size(); ++index) {
        reorderedEntries[index].index = index;
        reorderedEntries[index].isCurrent = sourcesReferToSameMedia(reorderedEntries.at(index).source, currentMediaSource_);
        if (reorderedEntries.at(index).isCurrent) {
            reorderedCurrentIndex = index;
        }
    }

    pendingPlaylistRequestedOrderSources_.clear();
    pendingPlaylistRequestedOrderSources_.reserve(reorderedEntries.size());
    for (const auto &entry : std::as_const(reorderedEntries)) {
        pendingPlaylistRequestedOrderSources_.push_back(entry.source.trimmed());
    }

    playbackPlaylistEntriesCache_ = reorderedEntries;
    if (reorderedCurrentIndex >= 0) {
        playbackPlaylistCurrentIndexCache_ = reorderedCurrentIndex;
        currentPlaylistIndex_ = reorderedCurrentIndex;
    }
    const QString reorderedFolderPath = playlistFolderPathForEntries(reorderedEntries);
    if (!reorderedFolderPath.isEmpty()
        && folderHasSavedManualOrderScope(settingsController_, reorderedFolderPath)) {
        persistPinnedCourseMediaOrder(
            settingsController_,
            reorderedFolderPath,
            orderedLocalSourcesForEntries(reorderedEntries, reorderedFolderPath));
    }
    if (!pinnedCourseBrowserActive_ && playlistController_ != nullptr) {
        playlistController_->setEntries(playbackPlaylistEntriesCache_, playbackPlaylistCurrentIndexCache_);
    }
    restorePlaylistSelectionFromSources(selectedSourcesBeforeReorder, currentSourceBeforeReorder);
    restorePendingPlaylistViewport(false);

    pendingPlaylistReorderSelectedSources_ = selectedSourcesBeforeReorder;
    pendingPlaylistReorderCurrentSource_ = currentSourceBeforeReorder;
    statusBar()->showMessage(uiText("Playlist order updated"), 2500);
}

void MainWindow::clearPendingCurrentMediaRestore()
{
    pendingCurrentMediaRestoreSource_.clear();
    pendingCurrentMediaRestorePositionSeconds_ = -1.0;
}

void MainWindow::armPendingCurrentMediaRestore(const QString &source, const double positionSeconds)
{
    if (source.isEmpty() || positionSeconds < 0.0) {
        clearPendingCurrentMediaRestore();
        return;
    }

    pendingCurrentMediaRestoreSource_ = source;
    pendingCurrentMediaRestorePositionSeconds_ = positionSeconds;
}

bool MainWindow::applyPendingCurrentMediaRestore()
{
    if (playbackController_ == nullptr
        || !mediaLoaded_
        || loadingMedia_
        || pendingCurrentMediaRestoreSource_.isEmpty()
        || pendingCurrentMediaRestorePositionSeconds_ < 0.0
        || !sourcesReferToSameMedia(currentMediaSource_, pendingCurrentMediaRestoreSource_)) {
        return false;
    }

    const double targetPosition = std::max(0.0, pendingCurrentMediaRestorePositionSeconds_);
    playbackController_->seekToSeconds(targetPosition);
    currentPositionSeconds_ = targetPosition;
    lastPersistedPositionSeconds_ = targetPosition;
    resumeAttemptedForCurrentMedia_ = true;
    clearPendingCurrentMediaRestore();
    return true;
}

QString MainWindow::playlistSourceForIndex(const int playlistIndex) const
{
    if (playlistIndex < 0) {
        return {};
    }

    const auto sourceFromEntries = [playlistIndex](const QVector<revaplayer::domain::PlaylistEntry> &entries) {
        for (const auto &entry : entries) {
            if (entry.index == playlistIndex) {
                return entry.source.trimmed();
            }
        }

        if (playlistIndex < entries.size()) {
            return entries.at(playlistIndex).source.trimmed();
        }

        return QString {};
    };

    QString source = sourceFromEntries(playbackPlaylistEntriesCache_);
    if (!source.isEmpty() || playlistController_ == nullptr) {
        return source;
    }

    return sourceFromEntries(playlistController_->entries());
}

bool MainWindow::preparePlaylistNavigationToIndex(const int playlistIndex)
{
    const QString targetSource = playlistSourceForIndex(playlistIndex);
    if (targetSource.isEmpty()
        || sourcesReferToSameMedia(targetSource, currentMediaSource_)) {
        return false;
    }

    if (!pendingCurrentMediaRestoreSource_.isEmpty()
        && !sourcesReferToSameMedia(pendingCurrentMediaRestoreSource_, targetSource)) {
        clearPendingCurrentMediaRestore();
    }

    loadingMediaSource_ = targetSource;
    beginLoadFeedback(displayTitleForHistory(targetSource, QString {}));
    return true;
}

void MainWindow::selectPlaylistIndexWithResumeGuard(const int playlistIndex)
{
    if (playbackController_ == nullptr || playlistIndex < 0) {
        return;
    }

    if (playlistIndex != currentPlaylistIndex_
        && !preparePlaylistNavigationToIndex(playlistIndex)) {
        persistPlaybackProgress(false, true);
    }

    playbackController_->selectPlaylistIndex(playlistIndex);
}

void MainWindow::navigatePlaylistWithResumeGuard(const bool forward)
{
    if (playbackController_ == nullptr) {
        return;
    }

    const int targetIndex = currentPlaylistIndex_ + (forward ? 1 : -1);
    const bool targetInRange = targetIndex >= 0 && targetIndex < playlistCount_;
    if (!targetInRange || !preparePlaylistNavigationToIndex(targetIndex)) {
        persistPlaybackProgress(false, true);
    }

    if (forward) {
        playbackController_->nextPlaylistItem();
    } else {
        playbackController_->previousPlaylistItem();
    }
}

void MainWindow::capturePanelStateForPlaylistActivation()
{
    playlistActivationPanelRestorePending_ = true;
    playlistActivationPlaylistVisible_ = isSidePanelVisible(SidePanel::Playlist);
    playlistActivationDetailsVisible_ = isSidePanelVisible(SidePanel::Details);
    playlistActivationDetailsTabIndex_ = detailsTabs_ != nullptr ? detailsTabs_->currentIndex() : 0;
    playlistActivationSidePanel_ = activeSidePanel_;
}

void MainWindow::restorePanelStateAfterPlaylistActivation(const bool clearPending)
{
    if (!playlistActivationPanelRestorePending_) {
        return;
    }

    const bool playlistVisible = playlistActivationPlaylistVisible_;
    const bool detailsVisible = playlistActivationDetailsVisible_;
    const bool raisePlaylist = playlistVisible && (!detailsVisible || playlistActivationSidePanel_ == SidePanel::Playlist);
    const bool raiseDetails = detailsVisible && (!playlistVisible || playlistActivationSidePanel_ == SidePanel::Details);

    suppressPanelPreferencePersistence_ = true;
    if (panelOverlayModeActive_) {
        if (playlistVisible && !detailsVisible) {
            setSidePanelVisible(SidePanel::Details, false, false);
            setSidePanelVisible(SidePanel::Playlist, true, true);
        } else if (detailsVisible && !playlistVisible) {
            setSidePanelVisible(SidePanel::Playlist, false, false);
            setSidePanelVisible(SidePanel::Details, true, true);
        } else if (!playlistVisible && !detailsVisible) {
            setSidePanelVisible(SidePanel::Playlist, false, false);
            setSidePanelVisible(SidePanel::Details, false, false);
        } else if (playlistActivationSidePanel_ == SidePanel::Details) {
            setSidePanelVisible(SidePanel::Playlist, false, false);
            setSidePanelVisible(SidePanel::Details, true, true);
        } else {
            setSidePanelVisible(SidePanel::Details, false, false);
            setSidePanelVisible(SidePanel::Playlist, true, true);
        }
    } else {
        setSidePanelVisible(SidePanel::Playlist, playlistVisible, raisePlaylist);
        setSidePanelVisible(SidePanel::Details, detailsVisible, raiseDetails);
    }
    suppressPanelPreferencePersistence_ = false;

    if (detailsTabs_ != nullptr && detailsTabs_->count() > 0) {
        const int targetTabIndex = std::clamp(playlistActivationDetailsTabIndex_, 0, detailsTabs_->count() - 1);
        detailsTabs_->setCurrentIndex(targetTabIndex);
    }
    activeSidePanel_ = playlistActivationSidePanel_;
    syncPanelToggleActions();

    if (clearPending) {
        playlistActivationPanelRestorePending_ = false;
    }
}

void MainWindow::hidePlaylistPanelImmediately()
{
    if (playlistDockAnimation_ != nullptr && playlistDockAnimation_->state() != QAbstractAnimation::Stopped) {
        playlistDockAnimation_->stop();
    }
    if (detailsDockAnimation_ != nullptr && detailsDockAnimation_->state() != QAbstractAnimation::Stopped) {
        detailsDockAnimation_->stop();
    }
    if (playlistDock_ != nullptr) {
        playlistDock_->setProperty("overlayHidePending", false);
        playlistDock_->hide();
    }
    if (detailsDock_ != nullptr) {
        detailsDock_->setProperty("overlayHidePending", false);
        detailsDock_->hide();
    }
    if (playlistResizeHandle_ != nullptr) {
        playlistResizeHandle_->hide();
    }
    if (detailsResizeHandle_ != nullptr) {
        detailsResizeHandle_->hide();
    }

    sidePanelEdgeRevealActive_ = false;
    syncPanelToggleActions();
    updateVideoOverlayGeometry();
}

void MainWindow::enforceHiddenSidePanelsAfterMediaOpen(const int suppressionMs, const bool scheduleFollowUp)
{
    const int effectiveSuppressionMs = std::clamp(suppressionMs, 240, 2400);
    pointerPanelSuppressionElapsed_.restart();
    pointerPanelSuppressionDurationMs_ = std::max(pointerPanelSuppressionDurationMs_, effectiveSuppressionMs);
    activeMouseZoneId_.clear();
    pointerNearRightEdge_ = false;
    playlistActivationPanelRestorePending_ = false;
    playlistActivationPlaylistVisible_ = false;
    playlistActivationDetailsVisible_ = false;
    temporaryPlaylistHidePending_ = false;

    const auto hidePanelsNow = [this]() {
        const bool previousSuppressState = suppressPanelPreferencePersistence_;
        suppressPanelPreferencePersistence_ = true;
        hidePlaylistPanelImmediately();
        suppressPanelPreferencePersistence_ = previousSuppressState;
    };

    hidePanelsNow();
    if (!scheduleFollowUp) {
        return;
    }

    QTimer::singleShot(0, this, hidePanelsNow);
    QTimer::singleShot(std::clamp(effectiveSuppressionMs / 2, 180, 960), this, [this, hidePanelsNow]() {
        if (loadingMedia_ || mediaLoaded_) {
            hidePanelsNow();
        }
    });
}

int MainWindow::resolvePendingPlaylistSelectionIndex(
    const QVector<revaplayer::domain::PlaylistEntry> &entries) const
{
    const QString requestedSource = pendingPlaylistSelectionSource_.trimmed();
    if (!requestedSource.isEmpty()) {
        for (int index = 0; index < entries.size(); ++index) {
            if (sourcesReferToSameMedia(entries.at(index).source, requestedSource)) {
                return index;
            }
        }
    }

    if (pendingPlaylistSelectionIndex_ >= 0 && pendingPlaylistSelectionIndex_ < entries.size()) {
        return pendingPlaylistSelectionIndex_;
    }

    return -1;
}

void MainWindow::schedulePendingPlaylistSelectionRetry(const int delayMs)
{
    if (pendingPlaylistSelectionSource_.trimmed().isEmpty() || pendingPlaylistSelectionTimer_ == nullptr) {
        return;
    }

    const int safeDelay = std::max(0, delayMs);
    if (!pendingPlaylistSelectionTimer_->isActive() || pendingPlaylistSelectionTimer_->remainingTime() > safeDelay) {
        pendingPlaylistSelectionTimer_->start(safeDelay);
    }
}

void MainWindow::attemptPendingPlaylistSelection()
{
    if (playbackController_ == nullptr || pendingPlaylistSelectionSource_.trimmed().isEmpty()) {
        return;
    }

    if (sourcesReferToSameMedia(currentMediaSource_, pendingPlaylistSelectionSource_)) {
        if (pendingPlaylistNaturalOrderSources_.isEmpty()) {
            clearPendingPlaylistSelection();
            return;
        }

        ++pendingPlaylistSelectionRetryCount_;
        if (pendingPlaylistSelectionRetryCount_ >= kPendingPlaylistSelectionMaxRetries) {
            pendingPlaylistNaturalOrderSources_.clear();
            clearPendingPlaylistSelection();
            return;
        }

        schedulePendingPlaylistSelectionRetry(kPendingPlaylistSelectionRetryDelayMs);
        return;
    }

    const int requestedIndex = resolvePendingPlaylistSelectionIndex(playbackPlaylistEntriesCache_);
    const bool playlistStillBuilding = pendingPlaylistSelectionExpectedCount_ > 0
        && playbackPlaylistEntriesCache_.size() < pendingPlaylistSelectionExpectedCount_;

    if (requestedIndex >= 0) {
        selectPlaylistIndexWithResumeGuard(requestedIndex);
    }

    ++pendingPlaylistSelectionRetryCount_;
    if (pendingPlaylistSelectionRetryCount_ >= kPendingPlaylistSelectionMaxRetries) {
        pendingPlaylistNaturalOrderSources_.clear();
        clearPendingPlaylistSelection();
        return;
    }

    if (requestedIndex >= 0 || playlistStillBuilding) {
        schedulePendingPlaylistSelectionRetry(kPendingPlaylistSelectionRetryDelayMs);
    }
}

void MainWindow::clearPendingPlaylistViewportRestore()
{
    preservePlaylistViewportAfterReorder_ = false;
    pendingPlaylistViewportScrollValue_ = -1;
    pendingPlaylistViewportHorizontalScrollValue_ = -1;
}

void MainWindow::restorePendingPlaylistViewport(const bool clearPending)
{
    if (!preservePlaylistViewportAfterReorder_ || playlistView_ == nullptr) {
        if (clearPending) {
            clearPendingPlaylistViewportRestore();
        }
        return;
    }

    if (QScrollBar *verticalBar = playlistView_->verticalScrollBar(); verticalBar != nullptr && pendingPlaylistViewportScrollValue_ >= 0) {
        verticalBar->setValue(std::clamp(pendingPlaylistViewportScrollValue_, 0, verticalBar->maximum()));
    }
    if (QScrollBar *horizontalBar = playlistView_->horizontalScrollBar(); horizontalBar != nullptr && pendingPlaylistViewportHorizontalScrollValue_ >= 0) {
        horizontalBar->setValue(std::clamp(pendingPlaylistViewportHorizontalScrollValue_, 0, horizontalBar->maximum()));
    }

    if (clearPending) {
        clearPendingPlaylistViewportRestore();
    }
}

void MainWindow::restorePendingPlaylistOrderIfReady()
{
    if (pendingPlaylistNaturalOrderSources_.isEmpty()
        || playbackController_ == nullptr
        || playbackPlaylistEntriesCache_.size() != pendingPlaylistNaturalOrderSources_.size()) {
        return;
    }

    QStringList currentSources;
    currentSources.reserve(playbackPlaylistEntriesCache_.size());
    for (const auto &entry : std::as_const(playbackPlaylistEntriesCache_)) {
        currentSources.push_back(entry.source.trimmed());
    }

    const auto sourcePositionInCurrentOrder = [&currentSources](const QString &targetSource) {
        for (int index = 0; index < currentSources.size(); ++index) {
            if (sourcesReferToSameMedia(currentSources.at(index), targetSource)) {
                return index;
            }
        }
        return -1;
    };

    for (const QString &targetSource : std::as_const(pendingPlaylistNaturalOrderSources_)) {
        if (sourcePositionInCurrentOrder(targetSource) < 0) {
            return;
        }
    }

    const QStringList desiredSources = pendingPlaylistNaturalOrderSources_;
    pendingPlaylistNaturalOrderSources_.clear();
    for (int targetIndex = 0; targetIndex < desiredSources.size(); ++targetIndex) {
        const int currentPosition = sourcePositionInCurrentOrder(desiredSources.at(targetIndex));
        if (currentPosition < 0 || currentPosition == targetIndex) {
            continue;
        }

        playbackController_->executeMpvCommand({
            QStringLiteral("playlist-move"),
            QString::number(currentPosition),
            QString::number(targetIndex),
        });
        currentSources.move(currentPosition, targetIndex);
    }
}

void MainWindow::onIdleChanged(const bool idleActive)
{
    if (!idleActive) {
        ++idleStateGeneration_;
        errorStateActive_ = false;
        if (loadingMedia_ && !mediaLoaded_) {
            QTimer::singleShot(120, this, [this]() {
                finalizeActiveMediaLoadFromBackend();
            });
        }
        if (homeDashboard_ != nullptr) {
            homeDashboard_->hide();
        }
        if (videoViewport_ != nullptr) {
            videoViewport_->setRenderHostVisible(true);
        }
        updateControlBarBufferedState();
        updateActionStates();
        updateVideoPointerAutoHide();
        updateDisplaySleepInhibition();
        return;
    }

    const int idleGeneration = ++idleStateGeneration_;
    if (loadingMedia_
        || (mediaLoaded_ && !stopRequested_ && !endOfFilePending_ && !errorStateActive_)) {
        QTimer::singleShot(180, this, [this, idleGeneration]() {
            if (idleGeneration != idleStateGeneration_
                || loadingMedia_
                || (mediaLoaded_ && !stopRequested_ && !endOfFilePending_ && !errorStateActive_)) {
                return;
            }
            applyIdleStateReset();
        });
        return;
    }

    applyIdleStateReset();
}

void MainWindow::applyIdleStateReset()
{
    if (loadingMedia_) {
        return;
    }

    ++idleStateGeneration_;

    playlistCount_ = 0;
    currentPlaylistIndex_ = -1;
    loadingMediaSource_.clear();
    pendingPlaylistRequestedOrderSources_.clear();
    chapterCount_ = 0;
    currentChapterIndex_ = -1;
    previewHoverSource_.clear();
    previewHoverBucketMilliseconds_ = -1;
    currentMediaSource_.clear();
    localSubtitlesAutoLoadedForCurrentMedia_ = false;
    if (metadataScanService_ != nullptr) {
        metadataScanService_->cancel();
    }
    if (metadataRefreshTimer_ != nullptr) {
        metadataRefreshTimer_->stop();
    }
    pendingMediaScanSources_.clear();
    failedMediaScanSources_.clear();
    playlistMetadataScanBatchTotal_ = 0;
    playlistMetadataScanElapsed_.invalidate();
    if (thumbnailService_ != nullptr) {
        thumbnailService_->setCurrentSource({});
    }
    clearPlaylistThumbnailQueue(true);
    currentPositionSeconds_ = 0.0;
    currentDurationSeconds_ = 0.0;
    currentSpeed_ = 1.0;
    currentVideoZoomFactor_ = 1.0;
    currentVideoAlignX_ = 0.0;
    currentVideoAlignY_ = 0.0;
    controlBar_->setPlaybackSpeed(1.0);
    currentSubtitleDelaySeconds_ = 0.0;
    currentAudioDelaySeconds_ = 0.0;
    loopStartSeconds_ = -1.0;
    loopEndSeconds_ = -1.0;
    hasVideoTrack_ = false;
    playbackPaused_ = true;
    updateVideoPointerAutoHide();
    updateDisplaySleepInhibition();
    currentTracks_.clear();
    rebuildVideoQualityMenu({});
    rebuildControlBarSubtitleMenu({});
    currentDiagnostics_ = {};
    updateControlBarBufferedState();
    lastPersistedPositionSeconds_ = -1.0;
    resumeAttemptedForCurrentMedia_ = false;
    updatePlaylistMetadataScanButtonState();
    populateBookmarks({});
    populateChapters({}, -1);
    populateTracks({});
    populateSecondarySubtitleOptions();
    clearSceneBrowser(QStringLiteral("Load a local video to browse scenes"));
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
    if (videoViewport_ != nullptr) {
        videoViewport_->hideActionOverlay();
    }
    currentTitle_.clear();
    updateWindowTitle();
    updateMediaInformationOverlay();
    updateMediaInfoDialog();
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();

    clearPendingPlaylistSelection();
    pendingPlaylistNaturalOrderSources_.clear();
    clearPendingPlaylistViewportRestore();
    mediaLoaded_ = false;

    if (stopRequested_) {
        stopRequested_ = false;
        showIdleOverlay(QStringLiteral("Playback stopped"));
        statusBar()->showMessage(uiText("Playback stopped"), 3000);
        updateHomeDashboardVisibility();
        return;
    }

    if (endOfFilePending_) {
        endOfFilePending_ = false;
        showIdleOverlay(QStringLiteral("Playback finished"));
        statusBar()->showMessage(uiText("Reached the end of playback"), 3000);
        updateHomeDashboardVisibility();
        return;
    }

    if (!errorStateActive_) {
        showIdleOverlay(defaultIdleOverlayText());
    }

    updateHomeDashboardVisibility();
    updateActionStates();
}

void MainWindow::onPlaylistActivated(const QModelIndex &index)
{
    if (!index.isValid() || playlistController_ == nullptr) {
        return;
    }

    if (pinnedCourseBrowserActive_ && !pinnedCourseBrowserFolderPath_.isEmpty()) {
        const QString clickedSource = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
        if (isFolderBrowserBackSource(clickedSource)) {
            browseBackFolder();
            return;
        }

        if (isFolderBrowserSource(clickedSource)) {
            browseFolderPath(folderBrowserPathFromSource(clickedSource));
            return;
        }

        const QStringList sources = playlistSourcesForPinnedCourse(
            settingsController_,
            pinnedCourseBrowserFolderPath_,
            naturalSortFolderPlaylistEnabled());
        if (!sources.isEmpty()) {
            const QString clickedLocalPath = localMediaPathForSource(clickedSource);
            if (playlistView_ != nullptr) {
                playlistView_->setCurrentIndex(index);
                playlistView_->setProperty("playlistActivationSource", clickedSource);
                playlistView_->viewport()->update();
                QTimer::singleShot(220, this, [this, clickedSource]() {
                    if (playlistView_ == nullptr) {
                        return;
                    }
                    if (playlistView_->property("playlistActivationSource").toString().trimmed() == clickedSource) {
                        playlistView_->setProperty("playlistActivationSource", QString {});
                        playlistView_->viewport()->update();
                    }
                });
            }
            int selectedIndex = !clickedSource.isEmpty() ? sources.indexOf(clickedSource) : -1;
            if (selectedIndex < 0 && !clickedLocalPath.isEmpty()) {
                for (qsizetype candidateIndex = 0; candidateIndex < sources.size(); ++candidateIndex) {
                    if (localMediaPathForSource(sources.at(candidateIndex)) == clickedLocalPath) {
                        selectedIndex = static_cast<int>(candidateIndex);
                        break;
                    }
                }
            }

            if (selectedIndex < 0) {
                selectedIndex = 0;
            }
            // Starting playback from inside the folder browser must not
            // discard the active navigation hierarchy.
            openPlaylistSources(sources, selectedIndex, false, false);
        }
        return;
    }

    capturePanelStateForPlaylistActivation();

    const int playlistIndex = playlistController_->playlistIndexFor(index);
    if (playlistIndex < 0) {
        return;
    }

    selectPlaylistIndexWithResumeGuard(playlistIndex);
}

void MainWindow::onChapterActivated(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }

    const int chapterIndex = item->data(Qt::UserRole).toInt();
    playbackController_->selectChapter(chapterIndex);
}

void MainWindow::onTrackActivated(QTreeWidgetItem *item, int)
{
    if (item == nullptr || item->data(0, Qt::UserRole).isNull()) {
        return;
    }

    const int trackId = item->data(0, Qt::UserRole).toInt();
    const auto trackType = static_cast<revaplayer::domain::TrackType>(item->data(0, Qt::UserRole + 1).toInt());
    playbackController_->selectTrack(trackType, trackId);
}

void MainWindow::onPlaybackPositionChanged(const double positionSeconds, const double durationSeconds)
{
    if (resumeAttemptedForCurrentMedia_
        && lastPersistedPositionSeconds_ >= 5.0
        && currentPositionSeconds_ >= 5.0
        && positionSeconds < 1.0) {
        if (durationSeconds > 0.0) {
            currentDurationSeconds_ = durationSeconds;
        }
        return;
    }

    const double previousDurationSeconds = currentDurationSeconds_;
    currentPositionSeconds_ = positionSeconds;
    currentDurationSeconds_ = durationSeconds;
    const bool loadingSourceConfirmed = !currentMediaSource_.trimmed().isEmpty()
        && (loadingMediaSource_.trimmed().isEmpty()
            || sourcesReferToSameMedia(currentMediaSource_, loadingMediaSource_));
    if (!mediaLoaded_
        && loadingMedia_
        && (loadingSourceConfirmed || positionSeconds > 0.0 || durationSeconds > 0.0)) {
        finalizeActiveMediaLoadFromBackend();
    }
    const bool durationBecameKnown = previousDurationSeconds <= 0.0 && currentDurationSeconds_ > 0.0;
    const bool durationChanged = std::abs(previousDurationSeconds - currentDurationSeconds_) >= 0.05;
    const bool canRefreshVisibleUi = isVisible() && !isMinimized();
    const bool refreshPrimaryUi = canRefreshVisibleUi
        && (durationChanged
            || playbackPaused_
            || !playbackUiRefreshElapsed_.isValid()
            || playbackUiRefreshElapsed_.elapsed() >= playbackUiRefreshIntervalMs_);
    if (refreshPrimaryUi) {
        controlBar_->setPosition(positionSeconds, durationSeconds);
        updateControlBarBufferedState();
        updateMediaInformationOverlay();
        refreshPlaylistInspector();
        playbackUiRefreshElapsed_.restart();
    }
    if (previousDurationSeconds <= 0.0
        && currentDurationSeconds_ > 0.0
        && mediaLoaded_
        && hasVideoTrack_) {
        refreshSceneBrowserPrompt(true);
    }
    if (durationBecameKnown && !refreshPrimaryUi && canRefreshVisibleUi) {
        updateMediaInformationOverlay();
    }
    if (playlistDock_ != nullptr
        && playlistDock_->isVisible()
        && (!playlistPlaybackRefreshElapsed_.isValid()
            || playlistPlaybackRefreshElapsed_.elapsed() >= playlistPlaybackRefreshIntervalMs_)) {
        refreshPlaylistPlaybackProgress();
        playlistPlaybackRefreshElapsed_.restart();
    }

    // Persistence is intentionally action-driven (pause, seek, stop, close,
    // media switch). Position updates can arrive many times per second.
}

void MainWindow::onPausedChanged(const bool paused)
{
    playbackPaused_ = paused;
    if (!paused && loadingMedia_ && !mediaLoaded_) {
        finalizeActiveMediaLoadFromBackend();
    }
    controlBar_->setPaused(paused);
    updateMediaInformationOverlay();
    updateDisplaySleepInhibition();
    if (paused) {
        controlBar_->setPosition(currentPositionSeconds_, currentDurationSeconds_);
        persistPlaybackProgress(false, true);
        playbackUiRefreshElapsed_.restart();
    }

    if (playPauseAction_ != nullptr) {
        playPauseAction_->setText(paused ? uiText("Play") : uiText("Pause"));
    }
}

void MainWindow::onVolumeChanged(const int volume)
{
    currentVolume_ = volume;
    if (settingsController_ != nullptr && settingsController_->rememberLastVolume()) {
        settingsController_->setStartupVolume(volume);
    }
    controlBar_->setVolume(volume);
}

void MainWindow::onMutedChanged(const bool muted)
{
    muted_ = muted;

    if (toggleMuteAction_ != nullptr) {
        const QSignalBlocker blocker(toggleMuteAction_);
        toggleMuteAction_->setChecked(muted);
        toggleMuteAction_->setText(muted ? uiText("Unmute") : uiText("Mute"));
    }
}

void MainWindow::onTitleChanged(const QString &title)
{
    currentTitle_ = title;
    updateWindowTitle();
    updateMediaInfoDialog();
    if (mediaLoaded_) {
        reloadHomeDashboard();
    }
    if (favoriteCurrentMedia_) {
        reloadFavoritesPanel();
    }
}

void MainWindow::onFileLoaded()
{
    ++idleStateGeneration_;
    if (mediaLoaded_ && !loadingMedia_) {
        updateActionStates();
        updateControlBarBufferedState();
        return;
    }

    loadingMedia_ = false;
    mediaLoaded_ = true;
    stopRequested_ = false;
    endOfFilePending_ = false;
    errorStateActive_ = false;
    if (homeDashboard_ != nullptr) {
        homeDashboard_->hide();
    }
    if (videoViewport_ != nullptr) {
        videoViewport_->setRenderHostVisible(true);
    }
    updateActionStates();
    videoViewport_->setOverlayVisible(false);
    updateControlBarBufferedState();
    const bool sourceConfirmed = loadingMediaSource_.trimmed().isEmpty()
        || sourcesReferToSameMedia(currentMediaSource_, loadingMediaSource_);
    if (sourceConfirmed
        && historyController_ != nullptr
        && historyController_->isReady()
        && !currentMediaSource_.isEmpty()
        && !progressResetSuppressedSources_.contains(currentMediaSource_.trimmed())) {
        historyController_->recordMediaOpened(
            currentMediaSource_,
            effectiveCurrentMediaTitle(),
            currentDurationSeconds_,
            historyEnabled());
        trimHistoryToLimit();
    }
    reloadBookmarks();
    if (sourceConfirmed) {
        if (!applyPendingCurrentMediaRestore()) {
            maybeResumePlayback();
        }
        loadingMediaSource_.clear();
    }
    applyRuntimePreferences();
    playbackController_->setSpeed(sessionWidePlaybackSpeedEnabled()
        ? sessionPlaybackSpeed_
        : configuredStartupPlaybackSpeed());
    applyStoredMediaProfiles();
    applyConfiguredZoomStateForCurrentMedia();
    applyAdvancedSubtitlePreferences();
    applyRememberedSubtitleDelayForCurrentMedia();
    reloadHistoryPanel();
    refreshSceneBrowserPrompt(false);
    populateSecondarySubtitleOptions();
    applySmartSubtitleSelection(true, false);
    applyAudioFilterState();
    applyVideoFilterState();
    updateMediaInformationOverlay();
    updateMediaInfoDialog();
    mediaFileSystemCache_.remove(currentMediaSource_);
    mediaScanCacheLookupCompleted_.remove(mediaScanSourceKey(currentMediaSource_));
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    updateHomeDashboardVisibility();
    refreshPlaylistInspector();
    updateVideoPointerAutoHide();
    updateDisplaySleepInhibition();
    if (!pendingPlaylistSelectionSource_.trimmed().isEmpty()) {
        if (sourcesReferToSameMedia(currentMediaSource_, pendingPlaylistSelectionSource_)
            && pendingPlaylistNaturalOrderSources_.isEmpty()) {
            clearPendingPlaylistSelection();
        } else {
            schedulePendingPlaylistSelectionRetry(35);
        }
    }
    if (!currentMediaSource_.trimmed().isEmpty()) {
        const QString source = currentMediaSource_.trimmed();
        QTimer::singleShot(220, this, [this, source]() {
            if (currentMediaSource_.trimmed() == source) {
                warmPlaylistThumbnail(source, currentDurationSeconds_);
            }
        });
    }
    enforceHiddenSidePanelsAfterMediaOpen(1200, true);
    const QString effectiveTitle = effectiveCurrentMediaTitle();
    statusBar()->showMessage(
        effectiveTitle.isEmpty() ? QStringLiteral("Media loaded") : QStringLiteral("Now playing: %1").arg(effectiveTitle),
        2500);
}

void MainWindow::onFileEnded(const revaplayer::domain::PlaybackEndReason reason, const QString &message)
{
    loadingMedia_ = false;
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
    if (revaplayer::domain::playbackEndReasonIsFailure(reason)) {
        playlistActivationPanelRestorePending_ = false;
        temporaryPlaylistHidePending_ = false;
        updateDisplaySleepInhibition();
        updateVideoPointerAutoHide();
        showError(message);
        return;
    }

    if (revaplayer::domain::playbackEndReasonRepresentsCompletion(reason)) {
        persistPlaybackProgress(true);
    }
    reloadHistoryPanel();
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    refreshPlaylistInspector();

    if (!stopRequested_ && revaplayer::domain::playbackEndReasonRepresentsCompletion(reason)) {
        endOfFilePending_ = true;
    }

    updateActionStates();
    updateDisplaySleepInhibition();
    updateVideoPointerAutoHide();
}

void MainWindow::onPlaylistChanged(const QVector<revaplayer::domain::PlaylistEntry> &entries, const int currentIndex)
{
    const QString newSource = (currentIndex >= 0 && currentIndex < entries.size())
        ? entries[currentIndex].source
        : QString {};
    if (!currentMediaSource_.isEmpty()
        && !sourcesReferToSameMedia(newSource, currentMediaSource_)
        && mediaLoaded_) {
        persistPlaybackProgress(false, true);
    }

    QVector<revaplayer::domain::PlaylistEntry> displayEntries = entries;
    int displayCurrentIndex = currentIndex;
    if (!pendingPlaylistRequestedOrderSources_.isEmpty()
        && !pinnedCourseBrowserActive_
        && entries.size() == pendingPlaylistRequestedOrderSources_.size()) {
        QStringList incomingSources;
        incomingSources.reserve(entries.size());
        for (const auto &entry : entries) {
            incomingSources.push_back(entry.source.trimmed());
        }

        const auto sourcePositionInDesiredOrder = [this](const QString &targetSource) {
            for (int index = 0; index < pendingPlaylistRequestedOrderSources_.size(); ++index) {
                if (sourcesReferToSameMedia(pendingPlaylistRequestedOrderSources_.at(index), targetSource)) {
                    return index;
                }
            }
            return -1;
        };

        bool sameSourceSet = true;
        for (const QString &source : std::as_const(incomingSources)) {
            if (sourcePositionInDesiredOrder(source) < 0) {
                sameSourceSet = false;
                break;
            }
        }

        if (sameSourceSet) {
            QVector<revaplayer::domain::PlaylistEntry> reorderedDisplayEntries;
            reorderedDisplayEntries.reserve(entries.size());
            for (const QString &desiredSource : std::as_const(pendingPlaylistRequestedOrderSources_)) {
                const auto it = std::find_if(entries.cbegin(), entries.cend(), [&desiredSource](const auto &entry) {
                    return sourcesReferToSameMedia(entry.source, desiredSource);
                });
                if (it != entries.cend()) {
                    reorderedDisplayEntries.push_back(*it);
                }
            }

            if (reorderedDisplayEntries.size() == entries.size()) {
                displayEntries = std::move(reorderedDisplayEntries);
                displayCurrentIndex = -1;
                for (int index = 0; index < displayEntries.size(); ++index) {
                    displayEntries[index].index = index;
                    displayEntries[index].isCurrent = sourcesReferToSameMedia(displayEntries.at(index).source, newSource);
                    if (sourcesReferToSameMedia(displayEntries.at(index).source, newSource)) {
                        displayCurrentIndex = index;
                    }
                }

                bool orderConfirmed = true;
                for (int index = 0; index < incomingSources.size(); ++index) {
                    if (!sourcesReferToSameMedia(incomingSources.at(index), pendingPlaylistRequestedOrderSources_.at(index))) {
                        orderConfirmed = false;
                        break;
                    }
                }
                if (orderConfirmed) {
                    pendingPlaylistRequestedOrderSources_.clear();
                }
            }
        } else {
            pendingPlaylistRequestedOrderSources_.clear();
        }
    }

    playbackPlaylistEntriesCache_ = displayEntries;
    playbackPlaylistCurrentIndexCache_ = displayCurrentIndex;
    playlistCount_ = displayEntries.size();
    currentPlaylistIndex_ = displayCurrentIndex;
    clearPlaylistThumbnailQueue(true);
    if (!sourcesReferToSameMedia(newSource, currentMediaSource_)) {
        const QString previousSource = currentMediaSource_.trimmed();
        if (!previousSource.isEmpty()) {
            progressResetSuppressedSources_.remove(previousSource);
        }
        currentMediaSource_ = newSource;
        favoriteCurrentMedia_ = settingsController_ != nullptr
            && settingsController_->customValue(favoriteStorageKey(currentMediaSource_)).trimmed().size() > 0;
        if (thumbnailService_ != nullptr) {
            thumbnailService_->setCurrentSource(currentMediaSource_);
        }
        currentPositionSeconds_ = 0.0;
        currentDurationSeconds_ = 0.0;
        currentSubtitleDelaySeconds_ = 0.0;
        loopStartSeconds_ = -1.0;
        loopEndSeconds_ = -1.0;
        lastPersistedPositionSeconds_ = -1.0;
        resumeAttemptedForCurrentMedia_ = false;
        localSubtitlesAutoLoadedForCurrentMedia_ = false;
        if (subtitleDelayCustomSpinBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleDelayCustomSpinBox_);
            subtitleDelayCustomSpinBox_->setValue(0.0);
        }
        if (subtitleRememberDelayForMediaCheckBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleRememberDelayForMediaCheckBox_);
            subtitleRememberDelayForMediaCheckBox_->setChecked(false);
        }
        reloadBookmarks();
        clearSceneBrowser(QStringLiteral("Preparing scene browser"));
    }
    if (!mediaLoaded_
        && loadingMedia_
        && !currentMediaSource_.trimmed().isEmpty()
        && (loadingMediaSource_.trimmed().isEmpty()
            || sourcesReferToSameMedia(currentMediaSource_, loadingMediaSource_))) {
        const QString sourceAtSignal = currentMediaSource_.trimmed();
        QTimer::singleShot(0, this, [this, sourceAtSignal]() {
            if (!mediaLoaded_
                && loadingMedia_
                && sourcesReferToSameMedia(currentMediaSource_, sourceAtSignal)) {
                finalizeActiveMediaLoadFromBackend();
            }
        });
    }
    if (mediaLoaded_
        && !loadingMedia_
        && !currentMediaSource_.trimmed().isEmpty()
        && (loadingMediaSource_.trimmed().isEmpty()
            || sourcesReferToSameMedia(currentMediaSource_, loadingMediaSource_))) {
        if (!applyPendingCurrentMediaRestore()) {
            maybeResumePlayback();
        }
        loadingMediaSource_.clear();
    }
    updatePlaylistMetadataScanButtonState();
    applyVisiblePlaylistEntries();
    refreshPlaylistPresentationData();
    refreshPlaylistSummary();
    rebuildPinnedCourseTabs();
    refreshPlaylistInspector();

    if (!pendingPlaylistReorderSelectedSources_.isEmpty() && !pinnedCourseBrowserActive_) {
        restorePlaylistSelectionFromSources(
            pendingPlaylistReorderSelectedSources_,
            pendingPlaylistReorderCurrentSource_);
        pendingPlaylistReorderSelectedSources_.clear();
        pendingPlaylistReorderCurrentSource_.clear();
    }

    restorePendingPlaylistOrderIfReady();

    if (!pendingPlaylistSelectionSource_.trimmed().isEmpty()) {
        if (sourcesReferToSameMedia(newSource, pendingPlaylistSelectionSource_)
            && pendingPlaylistNaturalOrderSources_.isEmpty()) {
            clearPendingPlaylistSelection();
        } else if (resolvePendingPlaylistSelectionIndex(entries) >= 0 || currentIndex >= 0) {
            schedulePendingPlaylistSelectionRetry(0);
        } else if (pendingPlaylistSelectionExpectedCount_ > 0 && entries.size() < pendingPlaylistSelectionExpectedCount_) {
            schedulePendingPlaylistSelectionRetry(kPendingPlaylistSelectionRetryDelayMs);
        }
    }

    if (!pinnedCourseBrowserActive_ && displayCurrentIndex >= 0) {
        focusCurrentPlaylistItem(!preservePlaylistViewportAfterReorder_ && playlistAutoFollowEnabled());
    } else if (!pinnedCourseBrowserActive_ && playlistView_ != nullptr) {
        playlistView_->clearSelection();
    }
    restorePendingPlaylistViewport();

    updateActionStates();
    updateVideoPointerAutoHide();
    updateDisplaySleepInhibition();
}

void MainWindow::onChaptersChanged(const QVector<revaplayer::domain::ChapterInfo> &chapters, const int currentIndex)
{
    chapterCount_ = chapters.size();
    currentChapterIndex_ = currentIndex;
    populateChapters(chapters, currentIndex);
    updateActionStates();
}

void MainWindow::onTracksChanged(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    const bool hadVideoTrack = hasVideoTrack_;
    currentTracks_ = tracks;
    hasVideoTrack_ = std::any_of(
        tracks.cbegin(),
        tracks.cend(),
        [](const revaplayer::domain::TrackInfo &track) {
            return track.type == revaplayer::domain::TrackType::Video;
        });
    if (!tracks.isEmpty()) {
        finalizeActiveMediaLoadFromBackend();
    }
    rebuildVideoQualityMenu(tracks);
    rebuildControlBarSubtitleMenu(tracks);
    populateTracks(tracks);
    populateSecondarySubtitleOptions();
    applySmartSubtitleSelection(false, false);
    if (mediaLoaded_ && hasVideoTrack_ && !hadVideoTrack) {
        applyConfiguredZoomStateForCurrentMedia();
    }
    updateVideoPointerAutoHide();
    updateDisplaySleepInhibition();
    if (applyingRememberedSubtitleTrackChoice_) {
        applyingRememberedSubtitleTrackChoice_ = false;
    } else {
        applyRememberedSubtitleTrackChoice(currentTracks_);
    }
    if (!applyingRememberedSubtitleTrackChoice_) {
        rememberSelectedSubtitleTrackChoice(currentTracks_);
    }
    if (mediaLoaded_) {
        applyAdvancedSubtitlePreferences();
    }
    refreshPlaylistPresentationData();
    refreshPlaylistInspector();
    updateMediaInfoDialog();
    if (mediaLoaded_ && hasVideoTrack_ && (!hadVideoTrack || currentDurationSeconds_ > 0.0)) {
        refreshSceneBrowserPrompt(true);
    } else if (hadVideoTrack && !hasVideoTrack_) {
        refreshSceneBrowserPrompt(false);
    }
    updateActionStates();
}

void MainWindow::onPreviewRequested(const double timeSeconds, const QPoint &globalAnchor)
{
    previewHoverAnchor_ = globalAnchor;
    previewHoverTimeSeconds_ = std::max(0.0, timeSeconds);
    previewHoverBucketMilliseconds_ = revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(previewHoverTimeSeconds_);
    previewHoverSource_ = currentMediaSource_.trimmed();

    if (hoverPreviewPopup_ == nullptr) {
        return;
    }

    if (videoViewport_ != nullptr) {
        const int viewportWidth = std::max(1, videoViewport_->width());
        const int viewportHeight = std::max(1, videoViewport_->height());
        const int requestedWidth = std::max(224, thumbnailPopupWidth() > 0 ? thumbnailPopupWidth() : kDefaultThumbnailPopupWidth);
        const int requestedHeight = std::max(90, static_cast<int>(std::lround(requestedWidth * 9.0 / 16.0)));
        if (requestedWidth >= viewportWidth || requestedHeight >= viewportHeight) {
            onPreviewHidden();
            return;
        }
    }

    if (!isVisible() || isMinimized()) {
        onPreviewHidden();
        return;
    }

    if (previewStatusTimer_ != nullptr) {
        previewStatusTimer_->stop();
    }

    if (thumbnailService_ != nullptr && !thumbnailService_->previewEnabled()) {
        hoverPreviewPopup_->showStatus(
            uiText("Preview disabled in Preferences"),
            formatPlaybackTime(previewHoverTimeSeconds_),
            previewHoverAnchor_);
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->start(1200);
        }
        return;
    }

    const QString timeText = formatPlaybackTime(previewHoverTimeSeconds_);
    const QString previewLocalPath = localMediaPathForSource(currentMediaSource_);
    const auto previewFileInfo = previewLocalPath.isEmpty()
        ? std::optional<QFileInfo> {}
        : std::optional<QFileInfo> {QFileInfo(previewLocalPath)};
    if (currentMediaSource_.trimmed().isEmpty()
        || currentDurationSeconds_ <= 0.0
        || !previewFileInfo.has_value()
        || !previewFileInfo->exists()
        || !previewFileInfo->isFile()) {
        hoverPreviewPopup_->showStatus(uiText("Preview unavailable"), timeText, previewHoverAnchor_);
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->start(1200);
        }
        return;
    }

    hoverPreviewPopup_->showStatus(uiText("Loading preview"), timeText, previewHoverAnchor_);
    if (previewRequestTimer_ != nullptr) {
        previewRequestTimer_->start();
    }
}

void MainWindow::onPreviewHidden()
{
    previewHoverSource_.clear();
    previewHoverBucketMilliseconds_ = -1;
    if (previewRequestTimer_ != nullptr) {
        previewRequestTimer_->stop();
    }
    if (previewStatusTimer_ != nullptr) {
        previewStatusTimer_->stop();
    }
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
}

void MainWindow::onThumbnailReady(const QString &source, const qint64 bucketMilliseconds, const QImage &image)
{
    if (playlistThumbnailPendingBuckets_.value(source, -1) == bucketMilliseconds && !image.isNull()) {
        playlistThumbnailPendingBuckets_.remove(source);
        playlistThumbnailCache_.insert(source, image);
        schedulePlaylistMetadataRefresh(120);
    }

    if (hoverPreviewPopup_ == nullptr
        || source != previewHoverSource_
        || bucketMilliseconds != previewHoverBucketMilliseconds_) {
    } else {
        if (videoViewport_ != nullptr
            && (image.width() >= videoViewport_->width() || image.height() >= videoViewport_->height())) {
            onPreviewHidden();
            return;
        }
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->stop();
        }
        hoverPreviewPopup_->showPreview(image, formatPlaybackTime(previewHoverTimeSeconds_), previewHoverAnchor_);
    }

    if (bookmarksList_ != nullptr && bookmarkRowsByBucket_.contains(bucketMilliseconds)) {
        const QVector<int> rows = bookmarkRowsByBucket_.value(bucketMilliseconds);
        bool updatedBookmarkItem = false;
        for (const int row : rows) {
            QListWidgetItem *item = row >= 0 ? bookmarksList_->item(row) : nullptr;
            if (item == nullptr || item->data(Qt::UserRole + 6).toString().trimmed() != source) {
                continue;
            }

            item->setIcon(QIcon(QPixmap::fromImage(image)));
            item->setData(Qt::UserRole + 5, image);
            updatedBookmarkItem = true;
        }
        if (updatedBookmarkItem) {
            updateBookmarkSelectionPreview();
            requestNextBookmarkThumbnail();
        }
    }

    if (sceneList_ != nullptr
        && source == sceneBrowserSource_
        && sceneRowByBucket_.contains(bucketMilliseconds)) {
        const int row = sceneRowByBucket_.value(bucketMilliseconds, -1);
        QListWidgetItem *item = row >= 0 ? sceneList_->item(row) : nullptr;
        if (item != nullptr) {
            item->setIcon(QIcon(QPixmap::fromImage(image)));
            item->setData(Qt::UserRole + 2, 1);
            item->setData(kSceneThumbnailImageRole, image);
        }
        requestNextSceneThumbnail();
    }

    updateFavoriteThumbnailForSource(source, image);
}

void MainWindow::onThumbnailUnavailable(const QString &source, const qint64 bucketMilliseconds, const QString &reason)
{
    if (playlistThumbnailPendingBuckets_.value(source, -1) == bucketMilliseconds) {
        playlistThumbnailPendingBuckets_.remove(source);
    }

    if (hoverPreviewPopup_ == nullptr
        || source != previewHoverSource_
        || bucketMilliseconds != previewHoverBucketMilliseconds_) {
    } else {
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->stop();
        }
        hoverPreviewPopup_->showStatus(
            revaplayer::application::translateUiText(reason),
            formatPlaybackTime(previewHoverTimeSeconds_),
            previewHoverAnchor_);
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->start(1400);
        }
    }

    if (bookmarksList_ != nullptr && bookmarkRowsByBucket_.contains(bucketMilliseconds)) {
        const QVector<int> rows = bookmarkRowsByBucket_.value(bucketMilliseconds);
        const bool hasMatchingBookmark = std::any_of(rows.cbegin(), rows.cend(), [this, source](const int row) {
            QListWidgetItem *item = row >= 0 && bookmarksList_ != nullptr ? bookmarksList_->item(row) : nullptr;
            return item != nullptr && item->data(Qt::UserRole + 6).toString().trimmed() == source;
        });
        if (hasMatchingBookmark) {
            requestNextBookmarkThumbnail();
        }
    }

    if (sceneList_ != nullptr
        && source == sceneBrowserSource_
        && sceneRowByBucket_.contains(bucketMilliseconds)) {
        const int row = sceneRowByBucket_.value(bucketMilliseconds, -1);
        QListWidgetItem *item = row >= 0 ? sceneList_->item(row) : nullptr;
        if (item != nullptr) {
            item->setToolTip(reason);
            item->setData(Qt::UserRole + 2, 2);
        }
        requestNextSceneThumbnail();
    }
}

void MainWindow::onMediaMetadataReady(const revaplayer::services::media::MediaScanResult &result)
{
    const QString key = mediaScanSourceKey(result.source);
    if (key.isEmpty()) {
        return;
    }

    revaplayer::services::media::MediaScanResult normalizedResult = result;
    normalizedResult.source = key;
    pendingMediaScanSources_.remove(key);
    failedMediaScanSources_.remove(key);
    mediaScanCache_.insert(key, normalizedResult);

    if (settingsController_ != nullptr) {
        settingsController_->setCustomValue(
            mediaScanStorageKey(key),
            QString::fromUtf8(QJsonDocument(mediaScanResultObject(normalizedResult)).toJson(QJsonDocument::Compact)));
    }
    if (mediaScanSourceKey(currentMediaSource_) == key) {
        updateWindowTitle();
        reloadHomeDashboard();
    }
    if (pendingMediaScanSources_.isEmpty()) {
        playlistMetadataScanBatchTotal_ = 0;
        playlistMetadataScanElapsed_.invalidate();
    }
    mediaScanFailureCounts_.remove(key);
    mediaScanFailureReasons_.remove(key);
    mediaScanCacheLookupCompleted_.insert(key);
    schedulePlaylistMetadataRefresh(48);
}

void MainWindow::onMediaMetadataUnavailable(const QString &source, const QString &reason)
{
    const QString key = mediaScanSourceKey(source);
    if (key.isEmpty()) {
        return;
    }

    pendingMediaScanSources_.remove(key);
    const int failureCount = mediaScanFailureCounts_.value(key, 0) + 1;
    mediaScanFailureCounts_.insert(key, failureCount);
    mediaScanFailureReasons_.insert(key, reason.trimmed());
    if (failureCount >= kPlaylistMetadataFailureRetryLimit) {
        failedMediaScanSources_.insert(key);
    }
    if (pendingMediaScanSources_.isEmpty()) {
        playlistMetadataScanBatchTotal_ = 0;
        playlistMetadataScanElapsed_.invalidate();
    }
    schedulePlaylistMetadataRefresh(failureCount >= kPlaylistMetadataFailureRetryLimit ? 48 : 180);
    if (statusBar() != nullptr
        && pendingMediaScanSources_.isEmpty()
        && failureCount >= kPlaylistMetadataFailureRetryLimit) {
        statusBar()->showMessage(reason.trimmed().isEmpty() ? uiText("Background metadata scan failed for one or more items") : reason, 2500);
    }
}

void MainWindow::applySubtitlePreviewState(const bool visible,
                                           const double scale,
                                           const int position,
                                           const QString &fontFamily,
                                           const int fontSize,
                                           const QString &assOverride,
                                           const bool updateCurrentState)
{
    if (playbackController_ == nullptr || !playbackController_->isInitialized()) {
        return;
    }

    playbackController_->setSubtitleVisible(visible);
    playbackController_->setSubtitleScale(revaplayer::application::clampSubtitleScale(scale));
    playbackController_->setSubtitlePosition(revaplayer::application::clampSubtitlePosition(position));
    playbackController_->setSubtitleFontFamily(fontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : fontFamily.trimmed());
    playbackController_->setSubtitleFontSize(revaplayer::application::clampSubtitleFontSize(fontSize));
    playbackController_->setSubtitleAssOverride(revaplayer::application::normalizeSubtitleAssOverride(assOverride));

    if (!updateCurrentState) {
        return;
    }

    subtitleVisible_ = visible;
    currentSubtitleScale_ = revaplayer::application::clampSubtitleScale(scale);
    currentSubtitlePosition_ = revaplayer::application::clampSubtitlePosition(position);
    currentSubtitleFontFamily_ = fontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : fontFamily.trimmed();
    currentSubtitleFontSize_ = revaplayer::application::clampSubtitleFontSize(fontSize);
    currentSubtitleAssOverride_ = revaplayer::application::normalizeSubtitleAssOverride(assOverride);
    syncSubtitleActionStates();
}

void MainWindow::showError(const QString &message)
{
    loadingMedia_ = false;
    mediaLoaded_ = false;
    stopRequested_ = false;
    endOfFilePending_ = false;
    errorStateActive_ = true;
    videoViewport_->setOverlayText(message);
    videoViewport_->setOverlayVisible(true);
    updateHomeDashboardVisibility();
    updateControlBarBufferedState();
    updateMediaInformationOverlay();
    statusBar()->showMessage(message, 6000);
    updateActionStates();
    updateDisplaySleepInhibition();
}

void MainWindow::setupUi()
{
    auto *centralWidget = new QWidget(this);
    centralWidget->setObjectName(QStringLiteral("mainCentralWidget"));
    centralWidget->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    videoViewportRow_ = new QWidget(centralWidget);
    videoViewportRow_->setObjectName(QStringLiteral("videoViewportRow"));
    auto *videoViewportRowLayout = new QHBoxLayout(videoViewportRow_);
    videoViewportRowLayout->setContentsMargins(0, 0, 0, 0);
    videoViewportRowLayout->setSpacing(10);

    videoViewport_ = new VideoViewport(videoViewportRow_);
    videoViewport_->installEventFilter(this);
    videoViewport_->setRenderHostVisible(false);
    videoViewportRowLayout->addWidget(videoViewport_, 1);

    controlBar_ = new ControlBar(centralWidget);
    controlBar_->setObjectName(QStringLiteral("controlBar"));
    controlBar_->setProperty("overlayMode", false);
    mediaInformationOverlay_ = new MediaInformationOverlay(videoViewport_);
    fullscreenTopBar_ = new QWidget(videoViewport_);
    fullscreenTopBar_->setObjectName(QStringLiteral("fullscreenTopBar"));
    fullscreenTopBar_->hide();

    homeDashboard_ = new QWidget(videoViewport_);
    homeDashboard_->setObjectName(QStringLiteral("homeDashboard"));
    homeDashboard_->setAttribute(Qt::WA_StyledBackground, true);
    homeDashboard_->hide();

    auto *homeDashboardLayout = new QVBoxLayout(homeDashboard_);
    homeDashboardLayout->setContentsMargins(24, 22, 24, 22);
    homeDashboardLayout->setSpacing(16);

    homeDashboardTitleLabel_ = new QLabel(uiText("Welcome back"), homeDashboard_);
    homeDashboardTitleLabel_->setObjectName(QStringLiteral("homeDashboardTitleLabel"));
    homeDashboardSubtitleLabel_ = new QLabel(uiText("Continue where you stopped, jump into favorites, or reopen a saved list."), homeDashboard_);
    homeDashboardSubtitleLabel_->setObjectName(QStringLiteral("homeDashboardSubtitleLabel"));
    homeDashboardSubtitleLabel_->setWordWrap(true);

    auto *dashboardActionsRow = new QWidget(homeDashboard_);
    dashboardActionsRow->setObjectName(QStringLiteral("homeDashboardActionsRow"));
    auto *dashboardActionsLayout = new QHBoxLayout(dashboardActionsRow);
    dashboardActionsLayout->setContentsMargins(0, 0, 0, 0);
    dashboardActionsLayout->setSpacing(10);

    dashboardOpenFileButton_ = new QToolButton(dashboardActionsRow);
    dashboardOpenFileButton_->setText(uiText("Open File"));
    dashboardOpenFileButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    dashboardOpenUrlButton_ = new QToolButton(dashboardActionsRow);
    dashboardOpenUrlButton_->setText(uiText("Open URL"));
    dashboardOpenUrlButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);

    dashboardActionsLayout->addWidget(dashboardOpenFileButton_, 0);
    dashboardActionsLayout->addWidget(dashboardOpenUrlButton_, 0);
    dashboardActionsLayout->addStretch(1);

    const auto createDashboardSection = [this](const QString &title, QListWidget **listOut) {
        auto *group = new QGroupBox(title, homeDashboard_);
        group->setObjectName(QStringLiteral("homeDashboardSection"));
        auto *layout = new QVBoxLayout(group);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);
        auto *list = new QListWidget(group);
        list->setObjectName(QStringLiteral("homeDashboardList"));
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        layout->addWidget(list, 1);
        if (listOut != nullptr) {
            *listOut = list;
        }
        return group;
    };

    homeDashboardGridLayout_ = new QGridLayout();
    homeDashboardGridLayout_->setHorizontalSpacing(14);
    homeDashboardGridLayout_->setVerticalSpacing(14);
    homeDashboardGridLayout_->addWidget(createDashboardSection(uiText("Continue Watching"), &dashboardContinueList_), 0, 0);
    homeDashboardGridLayout_->addWidget(createDashboardSection(uiText("Recent"), &dashboardRecentList_), 0, 1);
    homeDashboardGridLayout_->addWidget(createDashboardSection(uiText("Favorites"), &dashboardFavoritesList_), 1, 0);
    homeDashboardGridLayout_->addWidget(createDashboardSection(uiText("Saved Lists"), &dashboardPinnedCoursesList_), 1, 1);
    homeDashboardGridLayout_->setColumnStretch(0, 1);
    homeDashboardGridLayout_->setColumnStretch(1, 1);

    homeDashboardLayout->addWidget(homeDashboardTitleLabel_, 0);
    homeDashboardLayout->addWidget(homeDashboardSubtitleLabel_, 0);
    homeDashboardLayout->addWidget(dashboardActionsRow, 0);
    homeDashboardLayout->addLayout(homeDashboardGridLayout_, 1);

    sidePanelSelector_ = new QWidget(videoViewport_);
    sidePanelSelector_->setObjectName(QStringLiteral("sidePanelSelector"));
    sidePanelSelector_->setAttribute(Qt::WA_StyledBackground, true);
    sidePanelSelector_->hide();

    auto *sidePanelSelectorLayout = new QVBoxLayout(sidePanelSelector_);
    sidePanelSelectorLayout->setContentsMargins(5, 5, 5, 5);
    sidePanelSelectorLayout->setSpacing(4);

    sidePanelPlaylistButton_ = new QToolButton(sidePanelSelector_);
    sidePanelPlaylistButton_->setObjectName(QStringLiteral("sidePanelSelectorPlaylistButton"));
    sidePanelPlaylistButton_->setText(QStringLiteral("PL"));
    sidePanelPlaylistButton_->setCheckable(true);
    sidePanelPlaylistButton_->setAutoRaise(true);
    sidePanelPlaylistButton_->setToolTip(uiText("Show Playlist"));

    sidePanelDetailsButton_ = new QToolButton(sidePanelSelector_);
    sidePanelDetailsButton_->setObjectName(QStringLiteral("sidePanelSelectorDetailsButton"));
    sidePanelDetailsButton_->setText(QStringLiteral("DT"));
    sidePanelDetailsButton_->setCheckable(true);
    sidePanelDetailsButton_->setAutoRaise(true);
    sidePanelDetailsButton_->setToolTip(uiText("Show Details"));

    sidePanelSelectorLayout->addWidget(sidePanelPlaylistButton_, 0);
    sidePanelSelectorLayout->addWidget(sidePanelDetailsButton_, 0);

    auto *fullscreenTopLayout = new QHBoxLayout(fullscreenTopBar_);
    fullscreenTopLayout->setContentsMargins(14, 10, 14, 10);
    fullscreenTopLayout->setSpacing(8);

    fullscreenTitleLabel_ = new QLabel(QStringLiteral("Reva Player"), fullscreenTopBar_);
    fullscreenTitleLabel_->setObjectName(QStringLiteral("fullscreenTopBarTitle"));

    fullscreenOpenButton_ = new QToolButton(fullscreenTopBar_);
    fullscreenOpenButton_->setText(uiText("Open"));
    fullscreenOpenButton_->setAutoRaise(true);

    fullscreenPlaylistButton_ = new QToolButton(fullscreenTopBar_);
    fullscreenPlaylistButton_->setText(uiText("Playlist"));
    fullscreenPlaylistButton_->setCheckable(true);
    fullscreenPlaylistButton_->setAutoRaise(true);

    fullscreenDetailsButton_ = new QToolButton(fullscreenTopBar_);
    fullscreenDetailsButton_->setText(uiText("Details"));
    fullscreenDetailsButton_->setCheckable(true);
    fullscreenDetailsButton_->setAutoRaise(true);

    fullscreenPreferencesButton_ = new QToolButton(fullscreenTopBar_);
    fullscreenPreferencesButton_->setText(uiText("Settings"));
    fullscreenPreferencesButton_->setAutoRaise(true);

    fullscreenExitFullscreenButton_ = new QToolButton(fullscreenTopBar_);
    fullscreenExitFullscreenButton_->setText(uiText("Exit Fullscreen"));
    fullscreenExitFullscreenButton_->setAutoRaise(true);

    fullscreenTopLayout->addWidget(fullscreenTitleLabel_, 1);
    fullscreenTopLayout->addWidget(fullscreenOpenButton_, 0);
    fullscreenTopLayout->addWidget(fullscreenPlaylistButton_, 0);
    fullscreenTopLayout->addWidget(fullscreenDetailsButton_, 0);
    fullscreenTopLayout->addWidget(fullscreenPreferencesButton_, 0);
    fullscreenTopLayout->addWidget(fullscreenExitFullscreenButton_, 0);

    layout->addWidget(videoViewportRow_, 1);
    layout->addWidget(controlBar_, 0);

    setCentralWidget(centralWidget);
    updateVideoOverlayGeometry();
}

void MainWindow::setupMenuBar()
{
    menuBar()->setNativeMenuBar(false);

    auto *fileMenu = menuBar()->addMenu(uiText("&File"));
    openFileAction_ = fileMenu->addAction(uiText("Open File"));
    openFileAction_->setShortcut(QKeySequence::Open);
    connect(openFileAction_, &QAction::triggered, this, &MainWindow::openFiles);

    openFolderAction_ = fileMenu->addAction(uiText("Open Folder"));
    openFolderAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    openFolderAction_->setShortcutContext(Qt::ApplicationShortcut);
    connect(openFolderAction_, &QAction::triggered, this, &MainWindow::openFolder);

    openUrlAction_ = fileMenu->addAction(uiText("Open URL"));
    openUrlAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    connect(openUrlAction_, &QAction::triggered, this, &MainWindow::openUrl);

    auto *openRecentMenu = fileMenu->addMenu(uiText("Open Recent"));
    connect(openRecentMenu, &QMenu::aboutToShow, this, [this, openRecentMenu]() {
        openRecentMenu->clear();
        if (historyController_ == nullptr || !historyController_->isReady() || !historyEnabled()) {
            QAction *placeholder = openRecentMenu->addAction(uiText("No recent media yet"));
            placeholder->setEnabled(false);
            return;
        }

        const auto entries = historyController_->recentHistory(10);
        if (entries.isEmpty()) {
            QAction *placeholder = openRecentMenu->addAction(uiText("No recent media yet"));
            placeholder->setEnabled(false);
            return;
        }

        for (const auto &entry : entries) {
            const QString source = entry.source.trimmed();
            if (source.isEmpty()) {
                continue;
            }
            QAction *action = openRecentMenu->addAction(displayTitleForHistory(source, entry.title));
            action->setToolTip(source);
            connect(action, &QAction::triggered, this, [this, source]() {
                if (!openMediaSource(source)) {
                    if (historyController_ != nullptr && historyController_->isReady()) {
                        historyController_->removeHistoryEntry(source);
                    }
                    reloadHistoryPanel();
                    statusBar()->showMessage(uiText("That recent item is no longer available and was removed from History."), 5000);
                }
            });
        }

        if (openRecentMenu->actions().isEmpty()) {
            QAction *placeholder = openRecentMenu->addAction(uiText("No recent media yet"));
            placeholder->setEnabled(false);
        }
    });

    fileMenu->addSeparator();
    auto *specialSourceMenu = fileMenu->addMenu(uiText("Open Special Source"));
    QAction *openDvdAction = specialSourceMenu->addAction(uiText("DVD / ISO"));
    QAction *openBluRayAction = specialSourceMenu->addAction(uiText("Blu-ray"));
    QAction *openDvbAction = specialSourceMenu->addAction(uiText("DVB / Broadcast"));
    QAction *openLiveCaptureAction = specialSourceMenu->addAction(uiText("Live Capture"));
    connect(openDvdAction, &QAction::triggered, this, [this]() {
        showSpecialSourceDialog(
            uiText("Open DVD Source"),
            QStringLiteral("dvd://"),
            uiText("Enter an mpv DVD target such as dvd:// or dvd://2"));
    });
    connect(openBluRayAction, &QAction::triggered, this, [this]() {
        showSpecialSourceDialog(
            uiText("Open Blu-ray Source"),
            QStringLiteral("bd://"),
            uiText("Enter an mpv Blu-ray target such as bd:// or bd://1"));
    });
    connect(openDvbAction, &QAction::triggered, this, [this]() {
        showSpecialSourceDialog(
            uiText("Open DVB / Broadcast Source"),
            QStringLiteral("dvb://"),
            uiText("Enter an mpv DVB target such as dvb:// or a tuned backend URL"));
    });
    connect(openLiveCaptureAction, &QAction::triggered, this, [this]() {
        showSpecialSourceDialog(
            uiText("Open Live Capture Source"),
            QStringLiteral("av://v4l2:/dev/video0"),
            uiText("Enter a live capture target such as av://v4l2:/dev/video0"));
    });

    fileMenu->addSeparator();
    loadSubtitleAction_ = fileMenu->addAction(uiText("Load Subtitle File"));
    showMediaInfoAction_ = fileMenu->addAction(uiText("Media Info"));

    fileMenu->addSeparator();
    takeScreenshotAction_ = fileMenu->addAction(uiText("Take Screenshot"));
    takeScreenshotAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));

    preferencesAction_ = new QAction(uiText("Preferences"), this);
    preferencesAction_->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction_, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    fileMenu->addSeparator();
    closeApplicationAction_ = fileMenu->addAction(uiText("Exit"));
    closeApplicationAction_->setShortcut(QKeySequence::Quit);
    closeApplicationAction_->setShortcutContext(Qt::ApplicationShortcut);

    auto *playbackMenu = menuBar()->addMenu(uiText("&Playback"));
    auto *transportMenu = playbackMenu->addMenu(uiText("Transport"));
    playPauseAction_ = transportMenu->addAction(uiText("Play / Pause"));
    playPauseAction_->setShortcut(Qt::Key_Space);

    stopAction_ = transportMenu->addAction(uiText("Stop"));
    stopAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+.")));

    profileMenu_ = playbackMenu->addMenu(uiText("Profile"));
    profileActionGroup_ = new QActionGroup(this);
    profileActionGroup_->setExclusive(true);

    const auto addProfileAction = [this](QMenu *menu, const revaplayer::domain::PlayerProfile profile) {
        QAction *action = menu->addAction(revaplayer::domain::playerProfileLabel(profile));
        action->setCheckable(true);
        action->setData(revaplayer::domain::playerProfileId(profile));
        profileActionGroup_->addAction(action);
        connect(action, &QAction::triggered, this, [this, profile]() {
            if (settingsController_ != nullptr) {
                settingsController_->setPlaybackProfile(profile);
            }
            applyPlaybackProfile(true);
        });
    };

    addProfileAction(profileMenu_, revaplayer::domain::PlayerProfile::Battery);
    addProfileAction(profileMenu_, revaplayer::domain::PlayerProfile::Balanced);
    addProfileAction(profileMenu_, revaplayer::domain::PlayerProfile::Quality);

    auto *speedMenu = playbackMenu->addMenu(uiText("Speed"));
    speedDownAction_ = speedMenu->addAction(uiText("Decrease Speed"));
    speedUpAction_ = speedMenu->addAction(uiText("Increase Speed"));
    speedResetAction_ = speedMenu->addAction(uiText("Reset Speed"));

    auto *bookmarksMenu = playbackMenu->addMenu(uiText("Bookmarks"));
    addBookmarkAction_ = bookmarksMenu->addAction(uiText("Add Bookmark"));
    addBookmarkAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+B")));
    deleteBookmarkAction_ = new QAction(uiText("Delete Selected Bookmark"), this);
    favoriteCurrentMediaAction_ = bookmarksMenu->addAction(uiText("Favorite Current Media"));
    favoriteCurrentMediaAction_->setCheckable(true);

    auto *loopMenu = playbackMenu->addMenu(uiText("A-B Loop"));
    setLoopStartAction_ = loopMenu->addAction(uiText("Set Loop Start (A)"));
    setLoopEndAction_ = loopMenu->addAction(uiText("Set Loop End (B)"));
    clearLoopAction_ = loopMenu->addAction(uiText("Clear A-B Loop"));

    auto *repeatMenu = playbackMenu->addMenu(uiText("Repeat"));
    repeatModeActionGroup_ = new QActionGroup(this);
    repeatModeActionGroup_->setExclusive(true);
    repeatOffAction_ = repeatMenu->addAction(uiText("Off"));
    repeatFileAction_ = repeatMenu->addAction(uiText("Repeat Current File"));
    repeatPlaylistAction_ = repeatMenu->addAction(uiText("Repeat Playlist"));
    for (QAction *action : {repeatOffAction_, repeatFileAction_, repeatPlaylistAction_}) {
        action->setCheckable(true);
        repeatModeActionGroup_->addAction(action);
    }
    repeatOffAction_->setData(QStringLiteral("off"));
    repeatFileAction_->setData(QStringLiteral("file"));
    repeatPlaylistAction_->setData(QStringLiteral("playlist"));

    auto *navigationMenu = playbackMenu->addMenu(uiText("Navigation"));
    previousPlaylistAction_ = navigationMenu->addAction(uiText("Previous Playlist Item"));
    previousPlaylistAction_->setShortcut(QKeySequence(Qt::Key_PageUp));
    nextPlaylistAction_ = navigationMenu->addAction(uiText("Next Playlist Item"));
    nextPlaylistAction_->setShortcut(QKeySequence(Qt::Key_PageDown));
    navigationMenu->addSeparator();
    previousChapterAction_ = navigationMenu->addAction(uiText("Previous Chapter"));
    previousChapterAction_->setShortcut(QKeySequence(QStringLiteral("Alt+Left")));
    nextChapterAction_ = navigationMenu->addAction(uiText("Next Chapter"));
    nextChapterAction_->setShortcut(QKeySequence(QStringLiteral("Alt+Right")));
    navigationMenu->addSeparator();
    frameStepBackwardAction_ = navigationMenu->addAction(uiText("Previous Frame"));
    frameStepForwardAction_ = navigationMenu->addAction(uiText("Next Frame"));

    auto *seekMenu = playbackMenu->addMenu(uiText("Seek"));
    seekBackwardShortAction_ = seekMenu->addAction(uiText("Seek Backward 5s"));
    seekBackwardShortAction_->setShortcut(QKeySequence(Qt::Key_Left));
    seekForwardShortAction_ = seekMenu->addAction(uiText("Seek Forward 5s"));
    seekForwardShortAction_->setShortcut(QKeySequence(Qt::Key_Right));
    seekBackwardLongAction_ = seekMenu->addAction(uiText("Seek Backward 30s"));
    seekBackwardLongAction_->setShortcut(QKeySequence(QStringLiteral("Shift+Left")));
    seekForwardLongAction_ = seekMenu->addAction(uiText("Seek Forward 30s"));
    seekForwardLongAction_->setShortcut(QKeySequence(QStringLiteral("Shift+Right")));

    auto *volumeMenu = playbackMenu->addMenu(uiText("Volume"));
    volumeDownAction_ = volumeMenu->addAction(uiText("Volume Down"));
    volumeDownAction_->setShortcut(QKeySequence(Qt::Key_Down));
    volumeUpAction_ = volumeMenu->addAction(uiText("Volume Up"));
    volumeUpAction_->setShortcut(QKeySequence(Qt::Key_Up));
    toggleMuteAction_ = volumeMenu->addAction(uiText("Mute"));
    toggleMuteAction_->setShortcut(QKeySequence(Qt::Key_M));
    toggleMuteAction_->setCheckable(true);

    alwaysOnTopAction_ = new QAction(uiText("Always On Top"), this);
    alwaysOnTopAction_->setCheckable(true);
    alwaysOnTopAction_->setVisible(false);

    auto *subtitleMenu = menuBar()->addMenu(uiText("&Subtitle"));
    toggleSubtitleVisibilityAction_ = subtitleMenu->addAction(uiText("Show Subtitles"));
    toggleSubtitleVisibilityAction_->setCheckable(true);
    subtitleMenu->addSeparator();
    subtitleMenu->addAction(loadSubtitleAction_);
    subtitleMenu->addSeparator();
    subtitleDelayDownAction_ = subtitleMenu->addAction(uiText("Subtitle Delay Down"));
    subtitleDelayUpAction_ = subtitleMenu->addAction(uiText("Subtitle Delay Up"));
    subtitleDelayManualAction_ = subtitleMenu->addAction(uiText("Set Subtitle Delay"));
    subtitleDelayResetAction_ = subtitleMenu->addAction(uiText("Reset Subtitle Delay"));
    subtitleMenu->addSeparator();
    auto *subtitleScaleMenu = subtitleMenu->addMenu(uiText("Scale"));
    subtitleScaleDownAction_ = subtitleScaleMenu->addAction(uiText("Decrease Subtitle Scale"));
    subtitleScaleUpAction_ = subtitleScaleMenu->addAction(uiText("Increase Subtitle Scale"));
    subtitleScaleResetAction_ = subtitleScaleMenu->addAction(uiText("Reset Subtitle Scale"));

    auto *subtitlePositionMenu = subtitleMenu->addMenu(uiText("Position"));
    subtitlePositionUpAction_ = subtitlePositionMenu->addAction(uiText("Move Subtitles Up"));
    subtitlePositionDownAction_ = subtitlePositionMenu->addAction(uiText("Move Subtitles Down"));
    subtitlePositionResetAction_ = subtitlePositionMenu->addAction(uiText("Reset Subtitle Position"));

    auto *subtitleOverrideMenu = subtitleMenu->addMenu(uiText("Style Override"));
    subtitleOverrideActionGroup_ = new QActionGroup(this);
    subtitleOverrideActionGroup_->setExclusive(true);
    const auto addSubtitleOverrideAction = [this, subtitleOverrideMenu](const QString &id) {
        QAction *action = subtitleOverrideMenu->addAction(revaplayer::application::subtitleAssOverrideLabel(id));
        action->setCheckable(true);
        action->setData(id);
        subtitleOverrideActionGroup_->addAction(action);
        addAction(action);
        if (id == QStringLiteral("scale")) {
            subtitleOverrideScaleAction_ = action;
        } else if (id == QStringLiteral("yes")) {
            subtitleOverrideYesAction_ = action;
        } else if (id == QStringLiteral("force")) {
            subtitleOverrideForceAction_ = action;
        } else if (id == QStringLiteral("strip")) {
            subtitleOverrideStripAction_ = action;
        } else if (id == QStringLiteral("no")) {
            subtitleOverrideNoAction_ = action;
        }
        connect(action, &QAction::triggered, this, [this, action](const bool) {
            currentSubtitleAssOverride_ = revaplayer::application::normalizeSubtitleAssOverride(action->data().toString());
            playbackController_->setSubtitleAssOverride(currentSubtitleAssOverride_);
            if (settingsController_ != nullptr) {
                settingsController_->setSubtitleAssOverride(currentSubtitleAssOverride_);
            }
            syncSubtitleActionStates();
            statusBar()->showMessage(
                QStringLiteral("Subtitle style override: %1")
                    .arg(revaplayer::application::subtitleAssOverrideLabel(currentSubtitleAssOverride_)),
                3000);
        });
    };
    for (const auto &option : revaplayer::application::subtitleAssOverrideOptions()) {
        addSubtitleOverrideAction(option.id);
    }
    subtitleOverrideMenu->addSeparator();
    cycleSubtitleAssOverrideAction_ = subtitleOverrideMenu->addAction(uiText("Cycle Style Override"));

    auto *audioMenu = menuBar()->addMenu(uiText("&Audio"));
    audioMenu->addAction(toggleMuteAction_);
    audioMenu->addSeparator();
    audioDelayDownAction_ = audioMenu->addAction(uiText("Audio Delay Down"));
    audioDelayUpAction_ = audioMenu->addAction(uiText("Audio Delay Up"));
    audioDelayManualAction_ = audioMenu->addAction(uiText("Set Audio Delay"));
    audioDelayResetAction_ = audioMenu->addAction(uiText("Reset Audio Delay"));

    auto *videoMenu = menuBar()->addMenu(uiText("&Video"));
    videoQualityMenu_ = videoMenu->addMenu(uiText("Quality"));
    videoQualityActionGroup_ = new QActionGroup(this);
    videoQualityActionGroup_->setExclusive(true);
    rebuildVideoQualityMenu({});
    rebuildControlBarSubtitleMenu({});
    videoMenu->addSeparator();
    aspectMenu_ = videoMenu->addMenu(uiText("Aspect Ratio"));
    aspectActionGroup_ = new QActionGroup(this);
    aspectActionGroup_->setExclusive(true);

    const auto addAspectAction = [this](const QString &label, const QString &value) {
        QAction *action = aspectMenu_->addAction(label);
        action->setCheckable(true);
        action->setData(value);
        aspectActionGroup_->addAction(action);
        addAction(action);
        if (value == QStringLiteral("no")) {
            aspectDefaultAction_ = action;
        } else if (value == QStringLiteral("16:9")) {
            aspect16x9Action_ = action;
        } else if (value == QStringLiteral("4:3")) {
            aspect4x3Action_ = action;
        } else if (value == QStringLiteral("1.85:1")) {
            aspect185Action_ = action;
        } else if (value == QStringLiteral("2.35:1")) {
            aspect235Action_ = action;
        }
        connect(action, &QAction::triggered, this, [this, action](const bool) {
            currentAspectOverride_ = action->data().toString();
            if (currentAspectOverride_ == QStringLiteral("no")) {
                playbackController_->resetVideoAspectOverride();
            } else {
                playbackController_->setVideoAspectOverride(currentAspectOverride_);
            }
            syncVideoActionStates();
            statusBar()->showMessage(uiText("Aspect ratio: %1").arg(action->text()), 2500);
        });
    };

    addAspectAction(QStringLiteral("Default"), QStringLiteral("no"));
    addAspectAction(QStringLiteral("16:9"), QStringLiteral("16:9"));
    addAspectAction(QStringLiteral("4:3"), QStringLiteral("4:3"));
    addAspectAction(QStringLiteral("1.85:1"), QStringLiteral("1.85:1"));
    addAspectAction(QStringLiteral("2.35:1"), QStringLiteral("2.35:1"));

    cropMenu_ = videoMenu->addMenu(uiText("Crop"));
    cropActionGroup_ = new QActionGroup(this);
    cropActionGroup_->setExclusive(true);

    const auto addCropAction = [this](const QString &label, const QString &value) {
        QAction *action = cropMenu_->addAction(label);
        action->setCheckable(true);
        action->setData(value);
        cropActionGroup_->addAction(action);
        addAction(action);
        if (value.isEmpty()) {
            cropDefaultAction_ = action;
        } else if (value == QStringLiteral("16:9")) {
            crop16x9Action_ = action;
        } else if (value == QStringLiteral("1.85:1")) {
            crop185Action_ = action;
        } else if (value == QStringLiteral("2.35:1")) {
            crop235Action_ = action;
        } else if (value == QStringLiteral("0x0+0+0")) {
            cropDisableAction_ = action;
        }
        connect(action, &QAction::triggered, this, [this, action](const bool) {
            currentCropPreset_ = action->data().toString();
            if (currentCropPreset_.isEmpty()) {
                playbackController_->clearVideoCrop();
            } else {
                playbackController_->setVideoCrop(currentCropPreset_);
            }
            syncVideoActionStates();
            statusBar()->showMessage(uiText("Video crop: %1").arg(action->text()), 2500);
        });
    };

    addCropAction(QStringLiteral("Default / Container Crop"), QString {});
    addCropAction(QStringLiteral("16:9 Center Crop"), QStringLiteral("16:9"));
    addCropAction(QStringLiteral("1.85:1 Center Crop"), QStringLiteral("1.85:1"));
    addCropAction(QStringLiteral("2.35:1 Center Crop"), QStringLiteral("2.35:1"));
    addCropAction(QStringLiteral("Disable Crop"), QStringLiteral("0x0+0+0"));

    rotateMenu_ = videoMenu->addMenu(uiText("Rotate"));
    rotateActionGroup_ = new QActionGroup(this);
    rotateActionGroup_->setExclusive(true);

    const auto addRotateAction = [this](const QString &label, const int degrees) {
        QAction *action = rotateMenu_->addAction(label);
        action->setCheckable(true);
        action->setData(degrees);
        rotateActionGroup_->addAction(action);
        addAction(action);
        if (degrees == 0) {
            rotateDefaultAction_ = action;
        } else if (degrees == 90) {
            rotate90Action_ = action;
        } else if (degrees == 180) {
            rotate180Action_ = action;
        } else if (degrees == 270) {
            rotate270Action_ = action;
        }
        connect(action, &QAction::triggered, this, [this, action](const bool) {
            currentVideoRotationDegrees_ = revaplayer::application::normalizeRightAngleRotation(action->data().toInt());
            playbackController_->setVideoRotation(currentVideoRotationDegrees_);
            syncVideoActionStates();
            statusBar()->showMessage(uiText("Video rotation: %1").arg(action->text()), 2500);
        });
    };

    addRotateAction(QStringLiteral("0° / Default"), 0);
    addRotateAction(QStringLiteral("90°"), 90);
    addRotateAction(QStringLiteral("180°"), 180);
    addRotateAction(QStringLiteral("270°"), 270);

    auto *zoomMenu = videoMenu->addMenu(uiText("Zoom"));
    videoZoomOutAction_ = zoomMenu->addAction(uiText("Zoom Out"));
    videoZoomOutAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    videoZoomInAction_ = zoomMenu->addAction(uiText("Zoom In"));
    videoZoomInAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+=")));
    videoZoomResetAction_ = zoomMenu->addAction(uiText("Reset Zoom"));
    videoZoomResetAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    zoomMenu->addSeparator();
    videoPanLeftAction_ = zoomMenu->addAction(uiText("Pan Left"));
    videoPanLeftAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Left")));
    videoPanRightAction_ = zoomMenu->addAction(uiText("Pan Right"));
    videoPanRightAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Right")));
    videoPanUpAction_ = zoomMenu->addAction(uiText("Pan Up"));
    videoPanUpAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Up")));
    videoPanDownAction_ = zoomMenu->addAction(uiText("Pan Down"));
    videoPanDownAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Down")));

    auto *viewMenu = menuBar()->addMenu(uiText("&View"));
    togglePlaylistAction_ = viewMenu->addAction(uiText("Playlist Panel"));
    togglePlaylistAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+P")));
    togglePlaylistAction_->setShortcutContext(Qt::WindowShortcut);
    togglePlaylistAction_->setCheckable(true);
    addAction(togglePlaylistAction_);
    connect(togglePlaylistAction_, &QAction::triggered, this, [this]() {
        toggleSidePanel(SidePanel::Playlist);
    });

    toggleDetailsAction_ = viewMenu->addAction(uiText("Details Panel"));
    toggleDetailsAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    toggleDetailsAction_->setShortcutContext(Qt::WindowShortcut);
    toggleDetailsAction_->setCheckable(true);
    addAction(toggleDetailsAction_);
    connect(toggleDetailsAction_, &QAction::triggered, this, [this]() {
        toggleSidePanel(SidePanel::Details);
    });

    showMediaInformationOverlayAction_ = viewMenu->addAction(uiText("Media Information"));

    toggleFullscreenAction_ = viewMenu->addAction(uiText("Toggle Fullscreen"));
    toggleFullscreenAction_->setShortcut(Qt::Key_F);
    toggleFullscreenAction_->setShortcutContext(Qt::WindowShortcut);
    addAction(toggleFullscreenAction_);
    connect(toggleFullscreenAction_, &QAction::triggered, this, &MainWindow::toggleFullscreen);

    themeMenu_ = viewMenu->addMenu(uiText("Theme"));
    themeActionGroup_ = new QActionGroup(this);
    themeActionGroup_->setExclusive(true);
    for (const auto &theme : revaplayer::application::availableThemes()) {
        QAction *action = themeMenu_->addAction(revaplayer::application::translateUiText(theme.label));
        action->setCheckable(true);
        action->setData(theme.id);
        themeActionGroup_->addAction(action);
        connect(action, &QAction::triggered, this, [this, action]() {
            if (settingsController_ != nullptr) {
                settingsController_->setUiTheme(action->data().toString());
            }
            applySelectedTheme(true);
        });
    }

    auto *toolsMenu = menuBar()->addMenu(uiText("&Tools"));
    toolsMenu->addAction(uiText("Clear Cache"), this, &MainWindow::clearApplicationCache);
    toolsMenu->addAction(preferencesAction_);

    auto *helpMenu = menuBar()->addMenu(uiText("&Help"));
    QAction *quickHelpAction = helpMenu->addAction(uiText("Quick Help"));
    connect(quickHelpAction, &QAction::triggered, this, &MainWindow::showQuickHelpDialog);
    QAction *aboutAction = helpMenu->addAction(uiText("About Reva Player"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    syncRepeatModeActions();
    syncVideoActionStates();
}

void MainWindow::setupPlaybackActions()
{
    const auto bindAction = [this](QAction *action, const auto &slot) {
        if (action == nullptr) {
            return;
        }

        addAction(action);
        connect(action, &QAction::triggered, this, slot);
    };

    bindAction(playPauseAction_, [this]() {
        showPlaybackFeedback(playbackPaused_ ? QStringLiteral("▶ Play") : QStringLiteral("▮▮ Pause"));
        playbackController_->togglePause();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->togglePause();
        }
    });

    bindAction(loadSubtitleAction_, [this]() {
        loadSubtitleFile();
    });
    bindAction(toggleSubtitleVisibilityAction_, [this]() {
        playbackController_->toggleSubtitleVisible();
        const bool effectiveVisible = !subtitleVisible_;
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitleVisible(effectiveVisible);
        }
        statusBar()->showMessage(
            effectiveVisible ? QStringLiteral("Subtitles enabled") : QStringLiteral("Subtitles hidden"),
            2500);
    });
    bindAction(showMediaInfoAction_, [this]() {
        showMediaInfoDialog();
    });
    bindAction(closeApplicationAction_, [this]() {
        close();
    });
    bindAction(stopAction_, [this]() {
        stopRequested_ = true;
        persistPlaybackProgress(false, true);
        showPlaybackFeedback(QStringLiteral("■ Stop"));
        playbackController_->stop();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->stop();
        }
    });
    bindAction(takeScreenshotAction_, [this]() {
        takeScreenshot();
    });
    bindAction(addBookmarkAction_, [this]() {
        addBookmark();
    });
    bindAction(favoriteCurrentMediaAction_, [this]() {
        toggleFavoriteCurrentMedia();
    });
    bindAction(deleteBookmarkAction_, [this]() {
        removeSelectedBookmark();
    });
    bindAction(setLoopStartAction_, [this]() {
        const double newLoopStart = std::max(0.0, currentPositionSeconds_);
        if (loopEndSeconds_ >= 0.0 && newLoopStart >= loopEndSeconds_ - kLoopComparisonEpsilon) {
            playbackController_->clearLoop();
            loopEndSeconds_ = -1.0;
        }

        playbackController_->setLoopStart(newLoopStart);
        loopStartSeconds_ = newLoopStart;
        updateActionStates();
        statusBar()->showMessage(uiText("Loop A set at %1").arg(formatPlaybackTime(newLoopStart)), 3000);
    });
    bindAction(setLoopEndAction_, [this]() {
        if (loopStartSeconds_ < 0.0) {
            statusBar()->showMessage(uiText("Set loop start before setting loop end."), 3500);
            return;
        }

        const double newLoopEnd = std::max(0.0, currentPositionSeconds_);
        if (newLoopEnd <= loopStartSeconds_ + kLoopComparisonEpsilon) {
            statusBar()->showMessage(uiText("Loop end must be after loop start."), 3500);
            return;
        }

        playbackController_->setLoopEnd(newLoopEnd);
        loopEndSeconds_ = newLoopEnd;
        updateActionStates();
        statusBar()->showMessage(
            uiText("A-B loop active: %1 -> %2")
                .arg(formatPlaybackTime(loopStartSeconds_), formatPlaybackTime(loopEndSeconds_)),
            3500);
    });
    bindAction(clearLoopAction_, [this]() {
        playbackController_->clearLoop();
        loopStartSeconds_ = -1.0;
        loopEndSeconds_ = -1.0;
        updateActionStates();
        statusBar()->showMessage(uiText("A-B loop cleared"), 3000);
    });
    bindAction(frameStepBackwardAction_, [this]() {
        playbackController_->frameStepBackward();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->frameStepBackward();
        }
        showPlaybackFeedback(QStringLiteral("Frame -1"));
    });
    bindAction(frameStepForwardAction_, [this]() {
        playbackController_->frameStepForward();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->frameStepForward();
        }
        showPlaybackFeedback(QStringLiteral("Frame +1"));
    });
    setAlwaysOnTopEnabled(false, false);
    bindAction(repeatOffAction_, [this]() {
        setRepeatMode(QStringLiteral("off"), true);
    });
    bindAction(repeatFileAction_, [this]() {
        setRepeatMode(QStringLiteral("file"), true);
    });
    bindAction(repeatPlaylistAction_, [this]() {
        setRepeatMode(QStringLiteral("playlist"), true);
    });
    bindAction(toggleCompareViewAction_, [this]() {
        toggleCompareView();
    });
    bindAction(showMediaInformationOverlayAction_, [this]() {
        showMediaInformationOverlay();
        updateActionStates();
    });

    bindAction(previousPlaylistAction_, [this]() {
        navigatePlaylistWithResumeGuard(false);
    });
    bindAction(nextPlaylistAction_, [this]() {
        navigatePlaylistWithResumeGuard(true);
    });
    bindAction(previousChapterAction_, [this]() {
        playbackController_->previousChapter();
    });
    bindAction(nextChapterAction_, [this]() {
        playbackController_->nextChapter();
    });
    bindAction(seekBackwardShortAction_, [this]() {
        seekBySecondsWithFeedback(-shortSeekStepSeconds());
    });
    bindAction(seekForwardShortAction_, [this]() {
        seekBySecondsWithFeedback(shortSeekStepSeconds());
    });
    bindAction(seekBackwardLongAction_, [this]() {
        seekBySecondsWithFeedback(-longSeekStepSeconds());
    });
    bindAction(seekForwardLongAction_, [this]() {
        seekBySecondsWithFeedback(longSeekStepSeconds());
    });
    bindAction(volumeDownAction_, [this]() {
        adjustVolumeWithFeedback(-volumeStep());
    });
    bindAction(volumeUpAction_, [this]() {
        adjustVolumeWithFeedback(volumeStep());
    });
    bindAction(toggleMuteAction_, [this]() {
        const bool targetMuted = !muted_;
        playbackController_->setMuted(targetMuted);
        if (targetMuted) {
            statusBar()->showMessage(uiText("Audio muted"), 2500);
            showPlaybackFeedback(QStringLiteral("Mute"));
        } else {
            statusBar()->showMessage(uiText("Audio restored"), 2500);
            showVolumeFeedback(currentVolume_);
        }
    });
    bindAction(videoZoomOutAction_, [this]() {
        adjustVideoZoomFactor(-videoZoomStepSetting());
    });
    bindAction(videoZoomInAction_, [this]() {
        adjustVideoZoomFactor(videoZoomStepSetting());
    });
    bindAction(videoZoomResetAction_, [this]() {
        resetVideoZoomAndPan(true);
    });
    bindAction(videoPanLeftAction_, [this]() {
        adjustVideoAlignment(-keyboardVideoPanStep(videoPanSensitivitySetting()), 0.0, true);
    });
    bindAction(videoPanRightAction_, [this]() {
        adjustVideoAlignment(keyboardVideoPanStep(videoPanSensitivitySetting()), 0.0, true);
    });
    bindAction(videoPanUpAction_, [this]() {
        adjustVideoAlignment(0.0, -keyboardVideoPanStep(videoPanSensitivitySetting()), true);
    });
    bindAction(videoPanDownAction_, [this]() {
        adjustVideoAlignment(0.0, keyboardVideoPanStep(videoPanSensitivitySetting()), true);
    });
    bindAction(speedDownAction_, [this]() {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(currentSpeed_ - 0.10);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
    });
    bindAction(speedUpAction_, [this]() {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(currentSpeed_ + 0.10);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
    });
    bindAction(speedResetAction_, [this]() {
        playbackController_->resetSpeed();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->resetSpeed();
        }
        showPlaybackSpeedFeedback(1.0, true);
    });
    bindAction(subtitleDelayDownAction_, [this]() {
        adjustSubtitleDelayWithFeedback(-subtitleSyncSmallStep());
    });
    bindAction(subtitleDelayUpAction_, [this]() {
        adjustSubtitleDelayWithFeedback(subtitleSyncSmallStep());
    });
    bindAction(subtitleDelayManualAction_, [this]() {
        showManualSubtitleDelayDialog();
    });
    bindAction(subtitleDelayResetAction_, [this]() {
        resetSubtitleDelayWithFeedback();
    });
    bindAction(subtitleScaleDownAction_, [this]() {
        const double targetScale = revaplayer::application::clampSubtitleScale(currentSubtitleScale_ - 0.10);
        playbackController_->setSubtitleScale(targetScale);
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitleScale(targetScale);
        }
        statusBar()->showMessage(uiText("Subtitle scale: %1x").arg(QString::number(targetScale, 'f', 2)), 2500);
    });
    bindAction(subtitleScaleUpAction_, [this]() {
        const double targetScale = revaplayer::application::clampSubtitleScale(currentSubtitleScale_ + 0.10);
        playbackController_->setSubtitleScale(targetScale);
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitleScale(targetScale);
        }
        statusBar()->showMessage(uiText("Subtitle scale: %1x").arg(QString::number(targetScale, 'f', 2)), 2500);
    });
    bindAction(subtitleScaleResetAction_, [this]() {
        playbackController_->resetSubtitleScale();
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitleScale(1.0);
        }
        statusBar()->showMessage(uiText("Subtitle scale reset"), 2500);
    });
    bindAction(subtitlePositionUpAction_, [this]() {
        const int targetPosition = revaplayer::application::clampSubtitlePosition(currentSubtitlePosition_ - 5);
        playbackController_->setSubtitlePosition(targetPosition);
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitlePosition(targetPosition);
        }
        statusBar()->showMessage(uiText("Subtitle position: %1%").arg(targetPosition), 2500);
    });
    bindAction(subtitlePositionDownAction_, [this]() {
        const int targetPosition = revaplayer::application::clampSubtitlePosition(currentSubtitlePosition_ + 5);
        playbackController_->setSubtitlePosition(targetPosition);
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitlePosition(targetPosition);
        }
        statusBar()->showMessage(uiText("Subtitle position: %1%").arg(targetPosition), 2500);
    });
    bindAction(subtitlePositionResetAction_, [this]() {
        playbackController_->resetSubtitlePosition();
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitlePosition(100);
        }
        statusBar()->showMessage(uiText("Subtitle position reset"), 2500);
    });
    bindAction(cycleSubtitleAssOverrideAction_, [this]() {
        const QString nextMode = revaplayer::application::nextSubtitleAssOverride(currentSubtitleAssOverride_);
        playbackController_->setSubtitleAssOverride(nextMode);
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitleAssOverride(nextMode);
        }
        statusBar()->showMessage(
            QStringLiteral("Subtitle style override: %1")
                .arg(revaplayer::application::subtitleAssOverrideLabel(nextMode)),
            3000);
    });
    bindAction(audioDelayDownAction_, [this]() {
        setAudioDelayWithFeedback(currentAudioDelaySeconds_ - 0.10);
    });
    bindAction(audioDelayUpAction_, [this]() {
        setAudioDelayWithFeedback(currentAudioDelaySeconds_ + 0.10);
    });
    bindAction(audioDelayManualAction_, [this]() {
        showManualAudioDelayDialog();
    });
    bindAction(audioDelayResetAction_, [this]() {
        setAudioDelayWithFeedback(0.0, true);
    });
}

void MainWindow::setupDockWidgets()
{
    playlistDock_ = new QDockWidget(uiText("Playlist"), this);
    playlistDock_->setObjectName(QStringLiteral("playlistDock"));
    playlistDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    playlistDock_->setMinimumWidth(220);

    auto *playlistContainer = new QWidget(playlistDock_);
    auto *playlistContainerLayout = new QVBoxLayout(playlistContainer);
    playlistContainerLayout->setContentsMargins(6, 6, 6, 6);
    playlistContainerLayout->setSpacing(6);

    playlistHeaderBar_ = new QWidget(playlistContainer);
    playlistHeaderBar_->setObjectName(QStringLiteral("playlistHeaderBar"));
    playlistHeaderBar_->hide();

    playlistCoursesRow_ = new QWidget(playlistContainer);
    playlistCoursesRow_->setObjectName(QStringLiteral("playlistCoursesRow"));
    auto *playlistCoursesLayout = new QHBoxLayout(playlistCoursesRow_);
    playlistCoursesLayout->setContentsMargins(4, 0, 4, 0);
    playlistCoursesLayout->setSpacing(8);

    pinnedCoursesScrollArea_ = new QScrollArea(playlistCoursesRow_);
    pinnedCoursesScrollArea_->setObjectName(QStringLiteral("pinnedCoursesScrollArea"));
    pinnedCoursesScrollArea_->setWidgetResizable(false);
    pinnedCoursesScrollArea_->setFrameShape(QFrame::NoFrame);
    pinnedCoursesScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pinnedCoursesScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pinnedCoursesScrollArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    pinnedCoursesTabBar_ = new QTabBar(pinnedCoursesScrollArea_);
    pinnedCoursesTabBar_->setObjectName(QStringLiteral("pinnedCoursesTabBar"));
    pinnedCoursesTabBar_->setDocumentMode(true);
    pinnedCoursesTabBar_->setMovable(true);
    pinnedCoursesTabBar_->setChangeCurrentOnDrag(false);
    pinnedCoursesTabBar_->setExpanding(false);
    pinnedCoursesTabBar_->setUsesScrollButtons(false);
    pinnedCoursesTabBar_->setElideMode(Qt::ElideRight);
    pinnedCoursesTabBar_->setContextMenuPolicy(Qt::CustomContextMenu);
    pinnedCoursesTabBar_->setDrawBase(false);
    pinnedCoursesTabBar_->setMouseTracking(true);
    pinnedCoursesTabBar_->setToolTip(uiText("Saved lists. Select to browse, double-click to open, or right-click to edit."));
    pinnedCoursesScrollArea_->setWidget(pinnedCoursesTabBar_);

    playlistPinCourseButton_ = new QToolButton(playlistCoursesRow_);
    playlistPinCourseButton_->setObjectName(QStringLiteral("playlistPinCourseButton"));
    playlistPinCourseButton_->setText(uiText("Save Folder"));
    playlistPinCourseButton_->setAutoRaise(true);
    playlistPinCourseButton_->setToolTip(uiText("Save the current media folder as a reusable named list"));

    playlistManageCoursesButton_ = new QToolButton(playlistCoursesRow_);
    playlistManageCoursesButton_->setObjectName(QStringLiteral("playlistManageCoursesButton"));
    playlistManageCoursesButton_->setText(uiText("Manage"));
    playlistManageCoursesButton_->setAutoRaise(true);
    playlistManageCoursesButton_->setToolTip(uiText("Rename, categorize, or remove saved list tabs"));

    playlistCoursesLayout->addWidget(pinnedCoursesScrollArea_, 1);
    playlistCoursesLayout->addWidget(playlistPinCourseButton_, 0);
    playlistCoursesLayout->addWidget(playlistManageCoursesButton_, 0);

    auto *playlistSearchRow = new QWidget(playlistContainer);
    playlistSearchRow->setObjectName(QStringLiteral("playlistSearchRow"));
    auto *playlistSearchLayout = new QHBoxLayout(playlistSearchRow);
    playlistSearchLayout->setContentsMargins(10, 5, 10, 5);
    playlistSearchLayout->setSpacing(8);

    auto *playlistSearchShell = new QWidget(playlistSearchRow);
    playlistSearchShell->setObjectName(QStringLiteral("playlistSearchShell"));
    auto *playlistSearchShellLayout = new QHBoxLayout(playlistSearchShell);
    playlistSearchShellLayout->setContentsMargins(12, 0, 12, 0);
    playlistSearchShellLayout->setSpacing(8);

    auto *playlistSearchGlyph = new QLabel(QStringLiteral("⌕"), playlistSearchShell);
    playlistSearchGlyph->setObjectName(QStringLiteral("playlistSearchGlyph"));

    playlistSearchEdit_ = new QLineEdit(playlistSearchShell);
    playlistSearchEdit_->setObjectName(QStringLiteral("playlistSearchEdit"));
    playlistSearchEdit_->setPlaceholderText(uiText("Search title, path, or number"));
    playlistSearchEdit_->setClearButtonEnabled(true);
    playlistSearchEdit_->setFrame(false);

    playlistSearchShellLayout->addWidget(playlistSearchGlyph, 0);
    playlistSearchShellLayout->addWidget(playlistSearchEdit_, 1);

    playlistSummaryLabel_ = new QLabel(uiText("0 items"), playlistSearchRow);
    playlistSummaryLabel_->setObjectName(QStringLiteral("playlistSummaryLabel"));

    playlistRefreshButton_ = new QToolButton(playlistSearchRow);
    playlistRefreshButton_->setObjectName(QStringLiteral("playlistScanButton"));
    playlistRefreshButton_->setText(QStringLiteral("⟳"));
    playlistRefreshButton_->setAutoRaise(true);
    playlistRefreshButton_->setToolTip(uiText("Rescan metadata for every playlist item"));
    playlistRefreshButton_->setEnabled(false);

    playlistPanelSettingsButton_ = new QToolButton(playlistSearchRow);
    playlistPanelSettingsButton_->setObjectName(QStringLiteral("playlistPanelSettingsButton"));
    playlistPanelSettingsButton_->setText(QStringLiteral("⚙"));
    playlistPanelSettingsButton_->setAutoRaise(true);
    playlistPanelSettingsButton_->setToolTip(uiText("Settings"));

    playlistSearchLayout->addWidget(playlistSearchShell, 1);
    playlistSearchLayout->addWidget(playlistSummaryLabel_, 0);
    playlistSearchLayout->addWidget(playlistRefreshButton_, 0);
    playlistSearchLayout->addWidget(playlistPanelSettingsButton_, 0);

    playlistProgressRow_ = new QWidget(playlistContainer);
    playlistProgressRow_->setObjectName(QStringLiteral("playlistProgressRow"));
    auto *playlistProgressLayout = new QHBoxLayout(playlistProgressRow_);
    playlistProgressLayout->setContentsMargins(8, 0, 8, 2);
    playlistProgressLayout->setSpacing(8);

    playlistProgressTextLabel_ = new QLabel(uiText("List %1%").arg(0), playlistProgressRow_);
    playlistProgressTextLabel_->setObjectName(QStringLiteral("playlistProgressTextLabel"));

    playlistAggregateProgressBar_ = new QProgressBar(playlistProgressRow_);
    playlistAggregateProgressBar_->setObjectName(QStringLiteral("playlistAggregateProgressBar"));
    playlistAggregateProgressBar_->setRange(0, 100);
    playlistAggregateProgressBar_->setValue(0);
    playlistAggregateProgressBar_->setTextVisible(false);

    playlistTotalDurationLabel_ = new QLabel(uiText("Duration: %1").arg(uiText("Unknown")), playlistProgressRow_);
    playlistTotalDurationLabel_->setObjectName(QStringLiteral("playlistTotalDurationLabel"));

    playlistProgressLayout->addWidget(playlistProgressTextLabel_, 0);
    playlistProgressLayout->addWidget(playlistAggregateProgressBar_, 1);
    playlistProgressLayout->addWidget(playlistTotalDurationLabel_, 0);

    playlistFilterModel_ = new PlaylistFilterProxyModel(playlistContainer);
    playlistFilterModel_->setSourceModel(playlistController_->model());
    playlistFilterModel_->setFilterKeyColumn(0);
    playlistFilterModel_->setFilterRole(Qt::UserRole + 2);
    playlistFilterModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto *deferredPlaylistView = new DeferredDragListView(playlistContainer);
    playlistView_ = deferredPlaylistView;
    playlistView_->setObjectName(QStringLiteral("playlistView"));
    playlistView_->setModel(playlistFilterModel_);
    playlistView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playlistView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    playlistView_->setContextMenuPolicy(Qt::CustomContextMenu);
    playlistView_->setDragEnabled(true);
    playlistView_->setAcceptDrops(true);
    playlistView_->setDropIndicatorShown(false);
    playlistView_->setDefaultDropAction(Qt::MoveAction);
    playlistView_->setDragDropMode(QAbstractItemView::InternalMove);
    playlistView_->setDragDropOverwriteMode(false);
    playlistView_->setAutoScroll(true);
    playlistView_->setAutoScrollMargin(48);
    playlistView_->setUniformItemSizes(true);
    playlistView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    playlistView_->setMouseTracking(true);
    playlistView_->setProperty("playlistProgressModeEnabled", false);
    playlistView_->setProperty("playlistCardZoom", playlistCardZoomId());
    playlistView_->setProperty("playlistThumbnailShape", playlistThumbnailShapeId());
    playlistView_->setItemDelegate(new PlaylistItemDelegate(playlistView_));
    deferredPlaylistView->setActivationHandler([this](const QModelIndex &index) {
        onPlaylistActivated(index);
    });
    deferredPlaylistView->setManualReorderHandler([this](const QVector<int> &orderedIndices) {
        applyRequestedPlaylistOrder(orderedIndices);
    });

    playlistContainerLayout->addWidget(playlistCoursesRow_, 0);
    playlistContainerLayout->addWidget(playlistSearchRow, 0);
    playlistContainerLayout->addWidget(playlistProgressRow_, 0);
    playlistContainerLayout->addWidget(playlistView_, 1);
    playlistDock_->setWidget(playlistContainer);
    playlistResizeHandle_ = new QWidget(playlistDock_);
    playlistResizeHandle_->setObjectName(QStringLiteral("overlayPanelResizeHandle"));
    playlistResizeHandle_->setAttribute(Qt::WA_StyledBackground, true);
    playlistResizeHandle_->setCursor(Qt::SizeHorCursor);
    playlistResizeHandle_->hide();
    addDockWidget(Qt::RightDockWidgetArea, playlistDock_);

    detailsDock_ = new QDockWidget(uiText("Details"), this);
    detailsDock_->setObjectName(QStringLiteral("detailsDock"));
    detailsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    detailsDock_->setMinimumWidth(220);

    auto *detailsContainer = new QWidget(detailsDock_);
    detailsContainer->setObjectName(QStringLiteral("detailsContainer"));
    detailsContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *detailsContainerLayout = new QVBoxLayout(detailsContainer);
    detailsContainerLayout->setContentsMargins(6, 6, 6, 6);
    detailsContainerLayout->setSpacing(6);

    auto *detailsTabsHeader = new QWidget(detailsContainer);
    detailsTabsHeader->setObjectName(QStringLiteral("detailsTabsHeader"));
    auto *detailsTabsHeaderLayout = new QHBoxLayout(detailsTabsHeader);
    detailsTabsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    detailsTabsHeaderLayout->setSpacing(8);

    detailsTabsStripScrollArea_ = new QScrollArea(detailsTabsHeader);
    detailsTabsStripScrollArea_->setObjectName(QStringLiteral("detailsTabsStripScrollArea"));
    detailsTabsStripScrollArea_->setWidgetResizable(false);
    detailsTabsStripScrollArea_->setFrameShape(QFrame::NoFrame);
    detailsTabsStripScrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    detailsTabsStripScrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    detailsTabsStripContainer_ = new QWidget(detailsTabsStripScrollArea_);
    detailsTabsStripContainer_->setObjectName(QStringLiteral("detailsTabsStripContainer"));
    detailsTabsStripLayout_ = new QHBoxLayout(detailsTabsStripContainer_);
    detailsTabsStripLayout_->setContentsMargins(0, 0, 0, 0);
    detailsTabsStripLayout_->setSpacing(8);
    detailsTabsStripScrollArea_->setWidget(detailsTabsStripContainer_);

    detailsPanelSettingsButton_ = new QToolButton(detailsTabsHeader);
    detailsPanelSettingsButton_->setObjectName(QStringLiteral("detailsPanelSettingsButton"));
    detailsPanelSettingsButton_->setText(QStringLiteral("⚙"));
    detailsPanelSettingsButton_->setAutoRaise(true);
    detailsPanelSettingsButton_->setToolTip(uiText("Settings"));
    detailsPanelSettingsButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    detailsPanelSettingsButton_->setMinimumWidth(44);

    detailsTabsHeaderLayout->addWidget(detailsTabsStripScrollArea_, 0);
    detailsTabsHeaderLayout->addWidget(detailsPanelSettingsButton_, 0);

    detailsTabs_ = new QTabWidget(detailsContainer);
    detailsTabs_->setObjectName(QStringLiteral("detailsTabs"));
    detailsTabs_->setDocumentMode(true);
    detailsTabs_->setUsesScrollButtons(true);
    bookmarksPage_ = new QWidget(detailsTabs_);
    bookmarksPage_->setObjectName(QStringLiteral("detailsContentPage"));
    bookmarksPage_->setAttribute(Qt::WA_StyledBackground, true);
    auto *bookmarksLayout = new QVBoxLayout(bookmarksPage_);
    bookmarksLayout->setContentsMargins(8, 8, 8, 8);
    bookmarksLayout->setSpacing(8);

    auto *bookmarksHeader = new QWidget(bookmarksPage_);
    bookmarksHeader->setObjectName(QStringLiteral("bookmarksHeaderBar"));
    auto *bookmarksHeaderLayout = new QHBoxLayout(bookmarksHeader);
    bookmarksHeaderLayout->setContentsMargins(8, 6, 8, 6);
    bookmarksHeaderLayout->setSpacing(8);

    bookmarkAddButton_ = new QToolButton(bookmarksHeader);
    bookmarkAddButton_->setText(uiText("Add"));
    bookmarkAddButton_->setAutoRaise(true);

    bookmarkImportButton_ = new QToolButton(bookmarksHeader);
    bookmarkImportButton_->setText(uiText("Import"));
    bookmarkImportButton_->setAutoRaise(true);

    bookmarkExportButton_ = new QToolButton(bookmarksHeader);
    bookmarkExportButton_->setText(uiText("Export"));
    bookmarkExportButton_->setAutoRaise(true);

    bookmarkJumpButton_ = new QToolButton(bookmarksHeader);
    bookmarkJumpButton_->setText(uiText("Jump"));
    bookmarkJumpButton_->setAutoRaise(true);
    bookmarkJumpButton_->setToolTip(uiText("Seek directly to the selected bookmark time"));
    bookmarkJumpButton_->setEnabled(false);

    bookmarkQuizCsvButton_ = new QToolButton(bookmarksHeader);
    bookmarkQuizCsvButton_->setText(uiText("Quiz CSV"));
    bookmarkQuizCsvButton_->setAutoRaise(true);
    bookmarkQuizCsvButton_->setToolTip(uiText("Export bookmarks as CSV flashcards for spreadsheets or quiz tools"));

    bookmarkDeleteButton_ = new QToolButton(bookmarksHeader);
    bookmarkDeleteButton_->setText(uiText("Delete"));
    bookmarkDeleteButton_->setAutoRaise(true);

    bookmarkCategoryComboBox_ = new QComboBox(bookmarksHeader);
    bookmarkCategoryComboBox_->addItem(uiText("All Categories"), QStringLiteral("*"));

    bookmarkSearchEdit_ = new QLineEdit(bookmarksHeader);
    bookmarkSearchEdit_->setPlaceholderText(uiText("Search bookmarks, notes, categories, or times"));

    bookmarksHeaderLayout->addWidget(bookmarkAddButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkImportButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkExportButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkJumpButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkQuizCsvButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkDeleteButton_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkCategoryComboBox_, 0);
    bookmarksHeaderLayout->addWidget(bookmarkSearchEdit_, 1);

    bookmarksList_ = new QListWidget(bookmarksPage_);
    bookmarksList_->setObjectName(QStringLiteral("bookmarksList"));
    bookmarksList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    bookmarksList_->setSpacing(6);
    bookmarksList_->setIconSize(QSize(128, 72));
    bookmarksList_->setItemDelegate(new BookmarkItemDelegate(bookmarksList_));
    chaptersList_ = new QListWidget(detailsTabs_);
    tracksTree_ = new QTreeWidget(detailsTabs_);
    tracksTree_->setHeaderHidden(true);
    tracksTree_->setRootIsDecorated(true);
    bookmarksList_->setContextMenuPolicy(Qt::CustomContextMenu);
    bookmarksList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    bookmarksLayout->addWidget(bookmarksHeader, 0);
    bookmarksLayout->addWidget(bookmarksList_, 1);

    auto *historyPage = new QWidget(detailsTabs_);
    historyPage->setObjectName(QStringLiteral("detailsContentPage"));
    historyPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *historyLayout = new QVBoxLayout(historyPage);
    historyLayout->setContentsMargins(8, 8, 8, 8);
    historyLayout->setSpacing(8);

    auto *historyHeader = new QWidget(historyPage);
    historyHeader->setObjectName(QStringLiteral("historyHeaderBar"));
    auto *historyHeaderLayout = new QHBoxLayout(historyHeader);
    historyHeaderLayout->setContentsMargins(8, 6, 8, 6);
    historyHeaderLayout->setSpacing(8);

    historyRefreshButton_ = new QToolButton(historyHeader);
    historyRefreshButton_->setText(uiText("Refresh"));
    historyRefreshButton_->setAutoRaise(true);

    historyClearButton_ = new QToolButton(historyHeader);
    historyClearButton_->setText(uiText("Clear"));
    historyClearButton_->setAutoRaise(true);

    historySearchEdit_ = new QLineEdit(historyHeader);
    historySearchEdit_->setPlaceholderText(uiText("Filter recent files, URLs, or titles"));

    historySummaryLabel_ = new QLabel(uiText("No history yet"), historyHeader);
    historySummaryLabel_->setObjectName(QStringLiteral("historySummaryLabel"));

    historyHeaderLayout->addWidget(historyRefreshButton_, 0);
    historyHeaderLayout->addWidget(historyClearButton_, 0);
    historyHeaderLayout->addWidget(historySearchEdit_, 1);
    historyHeaderLayout->addWidget(historySummaryLabel_, 0);

    historyList_ = new QListWidget(historyPage);
    historyList_->setObjectName(QStringLiteral("historyList"));
    historyList_->setSelectionMode(QAbstractItemView::SingleSelection);
    historyList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    historyLayout->addWidget(historyHeader, 0);
    historyLayout->addWidget(historyList_, 1);

    favoritesPage_ = new QWidget(detailsTabs_);
    favoritesPage_->setObjectName(QStringLiteral("detailsContentPage"));
    favoritesPage_->setAttribute(Qt::WA_StyledBackground, true);
    auto *favoritesLayout = new QVBoxLayout(favoritesPage_);
    favoritesLayout->setContentsMargins(8, 8, 8, 8);
    favoritesLayout->setSpacing(8);

    auto *favoritesHeader = new QWidget(favoritesPage_);
    favoritesHeader->setObjectName(QStringLiteral("historyHeaderBar"));
    auto *favoritesHeaderLayout = new QHBoxLayout(favoritesHeader);
    favoritesHeaderLayout->setContentsMargins(8, 6, 8, 6);
    favoritesHeaderLayout->setSpacing(8);

    favoritesCurrentButton_ = new QToolButton(favoritesHeader);
    favoritesCurrentButton_->setText(uiText("Current"));
    favoritesCurrentButton_->setAutoRaise(true);
    favoritesCurrentButton_->setCheckable(true);

    favoritesAddButton_ = new QToolButton(favoritesHeader);
    favoritesAddButton_->setText(uiText("Add File"));
    favoritesAddButton_->setAutoRaise(true);

    favoritesOpenButton_ = new QToolButton(favoritesHeader);
    favoritesOpenButton_->setText(uiText("Open"));
    favoritesOpenButton_->setAutoRaise(true);

    favoritesOpenAllButton_ = new QToolButton(favoritesHeader);
    favoritesOpenAllButton_->setText(uiText("Open All"));
    favoritesOpenAllButton_->setAutoRaise(true);

    favoritesRemoveButton_ = new QToolButton(favoritesHeader);
    favoritesRemoveButton_->setText(uiText("Remove"));
    favoritesRemoveButton_->setAutoRaise(true);

    favoritesRefreshButton_ = new QToolButton(favoritesHeader);
    favoritesRefreshButton_->setText(uiText("Refresh"));
    favoritesRefreshButton_->setAutoRaise(true);

    favoritesSearchEdit_ = new QLineEdit(favoritesHeader);
    favoritesSearchEdit_->setPlaceholderText(uiText("Search favorite videos or paths"));

    favoritesHeaderLayout->addWidget(favoritesCurrentButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesAddButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesOpenButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesOpenAllButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesRemoveButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesRefreshButton_, 0);
    favoritesHeaderLayout->addWidget(favoritesSearchEdit_, 1);

    favoritesList_ = new QListWidget(favoritesPage_);
    favoritesList_->setObjectName(QStringLiteral("favoritesList"));
    favoritesList_->setSelectionMode(QAbstractItemView::SingleSelection);
    favoritesList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    favoritesList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    favoritesList_->setSpacing(6);
    favoritesList_->setIconSize(QSize(128, 72));
    favoritesList_->setItemDelegate(new FavoriteItemDelegate(favoritesList_));

    favoritesLayout->addWidget(favoritesHeader, 0);
    favoritesLayout->addWidget(favoritesList_, 1);

    auto *scenePage = new QWidget(detailsTabs_);
    scenePage->setObjectName(QStringLiteral("detailsContentPage"));
    scenePage->setAttribute(Qt::WA_StyledBackground, true);
    auto *sceneLayout = new QVBoxLayout(scenePage);
    sceneLayout->setContentsMargins(8, 8, 8, 8);
    sceneLayout->setSpacing(8);

    auto *sceneHeader = new QWidget(scenePage);
    sceneHeader->setObjectName(QStringLiteral("sceneBrowserHeader"));
    auto *sceneHeaderLayout = new QHBoxLayout(sceneHeader);
    sceneHeaderLayout->setContentsMargins(8, 6, 8, 6);
    sceneHeaderLayout->setSpacing(8);

    sceneRefreshButton_ = new QToolButton(sceneHeader);
    sceneRefreshButton_->setText(uiText("Refresh"));
    sceneRefreshButton_->setAutoRaise(true);

    sceneBookmarkButton_ = new QToolButton(sceneHeader);
    sceneBookmarkButton_->setText(uiText("Bookmark"));
    sceneBookmarkButton_->setAutoRaise(true);

    sceneExportButton_ = new QToolButton(sceneHeader);
    sceneExportButton_->setText(uiText("Export Images"));
    sceneExportButton_->setAutoRaise(true);

    sceneStepSpinBox_ = new QSpinBox(sceneHeader);
    sceneStepSpinBox_->setRange(1, 600);
    sceneStepSpinBox_->setSingleStep(1);
    sceneStepSpinBox_->setSuffix(QStringLiteral(" s"));
    sceneStepSpinBox_->setValue(sceneBrowserStepSeconds());

    sceneSearchEdit_ = new QLineEdit(sceneHeader);
    sceneSearchEdit_->setPlaceholderText(uiText("Filter scenes by time"));

    sceneStatusLabel_ = new QLabel(uiText("Load a local video to browse scenes"), sceneHeader);
    sceneStatusLabel_->setObjectName(QStringLiteral("sceneStatusLabel"));

    sceneHeaderLayout->addWidget(sceneRefreshButton_, 0);
    sceneHeaderLayout->addWidget(sceneBookmarkButton_, 0);
    sceneHeaderLayout->addWidget(sceneExportButton_, 0);
    sceneHeaderLayout->addWidget(sceneStepSpinBox_, 0);
    sceneHeaderLayout->addWidget(sceneSearchEdit_, 1);
    sceneHeaderLayout->addWidget(sceneStatusLabel_, 1);

    sceneList_ = new QListWidget(scenePage);
    sceneList_->setObjectName(QStringLiteral("sceneBrowserList"));
    sceneList_->setViewMode(QListView::IconMode);
    sceneList_->setResizeMode(QListView::Adjust);
    sceneList_->setMovement(QListView::Static);
    sceneList_->setSpacing(10);
    sceneList_->setWrapping(true);
    sceneList_->setWordWrap(true);
    sceneList_->setIconSize(QSize(168, 94));
    sceneList_->setGridSize(QSize(186, 136));
    sceneList_->setSelectionMode(QAbstractItemView::SingleSelection);
    sceneList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    sceneLayout->addWidget(sceneHeader, 0);
    sceneLayout->addWidget(sceneList_, 1);

    auto *mediaLabScrollArea = new QScrollArea(detailsTabs_);
    mediaLabScrollArea->setWidgetResizable(true);
    mediaLabScrollArea->setFrameShape(QFrame::NoFrame);

    auto *mediaLabPage = new QWidget(mediaLabScrollArea);
    mediaLabPage->setObjectName(QStringLiteral("mediaLabPage"));
    mediaLabPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *mediaLabLayout = new QVBoxLayout(mediaLabPage);
    mediaLabLayout->setContentsMargins(10, 10, 10, 10);
    mediaLabLayout->setSpacing(12);

    auto *secondaryGroup = new QGroupBox(uiText("Secondary Subtitles"), mediaLabPage);
    auto *secondaryLayout = new QVBoxLayout(secondaryGroup);
    auto *secondaryForm = new QFormLayout();
    secondarySubtitleCombo_ = new QComboBox(secondaryGroup);
    secondaryForm->addRow(uiText("Secondary track"), secondarySubtitleCombo_);
    secondaryLayout->addLayout(secondaryForm);
    auto *secondaryButtonRow = new QWidget(secondaryGroup);
    auto *secondaryButtonLayout = new QHBoxLayout(secondaryButtonRow);
    secondaryButtonLayout->setContentsMargins(0, 0, 0, 0);
    secondaryButtonLayout->setSpacing(8);
    auto *loadExtraSubtitleButton = new QPushButton(uiText("Load External Subtitle"), secondaryButtonRow);
    auto *showTrackPanelButton = new QPushButton(uiText("Show Tracks"), secondaryButtonRow);
    secondaryButtonLayout->addWidget(loadExtraSubtitleButton, 0);
    secondaryButtonLayout->addWidget(showTrackPanelButton, 0);
    secondaryButtonLayout->addStretch(1);
    secondaryLayout->addWidget(secondaryButtonRow);
    auto *secondaryHintLabel = new QLabel(
        uiText("Load more subtitle files, then pick one here as the secondary subtitle layer."),
        secondaryGroup);
    secondaryHintLabel->setWordWrap(true);
    secondaryLayout->addWidget(secondaryHintLabel);

    auto *subtitleWorkflowGroup = new QGroupBox(uiText("Subtitle Workflow"), mediaLabPage);
    auto *subtitleWorkflowLayout = new QVBoxLayout(subtitleWorkflowGroup);
    auto *subtitleWorkflowButtons = new QWidget(subtitleWorkflowGroup);
    auto *subtitleWorkflowButtonsLayout = new QHBoxLayout(subtitleWorkflowButtons);
    subtitleWorkflowButtonsLayout->setContentsMargins(0, 0, 0, 0);
    subtitleWorkflowButtonsLayout->setSpacing(8);
    auto *subtitleAutoSelectButton = new QPushButton(uiText("Auto Select"), subtitleWorkflowButtons);
    auto *subtitleDownloadButton = new QPushButton(uiText("Download"), subtitleWorkflowButtons);
    auto *subtitleDelayBackLargeButton = new QPushButton(uiText("Delay -1s"), subtitleWorkflowButtons);
    auto *subtitleDelayBackMediumButton = new QPushButton(uiText("Delay -0.5s"), subtitleWorkflowButtons);
    auto *subtitleDelayBackSmallButton = new QPushButton(uiText("Delay -0.1s"), subtitleWorkflowButtons);
    auto *subtitleDelayForwardSmallButton = new QPushButton(uiText("Delay +0.1s"), subtitleWorkflowButtons);
    auto *subtitleDelayForwardMediumButton = new QPushButton(uiText("Delay +0.5s"), subtitleWorkflowButtons);
    auto *subtitleDelayForwardLargeButton = new QPushButton(uiText("Delay +1s"), subtitleWorkflowButtons);
    auto *subtitleDelayResetButton = new QPushButton(uiText("Reset subtitle delay to 0"), subtitleWorkflowButtons);
    subtitleWorkflowButtonsLayout->addWidget(subtitleAutoSelectButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDownloadButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayBackLargeButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayBackMediumButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayBackSmallButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayForwardSmallButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayForwardMediumButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayForwardLargeButton, 0);
    subtitleWorkflowButtonsLayout->addWidget(subtitleDelayResetButton, 0);
    subtitleWorkflowButtonsLayout->addStretch(1);

    auto *subtitleWorkflowForm = new QFormLayout();
    auto *subtitleDelayCustomRow = new QWidget(subtitleWorkflowGroup);
    auto *subtitleDelayCustomRowLayout = new QHBoxLayout(subtitleDelayCustomRow);
    subtitleDelayCustomRowLayout->setContentsMargins(0, 0, 0, 0);
    subtitleDelayCustomRowLayout->setSpacing(8);
    subtitleDelayCustomSpinBox_ = new QDoubleSpinBox(subtitleDelayCustomRow);
    subtitleDelayCustomSpinBox_->setRange(-3600.0, 3600.0);
    subtitleDelayCustomSpinBox_->setDecimals(2);
    subtitleDelayCustomSpinBox_->setSingleStep(0.05);
    subtitleDelayCustomSpinBox_->setValue(0.0);
    subtitleDelayCustomSpinBox_->setSuffix(QStringLiteral(" s"));
    auto *subtitleDelayApplyButton = new QPushButton(uiText("Apply Delay"), subtitleDelayCustomRow);
    subtitleDelayCustomRowLayout->addWidget(subtitleDelayCustomSpinBox_, 1);
    subtitleDelayCustomRowLayout->addWidget(subtitleDelayApplyButton, 0);
    subtitleWorkflowForm->addRow(uiText("Subtitle delay"), subtitleDelayCustomRow);

    subtitleRememberDelayForMediaCheckBox_ = new QCheckBox(uiText("Remember subtitle delay for this media"), subtitleWorkflowGroup);
    subtitleWorkflowForm->addRow(subtitleRememberDelayForMediaCheckBox_);

    auto *subtitleLanguageHintLabel = new QLabel(uiText("Preferred languages"), subtitleWorkflowGroup);
    auto *subtitleLanguageHintRow = new QWidget(subtitleWorkflowGroup);
    auto *subtitleLanguageHintLayout = new QHBoxLayout(subtitleLanguageHintRow);
    subtitleLanguageHintLayout->setContentsMargins(0, 0, 0, 0);
    subtitleLanguageHintLayout->setSpacing(8);
    subtitleLanguageHintEdit_ = new QLineEdit(subtitleLanguageHintRow);
    subtitleLanguageHintEdit_->setPlaceholderText(QStringLiteral("ar,en,ja"));
    auto *applySubtitleLanguageHintsButton = new QPushButton(uiText("Apply"), subtitleLanguageHintRow);
    subtitleLanguageHintLayout->addWidget(subtitleLanguageHintEdit_, 1);
    subtitleLanguageHintLayout->addWidget(applySubtitleLanguageHintsButton, 0);
    subtitleWorkflowForm->addRow(subtitleLanguageHintLabel, subtitleLanguageHintRow);
    subtitleLanguageHintLabel->setVisible(false);
    subtitleLanguageHintRow->setVisible(false);

    subtitleAutomationStatusLabel_ = new QLabel(uiText("Automatic subtitle selection is ready."), subtitleWorkflowGroup);
    subtitleAutomationStatusLabel_->setWordWrap(true);

    subtitleWorkflowLayout->addLayout(subtitleWorkflowForm);
    subtitleWorkflowLayout->addWidget(subtitleWorkflowButtons);
    subtitleWorkflowLayout->addWidget(subtitleAutomationStatusLabel_);

    auto *audioGroup = new QGroupBox(uiText("Equalizer & Audio Filters"), mediaLabPage);
    auto *audioLayout = new QVBoxLayout(audioGroup);
    auto *eqGrid = new QGridLayout();
    eqGrid->setHorizontalSpacing(10);
    eqGrid->setVerticalSpacing(6);

    equalizerSliders_.clear();
    equalizerSliders_.reserve(static_cast<int>(kEqualizerFrequencies.size()));
    for (qsizetype index = 0; index < static_cast<qsizetype>(kEqualizerFrequencies.size()); ++index) {
        auto *freqLabel = new QLabel(QStringLiteral("%1 Hz").arg(kEqualizerFrequencies[static_cast<size_t>(index)]), audioGroup);
        auto *slider = new QSlider(Qt::Horizontal, audioGroup);
        slider->setRange(-12, 12);
        slider->setTickInterval(3);
        slider->setSingleStep(1);
        slider->setPageStep(3);
        slider->setValue(0);
        auto *valueLabel = new QLabel(QStringLiteral("0 dB"), audioGroup);
        valueLabel->setMinimumWidth(44);
        eqGrid->addWidget(freqLabel, static_cast<int>(index), 0);
        eqGrid->addWidget(slider, static_cast<int>(index), 1);
        eqGrid->addWidget(valueLabel, static_cast<int>(index), 2);
        equalizerSliders_.push_back(slider);
        connect(slider, &QSlider::valueChanged, this, [this, valueLabel](const int value) {
            valueLabel->setText(QStringLiteral("%1%2 dB").arg(value > 0 ? QStringLiteral("+") : QStringLiteral("")).arg(value));
            applyAudioFilterState();
        });
    }
    audioLayout->addLayout(eqGrid);

    audioNormalizeCheckBox_ = new QCheckBox(uiText("Normalize loudness (night-friendly dialogue balance)"), audioGroup);
    audioLayout->addWidget(audioNormalizeCheckBox_);

    auto *audioPresetRow = new QWidget(audioGroup);
    auto *audioPresetLayout = new QHBoxLayout(audioPresetRow);
    audioPresetLayout->setContentsMargins(0, 0, 0, 0);
    audioPresetLayout->setSpacing(8);
    auto *resetEqButton = new QPushButton(uiText("Reset EQ"), audioPresetRow);
    auto *bassBoostButton = new QPushButton(uiText("Bass Boost"), audioPresetRow);
    auto *voiceBoostButton = new QPushButton(uiText("Voice Boost"), audioPresetRow);
    audioPresetLayout->addWidget(resetEqButton, 0);
    audioPresetLayout->addWidget(bassBoostButton, 0);
    audioPresetLayout->addWidget(voiceBoostButton, 0);
    audioPresetLayout->addStretch(1);
    audioLayout->addWidget(audioPresetRow);

    auto *customAudioRow = new QWidget(audioGroup);
    auto *customAudioLayout = new QHBoxLayout(customAudioRow);
    customAudioLayout->setContentsMargins(0, 0, 0, 0);
    customAudioLayout->setSpacing(8);
    customAudioFilterEdit_ = new QLineEdit(customAudioRow);
    customAudioFilterEdit_->setPlaceholderText(uiText("Optional mpv af chain, for example lavfi=[aresample=48000]"));
    auto *applyCustomAudioButton = new QPushButton(uiText("Apply"), customAudioRow);
    auto *clearCustomAudioButton = new QPushButton(uiText("Clear"), customAudioRow);
    customAudioLayout->addWidget(customAudioFilterEdit_, 1);
    customAudioLayout->addWidget(applyCustomAudioButton, 0);
    customAudioLayout->addWidget(clearCustomAudioButton, 0);
    audioLayout->addWidget(customAudioRow);

    auto *videoGroup = new QGroupBox(uiText("Video Filters / Stereo / 3D"), mediaLabPage);
    auto *videoLayout = new QFormLayout(videoGroup);
    const auto addVideoSliderRow = [this, videoGroup, videoLayout](const QString &label, QSlider **target) {
        auto *rowWidget = new QWidget(videoGroup);
        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto *slider = new QSlider(Qt::Horizontal, rowWidget);
        slider->setRange(-100, 100);
        slider->setValue(0);
        auto *valueLabel = new QLabel(QStringLiteral("0"), rowWidget);
        valueLabel->setMinimumWidth(32);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(valueLabel, 0);
        *target = slider;
        connect(slider, &QSlider::valueChanged, this, [this, valueLabel](const int value) {
            valueLabel->setText(QString::number(value));
            applyVideoFilterState();
        });
        videoLayout->addRow(label, rowWidget);
    };

    addVideoSliderRow(uiText("Brightness"), &videoBrightnessSlider_);
    addVideoSliderRow(uiText("Contrast"), &videoContrastSlider_);
    addVideoSliderRow(uiText("Saturation"), &videoSaturationSlider_);
    addVideoSliderRow(uiText("Gamma"), &videoGammaSlider_);
    addVideoSliderRow(uiText("Hue"), &videoHueSlider_);
    addVideoSliderRow(uiText("Sharpen"), &videoSharpenSlider_);
    addVideoSliderRow(uiText("Denoise"), &videoDenoiseSlider_);

    videoDebandCheckBox_ = new QCheckBox(uiText("Reduce banding with gradfun"), videoGroup);
    videoLayout->addRow(uiText("Deband"), videoDebandCheckBox_);

    videoDeinterlaceCheckBox_ = new QCheckBox(uiText("Force deinterlace"), videoGroup);
    videoLayout->addRow(uiText("Deinterlace"), videoDeinterlaceCheckBox_);

    auto *videoPresetRow = new QWidget(videoGroup);
    auto *videoPresetLayout = new QHBoxLayout(videoPresetRow);
    videoPresetLayout->setContentsMargins(0, 0, 0, 0);
    videoPresetLayout->setSpacing(8);
    auto *resetVideoButton = new QPushButton(uiText("Reset Tone"), videoPresetRow);
    auto *cinemaVideoButton = new QPushButton(uiText("Cinema"), videoPresetRow);
    auto *vividVideoButton = new QPushButton(uiText("Vivid"), videoPresetRow);
    videoPresetLayout->addWidget(resetVideoButton, 0);
    videoPresetLayout->addWidget(cinemaVideoButton, 0);
    videoPresetLayout->addWidget(vividVideoButton, 0);
    videoPresetLayout->addStretch(1);
    videoLayout->addRow(uiText("Presets"), videoPresetRow);

    stereo3dFilterEdit_ = new QLineEdit(videoGroup);
    stereo3dFilterEdit_->setPlaceholderText(uiText("Optional stereo3d filter, for example stereo3d=sbsl:arcd"));
    videoLayout->addRow(uiText("Stereo / 3D chain"), stereo3dFilterEdit_);

    auto *customVideoRow = new QWidget(videoGroup);
    auto *customVideoLayout = new QHBoxLayout(customVideoRow);
    customVideoLayout->setContentsMargins(0, 0, 0, 0);
    customVideoLayout->setSpacing(8);
    customVideoFilterEdit_ = new QLineEdit(customVideoRow);
    customVideoFilterEdit_->setPlaceholderText(uiText("Optional mpv vf chain, for example hqdn3d=1.5:1.5:6:6"));
    auto *applyCustomVideoButton = new QPushButton(uiText("Apply"), customVideoRow);
    auto *clearCustomVideoButton = new QPushButton(uiText("Clear"), customVideoRow);
    customVideoLayout->addWidget(customVideoFilterEdit_, 1);
    customVideoLayout->addWidget(applyCustomVideoButton, 0);
    customVideoLayout->addWidget(clearCustomVideoButton, 0);
    videoLayout->addRow(uiText("Custom filter chain"), customVideoRow);

    auto *shaderRow = new QWidget(videoGroup);
    auto *shaderLayout = new QHBoxLayout(shaderRow);
    shaderLayout->setContentsMargins(0, 0, 0, 0);
    shaderLayout->setSpacing(8);
    shaderPathEdit_ = new QLineEdit(shaderRow);
    shaderPathEdit_->setPlaceholderText(uiText("Optional GLSL shader path"));
    auto *browseShaderButton = new QPushButton(uiText("Browse"), shaderRow);
    auto *clearShaderButton = new QPushButton(uiText("Clear"), shaderRow);
    shaderLayout->addWidget(shaderPathEdit_, 1);
    shaderLayout->addWidget(browseShaderButton, 0);
    shaderLayout->addWidget(clearShaderButton, 0);
    videoLayout->addRow(uiText("Shader"), shaderRow);

    auto *profileGroup = new QGroupBox(uiText("Media Profiles"), mediaLabPage);
    auto *profileLayout = new QVBoxLayout(profileGroup);
    auto *profileButtonRow = new QWidget(profileGroup);
    auto *profileButtonLayout = new QHBoxLayout(profileButtonRow);
    profileButtonLayout->setContentsMargins(0, 0, 0, 0);
    profileButtonLayout->setSpacing(8);
    auto *saveFileProfileButton = new QPushButton(uiText("Save for This File"), profileButtonRow);
    auto *saveTypeProfileButton = new QPushButton(uiText("Save for This Type"), profileButtonRow);
    auto *clearFileProfileButton = new QPushButton(uiText("Clear File Profile"), profileButtonRow);
    auto *clearTypeProfileButton = new QPushButton(uiText("Clear Type Profile"), profileButtonRow);
    profileButtonLayout->addWidget(saveFileProfileButton, 0);
    profileButtonLayout->addWidget(saveTypeProfileButton, 0);
    profileButtonLayout->addWidget(clearFileProfileButton, 0);
    profileButtonLayout->addWidget(clearTypeProfileButton, 0);
    profileButtonLayout->addStretch(1);
    auto *profileHintLabel = new QLabel(
        uiText("Store the current media lab state per-file or by detected content type."),
        profileGroup);
    profileHintLabel->setWordWrap(true);
    profileLayout->addWidget(profileButtonRow);
    profileLayout->addWidget(profileHintLabel);

    mediaLabStatusLabel_ = new QLabel(uiText("Media Lab is ready. Changes apply to the current playback."), mediaLabPage);
    mediaLabStatusLabel_->setObjectName(QStringLiteral("mediaLabStatusLabel"));
    mediaLabStatusLabel_->setWordWrap(true);

    mediaLabLayout->addWidget(secondaryGroup);
    mediaLabLayout->addWidget(subtitleWorkflowGroup);
    mediaLabLayout->addWidget(audioGroup);
    mediaLabLayout->addWidget(videoGroup);
    mediaLabLayout->addWidget(profileGroup);
    mediaLabLayout->addWidget(mediaLabStatusLabel_);
    mediaLabLayout->addStretch(1);
    mediaLabScrollArea->setWidget(mediaLabPage);

    connect(loadExtraSubtitleButton, &QPushButton::clicked, this, &MainWindow::loadSubtitleFile);
    connect(showTrackPanelButton, &QPushButton::clicked, this, [this]() {
        setSidePanelVisible(SidePanel::Details, true, true);
        if (detailsTabs_ != nullptr && tracksTree_ != nullptr) {
            detailsTabs_->setCurrentWidget(tracksTree_);
        }
    });
    connect(subtitleAutoSelectButton, &QPushButton::clicked, this, [this]() {
        applySmartSubtitleSelection(true, true);
    });
    connect(subtitleDownloadButton, &QPushButton::clicked, this, &MainWindow::triggerSubtitleDownload);
    connect(subtitleDelayBackSmallButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(-subtitleSyncSmallStep());
    });
    connect(subtitleDelayForwardSmallButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(subtitleSyncSmallStep());
    });
    connect(subtitleDelayBackMediumButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(-(subtitleSyncSmallStep() * 2.0));
    });
    connect(subtitleDelayForwardMediumButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(subtitleSyncSmallStep() * 2.0);
    });
    connect(subtitleDelayBackLargeButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(-(subtitleSyncSmallStep() * 4.0));
    });
    connect(subtitleDelayForwardLargeButton, &QPushButton::clicked, this, [this]() {
        adjustSubtitleDelayWithFeedback(subtitleSyncSmallStep() * 4.0);
    });
    connect(subtitleDelayResetButton, &QPushButton::clicked, this, &MainWindow::resetSubtitleDelayWithFeedback);
    connect(subtitleDelayApplyButton, &QPushButton::clicked, this, [this]() {
        if (subtitleDelayCustomSpinBox_ == nullptr) {
            return;
        }
        setSubtitleDelayWithFeedback(subtitleDelayCustomSpinBox_->value());
    });
    connect(subtitleRememberDelayForMediaCheckBox_, &QCheckBox::toggled, this, [this](const bool enabled) {
        if (enabled) {
            saveRememberedSubtitleDelayForCurrentMedia();
        } else {
            removeRememberedSubtitleDelayForCurrentMedia();
        }
    });
    connect(applySubtitleLanguageHintsButton, &QPushButton::clicked, this, [this]() {
        if (subtitleLanguageHintEdit_ == nullptr) {
            return;
        }
        if (settingsController_ != nullptr) {
            settingsController_->setSubtitlePreferredLanguages(subtitleLanguageHintEdit_->text());
            subtitleLanguageHintEdit_->setText(settingsController_->subtitlePreferredLanguages());
        }
        applySmartSubtitleSelection(false, true);
    });
    connect(audioNormalizeCheckBox_, &QCheckBox::toggled, this, [this](const bool) {
        applyAudioFilterState();
    });
    connect(customAudioFilterEdit_, &QLineEdit::editingFinished, this, &MainWindow::applyAudioFilterState);
    connect(applyCustomAudioButton, &QPushButton::clicked, this, &MainWindow::applyAudioFilterState);
    connect(clearCustomAudioButton, &QPushButton::clicked, this, [this]() {
        if (customAudioFilterEdit_ != nullptr) {
            customAudioFilterEdit_->clear();
        }
        applyAudioFilterState();
    });
    connect(resetEqButton, &QPushButton::clicked, this, [this]() {
        for (QSlider *slider : equalizerSliders_) {
            if (slider == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(slider);
            slider->setValue(0);
        }
        if (audioNormalizeCheckBox_ != nullptr) {
            const QSignalBlocker blocker(audioNormalizeCheckBox_);
            audioNormalizeCheckBox_->setChecked(false);
        }
        applyAudioFilterState();
    });
    connect(bassBoostButton, &QPushButton::clicked, this, [this]() {
        const QVector<int> values {6, 5, 4, 3, 1, 0, 0, -1, -2, -2};
        for (qsizetype index = 0; index < equalizerSliders_.size() && index < values.size(); ++index) {
            if (equalizerSliders_[index] == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(equalizerSliders_[index]);
            equalizerSliders_[index]->setValue(values[index]);
        }
        applyAudioFilterState();
    });
    connect(voiceBoostButton, &QPushButton::clicked, this, [this]() {
        const QVector<int> values {-2, -1, 0, 2, 4, 5, 4, 2, 0, -1};
        for (qsizetype index = 0; index < equalizerSliders_.size() && index < values.size(); ++index) {
            if (equalizerSliders_[index] == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(equalizerSliders_[index]);
            equalizerSliders_[index]->setValue(values[index]);
        }
        if (audioNormalizeCheckBox_ != nullptr) {
            const QSignalBlocker blocker(audioNormalizeCheckBox_);
            audioNormalizeCheckBox_->setChecked(true);
        }
        applyAudioFilterState();
    });
    connect(stereo3dFilterEdit_, &QLineEdit::editingFinished, this, &MainWindow::applyVideoFilterState);
    connect(customVideoFilterEdit_, &QLineEdit::editingFinished, this, &MainWindow::applyVideoFilterState);
    connect(applyCustomVideoButton, &QPushButton::clicked, this, &MainWindow::applyVideoFilterState);
    connect(clearCustomVideoButton, &QPushButton::clicked, this, [this]() {
        if (customVideoFilterEdit_ != nullptr) {
            customVideoFilterEdit_->clear();
        }
        if (stereo3dFilterEdit_ != nullptr) {
            stereo3dFilterEdit_->clear();
        }
        if (shaderPathEdit_ != nullptr) {
            shaderPathEdit_->clear();
        }
        applyVideoFilterState();
    });
    connect(videoDebandCheckBox_, &QCheckBox::toggled, this, [this](const bool) {
        applyVideoFilterState();
    });
    connect(videoDeinterlaceCheckBox_, &QCheckBox::toggled, this, [this](const bool) {
        applyVideoFilterState();
    });
    connect(shaderPathEdit_, &QLineEdit::editingFinished, this, &MainWindow::applyVideoFilterState);
    connect(browseShaderButton, &QPushButton::clicked, this, [this]() {
        const QString path = filedialog::getOpenFileName(
            this,
            uiText("Choose GLSL Shader"),
            shaderPathEdit_ != nullptr ? shaderPathEdit_->text().trimmed() : QString {},
            uiText("Shader Files (*.glsl *.hook *.frag *.vert);;All Files (*)"));
        if (!path.trimmed().isEmpty() && shaderPathEdit_ != nullptr) {
            shaderPathEdit_->setText(path);
            applyVideoFilterState();
        }
    });
    connect(clearShaderButton, &QPushButton::clicked, this, [this]() {
        if (shaderPathEdit_ != nullptr) {
            shaderPathEdit_->clear();
        }
        applyVideoFilterState();
    });
    connect(resetVideoButton, &QPushButton::clicked, this, [this]() {
        for (QSlider *slider : {videoBrightnessSlider_,
                                videoContrastSlider_,
                                videoSaturationSlider_,
                                videoGammaSlider_,
                                videoHueSlider_,
                                videoSharpenSlider_,
                                videoDenoiseSlider_}) {
            if (slider == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(slider);
            slider->setValue(0);
        }
        if (videoDebandCheckBox_ != nullptr) {
            const QSignalBlocker blocker(videoDebandCheckBox_);
            videoDebandCheckBox_->setChecked(false);
        }
        if (videoDeinterlaceCheckBox_ != nullptr) {
            const QSignalBlocker blocker(videoDeinterlaceCheckBox_);
            videoDeinterlaceCheckBox_->setChecked(false);
        }
        applyVideoFilterState();
    });
    connect(cinemaVideoButton, &QPushButton::clicked, this, [this]() {
        const QList<QPair<QSlider *, int>> values {
            {videoBrightnessSlider_, -4},
            {videoContrastSlider_, 14},
            {videoSaturationSlider_, 6},
            {videoGammaSlider_, 10},
            {videoHueSlider_, 0},
        };
        for (const auto &entry : values) {
            if (entry.first == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(entry.first);
            entry.first->setValue(entry.second);
        }
        applyVideoFilterState();
    });
    connect(vividVideoButton, &QPushButton::clicked, this, [this]() {
        const QList<QPair<QSlider *, int>> values {
            {videoBrightnessSlider_, 2},
            {videoContrastSlider_, 18},
            {videoSaturationSlider_, 16},
            {videoGammaSlider_, 6},
            {videoHueSlider_, 0},
        };
        for (const auto &entry : values) {
            if (entry.first == nullptr) {
                continue;
            }
            const QSignalBlocker blocker(entry.first);
            entry.first->setValue(entry.second);
        }
        applyVideoFilterState();
    });
    connect(saveFileProfileButton, &QPushButton::clicked, this, &MainWindow::saveMediaProfileForFile);
    connect(saveTypeProfileButton, &QPushButton::clicked, this, &MainWindow::saveMediaProfileForType);
    connect(clearFileProfileButton, &QPushButton::clicked, this, &MainWindow::clearMediaProfileForFile);
    connect(clearTypeProfileButton, &QPushButton::clicked, this, &MainWindow::clearMediaProfileForType);

    detailsTabs_->addTab(bookmarksPage_, uiText("Bookmarks"));
    detailsTabs_->addTab(chaptersList_, uiText("Chapters"));
    detailsTabs_->addTab(tracksTree_, uiText("Tracks"));
    detailsTabs_->addTab(historyPage, uiText("History"));
    detailsTabs_->addTab(favoritesPage_, uiText("Favorites"));
    detailsTabs_->addTab(scenePage, uiText("Scenes"));
    detailsTabs_->addTab(mediaLabScrollArea, uiText("Media Lab"));
    detailsTabs_->setMovable(true);
    bookmarksPage_->setProperty("detailsTabId", QStringLiteral("bookmarks"));
    chaptersList_->setProperty("detailsTabId", QStringLiteral("chapters"));
    tracksTree_->setProperty("detailsTabId", QStringLiteral("tracks"));
    historyPage->setProperty("detailsTabId", QStringLiteral("history"));
    favoritesPage_->setProperty("detailsTabId", QStringLiteral("favorites"));
    scenePage->setProperty("detailsTabId", QStringLiteral("scenes"));
    mediaLabScrollArea->setProperty("detailsTabId", QStringLiteral("media_lab"));
    if (detailsTabs_->tabBar() != nullptr) {
        detailsTabs_->tabBar()->hide();
        connect(detailsTabs_->tabBar(), &QTabBar::tabMoved, this, [this](const int, const int) {
            rebuildDetailsTabStrip();
        });
    }
    connect(detailsTabs_, &QTabWidget::currentChanged, this, [this](const int index) {
        syncDetailsTabStripState();
        scrollDetailsTabButtonIntoView(index);
    });
    rebuildDetailsTabStrip();
    detailsContainerLayout->addWidget(detailsTabsHeader, 0);
    detailsContainerLayout->addWidget(detailsTabs_, 1);
    detailsDock_->setWidget(detailsContainer);
    detailsResizeHandle_ = new QWidget(detailsDock_);
    detailsResizeHandle_->setObjectName(QStringLiteral("overlayPanelResizeHandle"));
    detailsResizeHandle_->setAttribute(Qt::WA_StyledBackground, true);
    detailsResizeHandle_->setCursor(Qt::SizeHorCursor);
    detailsResizeHandle_->hide();
    addDockWidget(Qt::RightDockWidgetArea, detailsDock_);

    playlistDockAnimation_ = new QPropertyAnimation(playlistDock_, "geometry", this);
    playlistDockAnimation_->setDuration(180);
    playlistDockAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(playlistDockAnimation_, &QPropertyAnimation::finished, this, [this]() {
        if (playlistDock_ == nullptr) {
            return;
        }
        if (playlistDock_->property("overlayHidePending").toBool()) {
            playlistDock_->setProperty("overlayHidePending", false);
            playlistDock_->hide();
        }
        if (panelOverlayModeActive_ && playlistDock_->isVisible()) {
            playlistDock_->raise();
        }
        syncPanelToggleActions();
        updateVideoOverlayGeometry();
    });

    detailsDockAnimation_ = new QPropertyAnimation(detailsDock_, "geometry", this);
    detailsDockAnimation_->setDuration(180);
    detailsDockAnimation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(detailsDockAnimation_, &QPropertyAnimation::finished, this, [this]() {
        if (detailsDock_ == nullptr) {
            return;
        }
        if (detailsDock_->property("overlayHidePending").toBool()) {
            detailsDock_->setProperty("overlayHidePending", false);
            detailsDock_->hide();
        }
        if (panelOverlayModeActive_ && detailsDock_->isVisible()) {
            detailsDock_->raise();
        }
        syncPanelToggleActions();
        updateVideoOverlayGeometry();
    });

    tabifyDockWidget(playlistDock_, detailsDock_);
    playlistDock_->raise();
    updatePlaylistChromeState();
}

void MainWindow::connectUi()
{
    controlBar_->installEventFilter(this);
    fullscreenTopBar_->installEventFilter(this);
    playlistDock_->installEventFilter(this);
    detailsDock_->installEventFilter(this);
    if (playlistDock_->widget() != nullptr) {
        playlistDock_->widget()->installEventFilter(this);
    }
    if (detailsDock_->widget() != nullptr) {
        detailsDock_->widget()->installEventFilter(this);
    }
    if (playlistResizeHandle_ != nullptr) {
        playlistResizeHandle_->installEventFilter(this);
    }
    if (detailsResizeHandle_ != nullptr) {
        detailsResizeHandle_->installEventFilter(this);
    }
    if (sidePanelSelector_ != nullptr) {
        sidePanelSelector_->installEventFilter(this);
    }

    connect(controlBar_, &ControlBar::openRequested, this, &MainWindow::openFiles);
    connect(controlBar_, &ControlBar::previousRequested, this, [this]() {
        navigatePlaylistWithResumeGuard(false);
    });
    connect(controlBar_, &ControlBar::playPauseRequested, this, [this]() {
        playbackController_->togglePause();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->togglePause();
        }
    });
    connect(controlBar_, &ControlBar::nextRequested, this, [this]() {
        navigatePlaylistWithResumeGuard(true);
    });
    connect(controlBar_, &ControlBar::fullscreenRequested, this, &MainWindow::toggleFullscreen);
    connect(controlBar_, &ControlBar::stopRequested, this, [this]() {
        stopRequested_ = true;
        persistPlaybackProgress(false, true);
        playbackController_->stop();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->stop();
        }
    });
    connect(controlBar_, &ControlBar::seekRequested, this, [this](const double fraction) {
        playbackController_->seekToFraction(fraction);
        const double targetPosition = std::clamp(
            currentDurationSeconds_ * std::clamp(fraction, 0.0, 1.0),
            0.0,
            std::max(0.0, currentDurationSeconds_));
        currentPositionSeconds_ = targetPosition;
        persistPlaybackProgress(false, true);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->seekToSeconds(targetPosition);
        }
        showPlaybackFeedback(QStringLiteral("⏩ Seek  •  %1").arg(formatPlaybackTime(targetPosition)));
    });
    connect(controlBar_, &ControlBar::previewRequested, this, &MainWindow::onPreviewRequested);
    connect(controlBar_, &ControlBar::previewHidden, this, &MainWindow::onPreviewHidden);
    connect(controlBar_, &ControlBar::volumeRequested, playbackController_, &revaplayer::application::PlaybackController::setVolume);
    connect(controlBar_, &ControlBar::volumeRequested, this, [this](const int volume) {
        if (settingsController_ != nullptr && settingsController_->rememberLastVolume()) {
            settingsController_->setStartupVolume(volume);
        }
        showVolumeFeedback(volume);
    });
    connect(controlBar_, &ControlBar::playlistPanelToggled, this, [this](const bool visible) {
        setSidePanelVisible(SidePanel::Playlist, visible, visible);
    });
    connect(controlBar_, &ControlBar::detailsPanelToggled, this, [this](const bool visible) {
        setSidePanelVisible(SidePanel::Details, visible, visible);
    });
    connect(controlBar_, &ControlBar::playbackSpeedRequested, this, [this](const double speed) {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(speed);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
    });
    connect(controlBar_, &ControlBar::playbackSpeedAdjusted, this, [this](const double delta) {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(currentSpeed_ + delta);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
    });
    connect(controlBar_, &ControlBar::playbackSpeedResetRequested, this, [this]() {
        playbackController_->resetSpeed();
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->resetSpeed();
        }
        showPlaybackSpeedFeedback(1.0, true);
    });
    connect(controlBar_, &ControlBar::repeatModeRequested, this, [this](const QString &mode) {
        setRepeatMode(mode, true);
    });
    connect(controlBar_, &ControlBar::loopStartRequested, this, [this]() {
        if (setLoopStartAction_ != nullptr) {
            setLoopStartAction_->trigger();
        }
    });
    connect(controlBar_, &ControlBar::loopEndRequested, this, [this]() {
        if (setLoopEndAction_ != nullptr) {
            setLoopEndAction_->trigger();
        }
    });
    connect(controlBar_, &ControlBar::loopClearRequested, this, [this]() {
        if (clearLoopAction_ != nullptr) {
            clearLoopAction_->trigger();
        }
    });
    if (dashboardOpenFileButton_ != nullptr) {
        connect(dashboardOpenFileButton_, &QToolButton::clicked, this, &MainWindow::openFiles);
    }
    if (dashboardOpenUrlButton_ != nullptr) {
        connect(dashboardOpenUrlButton_, &QToolButton::clicked, this, &MainWindow::openUrl);
    }
    const auto connectDashboardList = [this](QListWidget *listWidget, const bool pinnedCourseList) {
        if (listWidget == nullptr) {
            return;
        }
        connect(listWidget, &QListWidget::itemActivated, this, [this, pinnedCourseList](QListWidgetItem *item) {
            if (item == nullptr) {
                return;
            }
            const QString payload = item->data(Qt::UserRole).toString();
            if (payload.isEmpty()) {
                return;
            }
            if (pinnedCourseList) {
                const QStringList sources = supportedMediaFilesInDirectory(QDir(payload), naturalSortFolderPlaylistEnabled());
                if (!sources.isEmpty()) {
                    beginLoadFeedback(displayTitleForHistory(sources.first(), QString {}));
                    openPlaylistSources(sources, 0);
                }
                return;
            }
            const double restorePosition = item->data(Qt::UserRole + 1).toDouble();
            if (restorePosition >= 5.0) {
                armPendingCurrentMediaRestore(payload, restorePosition);
            }
            openMediaSource(payload);
        });
    };
    connectDashboardList(dashboardContinueList_, false);
    connectDashboardList(dashboardRecentList_, false);
    connectDashboardList(dashboardFavoritesList_, false);
    connectDashboardList(dashboardPinnedCoursesList_, true);
    if (detailsSwitchPanelButton_ != nullptr) {
        connect(detailsSwitchPanelButton_, &QToolButton::clicked, this, [this]() {
            setSidePanelVisible(SidePanel::Details, false, false);
            setSidePanelVisible(SidePanel::Playlist, true, true);
        });
    }
    if (detailsPanelSettingsButton_ != nullptr) {
        connect(detailsPanelSettingsButton_, &QToolButton::clicked, this, &MainWindow::showDetailsPanelSettingsDialog);
    }
    connect(videoViewport_, &QWidget::customContextMenuRequested, this, &MainWindow::showViewportContextMenu);
    connect(videoViewport_, &VideoViewport::clicked, this, [this](const Qt::MouseButton button) {
        dispatchClickAction(button);
    });
    connect(videoViewport_, &VideoViewport::doubleClicked, this, [this](const Qt::MouseButton button) {
        dispatchDoubleClickAction(button);
    });
    connect(videoViewport_, &VideoViewport::middleClicked, this, &MainWindow::dispatchMiddleClickAction);
    connect(videoViewport_, &VideoViewport::gestureTriggered, this, &MainWindow::dispatchGestureAction);
    connect(videoViewport_, &VideoViewport::panDragged, this, &MainWindow::handleVideoPanDelta);
    connect(videoViewport_, &VideoViewport::pointerActivity, this, &MainWindow::handleFullscreenPointerActivity);
    connect(videoViewport_, &VideoViewport::pointerLeft, this, [this]() {
        handleViewportPointerLeave();
    });
    connect(videoViewport_, &VideoViewport::wheelAdjusted, this, &MainWindow::dispatchWheelAction);
    connect(videoViewport_, &VideoViewport::backNavigationRequested, this, [this]() {
        dispatchMouseSideButtonAction(false);
    });
    connect(videoViewport_, &VideoViewport::forwardNavigationRequested, this, [this]() {
        dispatchMouseSideButtonAction(true);
    });
    connect(fullscreenOpenButton_, &QToolButton::clicked, this, &MainWindow::openFiles);
    connect(fullscreenPlaylistButton_, &QToolButton::clicked, this, [this]() {
        if (togglePlaylistAction_ != nullptr) {
            togglePlaylistAction_->trigger();
        }
    });
    connect(fullscreenDetailsButton_, &QToolButton::clicked, this, [this]() {
        if (toggleDetailsAction_ != nullptr) {
            toggleDetailsAction_->trigger();
        }
    });
    connect(sidePanelPlaylistButton_, &QToolButton::clicked, this, [this]() {
        setSidePanelVisible(SidePanel::Playlist, true, true);
    });
    connect(sidePanelDetailsButton_, &QToolButton::clicked, this, [this]() {
        setSidePanelVisible(SidePanel::Details, true, true);
    });
    connect(fullscreenPreferencesButton_, &QToolButton::clicked, this, &MainWindow::showSettingsDialog);
    connect(fullscreenExitFullscreenButton_, &QToolButton::clicked, this, &MainWindow::toggleFullscreen);

    connect(playlistView_, &QListView::customContextMenuRequested, this, &MainWindow::showPlaylistContextMenu);
    if (playlistView_->viewport() != nullptr) {
        playlistView_->viewport()->installEventFilter(this);
    }
    if (pinnedCoursesTabBar_ != nullptr) {
        pinnedCoursesTabBar_->installEventFilter(this);
    }
    if (playlistView_->verticalScrollBar() != nullptr) {
        connect(playlistView_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](const int) {
            requestPlaylistThumbnailBatch(false);
            schedulePlaylistMetadataRefresh(64);
        });
    }
    if (playlistView_->selectionModel() != nullptr) {
        connect(playlistView_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
            refreshPlaylistInspector();
        });
        connect(playlistView_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex &, const QModelIndex &) {
            refreshPlaylistInspector();
            schedulePlaylistMetadataRefresh(0);
        });
    }
    if (pinnedCoursesTabBar_ != nullptr) {
        connect(pinnedCoursesTabBar_, &QTabBar::currentChanged, this, [this](const int index) {
            scrollPinnedCourseTabIntoView(index);
            if (QApplication::mouseButtons() == Qt::NoButton) {
                previewPinnedCourseTab(index);
            }
        });
        connect(pinnedCoursesTabBar_, &QTabBar::tabBarClicked, this, [this](const int index) {
            previewPinnedCourseTab(index);
            scrollPinnedCourseTabIntoView(index);
        });
        connect(pinnedCoursesTabBar_, &QTabBar::tabBarDoubleClicked, this, [this](const int index) {
            openPinnedCourseTab(index);
        });
        connect(pinnedCoursesTabBar_, &QTabBar::tabMoved, this, [this](const int, const int) {
            if (settingsController_ == nullptr || pinnedCoursesTabBar_ == nullptr) {
                return;
            }

            const QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
            QVector<PinnedCourseFolder> reordered;
            reordered.reserve(courses.size());

            for (int i = 0; i < pinnedCoursesTabBar_->count(); ++i) {
                const QString path = pinnedCoursesTabBar_->tabData(i).toString();
                if (path.isEmpty()) {
                    continue; // Skip "Current" tab
                }

                const auto it = std::find_if(courses.cbegin(), courses.cend(), [&path](const PinnedCourseFolder &c) {
                    return c.path == path;
                });
                if (it != courses.cend()) {
                    reordered.push_back(*it);
                }
            }

            if (reordered.size() == courses.size()) {
                persistPinnedCourses(settingsController_, reordered);
                statusBar()->showMessage(uiText("Saved folder order updated"), 2500);
            }
        });
        connect(pinnedCoursesTabBar_, &QTabBar::customContextMenuRequested, this, &MainWindow::showPinnedCourseContextMenu);
    }
    if (playlistViewPresetsTabBar_ != nullptr) {
        connect(playlistViewPresetsTabBar_, &QTabBar::currentChanged, this, [this](const int index) {
            if (playlistViewPresetApplyInProgress_ || playlistViewPresetsTabBar_ == nullptr || index < 0) {
                return;
            }
            const QString presetKey = playlistViewPresetsTabBar_->tabData(index).toString().trimmed();
            if (!presetKey.isEmpty()) {
                applyPlaylistViewPreset(presetKey, true);
            }
        });
        connect(playlistViewPresetsTabBar_, &QTabBar::tabCloseRequested, this, [this](const int index) {
            if (settingsController_ == nullptr || playlistViewPresetsTabBar_ == nullptr || index < 0) {
                return;
            }
            const QString presetKey = playlistViewPresetsTabBar_->tabData(index).toString().trimmed();
            if (presetKey.isEmpty()) {
                return;
            }
            const QString presetName = playlistViewPresetsTabBar_->tabText(index);
            if (QMessageBox::question(
                    this,
                    uiText("Delete Playlist View"),
                    uiText("Delete the saved view \"%1\"?").arg(presetName))
                != QMessageBox::Yes) {
                return;
            }
            settingsController_->removeCustomValue(presetKey);
            if (settingsController_->customValue(QString::fromLatin1(kPlaylistViewActiveSetting)) == presetKey) {
                settingsController_->removeCustomValue(QString::fromLatin1(kPlaylistViewActiveSetting));
            }
            activePlaylistViewPresetKey_.clear();
            rebuildPlaylistViewTabs();
            refreshPlaylistInspector();
        });
        connect(playlistViewPresetsTabBar_, &QTabBar::tabMoved, this, [this](const int, const int) {
            if (settingsController_ == nullptr || playlistViewPresetsTabBar_ == nullptr) {
                return;
            }
            for (int index = 0; index < playlistViewPresetsTabBar_->count(); ++index) {
                const QString presetKey = playlistViewPresetsTabBar_->tabData(index).toString().trimmed();
                if (presetKey.isEmpty()) {
                    continue;
                }
                QJsonObject object = QJsonDocument::fromJson(settingsController_->customValue(presetKey).toUtf8()).object();
                object.insert(QStringLiteral("order"), index);
                settingsController_->setCustomValue(presetKey, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
            }
        });
        connect(playlistViewPresetsTabBar_, &QWidget::customContextMenuRequested, this, [this](const QPoint &position) {
            if (playlistViewPresetsTabBar_ == nullptr) {
                return;
            }
            QMenu menu(this);
            QAction *saveAction = menu.addAction(uiText("Save Current View"));
            QAction *editAction = menu.addAction(uiText("Edit Active View"));
            forceMenuLeftToRight(&menu);
            const QAction *chosen = menu.exec(playlistViewPresetsTabBar_->mapToGlobal(position));
            if (chosen == saveAction) {
                saveCurrentPlaylistView();
            } else if (chosen == editAction) {
                editCurrentPlaylistView();
            }
        });
    }
    if (playlistSaveViewButton_ != nullptr) {
        connect(playlistSaveViewButton_, &QToolButton::clicked, this, &MainWindow::saveCurrentPlaylistView);
    }
    if (playlistEditViewButton_ != nullptr) {
        connect(playlistEditViewButton_, &QToolButton::clicked, this, &MainWindow::editCurrentPlaylistView);
    }
    if (playlistPinCourseButton_ != nullptr) {
        connect(playlistPinCourseButton_, &QToolButton::clicked, this, &MainWindow::pinCurrentMediaFolder);
    }
    if (playlistManageCoursesButton_ != nullptr) {
        connect(playlistManageCoursesButton_, &QToolButton::clicked, this, &MainWindow::managePinnedCourses);
    }
    if (playlistRefreshButton_ != nullptr) {
        connect(playlistRefreshButton_, &QToolButton::clicked, this, [this]() {
            clearFileSystemCache();
            const int enqueued = enqueuePlaylistMetadataScan(true);
            if (enqueued > 0) {
                statusBar()->showMessage(uiText("Scanning metadata for %1 playlist items").arg(enqueued), 2500);
            } else {
                statusBar()->showMessage(uiText("Playlist metadata is already up to date"), 2500);
            }
        });
    }
    if (playlistPanelSettingsButton_ != nullptr) {
        connect(playlistPanelSettingsButton_, &QToolButton::clicked, this, [this]() {
            QMenu menu(this);
            QAction *settingsAction = menu.addAction(uiText("Playlist Settings"));
            menu.addSeparator();
            QAction *resetProgressAction = menu.addAction(uiText("Reset Progress for This List"));
            resetProgressAction->setEnabled(
                historyController_ != nullptr
                && historyController_->isReady()
                && !currentPlaylistProgressSources().isEmpty());
            forceMenuLeftToRight(&menu);
            QAction *chosenAction = menu.exec(playlistPanelSettingsButton_->mapToGlobal(
                QPoint(0, playlistPanelSettingsButton_->height())));
            if (chosenAction == settingsAction) {
                showPlaylistPanelSettingsDialog();
            } else if (chosenAction == resetProgressAction) {
                resetCurrentPlaylistProgress();
            }
        });
    }
    connect(playlistSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (playlistFilterModel_ != nullptr) {
            const QString trimmed = text.trimmed();
            playlistFilterModel_->setFilterRegularExpression(
                trimmed.isEmpty()
                    ? QRegularExpression {}
                    : QRegularExpression(QRegularExpression::escape(trimmed), QRegularExpression::CaseInsensitiveOption));
        }
        updatePlaylistReorderAvailability();
        refreshPlaylistSummary();
        if (playlistAutoFollowEnabled()) {
            focusCurrentPlaylistItem(false);
        }
        requestPlaylistThumbnailBatch(true);
    });
    connect(playlistController_, &revaplayer::application::PlaylistController::playlistReorderRequested, this, [this](const QVector<int> &orderedIndices) {
        applyRequestedPlaylistOrder(orderedIndices);
    });
    connect(bookmarkAddButton_, &QToolButton::clicked, this, &MainWindow::addBookmark);
    connect(bookmarkImportButton_, &QToolButton::clicked, this, &MainWindow::importBookmarksForCurrentMedia);
    connect(bookmarkExportButton_, &QToolButton::clicked, this, &MainWindow::exportBookmarksForCurrentMedia);
    connect(bookmarkDeleteButton_, &QToolButton::clicked, this, &MainWindow::removeSelectedBookmark);
    if (bookmarkQuizCsvButton_ != nullptr) {
        connect(bookmarkQuizCsvButton_, &QToolButton::clicked, this, &MainWindow::exportBookmarksAsQuizCsv);
    }
    connect(bookmarkSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        filterBookmarks();
    });
    connect(bookmarkCategoryComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        filterBookmarks();
    });
    connect(bookmarksList_, &QListWidget::itemActivated, this, &MainWindow::onBookmarkActivated);
    connect(bookmarksList_, &QListWidget::customContextMenuRequested, this, &MainWindow::showBookmarksContextMenu);
    connect(bookmarksList_, &QListWidget::itemSelectionChanged, this, [this]() {
        updateActionStates();
        updateBookmarkSelectionPreview();
    });
    connect(bookmarkJumpButton_, &QToolButton::clicked, this, [this]() {
        if (bookmarksList_ != nullptr && bookmarksList_->currentItem() != nullptr) {
            onBookmarkActivated(bookmarksList_->currentItem());
        }
    });
    connect(chaptersList_, &QListWidget::itemActivated, this, &MainWindow::onChapterActivated);
    connect(tracksTree_, &QTreeWidget::itemActivated, this, &MainWindow::onTrackActivated);
    connect(historyRefreshButton_, &QToolButton::clicked, this, &MainWindow::reloadHistoryPanel);
    connect(historyClearButton_, &QToolButton::clicked, this, &MainWindow::clearHistoryPanel);
    connect(historySearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        filterHistoryPanel();
    });
    connect(historyList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        const QString source = item->data(Qt::UserRole).toString().trimmed();
        if (source.isEmpty()) {
            return;
        }
        if (!openMediaSource(source)) {
            if (historyController_ != nullptr && historyController_->isReady()) {
                historyController_->removeHistoryEntry(source);
            }
            reloadHistoryPanel();
            statusBar()->showMessage(uiText("That recent item is no longer available and was removed from History."), 5000);
        }
    });
    connect(favoritesRefreshButton_, &QToolButton::clicked, this, &MainWindow::reloadFavoritesPanel);
    connect(favoritesCurrentButton_, &QToolButton::clicked, this, &MainWindow::toggleFavoriteCurrentMedia);
    connect(favoritesAddButton_, &QToolButton::clicked, this, &MainWindow::addFavoriteFile);
    connect(favoritesOpenButton_, &QToolButton::clicked, this, &MainWindow::openSelectedFavorite);
    connect(favoritesOpenAllButton_, &QToolButton::clicked, this, [this]() {
        if (favoritesList_ == nullptr) {
            return;
        }

        QStringList sources;
        QListWidgetItem *currentItem = favoritesList_->currentItem();
        int currentIndex = 0;
        for (int row = 0; row < favoritesList_->count(); ++row) {
            QListWidgetItem *item = favoritesList_->item(row);
            if (item == nullptr || item->isHidden()) {
                continue;
            }

            const QString source = item->data(Qt::UserRole).toString().trimmed();
            if (source.isEmpty()) {
                continue;
            }

            if (item == currentItem) {
                currentIndex = sources.size();
            }
            if (!sources.contains(source)) {
                sources.push_back(source);
            }
        }

        if (sources.isEmpty()) {
            statusBar()->showMessage(uiText("No favorites to open"), 2500);
            return;
        }

        const int lastIndex = std::max(0, static_cast<int>(sources.size()) - 1);
        openPlaylistSources(sources, std::clamp(currentIndex, 0, lastIndex));
    });
    connect(favoritesRemoveButton_, &QToolButton::clicked, this, &MainWindow::removeSelectedFavorite);
    connect(favoritesSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        filterFavoritesPanel();
    });
    connect(favoritesList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        openSelectedFavorite();
    });
    connect(favoritesList_, &QListWidget::itemSelectionChanged, this, [this]() {
        updateActionStates();
    });
    connect(sceneRefreshButton_, &QToolButton::clicked, this, [this]() {
        rebuildSceneBrowser(true);
    });
    connect(sceneExportButton_, &QToolButton::clicked, this, &MainWindow::exportSceneImages);
    connect(sceneBookmarkButton_, &QToolButton::clicked, this, &MainWindow::bookmarkSelectedScene);
    connect(sceneStepSpinBox_, qOverload<int>(&QSpinBox::valueChanged), this, [this](const int) {
        if (settingsController_ != nullptr) {
            settingsController_->setSceneBrowserStepSeconds(sceneStepSpinBox_->value());
        }
        refreshSceneBrowserPrompt(false);
    });
    connect(sceneSearchEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        filterSceneBrowser();
    });
    connect(sceneList_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        if (item == nullptr) {
            return;
        }
        playbackController_->seekToSeconds(item->data(Qt::UserRole).toDouble());
        showPlaybackFeedback(QStringLiteral("Scene  •  %1").arg(formatPlaybackTime(item->data(Qt::UserRole).toDouble())));
    });
    connect(secondarySubtitleCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](const int) {
        applySecondarySubtitleSelection();
    });
    connect(playlistDock_, &QDockWidget::visibilityChanged, this, [this](const bool visible) {
        if (visible) {
            activeSidePanel_ = SidePanel::Playlist;
            schedulePlaylistMetadataRefresh(120);
        }
        syncPanelToggleActions();
    });
    connect(detailsDock_, &QDockWidget::visibilityChanged, this, [this](const bool visible) {
        if (visible) {
            activeSidePanel_ = SidePanel::Details;
        }
        syncPanelToggleActions();
    });
    connect(fullscreenChromeTimer_, &QTimer::timeout, this, [this]() {
        if (isFullScreen() && fullscreenAutoHideEnabled()) {
            setFullscreenChromeVisible(false, false);
            if (panelOverlayModeActive_) {
                suppressPanelPreferencePersistence_ = true;
                setSidePanelVisible(SidePanel::Playlist, false, false);
                setSidePanelVisible(SidePanel::Details, false, false);
                suppressPanelPreferencePersistence_ = false;
                syncPanelToggleActions();
                updateVideoOverlayGeometry();
            }
        }
    });

    connect(playbackController_, &revaplayer::application::PlaybackController::loadStarted, this, &MainWindow::onLoadStarted);
    connect(playbackController_, &revaplayer::application::PlaybackController::idleChanged, this, &MainWindow::onIdleChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::playbackPositionChanged, this, &MainWindow::onPlaybackPositionChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::pausedChanged, this, &MainWindow::onPausedChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::volumeChanged, this, &MainWindow::onVolumeChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::mutedChanged, this, &MainWindow::onMutedChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::speedChanged, this, [this](const double speed) {
        currentSpeed_ = speed;
        if (sessionWidePlaybackSpeedEnabled()) {
            sessionPlaybackSpeed_ = speed;
        }
        controlBar_->setPlaybackSpeed(speed);
        updateMediaInformationOverlay();
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleDelayChanged, this, [this](const double delaySeconds) {
        currentSubtitleDelaySeconds_ = delaySeconds;
        if (subtitleDelayCustomSpinBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleDelayCustomSpinBox_);
            subtitleDelayCustomSpinBox_->setValue(delaySeconds);
        }
        if (subtitleRememberDelayForMediaCheckBox_ != nullptr && subtitleRememberDelayForMediaCheckBox_->isChecked()) {
            saveRememberedSubtitleDelayForCurrentMedia();
        }
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleVisibilityChanged, this, [this](const bool visible) {
        subtitleVisible_ = visible;
        syncSubtitleActionStates();
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleScaleChanged, this, [this](const double scale) {
        currentSubtitleScale_ = scale;
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitlePositionChanged, this, [this](const int position) {
        currentSubtitlePosition_ = position;
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleFontFamilyChanged, this, [this](const QString &fontFamily) {
        currentSubtitleFontFamily_ = fontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : fontFamily.trimmed();
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleFontSizeChanged, this, [this](const int fontSize) {
        currentSubtitleFontSize_ = fontSize;
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::subtitleAssOverrideChanged, this, [this](const QString &mode) {
        currentSubtitleAssOverride_ = revaplayer::application::normalizeSubtitleAssOverride(mode);
        syncSubtitleActionStates();
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::audioDelayChanged, this, [this](const double delaySeconds) {
        currentAudioDelaySeconds_ = delaySeconds;
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::titleChanged, this, &MainWindow::onTitleChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::diagnosticsChanged, this, [this](const revaplayer::domain::PlaybackDiagnostics &diagnostics) {
        currentDiagnostics_ = diagnostics;
        updateControlBarBufferedState();
        updateMediaInformationOverlay();
        updateMediaInfoDialog();
    });
    connect(playbackController_, &revaplayer::application::PlaybackController::fileLoaded, this, &MainWindow::onFileLoaded);
    connect(playbackController_, &revaplayer::application::PlaybackController::fileEnded, this, &MainWindow::onFileEnded);
    connect(playbackController_, &revaplayer::application::PlaybackController::playlistChanged, this, &MainWindow::onPlaylistChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::chaptersChanged, this, &MainWindow::onChaptersChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::tracksChanged, this, &MainWindow::onTracksChanged);
    connect(playbackController_, &revaplayer::application::PlaybackController::clientMessageReceived, thumbnailService_, &revaplayer::services::media::ThumbnailService::handleMpvClientMessage);
    connect(playbackController_, &revaplayer::application::PlaybackController::errorOccurred, this, &MainWindow::showError);
    connect(previewRequestTimer_, &QTimer::timeout, this, &MainWindow::requestPendingThumbnailPreview);
    connect(previewStatusTimer_, &QTimer::timeout, this, [this]() {
        onPreviewHidden();
    });
    connect(thumbnailService_, &revaplayer::services::media::ThumbnailService::thumbnailReady, this, &MainWindow::onThumbnailReady);
    connect(thumbnailService_, &revaplayer::services::media::ThumbnailService::thumbnailUnavailable, this, &MainWindow::onThumbnailUnavailable);
    if (metadataScanService_ != nullptr) {
        connect(metadataScanService_, &revaplayer::services::media::MetadataScanService::metadataReady,
                this, &MainWindow::onMediaMetadataReady);
        connect(metadataScanService_, &revaplayer::services::media::MetadataScanService::metadataUnavailable,
                this, &MainWindow::onMediaMetadataUnavailable);
    }
    thumbnailService_->setCommandDispatcher([this](const QStringList &arguments) {
        return playbackController_ != nullptr && playbackController_->executeMpvCommand(arguments);
    });
    thumbnailService_->setCurrentSource(currentMediaSource_);
    connect(burstScreenshotTimer_, &QTimer::timeout, this, &MainWindow::performBurstScreenshotStep);

}

void MainWindow::applyUiPreferences()
{
    if (settingsController_ == nullptr) {
        updatePanelPresentationMode();
        applyControlBarPreferences();
        syncPanelToggleActions();
        return;
    }

    applyLegacySidePanelBehaviorMigration(settingsController_);
    loadOverlayPanelWidthsFromSettings();

    const bool showMenuBarInWindowedMode = settingsController_->showMenuBarInWindowedMode();
    const bool showStatusBarInWindowedMode = settingsController_->showStatusBarInWindowedMode();
    preFullscreenMenuBarVisible_ = showMenuBarInWindowedMode;
    preFullscreenStatusBarVisible_ = showStatusBarInWindowedMode;
    updatePanelPresentationMode();
    applyControlBarPreferences();
    applyStartupCanvasAppearance();
    applyCustomPlaylistThemeColors();

    const bool hasSavedWindowState = settingsController_->mainWindowState().has_value();
    const bool applyStartupPanels = !settingsController_->rememberWindowState()
        || !hasSavedWindowState
        || !settingsController_->restoreSidePanelsFromWindowState();
    if (applyStartupPanels) {
        applyStartupSidePanelVisibility();
    }

    updateHomeDashboardSectionLayout();

    playlistController_->setDisplayOptions(
        settingsController_->playlistShowFullPaths(),
        settingsController_->playlistShowIndexPrefixes());

    if (playlistAutoFollowButton_ != nullptr) {
        const QSignalBlocker blocker(playlistAutoFollowButton_);
        playlistAutoFollowButton_->setChecked(settingsController_->playlistAutoFollowCurrent());
    }
    if (detailsTabs_ != nullptr) {
        syncDetailsTabStripState();
    }
    applyPlaylistPanelDisplayPreferences();
    if (themeActionGroup_ != nullptr) {
        for (QAction *action : themeActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == selectedThemeId());
        }
    }

    if (!isFullScreen()) {
        if (menuBar() != nullptr) {
            menuBar()->setVisible(showMenuBarInWindowedMode);
        }
    }
    updateStatusBarVisibility();
    rebuildPinnedCourseTabs();
    refreshPlaylistPresentationData();
    reloadHomeDashboard();
    updateHomeDashboardVisibility();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    focusCurrentPlaylistItem(false);
    updateAdaptiveUiLayout();

    syncPanelToggleActions();
}

void MainWindow::updateStatusBarVisibility()
{
    if (statusBar() == nullptr) {
        return;
    }

    const bool visible = !isFullScreen()
        && (settingsController_ == nullptr || settingsController_->showStatusBarInWindowedMode());
    statusBar()->setVisible(visible);
}

void MainWindow::loadOverlayPanelWidthsFromSettings()
{
    if (settingsController_ == nullptr) {
        playlistOverlayPanelWidth_ = 0;
        detailsOverlayPanelWidth_ = 0;
        overlayPanelWidth_ = 0;
        return;
    }

    playlistOverlayPanelWidth_ = settingsController_->playlistOverlayPanelWidth();
    detailsOverlayPanelWidth_ = settingsController_->detailsOverlayPanelWidth();
    syncOverlayPanelWidthForActivePanel();
}

void MainWindow::applyStartupSidePanelVisibility()
{
    if (settingsController_ == nullptr) {
        return;
    }

    suppressPanelPreferencePersistence_ = true;
    const bool showPlaylistOnStartup = settingsController_->showPlaylistPanelOnStartup();
    const bool showDetailsOnStartup = settingsController_->showDetailsPanelOnStartup();
    if (panelsOverlayEnabled()) {
        if (showDetailsOnStartup) {
            setSidePanelVisible(SidePanel::Playlist, false, false);
            setSidePanelVisible(SidePanel::Details, true, true);
        } else if (showPlaylistOnStartup) {
            setSidePanelVisible(SidePanel::Details, false, false);
            setSidePanelVisible(SidePanel::Playlist, true, true);
        } else {
            setSidePanelVisible(SidePanel::Playlist, false, false);
            setSidePanelVisible(SidePanel::Details, false, false);
        }
    } else {
        setSidePanelVisible(SidePanel::Playlist, showPlaylistOnStartup, showPlaylistOnStartup);
        setSidePanelVisible(SidePanel::Details, showDetailsOnStartup, showDetailsOnStartup && !showPlaylistOnStartup);
    }
    suppressPanelPreferencePersistence_ = false;
}

bool MainWindow::ensureBookmarkStorageReady(const bool announceFailure)
{
    if (bookmarkController_ == nullptr) {
        if (announceFailure && statusBar() != nullptr) {
            statusBar()->showMessage(uiText("Bookmark storage is not available."), 5000);
        }
        return false;
    }

    if (bookmarkController_->isReady()) {
        return true;
    }

    if (bookmarkController_->initialize()) {
        return true;
    }

    if (announceFailure && statusBar() != nullptr) {
        const QString reason = bookmarkController_->lastError().trimmed();
        statusBar()->showMessage(
            reason.isEmpty()
                ? uiText("Bookmark storage is not available.")
                : uiText("Bookmark storage is not available: %1").arg(reason),
            6000);
    }
    return false;
}

void MainWindow::applyShortcutPreferences()
{
    QHash<QAction *, QList<QKeySequence>> shortcutsByAction;
    QSet<QAction *> configuredActions;

    for (const auto &definition : kShortcutDefinitions) {
        QAction *action = actionForShortcutId(QString::fromLatin1(definition.id));
        if (action == nullptr) {
            continue;
        }
        configuredActions.insert(action);

        if (!actions().contains(action)) {
            addAction(action);
        }
        action->setShortcutContext(QString::fromLatin1(definition.id) == QStringLiteral("open_folder")
                                       ? Qt::ApplicationShortcut
                                       : Qt::WindowShortcut);

        const QKeySequence defaultSequence = portableShortcut(definition.defaultSequence);
        const QString overrideSequence = settingsController_ != nullptr
            ? settingsController_->shortcutOverride(QString::fromLatin1(definition.id))
            : QString {};
        const QKeySequence effectiveSequence = overrideSequence.trimmed().isEmpty()
            ? defaultSequence
            : QKeySequence(overrideSequence, QKeySequence::PortableText);

        if (!effectiveSequence.isEmpty()) {
            shortcutsByAction[action].push_back(effectiveSequence);
        }
    }

    for (auto it = shortcutsByAction.cbegin(); it != shortcutsByAction.cend(); ++it) {
        it.key()->setShortcuts(it.value());
    }
    for (QAction *action : configuredActions) {
        if (action != nullptr && !shortcutsByAction.contains(action)) {
            action->setShortcut(QKeySequence {});
        }
    }
}

bool MainWindow::triggerConfiguredShortcut(const QKeyEvent *event)
{
    if (event == nullptr
        || QApplication::activePopupWidget() != nullptr
        || QApplication::activeModalWidget() != nullptr
        || isTextInputLikeWidget(QApplication::focusWidget())) {
        return false;
    }

    QWidget *activeWindow = QApplication::activeWindow();
    if (activeWindow != nullptr && activeWindow != this) {
        return false;
    }

    const QKeySequence pressedSequence = keySequenceForKeyEvent(event);
    if (pressedSequence.isEmpty()) {
        return false;
    }

    QSet<QAction *> inspectedActions;
    for (const auto &definition : kShortcutDefinitions) {
        QAction *action = actionForShortcutId(QString::fromLatin1(definition.id));
        if (action == nullptr || inspectedActions.contains(action) || !action->isEnabled()) {
            continue;
        }
        inspectedActions.insert(action);

        if (event->isAutoRepeat() && !action->autoRepeat()) {
            continue;
        }

        const QList<QKeySequence> shortcuts = action->shortcuts();
        for (const QKeySequence &shortcut : shortcuts) {
            if (!shortcut.isEmpty() && pressedSequence.matches(shortcut) == QKeySequence::ExactMatch) {
                action->trigger();
                return true;
            }
        }
    }

    return false;
}

void MainWindow::applyControlBarPreferences()
{
    if (controlBar_ == nullptr) {
        return;
    }

    const ControlBarPanelButtonSettings panelButtonSettings = resolvedControlBarPanelButtonSettings(settingsController_);
    constexpr int defaultTimelineThickness = 8;
    constexpr int defaultTimelineHandleSize = 14;
    controlBar_->setExpressiveLabelsEnabled(expressiveControlLabelsEnabled());
    controlBar_->setTimelineThickness(
        customSettingInt(settingsController_, kControlBarTimelineThicknessSetting, defaultTimelineThickness, 4, 18));
    controlBar_->setTimelineHandleSize(
        customSettingInt(settingsController_, kControlBarTimelineHandleSizeSetting, defaultTimelineHandleSize, 10, 30));
    controlBar_->setVolumeSliderThickness(
        customSettingInt(settingsController_, kControlBarVolumeSliderThicknessSetting, kDefaultControlBarVolumeSliderThickness, 4, 18));
    controlBar_->setVolumeSliderPreferredWidth(resolvedControlBarVolumeSliderWidth(settingsController_));
    controlBar_->setShowOpenButton(
        customSettingFlag(settingsController_, kControlBarShowOpenButtonSetting, true));
    controlBar_->setShowStopButton(
        customSettingFlag(settingsController_, kControlBarShowStopButtonSetting, false));
    controlBar_->setShowPlaylistButton(panelButtonSettings.showPlaylistButton);
    controlBar_->setShowDetailsButton(panelButtonSettings.showDetailsButton);
    controlBar_->setShowTimeLabel(
        customSettingFlag(settingsController_, kControlBarShowTimeLabelSetting, true));
    controlBar_->setShowSpeedButton(
        customSettingFlag(settingsController_, kControlBarShowSpeedButtonSetting, true));
    controlBar_->setShowRepeatLoopButtons(
        customSettingFlag(settingsController_, kControlBarShowRepeatLoopButtonsSetting, false));
    controlBar_->setShowTrackMenus(
        customSettingFlag(settingsController_, kControlBarShowTrackMenusSetting, true));
    controlBar_->setShowVolumeControls(
        customSettingFlag(settingsController_, kControlBarShowVolumeControlsSetting, true));
    controlBar_->setShowFullscreenButton(
        customSettingFlag(settingsController_, kControlBarShowFullscreenButtonSetting, true));
}

void MainWindow::updateControlBarBufferedState()
{
    if (controlBar_ == nullptr) {
        return;
    }

    if (!mediaLoaded_ || loadingMedia_ || currentDurationSeconds_ <= 0.0) {
        controlBar_->setBufferedPosition(0.0);
        return;
    }

    const QString effectiveSource = currentDiagnostics_.source.trimmed().isEmpty()
        ? currentMediaSource_.trimmed()
        : currentDiagnostics_.source.trimmed();
    const double cachedAheadSeconds = std::max(0.0, currentDiagnostics_.cacheDurationSeconds);
    if (!sourceUsesStreamingCache(effectiveSource) || cachedAheadSeconds <= 0.05) {
        controlBar_->setBufferedPosition(0.0);
        return;
    }

    controlBar_->setBufferedPosition(std::clamp(
        currentPositionSeconds_ + cachedAheadSeconds,
        0.0,
        currentDurationSeconds_));
}

void MainWindow::applyPlaybackProfile(const bool announce)
{
    const revaplayer::domain::PlayerProfile profile = settingsController_ != nullptr
        ? settingsController_->playbackProfile()
        : revaplayer::domain::PlayerProfile::Balanced;
    const PlaybackPowerPolicy powerPolicy = playbackPowerPolicyForProfile(profile);

    playbackUiRefreshIntervalMs_ = powerPolicy.playbackUiRefreshIntervalMs;
    playlistPlaybackRefreshIntervalMs_ = powerPolicy.playlistPlaybackRefreshIntervalMs;
    progressSaveDeltaSeconds_ = powerPolicy.progressSaveDeltaSeconds;
    deferredStartupRefreshMs_ = powerPolicy.deferredStartupRefreshMs;
    playlistThumbnailInitialRows_ = powerPolicy.playlistThumbnailInitialRows;
    playlistThumbnailLookaheadRows_ = powerPolicy.playlistThumbnailLookaheadRows;

    if (thumbnailService_ != nullptr) {
        thumbnailService_->setProfile(profile);
    }
    if (previewRequestTimer_ != nullptr && thumbnailService_ != nullptr) {
        previewRequestTimer_->setInterval(thumbnailService_->recommendedDebounceIntervalMs());
    }

    if (profileActionGroup_ != nullptr) {
        for (QAction *action : profileActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == revaplayer::domain::playerProfileId(profile));
        }
    }

    if (announce && statusBar() != nullptr) {
        statusBar()->showMessage(
            uiText("Profile applied: %1").arg(
                revaplayer::application::translateUiText(revaplayer::domain::playerProfileLabel(profile))),
            3000);
    }
}

void MainWindow::applyRuntimePreferences()
{
    if (settingsController_ != nullptr) {
        setAlwaysOnTopEnabled(settingsController_->alwaysOnTopEnabled(), false);
    } else {
        setAlwaysOnTopEnabled(false, false);
    }

    if (thumbnailService_ != nullptr) {
        thumbnailService_->setPreviewEnabled(settingsController_ == nullptr || settingsController_->thumbnailPreviewsEnabled());
        thumbnailService_->setPreviewWidthOverride(settingsController_ != nullptr ? settingsController_->thumbnailPreviewWidth() : 0);
    }
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->setPreviewWidth(thumbnailPopupWidth() > 0 ? thumbnailPopupWidth() : kDefaultThumbnailPopupWidth);
        hoverPreviewPopup_->setVerticalOffset(thumbnailPopupVerticalOffset());
        hoverPreviewPopup_->setScreenPadding(thumbnailPopupScreenPadding());
    }
    if (thumbnailService_ != nullptr && !thumbnailService_->previewEnabled()) {
        previewHoverSource_.clear();
        previewHoverBucketMilliseconds_ = -1;
        if (previewRequestTimer_ != nullptr) {
            previewRequestTimer_->stop();
        }
        if (previewStatusTimer_ != nullptr) {
            previewStatusTimer_->stop();
        }
        if (hoverPreviewPopup_ != nullptr) {
            hoverPreviewPopup_->hide();
        }
        clearSceneBrowser(uiText("Enable preview thumbnails in Preferences to use Scene Browser"));
    }

    if (!isSidePanelVisible(SidePanel::Playlist) && !isSidePanelVisible(SidePanel::Details)) {
        const QString sidePanelId = defaultSidePanelId();
        if (sidePanelId == QStringLiteral("details")) {
            activeSidePanel_ = SidePanel::Details;
        } else if (sidePanelId == QStringLiteral("playlist")) {
            activeSidePanel_ = SidePanel::Playlist;
        }
    }

    bool sceneStepChanged = false;
    if (sceneStepSpinBox_ != nullptr) {
        const int configuredSceneStep = sceneBrowserStepSeconds();
        sceneStepChanged = sceneStepSpinBox_->value() != configuredSceneStep;
        const QSignalBlocker blocker(sceneStepSpinBox_);
        sceneStepSpinBox_->setValue(configuredSceneStep);
    }

    const int shortStep = shortSeekStepSeconds();
    const int longStep = longSeekStepSeconds();
    const int configuredVolumeStep = volumeStep();

    if (seekBackwardShortAction_ != nullptr) {
        seekBackwardShortAction_->setText(uiText("Seek Backward %1s").arg(shortStep));
    }
    if (seekForwardShortAction_ != nullptr) {
        seekForwardShortAction_->setText(uiText("Seek Forward %1s").arg(shortStep));
    }
    if (seekBackwardLongAction_ != nullptr) {
        seekBackwardLongAction_->setText(uiText("Seek Backward %1s").arg(longStep));
    }
    if (seekForwardLongAction_ != nullptr) {
        seekForwardLongAction_->setText(uiText("Seek Forward %1s").arg(longStep));
    }
    if (volumeDownAction_ != nullptr) {
        volumeDownAction_->setText(uiText("Volume Down %1%").arg(configuredVolumeStep));
    }
    if (volumeUpAction_ != nullptr) {
        volumeUpAction_->setText(uiText("Volume Up %1%").arg(configuredVolumeStep));
    }
    if (toggleMuteAction_ != nullptr) {
        toggleMuteAction_->setText(muted_ ? uiText("Unmute") : uiText("Mute"));
    }

    if (controlBar_ != nullptr) {
        controlBar_->setExpressiveLabelsEnabled(expressiveControlLabelsEnabled());
    }
    if (playlistAutoFolderButton_ != nullptr) {
        const QSignalBlocker blocker(playlistAutoFolderButton_);
        playlistAutoFolderButton_->setChecked(autoLoadSiblingMediaEnabled());
    }
    if (playlistAutoFollowButton_ != nullptr) {
        const QSignalBlocker blocker(playlistAutoFollowButton_);
        playlistAutoFollowButton_->setChecked(playlistAutoFollowEnabled());
    }
    updatePlaylistMetadataScanButtonState();
    playlistController_->setDisplayOptions(playlistShowFullPathsEnabled(), playlistShowIndexPrefixesEnabled());
    refreshPlaylistSummary();

    actionFeedbackOverlayEnabled_ = actionFeedbackOverlayEnabled();
    if (!actionFeedbackOverlayEnabled_ && videoViewport_ != nullptr) {
        videoViewport_->hideActionOverlay();
    }
    if (videoViewport_ != nullptr) {
        videoViewport_->setMouseGesturesEnabled(customSettingFlag(settingsController_, kGestureEnabledSetting, true));
        videoViewport_->setMouseGestureThreshold(customSettingInt(settingsController_, kGestureThresholdSetting, 54, 24, 220));
    }

    subtitleVisible_ = settingsController_ == nullptr || settingsController_->subtitleVisible();
    currentSubtitleScale_ = settingsController_ != nullptr ? settingsController_->subtitleScale() : 1.0;
    currentSubtitlePosition_ = settingsController_ != nullptr ? settingsController_->subtitlePosition() : 100;
    currentSubtitleFontFamily_ = settingsController_ != nullptr ? settingsController_->subtitleFontFamily() : QStringLiteral("sans-serif");
    currentSubtitleFontSize_ = settingsController_ != nullptr ? settingsController_->subtitleFontSize() : 38;
    currentSubtitleAssOverride_ = settingsController_ != nullptr ? settingsController_->subtitleAssOverride() : QStringLiteral("scale");
    if (subtitleLanguageHintEdit_ != nullptr && settingsController_ != nullptr) {
        subtitleLanguageHintEdit_->setText(settingsController_->subtitlePreferredLanguages());
    }
    applySubtitlePreviewState(
        subtitleVisible_,
        currentSubtitleScale_,
        currentSubtitlePosition_,
        currentSubtitleFontFamily_,
        currentSubtitleFontSize_,
        currentSubtitleAssOverride_,
        true);
    applyAdvancedSubtitlePreferences();
    syncSubtitleActionStates();
    populateSecondarySubtitleOptions();
    if (videoDeinterlaceCheckBox_ != nullptr) {
        const QSignalBlocker blocker(videoDeinterlaceCheckBox_);
        videoDeinterlaceCheckBox_->setChecked(deinterlaceEnabled_);
    }

    if (mediaLoaded_) {
        refreshSceneBrowserPrompt(!sceneStepChanged);
    }
    updateBookmarkSelectionPreview();
    applyAudioFilterState();
    applyVideoFilterState();

    updateActionStates();
    updateMediaInformationOverlay();
    updateFullscreenChromeMode();
}

void MainWindow::updateVideoPointerAutoHide()
{
    if (videoViewport_ == nullptr) {
        return;
    }

    videoViewport_->setPointerAutoHideEnabled(mediaLoaded_ && hasVideoTrack_ && !loadingMedia_ && !errorStateActive_);
}

bool MainWindow::startSystemdDisplayInhibitor()
{
    if (displaySleepInhibitorProcess_ != nullptr
        && displaySleepInhibitorProcess_->state() != QProcess::NotRunning) {
        return true;
    }

    const QString systemdInhibit = QStandardPaths::findExecutable(QStringLiteral("systemd-inhibit"));
    const QString sleepBinary = QStandardPaths::findExecutable(QStringLiteral("sleep"));
    if (systemdInhibit.isEmpty() || sleepBinary.isEmpty()) {
        return false;
    }

    if (displaySleepInhibitorProcess_ == nullptr) {
        displaySleepInhibitorProcess_ = new QProcess(this);
        displaySleepInhibitorProcess_->setProcessChannelMode(QProcess::MergedChannels);
    }

    displaySleepInhibitorProcess_->setProgram(systemdInhibit);
    displaySleepInhibitorProcess_->setArguments({
        QStringLiteral("--what=idle:sleep"),
        QStringLiteral("--who=Reva Player"),
        QStringLiteral("--why=Video playback is active"),
        QStringLiteral("--mode=block"),
        sleepBinary,
        QStringLiteral("infinity"),
    });
    displaySleepInhibitorProcess_->start();
    if (!displaySleepInhibitorProcess_->waitForStarted(250)) {
        displaySleepInhibitorProcess_->deleteLater();
        displaySleepInhibitorProcess_ = nullptr;
        return false;
    }

    return true;
}

void MainWindow::stopSystemdDisplayInhibitor()
{
    if (displaySleepInhibitorProcess_ == nullptr) {
        return;
    }

    if (displaySleepInhibitorProcess_->state() != QProcess::NotRunning) {
        displaySleepInhibitorProcess_->terminate();
        if (!displaySleepInhibitorProcess_->waitForFinished(700)) {
            displaySleepInhibitorProcess_->kill();
            displaySleepInhibitorProcess_->waitForFinished(700);
        }
    }
    displaySleepInhibitorProcess_->deleteLater();
    displaySleepInhibitorProcess_ = nullptr;
}

void MainWindow::acquireDisplaySleepInhibition()
{
    if (displaySleepInhibited_) {
        return;
    }

    bool inhibited = false;
    inhibited = callSessionInhibit(
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/org/freedesktop/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("Inhibit"),
        {QStringLiteral("Reva Player"), uiText("Video playback is active")},
        &displaySleepScreenSaverCookie_) || inhibited;
    inhibited = callSessionInhibit(
        QStringLiteral("org.gnome.SessionManager"),
        QStringLiteral("/org/gnome/SessionManager"),
        QStringLiteral("org.gnome.SessionManager"),
        QStringLiteral("Inhibit"),
        {QStringLiteral("Reva Player"),
         uint(0),
         uiText("Video playback is active"),
         uint(12)},
        &displaySleepGnomeSessionCookie_) || inhibited;
    inhibited = startSystemdDisplayInhibitor() || inhibited;

    const int xdgExitCode = QProcess::execute(
        QStringLiteral("xdg-screensaver"),
        {QStringLiteral("suspend"), QString::number(static_cast<qulonglong>(winId()))});
    displaySleepXdgSuspended_ = xdgExitCode == 0;
    inhibited = displaySleepXdgSuspended_ || inhibited;

    displaySleepInhibited_ = inhibited;
}

void MainWindow::releaseDisplaySleepInhibition()
{
    if (displaySleepScreenSaverCookie_ != 0) {
        callSessionUninhibit(
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("UnInhibit"),
            displaySleepScreenSaverCookie_);
        displaySleepScreenSaverCookie_ = 0;
    }

    if (displaySleepGnomeSessionCookie_ != 0) {
        callSessionUninhibit(
            QStringLiteral("org.gnome.SessionManager"),
            QStringLiteral("/org/gnome/SessionManager"),
            QStringLiteral("org.gnome.SessionManager"),
            QStringLiteral("Uninhibit"),
            displaySleepGnomeSessionCookie_);
        displaySleepGnomeSessionCookie_ = 0;
    }

    stopSystemdDisplayInhibitor();

    if (displaySleepXdgSuspended_) {
        QProcess::execute(
            QStringLiteral("xdg-screensaver"),
            {QStringLiteral("resume"), QString::number(static_cast<qulonglong>(winId()))});
        displaySleepXdgSuspended_ = false;
    }

    displaySleepInhibited_ = false;
}

void MainWindow::updateDisplaySleepInhibition()
{
    const bool shouldInhibit = mediaLoaded_
        && hasVideoTrack_
        && !playbackPaused_
        && !loadingMedia_
        && !errorStateActive_
        && !stopRequested_
        && !endOfFilePending_;

    if (shouldInhibit == displaySleepInhibited_) {
        return;
    }

    if (shouldInhibit) {
        acquireDisplaySleepInhibition();
    } else {
        releaseDisplaySleepInhibition();
    }
}

void MainWindow::syncVideoActionStates()
{
    if (aspectActionGroup_ != nullptr) {
        for (QAction *action : aspectActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == currentAspectOverride_);
        }
    }

    if (cropActionGroup_ != nullptr) {
        for (QAction *action : cropActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == currentCropPreset_);
        }
    }

    if (rotateActionGroup_ != nullptr) {
        for (QAction *action : rotateActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toInt() == currentVideoRotationDegrees_);
        }
    }

    if (deinterlaceAction_ != nullptr) {
        const QSignalBlocker blocker(deinterlaceAction_);
        deinterlaceAction_->setChecked(deinterlaceEnabled_);
    }
}

void MainWindow::rebuildVideoQualityMenu(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    if (videoQualityMenu_ == nullptr || videoQualityActionGroup_ == nullptr) {
        return;
    }

    QMenu *controlQualityMenu = controlBar_ != nullptr ? controlBar_->qualityMenu() : nullptr;
    const QList<QAction *> existingActions = videoQualityActionGroup_->actions();
    for (QAction *action : existingActions) {
        videoQualityActionGroup_->removeAction(action);
        delete action;
    }

    videoQualityMenu_->clear();
    if (controlQualityMenu != nullptr) {
        controlQualityMenu->clear();
    }

    const QVector<revaplayer::domain::TrackInfo> videoTracks = collapsedVideoQualityTracks(tracks);

    if (videoTracks.isEmpty()) {
        QAction *placeholder = videoQualityMenu_->addAction(uiText("No quality options available"));
        placeholder->setEnabled(false);
        if (controlQualityMenu != nullptr) {
            QAction *controlPlaceholder = controlQualityMenu->addAction(uiText("No quality options available"));
            controlPlaceholder->setEnabled(false);
        }
        videoQualityMenu_->setEnabled(false);
        return;
    }

    for (const auto &track : videoTracks) {
        QAction *action = new QAction(buildSimpleVideoQualityLabel(track), this);
        action->setCheckable(true);
        action->setChecked(track.selected);
        action->setData(track.id);
        videoQualityActionGroup_->addAction(action);
        videoQualityMenu_->addAction(action);
        if (controlQualityMenu != nullptr) {
            controlQualityMenu->addAction(action);
        }
        connect(action, &QAction::triggered, this, [this, action](const bool checked) {
            if (!checked || playbackController_ == nullptr) {
                return;
            }

            const int trackId = action->data().toInt();
            playbackController_->selectTrack(revaplayer::domain::TrackType::Video, trackId);
            statusBar()->showMessage(uiText("Video quality: %1").arg(action->text()), 2500);
        });
    }

    videoQualityMenu_->setEnabled(mediaLoaded_ && hasVideoTrack_);
}

void MainWindow::rebuildControlBarSubtitleMenu(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    if (controlBar_ == nullptr
        || controlBar_->subtitleMenu() == nullptr
        || toggleSubtitleVisibilityAction_ == nullptr
        || loadSubtitleAction_ == nullptr) {
        return;
    }

    if (controlBarSubtitleTrackActionGroup_ == nullptr) {
        controlBarSubtitleTrackActionGroup_ = new QActionGroup(this);
        controlBarSubtitleTrackActionGroup_->setExclusive(true);
    }

    const QList<QAction *> existingActions = controlBarSubtitleTrackActionGroup_->actions();
    for (QAction *action : existingActions) {
        controlBarSubtitleTrackActionGroup_->removeAction(action);
        delete action;
    }

    QMenu *subtitleMenu = controlBar_->subtitleMenu();
    subtitleMenu->clear();
    subtitleMenu->addAction(toggleSubtitleVisibilityAction_);
    subtitleMenu->addSeparator();

    QVector<revaplayer::domain::TrackInfo> subtitleTracks;
    subtitleTracks.reserve(tracks.size());
    for (const auto &track : tracks) {
        if (track.type != revaplayer::domain::TrackType::Subtitle) {
            continue;
        }

        subtitleTracks.push_back(track);
    }

    if (subtitleTracks.isEmpty()) {
        QAction *placeholder = subtitleMenu->addAction(uiText("No subtitles available"));
        placeholder->setEnabled(false);
    } else {
        for (const auto &track : subtitleTracks) {
            QAction *action = new QAction(buildSimpleSubtitleLabel(track), this);
            action->setCheckable(true);
            action->setChecked(track.selected);
            action->setData(track.id);
            controlBarSubtitleTrackActionGroup_->addAction(action);
            subtitleMenu->addAction(action);
            connect(action, &QAction::triggered, this, [this, action](const bool checked) {
                if (!checked || playbackController_ == nullptr) {
                    return;
                }

                playbackController_->setSubtitleVisible(true);
                playbackController_->selectTrack(revaplayer::domain::TrackType::Subtitle, action->data().toInt());
                statusBar()->showMessage(uiText("Subtitles: %1").arg(action->text()), 2500);
            });
        }
    }

    subtitleMenu->addSeparator();
    subtitleMenu->addAction(loadSubtitleAction_);
}

void MainWindow::syncSubtitleActionStates()
{
    if (toggleSubtitleVisibilityAction_ != nullptr) {
        const QSignalBlocker blocker(toggleSubtitleVisibilityAction_);
        toggleSubtitleVisibilityAction_->setChecked(subtitleVisible_);
    }

    if (subtitleOverrideActionGroup_ != nullptr) {
        for (QAction *action : subtitleOverrideActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(
                revaplayer::application::normalizeSubtitleAssOverride(action->data().toString()) == currentSubtitleAssOverride_);
        }
    }
}

void MainWindow::updateMediaInformationOverlay()
{
    if (mediaInformationOverlay_ == nullptr) {
        return;
    }

    const bool visible = mediaInformationOverlayVisible_ && mediaLoaded_ && !loadingMedia_ && !errorStateActive_;
    if (!visible) {
        if (!mediaLoaded_ || loadingMedia_ || errorStateActive_) {
            mediaInformationOverlayVisible_ = false;
            if (mediaInformationOverlayTimer_ != nullptr) {
                mediaInformationOverlayTimer_->stop();
            }
        }
        mediaInformationOverlay_->hide();
        return;
    }

    revaplayer::domain::PlaybackDiagnostics diagnostics = currentDiagnostics_;
    if (diagnostics.source.trimmed().isEmpty()) {
        diagnostics.source = currentMediaSource_;
    }

    mediaInformationOverlay_->setInformationText(revaplayer::application::buildMediaInformationOverlayText(
        diagnostics,
        currentTracks_,
        effectiveCurrentMediaTitle(),
        currentPositionSeconds_,
        currentDurationSeconds_,
        currentSpeed_));
    mediaInformationOverlay_->show();
    mediaInformationOverlay_->raise();
}

void MainWindow::showMediaInformationOverlay()
{
    if (!mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        statusBar()->showMessage(uiText("Load media before showing media information."), 3000);
        return;
    }

    mediaInformationOverlayVisible_ = true;
    updateMediaInformationOverlay();
    if (mediaInformationOverlayTimer_ != nullptr) {
        mediaInformationOverlayTimer_->start(5000);
    }
}

void MainWindow::hideMediaInformationOverlay()
{
    mediaInformationOverlayVisible_ = false;
    if (mediaInformationOverlay_ != nullptr) {
        mediaInformationOverlay_->hide();
    }
}

void MainWindow::updateMediaInfoDialog(const bool force)
{
    if (mediaInfoDialog_ == nullptr) {
        return;
    }

    if (!force && !mediaInfoDialog_->isVisible()) {
        return;
    }

    if (!force
        && mediaInfoDialogRefreshElapsed_.isValid()
        && mediaInfoDialogRefreshElapsed_.elapsed() < kMediaInfoDialogRefreshIntervalMs) {
        return;
    }

    if (!mediaLoaded_) {
        mediaInfoDialog_->setReport(QStringLiteral("No media loaded."));
        mediaInfoDialogRefreshElapsed_.restart();
        return;
    }

    revaplayer::domain::PlaybackDiagnostics diagnostics = currentDiagnostics_;
    if (diagnostics.source.trimmed().isEmpty()) {
        diagnostics.source = currentMediaSource_;
    }

    mediaInfoDialog_->setReport(revaplayer::application::buildMediaInfoReport(
        effectiveCurrentMediaTitle(),
        diagnostics,
        currentTracks_,
        currentDurationSeconds_));
    mediaInfoDialogRefreshElapsed_.restart();
}

void MainWindow::requestPendingThumbnailPreview()
{
    if (thumbnailService_ == nullptr
        || previewHoverSource_.trimmed().isEmpty()
        || previewHoverBucketMilliseconds_ < 0) {
        return;
    }

    if (!isVisible() || isMinimized()) {
        onPreviewHidden();
        return;
    }

    if (previewHoverSource_ != currentMediaSource_.trimmed()) {
        onPreviewHidden();
        return;
    }

    thumbnailService_->requestThumbnail(previewHoverSource_, previewHoverBucketMilliseconds_ / 1000.0);
}

void MainWindow::requestPlaylistThumbnailBatch(const bool includeInitialRows)
{
    if (playlistView_ == nullptr || playlistView_->model() == nullptr) {
        return;
    }
    if (!isVisible()
        || isMinimized()
        || playlistDock_ == nullptr
        || !playlistDock_->isVisible()) {
        return;
    }

    const int rowCount = playlistView_->model()->rowCount();
    if (rowCount <= 0) {
        return;
    }

    QVector<int> rows;
    rows.reserve(playlistThumbnailInitialRows_ + playlistThumbnailLookaheadRows_ + 12);
    const auto pushRow = [&rows, rowCount](const int row) {
        if (row >= 0 && row < rowCount && !rows.contains(row)) {
            rows.push_back(row);
        }
    };

    if (includeInitialRows) {
        for (int row = 0; row < std::min(rowCount, playlistThumbnailInitialRows_); ++row) {
            pushRow(row);
        }
    }

    const int topRow = playlistVisibleRowFromTop(playlistView_);
    int bottomRow = playlistVisibleRowFromBottom(playlistView_);
    if (topRow >= 0) {
        if (bottomRow < topRow) {
            bottomRow = std::min(rowCount - 1, topRow + playlistThumbnailInitialRows_ - 1);
        }
        for (int row = topRow; row <= std::min(rowCount - 1, bottomRow + playlistThumbnailLookaheadRows_); ++row) {
            pushRow(row);
        }
    }

    for (const int row : std::as_const(rows)) {
        const QModelIndex index = playlistView_->model()->index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        enqueuePlaylistThumbnail(
            index.data(revaplayer::application::PlaylistRoles::SourceRole).toString(),
            index.data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble());
    }

    startNextPlaylistThumbnailRequest();
}

void MainWindow::enqueuePlaylistThumbnail(const QString &source, const double durationSeconds)
{
    if (playlistThumbnailProcess_ == nullptr) {
        return;
    }

    const QString normalizedSource = source.trimmed();
    if (normalizedSource.isEmpty()
        || playlistThumbnailCache_.contains(normalizedSource)
        || playlistThumbnailPendingBuckets_.contains(normalizedSource)
        || playlistThumbnailQueuedDurations_.contains(normalizedSource)
        || activePlaylistThumbnailSource_ == normalizedSource) {
        return;
    }

    QString localPath;
    QString requestKey;
    QString localSuffix;
    qint64 localLastModifiedMs = -1;
    qint64 localSize = -1;
    bool localFileReady = false;
    const auto fsIt = mediaFileSystemCache_.constFind(normalizedSource);
    if (fsIt != mediaFileSystemCache_.constEnd() && fsIt->exists && fsIt->isFile) {
        localPath = fsIt->absoluteFilePath;
        localSuffix = fsIt->suffix;
        localLastModifiedMs = fsIt->lastModifiedMs;
        localSize = fsIt->size;
        localFileReady = true;
        requestKey = playlistThumbnailRequestKeyForFileMetadata(
            fsIt->absoluteFilePath,
            fsIt->lastModifiedMs,
            fsIt->size,
            durationSeconds);
    } else {
        localPath = playlistThumbnailLocalPath(normalizedSource);
    }

    if (localPath.isEmpty()) {
        return;
    }
    if (localFileReady) {
        if (!isLikelyVideoMediaSuffix(localSuffix)) {
            return;
        }
    } else {
        const QFileInfo localInfo(localPath);
        if (!isLikelyVideoMediaFile(localInfo)) {
            return;
        }
        if (!localInfo.exists() || !localInfo.isFile()) {
            return;
        }
        localPath = localInfo.absoluteFilePath();
        localLastModifiedMs = localInfo.lastModified().toMSecsSinceEpoch();
        localSize = localInfo.size();
        localFileReady = true;
    }
    if (!localFileReady) {
        return;
    }

    const QString scanKey = mediaScanSourceKey(normalizedSource);
    if (!scanKey.isEmpty()) {
        auto scanIt = mediaScanCache_.constFind(scanKey);
        if (scanIt == mediaScanCache_.constEnd() && !mediaScanCacheLookupCompleted_.contains(scanKey)) {
            mediaScanCacheLookupCompleted_.insert(scanKey);
            if (const auto cachedScan = loadCachedMediaScanResult(settingsController_, scanKey); cachedScan.has_value()) {
                mediaScanCache_.insert(scanKey, *cachedScan);
                scanIt = mediaScanCache_.constFind(scanKey);
            }
        }
        if (scanIt != mediaScanCache_.constEnd() && !scanIt->hasVideoTrack) {
            return;
        }
    }

    if (requestKey.isEmpty()) {
        requestKey = playlistThumbnailRequestKey(normalizedSource, durationSeconds);
    }
    if (requestKey.isEmpty() || playlistThumbnailFailedRequestKeys_.contains(requestKey)) {
        return;
    }

    QImage cachedImage;
    if (loadPlaylistThumbnailDiskCacheForFileMetadata(
            localPath,
            localLastModifiedMs,
            localSize,
            durationSeconds,
            &cachedImage)
        && !cachedImage.isNull()) {
        playlistThumbnailCache_.insert(normalizedSource, cachedImage);
        schedulePlaylistMetadataRefresh(120);
        return;
    }

    playlistThumbnailQueue_.push_back(normalizedSource);
    playlistThumbnailQueuedDurations_.insert(normalizedSource, std::max(0.0, durationSeconds));
}

void MainWindow::startNextPlaylistThumbnailRequest()
{
    if (playlistThumbnailProcess_ == nullptr
        || playlistThumbnailProcess_->state() != QProcess::NotRunning
        || !activePlaylistThumbnailSource_.isEmpty()) {
        return;
    }
    if (!isVisible()
        || isMinimized()
        || playlistDock_ == nullptr
        || !playlistDock_->isVisible()) {
        return;
    }

    while (!playlistThumbnailQueue_.isEmpty()) {
        const QString source = playlistThumbnailQueue_.takeFirst();
        const double durationSeconds = playlistThumbnailQueuedDurations_.take(source);
        if (source.trimmed().isEmpty() || playlistThumbnailCache_.contains(source)) {
            continue;
        }

        QString localPath;
        QString requestKey;
        const auto fsIt = mediaFileSystemCache_.constFind(source);
        if (fsIt != mediaFileSystemCache_.constEnd() && fsIt->exists && fsIt->isFile) {
            localPath = fsIt->absoluteFilePath;
            requestKey = playlistThumbnailRequestKeyForFileMetadata(
                fsIt->absoluteFilePath,
                fsIt->lastModifiedMs,
                fsIt->size,
                durationSeconds);
        } else {
            localPath = playlistThumbnailLocalPath(source);
            requestKey = playlistThumbnailRequestKey(source, durationSeconds);
        }
        const QString outputPath = playlistThumbnailCachePathForRequestKey(requestKey);
        if (localPath.isEmpty() || requestKey.isEmpty() || outputPath.isEmpty()) {
            continue;
        }
        if (playlistThumbnailFailedRequestKeys_.contains(requestKey)) {
            continue;
        }

        QFile::remove(outputPath);
        activePlaylistThumbnailSource_ = source;
        activePlaylistThumbnailRequestKey_ = requestKey;
        activePlaylistThumbnailOutputPath_ = outputPath;

        const double requestTime = playlistThumbnailTimeSeconds(durationSeconds);
        const QStringList arguments {
            QStringLiteral("-v"),
            QStringLiteral("error"),
            QStringLiteral("-ss"),
            QString::number(requestTime, 'f', 3),
            QStringLiteral("-i"),
            localPath,
            QStringLiteral("-frames:v"),
            QStringLiteral("1"),
            QStringLiteral("-vf"),
            QStringLiteral("scale=%1:-1:force_original_aspect_ratio=decrease").arg(kPlaylistThumbnailPreviewWidth),
            QStringLiteral("-q:v"),
            QStringLiteral("4"),
            QStringLiteral("-y"),
            outputPath,
        };
        playlistThumbnailProcess_->start(QStringLiteral("ffmpeg"), arguments);
        return;
    }
}

void MainWindow::handlePlaylistThumbnailProcessFinished(const int exitCode, const bool normalExit)
{
    if (activePlaylistThumbnailRequestKey_.isEmpty()) {
        return;
    }

    const QString source = activePlaylistThumbnailSource_;
    const QString requestKey = activePlaylistThumbnailRequestKey_;
    const QString outputPath = activePlaylistThumbnailOutputPath_;
    activePlaylistThumbnailSource_.clear();
    activePlaylistThumbnailRequestKey_.clear();
    activePlaylistThumbnailOutputPath_.clear();

    QImage image;
    const bool loaded = normalExit
        && exitCode == 0
        && !outputPath.trimmed().isEmpty()
        && image.load(outputPath)
        && !image.isNull();
    if (loaded) {
        playlistThumbnailCache_.insert(source, image);
        schedulePlaylistMetadataRefresh(120);
    } else {
        playlistThumbnailFailedRequestKeys_.insert(requestKey);
        QFile::remove(outputPath);
    }

    startNextPlaylistThumbnailRequest();
}

void MainWindow::clearPlaylistThumbnailQueue(const bool cancelActiveRequest)
{
    playlistThumbnailQueue_.clear();
    playlistThumbnailQueuedDurations_.clear();
    activePlaylistThumbnailSource_.clear();
    activePlaylistThumbnailRequestKey_.clear();
    activePlaylistThumbnailOutputPath_.clear();

    if (cancelActiveRequest && playlistThumbnailProcess_ != nullptr && playlistThumbnailProcess_->state() != QProcess::NotRunning) {
        playlistThumbnailProcess_->blockSignals(true);
        playlistThumbnailProcess_->kill();
        playlistThumbnailProcess_->waitForFinished(150);
        playlistThumbnailProcess_->blockSignals(false);
    }
}

void MainWindow::warmPlaylistThumbnail(const QString &source, const double durationSeconds)
{
    if (thumbnailService_ == nullptr || !thumbnailService_->previewEnabled()) {
        return;
    }

    const QString normalizedSource = source.trimmed();
    if (normalizedSource.isEmpty()
        || normalizedSource != currentMediaSource_.trimmed()
        || playlistThumbnailCache_.contains(normalizedSource)
        || playlistThumbnailPendingBuckets_.contains(normalizedSource)) {
        return;
    }

    const double requestTime = playlistThumbnailTimeSeconds(durationSeconds);
    const qint64 bucket = revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(requestTime);
    playlistThumbnailPendingBuckets_.insert(normalizedSource, bucket);
    thumbnailService_->requestThumbnail(normalizedSource, requestTime);
}

void MainWindow::beginLoadFeedback(const QString &displayTarget)
{
    ++idleStateGeneration_;
    persistPlaybackProgress(false, true);
    resetVideoZoomAndPan(false);
    if (previewRequestTimer_ != nullptr) {
        previewRequestTimer_->stop();
    }
    if (previewStatusTimer_ != nullptr) {
        previewStatusTimer_->stop();
    }
    previewHoverSource_.clear();
    previewHoverBucketMilliseconds_ = -1;
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
    if (thumbnailService_ != nullptr) {
        thumbnailService_->cancel();
    }
    loadingMedia_ = true;
    mediaLoaded_ = false;
    stopRequested_ = false;
    endOfFilePending_ = false;
    errorStateActive_ = false;
    playlistPlaybackRefreshElapsed_.invalidate();
    controlBar_->setPlaybackAvailable(false);
    updateHomeDashboardVisibility();
    updateControlBarBufferedState();
    updateActionStates();

    const QString label = displayTarget.trimmed().isEmpty() ? QStringLiteral("media") : displayTarget.trimmed();
    videoViewport_->setOverlayText(uiText("Loading %1").arg(label));
    videoViewport_->setOverlayVisible(true);
    statusBar()->showMessage(uiText("Loading %1").arg(label), 3000);
}

void MainWindow::finalizeActiveMediaLoadFromBackend()
{
    if (!loadingMedia_ || mediaLoaded_ || errorStateActive_) {
        return;
    }

    if (currentMediaSource_.trimmed().isEmpty()) {
        const QString fallbackSource = loadingMediaSource_.trimmed();
        if (fallbackSource.isEmpty()) {
            return;
        }
        currentMediaSource_ = fallbackSource;
    }

    onFileLoaded();
}

void MainWindow::persistPlaybackProgress(const bool completed, const bool force)
{
    if (historyController_ == nullptr || !historyController_->isReady() || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString source = currentMediaSource_.trimmed();
    if (!completed && progressResetSuppressedSources_.contains(source)) {
        return;
    }

    const QString title = source == currentMediaSource_ ? effectiveCurrentMediaTitle() : displayTitleForHistory(source, currentTitle_);
    const double safeDuration = std::max(0.0, currentDurationSeconds_);
    const bool keepHistory = historyEnabled();
    const bool keepResume = resumeEnabled();
    const QString nowIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    if (completed) {
        historyController_->markPlaybackCompleted(source, title, safeDuration, keepHistory, keepResume);
        resumeStateCache_.remove(source);
        resumeStateLookupCompleted_.insert(source);
        if (keepHistory) {
            historyEntriesBySource_.insert(
                source,
                revaplayer::infrastructure::storage::PlaybackHistoryRecord {
                    source,
                    title,
                    safeDuration,
                    safeDuration,
                    true,
                    nowIso,
                    nowIso,
                });
        } else {
            historyEntriesBySource_.remove(source);
        }
        trimHistoryToLimit();
        lastPersistedPositionSeconds_ = safeDuration;
        return;
    }

    const double safePosition = persistedPlaybackPosition(currentPositionSeconds_, safeDuration, force);
    if (!force && safePosition < 5.0) {
        return;
    }
    if (force && safePosition <= 0.0) {
        return;
    }
    if (!force
        && lastPersistedPositionSeconds_ >= 0.0
        && std::abs(safePosition - lastPersistedPositionSeconds_) < progressSaveDeltaSeconds_) {
        return;
    }

    historyController_->savePlaybackProgress(
        source,
        title,
        safePosition,
        safeDuration,
        keepHistory,
        keepResume);
    if (keepResume) {
        resumeStateCache_.insert(
            source,
            revaplayer::infrastructure::storage::ResumeStateRecord {
                source,
                title,
                safePosition,
                safeDuration,
                nowIso,
            });
        resumeStateLookupCompleted_.insert(source);
    } else {
        resumeStateCache_.remove(source);
    }
    if (keepHistory) {
        historyEntriesBySource_.insert(
            source,
            revaplayer::infrastructure::storage::PlaybackHistoryRecord {
                source,
                title,
                safePosition,
                safeDuration,
                false,
                nowIso,
                nowIso,
            });
    } else {
        historyEntriesBySource_.remove(source);
    }
    trimHistoryToLimit();
    lastPersistedPositionSeconds_ = safePosition;
}

void MainWindow::maybeResumePlayback()
{
    if (resumeAttemptedForCurrentMedia_
        || historyController_ == nullptr
        || !historyController_->isReady()
        || !resumeEnabled()
        || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    resumeAttemptedForCurrentMedia_ = true;
    const auto stateRecord = historyController_->resumeStateFor(currentMediaSource_);
    if (!stateRecord.has_value()) {
        return;
    }

    const double resumePosition = std::max(0.0, stateRecord->positionSeconds);
    const double effectiveDuration = currentDurationSeconds_ > 0.0
        ? currentDurationSeconds_
        : std::max(0.0, stateRecord->durationSeconds);

    if (resumePosition <= 0.0) {
        return;
    }

    if (effectiveDuration > 0.0 && resumePosition >= effectiveDuration - 5.0) {
        return;
    }

    playbackController_->seekToSeconds(resumePosition);
    currentPositionSeconds_ = resumePosition;
    lastPersistedPositionSeconds_ = resumePosition;
    statusBar()->showMessage(uiText("Resumed playback at %1").arg(formatPlaybackTime(resumePosition)), 3500);
}

void MainWindow::showIdleOverlay(const QString &message)
{
    const QString trimmedMessage = message.trimmed();
    videoViewport_->setOverlayText(
        trimmedMessage == defaultIdleOverlayText()
            ? defaultIdleOverlayMarkup()
            : trimmedMessage);
    videoViewport_->setOverlayVisible(true);
    updateHomeDashboardVisibility();
}

bool MainWindow::panelsOverlayEnabled() const
{
    return settingsController_ == nullptr || settingsController_->overlayPanelsOnVideo();
}

QDockWidget *MainWindow::sidePanelDock(const SidePanel panel) const
{
    return panel == SidePanel::Playlist ? playlistDock_ : detailsDock_;
}

bool MainWindow::isSidePanelVisible(const SidePanel panel) const
{
    const QDockWidget *dock = sidePanelDock(panel);
    return dock != nullptr && dock->isVisible();
}

void MainWindow::updatePanelPresentationMode()
{
    if (playlistDock_ == nullptr || detailsDock_ == nullptr || videoViewport_ == nullptr) {
        return;
    }

    const bool shouldOverlayPanels = panelsOverlayEnabled();
    if (shouldOverlayPanels == panelOverlayModeActive_) {
        updateVideoOverlayGeometry();
        return;
    }

    const bool playlistVisible = playlistDock_->isVisible();
    const bool detailsVisible = detailsDock_->isVisible();
    const auto dockFeatures = QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable;
    const auto resetDockAnimation = [](QDockWidget *dock, QPropertyAnimation *animation) {
        if (animation != nullptr && animation->state() != QAbstractAnimation::Stopped) {
            animation->stop();
        }
        if (dock != nullptr) {
            dock->setProperty("overlayHidePending", false);
        }
    };

    suppressPanelPreferencePersistence_ = true;
    if (shouldOverlayPanels) {
        resetDockAnimation(playlistDock_, playlistDockAnimation_);
        resetDockAnimation(detailsDock_, detailsDockAnimation_);
        removeDockWidget(playlistDock_);
        removeDockWidget(detailsDock_);

        playlistDock_->hide();
        detailsDock_->hide();

        playlistDock_->setParent(videoViewport_);
        detailsDock_->setParent(videoViewport_);
        playlistDock_->setAllowedAreas(Qt::NoDockWidgetArea);
        detailsDock_->setAllowedAreas(Qt::NoDockWidgetArea);
        playlistDock_->setFeatures(QDockWidget::DockWidgetClosable);
        detailsDock_->setFeatures(QDockWidget::DockWidgetClosable);

        panelOverlayModeActive_ = true;

        if (detailsVisible && activeSidePanel_ == SidePanel::Details) {
            overlayPanelWidth_ = overlayPanelWidthFor(SidePanel::Details);
            detailsDock_->show();
            detailsDock_->raise();
        } else if (playlistVisible) {
            overlayPanelWidth_ = overlayPanelWidthFor(SidePanel::Playlist);
            playlistDock_->show();
            playlistDock_->raise();
            detailsDock_->hide();
            activeSidePanel_ = SidePanel::Playlist;
        } else if (detailsVisible) {
            overlayPanelWidth_ = overlayPanelWidthFor(SidePanel::Details);
            detailsDock_->show();
            detailsDock_->raise();
            activeSidePanel_ = SidePanel::Details;
        } else {
            syncOverlayPanelWidthForActivePanel();
        }
    } else {
        resetDockAnimation(playlistDock_, playlistDockAnimation_);
        resetDockAnimation(detailsDock_, detailsDockAnimation_);
        playlistDock_->hide();
        detailsDock_->hide();

        playlistDock_->setParent(this);
        detailsDock_->setParent(this);
        playlistDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        detailsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
        playlistDock_->setFeatures(dockFeatures);
        detailsDock_->setFeatures(dockFeatures);
        addDockWidget(Qt::RightDockWidgetArea, playlistDock_);
        addDockWidget(Qt::RightDockWidgetArea, detailsDock_);
        tabifyDockWidget(playlistDock_, detailsDock_);

        panelOverlayModeActive_ = false;

        playlistDock_->setVisible(playlistVisible);
        detailsDock_->setVisible(detailsVisible);
        if (detailsVisible && activeSidePanel_ == SidePanel::Details) {
            detailsDock_->raise();
        } else if (playlistVisible) {
            playlistDock_->raise();
        } else if (detailsVisible) {
            detailsDock_->raise();
        }
    }
    suppressPanelPreferencePersistence_ = false;
    syncPanelToggleActions();
    updateVideoOverlayGeometry();
}

void MainWindow::setSidePanelVisible(const SidePanel panel, const bool visible, const bool raisePanel)
{
    QDockWidget *dock = sidePanelDock(panel);
    if (dock == nullptr) {
        return;
    }

    updatePanelPresentationMode();

    if (visible) {
        activeSidePanel_ = panel;
        if (panelOverlayModeActive_) {
            overlayPanelWidth_ = overlayPanelWidthFor(panel);
        }
        if (panelOverlayModeActive_) {
            const SidePanel otherPanel = panel == SidePanel::Playlist ? SidePanel::Details : SidePanel::Playlist;
            if (QDockWidget *otherDock = sidePanelDock(otherPanel); otherDock != nullptr && otherDock->isVisible()) {
                animateOverlayDock(otherDock, false, false);
            }
        }
        animateOverlayDock(dock, true, raisePanel);
    } else {
        sidePanelEdgeRevealActive_ = false;
        animateOverlayDock(dock, false, raisePanel);
    }

    syncPanelToggleActions();
    updateVideoOverlayGeometry();
}

int MainWindow::overlayPanelWidthFor(const SidePanel panel) const
{
    const int storedWidth = panel == SidePanel::Playlist ? playlistOverlayPanelWidth_ : detailsOverlayPanelWidth_;
    if (storedWidth > 0) {
        return storedWidth;
    }
    if (overlayPanelWidth_ > 0) {
        return overlayPanelWidth_;
    }
    return 0;
}

void MainWindow::setOverlayPanelWidthFor(const SidePanel panel, const int width, const bool persist)
{
    const int normalizedWidth = clampPersistedOverlayPanelWidth(width);
    if (panel == SidePanel::Playlist) {
        playlistOverlayPanelWidth_ = normalizedWidth;
    } else {
        detailsOverlayPanelWidth_ = normalizedWidth;
    }

    if (panel == activeSidePanel_ || !panelOverlayModeActive_ || isSidePanelVisible(panel)) {
        overlayPanelWidth_ = normalizedWidth;
    }

    if (!persist || settingsController_ == nullptr) {
        return;
    }

    if (panel == SidePanel::Playlist) {
        settingsController_->setPlaylistOverlayPanelWidth(normalizedWidth);
    } else {
        settingsController_->setDetailsOverlayPanelWidth(normalizedWidth);
    }
}

void MainWindow::syncOverlayPanelWidthForActivePanel()
{
    SidePanel panel = activeSidePanel_;
    if (panelOverlayModeActive_) {
        if (isSidePanelVisible(SidePanel::Details) && !isSidePanelVisible(SidePanel::Playlist)) {
            panel = SidePanel::Details;
        } else if (isSidePanelVisible(SidePanel::Playlist)) {
            panel = SidePanel::Playlist;
        }
    }
    overlayPanelWidth_ = overlayPanelWidthFor(panel);
}

void MainWindow::toggleSidePanel(const SidePanel panel)
{
    setSidePanelVisible(panel, !isSidePanelVisible(panel), true);
}

void MainWindow::reloadBookmarks()
{
    if (!ensureBookmarkStorageReady(false) || currentMediaSource_.trimmed().isEmpty()) {
        populateBookmarks({});
        return;
    }

    populateBookmarks(bookmarkController_->bookmarksFor(currentMediaSource_));
}

void MainWindow::removeSelectedBookmark()
{
    if (!ensureBookmarkStorageReady(true) || bookmarksList_ == nullptr) {
        return;
    }

    const QList<QListWidgetItem *> selectedItems = bookmarksList_->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    int removedCount = 0;
    for (QListWidgetItem *item : selectedItems) {
        const qint64 bookmarkId = item != nullptr ? item->data(Qt::UserRole).toLongLong() : -1;
        if (bookmarkId >= 0 && bookmarkController_->deleteBookmark(bookmarkId)) {
            ++removedCount;
        }
    }

    if (removedCount != selectedItems.size()) {
        const QString failureMessage = bookmarkController_->lastError().trimmed().isEmpty()
            ? uiText("Could not delete the bookmark.")
            : uiText("Could not delete the bookmark: %1").arg(bookmarkController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }

    reloadBookmarks();
    const QString message = removedCount == 1
        ? uiText("Bookmark deleted")
        : uiText("%1 bookmarks deleted").arg(removedCount);
    showActionResult(message, message, 3000);
}

void MainWindow::filterBookmarks()
{
    if (bookmarksList_ == nullptr) {
        return;
    }

    const QString searchText = bookmarkSearchEdit_ != nullptr ? bookmarkSearchEdit_->text().trimmed().toLower() : QString {};
    const QString categoryFilter = bookmarkCategoryComboBox_ != nullptr ? bookmarkCategoryComboBox_->currentData().toString() : QStringLiteral("*");

    for (int row = 0; row < bookmarksList_->count(); ++row) {
        QListWidgetItem *item = bookmarksList_->item(row);
        if (item == nullptr) {
            continue;
        }

        const QString category = item->data(Qt::UserRole + 4).toString().trimmed();
        const QString haystack = QStringLiteral("%1 %2 %3 %4")
            .arg(item->text(),
                 item->data(Qt::UserRole + 2).toString(),
                 item->data(Qt::UserRole + 3).toString(),
                 category)
            .toLower();

        const bool matchesSearch = searchText.isEmpty() || haystack.contains(searchText);
        const bool matchesCategory = categoryFilter == QStringLiteral("*")
            || category.compare(categoryFilter, Qt::CaseInsensitive) == 0;
        item->setHidden(!(matchesSearch && matchesCategory));
    }
}

void MainWindow::refreshBookmarkFilters()
{
    if (bookmarkCategoryComboBox_ == nullptr || bookmarksList_ == nullptr) {
        return;
    }

    const QString currentCategory = bookmarkCategoryComboBox_->currentData().toString();
    QStringList categories;
    for (int row = 0; row < bookmarksList_->count(); ++row) {
        QListWidgetItem *item = bookmarksList_->item(row);
        if (item == nullptr) {
            continue;
        }
        const QString category = item->data(Qt::UserRole + 4).toString().trimmed();
        if (!category.isEmpty() && !categories.contains(category, Qt::CaseInsensitive)) {
            categories.push_back(category);
        }
    }
    std::sort(categories.begin(), categories.end(), [](const QString &left, const QString &right) {
        return left.localeAwareCompare(right) < 0;
    });

    const QSignalBlocker blocker(bookmarkCategoryComboBox_);
    bookmarkCategoryComboBox_->clear();
    bookmarkCategoryComboBox_->addItem(uiText("All Categories"), QStringLiteral("*"));
    for (const QString &category : categories) {
        bookmarkCategoryComboBox_->addItem(category, category);
    }

    const int index = bookmarkCategoryComboBox_->findData(currentCategory);
    bookmarkCategoryComboBox_->setCurrentIndex(index >= 0 ? index : 0);
    filterBookmarks();
}

void MainWindow::importBookmarksForCurrentMedia()
{
    if (currentMediaSource_.trimmed().isEmpty()) {
        statusBar()->showMessage(uiText("Load media before importing bookmarks."), 4000);
        return;
    }

    if (!ensureBookmarkStorageReady(true)) {
        return;
    }

    const QString path = filedialog::getOpenFileName(
        this,
        uiText("Import Bookmarks"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        uiText("Bookmark Files (*.json *.csv);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        statusBar()->showMessage(uiText("Could not open the bookmark file."), 5000);
        return;
    }

    int importedCount = 0;
    const QFileInfo fileInfo(path);
    const QByteArray payload = file.readAll();
    file.close();

    if (fileInfo.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0) {
        const QStringList lines = QString::fromUtf8(payload).split(QChar('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("time_seconds"), Qt::CaseInsensitive)) {
                continue;
            }

            const QStringList fields = trimmed.split(QChar(','));
            bool ok = false;
            const double positionSeconds = fields.value(0).trimmed().toDouble(&ok);
            if (!ok) {
                continue;
            }
            const QString title = fields.value(1).trimmed();
            const QString category = fields.value(2).trimmed();
            const QString note = fields.mid(3).join(QStringLiteral(",")).trimmed();
            if (bookmarkController_->createBookmark(currentMediaSource_, title, positionSeconds, note, category).has_value()) {
                ++importedCount;
            }
        }
    } else {
        const QJsonDocument document = QJsonDocument::fromJson(payload);
        const QJsonArray bookmarksArray = document.object().value(QStringLiteral("bookmarks")).toArray();
        for (const QJsonValue &value : bookmarksArray) {
            const QJsonObject bookmarkObject = value.toObject();
            const double positionSeconds = bookmarkObject.value(QStringLiteral("position_seconds")).toDouble(-1.0);
            if (positionSeconds < 0.0) {
                continue;
            }
            if (bookmarkController_->createBookmark(
                    currentMediaSource_,
                    bookmarkObject.value(QStringLiteral("title")).toString().trimmed(),
                    positionSeconds,
                    bookmarkObject.value(QStringLiteral("note")).toString().trimmed(),
                    bookmarkObject.value(QStringLiteral("category")).toString().trimmed()).has_value()) {
                ++importedCount;
            }
        }
    }

    reloadBookmarks();
    const QString message = uiText("Imported %1 bookmarks").arg(importedCount);
    showActionResult(message, message, 4000);
}

void MainWindow::exportBookmarksForCurrentMedia() const
{
    if (bookmarksList_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString path = filedialog::getSaveFileName(
        const_cast<MainWindow *>(this),
        uiText("Export Bookmarks"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        uiText("Bookmark JSON (*.json);;Bookmark CSV (*.csv)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(path);
    if (fileInfo.suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0) {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return;
        }

        file.write("time_seconds,title,category,note\n");
        for (int row = 0; row < bookmarksList_->count(); ++row) {
            QListWidgetItem *item = bookmarksList_->item(row);
            if (item == nullptr) {
                continue;
            }
            const QString line = QStringLiteral("%1,%2,%3,%4\n")
                .arg(QString::number(item->data(Qt::UserRole + 1).toDouble(), 'f', 3),
                     item->data(Qt::UserRole + 2).toString().replace(QChar(','), QChar(' ')),
                     item->data(Qt::UserRole + 4).toString().replace(QChar(','), QChar(' ')),
                     item->data(Qt::UserRole + 3).toString().replace(QChar(','), QChar(' ')));
            file.write(line.toUtf8());
        }
        file.commit();
        return;
    }

    QJsonArray bookmarksArray;
    for (int row = 0; row < bookmarksList_->count(); ++row) {
        QListWidgetItem *item = bookmarksList_->item(row);
        if (item == nullptr) {
            continue;
        }

        QJsonObject bookmarkObject;
        bookmarkObject.insert(QStringLiteral("title"), item->data(Qt::UserRole + 2).toString());
        bookmarkObject.insert(QStringLiteral("category"), item->data(Qt::UserRole + 4).toString());
        bookmarkObject.insert(QStringLiteral("note"), item->data(Qt::UserRole + 3).toString());
        bookmarkObject.insert(QStringLiteral("position_seconds"), item->data(Qt::UserRole + 1).toDouble());
        bookmarksArray.push_back(bookmarkObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("source"), currentMediaSource_);
    root.insert(QStringLiteral("title"), currentTitle_);
    root.insert(QStringLiteral("bookmarks"), bookmarksArray);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

bool MainWindow::openLocalMediaFiles(const QStringList &requestedFiles, const bool revealPlaylistPanel)
{
    if (playbackController_ == nullptr) {
        return false;
    }

    QStringList cleanedFiles;
    QStringList requestedDirectories;
    cleanedFiles.reserve(requestedFiles.size());
    for (const QString &path : requestedFiles) {
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists()) {
            continue;
        }

        if (fileInfo.isDir()) {
            const QString normalizedDirectory = fileInfo.canonicalFilePath().isEmpty()
                ? fileInfo.absoluteFilePath()
                : fileInfo.canonicalFilePath();
            if (!requestedDirectories.contains(normalizedDirectory)) {
                requestedDirectories.push_back(normalizedDirectory);
            }
            continue;
        }

        if (!fileInfo.isFile()) {
            continue;
        }

        const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
            ? fileInfo.absoluteFilePath()
            : fileInfo.canonicalFilePath();
        if (!cleanedFiles.contains(normalizedPath)) {
            cleanedFiles.push_back(normalizedPath);
        }
    }

    if (cleanedFiles.isEmpty() && !requestedDirectories.isEmpty()) {
        const QString folderPath = requestedDirectories.first();
        if (settingsController_ != nullptr && settingsController_->rememberLastOpenDirectory()) {
            settingsController_->setLastOpenDirectory(folderPath);
        }
        browseFolderPath(folderPath, true);
        return pinnedCourseBrowserActive_
            && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() == QFileInfo(folderPath).absoluteFilePath();
    }

    const QStringList resolvedFiles = resolvedOpenFiles(cleanedFiles);
    if (resolvedFiles.isEmpty()) {
        statusBar()->showMessage(uiText("No playable media files were found."), 4000);
        return false;
    }

    const QString requestedCurrentSource = cleanedFiles.isEmpty() ? resolvedFiles.first() : cleanedFiles.first();
    if (!pendingCurrentMediaRestoreSource_.isEmpty()
        && !sourcesReferToSameMedia(pendingCurrentMediaRestoreSource_, requestedCurrentSource)) {
        clearPendingCurrentMediaRestore();
    }
    int selectedIndex = 0;
    for (int index = 0; index < resolvedFiles.size(); ++index) {
        if (sourcesReferToSameMedia(resolvedFiles.at(index), requestedCurrentSource)) {
            selectedIndex = index;
            break;
        }
    }

    const QFileInfo selectedFileInfo(requestedCurrentSource);
    beginLoadFeedback(
        resolvedFiles.size() == 1
            ? selectedFileInfo.fileName()
            : QStringLiteral("%1 items").arg(resolvedFiles.size()));
    if (settingsController_ != nullptr
        && settingsController_->rememberLastOpenDirectory()
        && selectedFileInfo.dir().exists()) {
        settingsController_->setLastOpenDirectory(selectedFileInfo.absolutePath());
    }

    enforceHiddenSidePanelsAfterMediaOpen(1100, true);
    openPlaylistSources(resolvedFiles, selectedIndex, revealPlaylistPanel);
    statusBar()->showMessage(
        resolvedFiles.size() == 1
            ? uiText("Opening %1").arg(selectedFileInfo.fileName())
            : uiText("Opening %1 media items").arg(resolvedFiles.size()),
        3000);
    return true;
}

bool MainWindow::openMediaSource(const QString &input)
{
    const QString trimmedInput = input.trimmed();
    if (input.isEmpty() || trimmedInput.isEmpty()) {
        statusBar()->showMessage(uiText("Enter a valid media file, path, or source URL."), 4000);
        return false;
    }

    const QFileInfo directFileInfo(input);
    if (directFileInfo.exists()) {
        if (directFileInfo.isDir()) {
            browseFolderPath(directFileInfo.absoluteFilePath(), true);
            return pinnedCourseBrowserActive_
                && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() == directFileInfo.absoluteFilePath();
        }
        if (directFileInfo.isFile()) {
            return openLocalMediaFiles(QStringList {directFileInfo.absoluteFilePath()});
        }
    }

    const QUrl normalized = QUrl::fromUserInput(trimmedInput);
    if (normalized.isLocalFile()) {
        const QFileInfo fileInfo(normalized.toLocalFile());
        if (!fileInfo.exists()) {
            statusBar()->showMessage(uiText("The selected local file does not exist."), 5000);
            return false;
        }

        if (fileInfo.isDir()) {
            browseFolderPath(fileInfo.absoluteFilePath(), true);
            return pinnedCourseBrowserActive_
                && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() == fileInfo.absoluteFilePath();
        }

        return openLocalMediaFiles(QStringList {fileInfo.absoluteFilePath()});
    }

    if (!normalized.isValid() || normalized.scheme().isEmpty()) {
        statusBar()->showMessage(uiText("Enter a valid media URL, special source, or local path."), 5000);
        return false;
    }

    beginLoadFeedback(trimmedInput);
    enforceHiddenSidePanelsAfterMediaOpen(1100, true);
    loadingMediaSource_ = trimmedInput;
    playbackController_->openUrl(trimmedInput);
    statusBar()->showMessage(
        uiText("Opening source: %1").arg(normalized.toDisplayString(QUrl::RemovePassword)),
        3000);
    return true;
}

void MainWindow::showSpecialSourceDialog(const QString &title,
                                         const QString &placeholderText,
                                         const QString &hintText)
{
    bool accepted = false;
    const QString input = QInputDialog::getText(
        this,
        title,
        hintText,
        QLineEdit::Normal,
        placeholderText,
        &accepted);
    if (!accepted || input.trimmed().isEmpty()) {
        return;
    }

    openMediaSource(input);
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutDialog(this);
    aboutDialog.setIcon(QMessageBox::Information);
    aboutDialog.setTextFormat(Qt::RichText);
    aboutDialog.setTextInteractionFlags(Qt::TextBrowserInteraction);
    aboutDialog.setWindowTitle(uiText("About Reva Player"));
    aboutDialog.setText(uiText("Reva Player"));
    const QString projectUrl = QStringLiteral("https://github.com/Moayad30/Reva_Player");
    const QString aboutText = uiText("Version %1\n\nA desktop media player built with Qt Widgets and libmpv for local files, playlists, subtitles, and lightweight library tools.")
                                  .arg(QCoreApplication::applicationVersion().trimmed().isEmpty()
                                           ? QStringLiteral("1.0.0")
                                           : QCoreApplication::applicationVersion().trimmed())
                                  .toHtmlEscaped()
                                  .replace(QStringLiteral("\n"), QStringLiteral("<br>"));
    aboutDialog.setInformativeText(
        aboutText
        + QStringLiteral("<br><br>%1: <a href=\"%2\">%2</a>").arg(uiText("Project").toHtmlEscaped(), projectUrl));
    for (QLabel *label : aboutDialog.findChildren<QLabel *>()) {
        label->setOpenExternalLinks(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    }
    if (settingsController_ != nullptr && !settingsController_->databasePath().trimmed().isEmpty()) {
        aboutDialog.setDetailedText(
            uiText("Local data path:\n%1").arg(QDir::toNativeSeparators(settingsController_->databasePath())));
    }
    aboutDialog.exec();
}

void MainWindow::showQuickHelpDialog()
{
    QMessageBox helpDialog(this);
    helpDialog.setIcon(QMessageBox::Information);
    helpDialog.setWindowTitle(uiText("Quick Help"));
    helpDialog.setText(uiText("Reva Player Quick Help"));
    helpDialog.setInformativeText(
        uiText("Open local media with Ctrl+O, folders with Ctrl+Shift+O, and URLs with Ctrl+L.\n\n"
               "Use the Playlist panel for current files, saved folders, search, progress, and drag reorder. "
               "Use Details for tracks, subtitles, history, bookmarks, scenes, and diagnostics.\n\n"
               "Clear Cache is available from File > Maintenance and Settings > Maintenance / Data Management. "
               "Factory Reset is only available from Settings and never deletes media files from disk."));
    helpDialog.exec();
}

void MainWindow::clearRuntimeCaches()
{
    if (previewRequestTimer_ != nullptr) {
        previewRequestTimer_->stop();
    }
    if (previewStatusTimer_ != nullptr) {
        previewStatusTimer_->stop();
    }
    if (metadataRefreshTimer_ != nullptr) {
        metadataRefreshTimer_->stop();
    }
    if (hoverPreviewPopup_ != nullptr) {
        hoverPreviewPopup_->hide();
    }
    if (thumbnailService_ != nullptr) {
        thumbnailService_->cancel();
    }
    if (metadataScanService_ != nullptr) {
        metadataScanService_->cancel();
    }

    clearPlaylistThumbnailQueue(true);
    mediaScanCache_.clear();
    mediaScanCacheLookupCompleted_.clear();
    playlistThumbnailCache_.clear();
    playlistThumbnailPendingBuckets_.clear();
    playlistThumbnailQueuedDurations_.clear();
    playlistThumbnailFailedRequestKeys_.clear();
    pendingMediaScanSources_.clear();
    failedMediaScanSources_.clear();
    mediaScanFailureCounts_.clear();
    mediaScanFailureReasons_.clear();
    bookmarkThumbnailQueue_.clear();
    sceneThumbnailQueue_.clear();
    bookmarkRowsByBucket_.clear();
    sceneRowByBucket_.clear();
}

bool MainWindow::clearDiskCaches(QString *errorMessage)
{
    clearRuntimeCaches();

    QStringList failures;
    if (settingsController_ != nullptr) {
        const QStringList cachedScanKeys = settingsController_->customKeys(QString::fromLatin1(kMediaScanCachePrefix));
        for (const QString &key : cachedScanKeys) {
            if (!settingsController_->removeCustomValue(key)) {
                failures.push_back(uiText("metadata cache"));
                break;
            }
        }
    }

    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation).trimmed();
    if (!cacheRoot.isEmpty()) {
        QDir cacheDirectory(cacheRoot);
        if (cacheDirectory.exists() && !cacheDirectory.removeRecursively()) {
            failures.push_back(QDir::toNativeSeparators(cacheRoot));
        }
        if (!QDir().mkpath(cacheRoot)) {
            failures.push_back(QDir::toNativeSeparators(cacheRoot));
        }
    }

    if (thumbnailService_ != nullptr) {
        thumbnailService_->clearCache();
    }

    if (!failures.isEmpty() && errorMessage != nullptr) {
        *errorMessage = failures.join(QStringLiteral(", "));
    }

    return failures.isEmpty();
}

void MainWindow::clearApplicationCache()
{
    const QMessageBox::StandardButton choice = QMessageBox::question(
        this,
        uiText("Clear Cache"),
        uiText("Delete cached thumbnails and temporary media data only? Your preferences, history, bookmarks, and saved lists will be kept."));
    if (choice != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!clearDiskCaches(&errorMessage)) {
        statusBar()->showMessage(
            errorMessage.trimmed().isEmpty()
                ? uiText("Failed to clear the application cache.")
                : uiText("Failed to clear the application cache: %1").arg(errorMessage),
            5000);
        return;
    }

    reloadBookmarks();
    reloadFavoritesPanel();
    refreshPlaylistPresentationData();
    refreshFavoritesViewLayout();
    clearSceneBrowser(uiText("Cache cleared. Load a local video to browse scenes again."));
    statusBar()->showMessage(uiText("Application cache cleared"), 3500);
}

void MainWindow::factoryResetApplication()
{
    if (settingsController_ == nullptr) {
        statusBar()->showMessage(uiText("Settings storage is unavailable"), 4000);
        return;
    }

    const QMessageBox::StandardButton firstConfirmation = QMessageBox::warning(
        this,
        uiText("Factory Reset"),
        uiText("This will remove all local data and reset Reva Player to a fresh state.\n\nThis includes settings, history, recent items, bookmarks, favorites, saved lists, layout presets, and cache."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (firstConfirmation != QMessageBox::Yes) {
        return;
    }

    const QMessageBox::StandardButton secondConfirmation = QMessageBox::warning(
        this,
        uiText("Confirm Factory Reset"),
        uiText("This action cannot be undone. Clear all local Reva Player data now?\n\nYour media files on disk will not be deleted."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (secondConfirmation != QMessageBox::Yes) {
        return;
    }

    if (playbackController_ != nullptr) {
        playbackController_->stop();
    }
    if (comparePlaybackController_ != nullptr) {
        comparePlaybackController_->stop();
    }

    if (!settingsController_->resetApplicationData()) {
        statusBar()->showMessage(
            uiText("Factory reset failed: %1").arg(settingsController_->lastError().trimmed().isEmpty()
                                                      ? uiText("unknown storage error")
                                                      : settingsController_->lastError().trimmed()),
            5000);
        return;
    }

    QString cacheError;
    clearDiskCaches(&cacheError);
    playbackPlaylistEntriesCache_.clear();
    playbackPlaylistCurrentIndexCache_ = -1;
    pendingPlaylistReorderSelectedSources_.clear();
    pendingPlaylistReorderCurrentSource_.clear();
    currentTracks_.clear();
    historyEntriesBySource_.clear();
    resumeStateCache_.clear();
    currentMediaSource_.clear();
    compareSource_.clear();
    currentTitle_.clear();
    favoriteCurrentMedia_ = false;
    compareSourceLoaded_ = false;
    mediaLoaded_ = false;
    loadingMedia_ = false;
    errorStateActive_ = false;
    stopRequested_ = false;
    endOfFilePending_ = false;
    playbackPaused_ = true;
    localSubtitlesAutoLoadedForCurrentMedia_ = false;
    currentPositionSeconds_ = 0.0;
    currentDurationSeconds_ = 0.0;
    currentSubtitleDelaySeconds_ = 0.0;
    currentAudioDelaySeconds_ = 0.0;
    currentVideoZoomFactor_ = 1.0;
    currentVideoAlignX_ = 0.0;
    currentVideoAlignY_ = 0.0;
    currentDiagnostics_ = {};
    previewHoverSource_.clear();
    previewHoverBucketMilliseconds_ = -1;
    updatePlaylistMetadataScanButtonState();
    exitPinnedCourseBrowserMode();
    clearFileSystemCache();
    if (playlistController_ != nullptr) {
        playlistController_->setEntries({}, -1);
    }
    if (playlistView_ != nullptr) {
        playlistView_->clearSelection();
        if (playlistView_->selectionModel() != nullptr) {
            playlistView_->selectionModel()->clearCurrentIndex();
        }
        playlistView_->scrollToTop();
    }
    rebuildVideoQualityMenu({});
    rebuildControlBarSubtitleMenu({});
    clearPendingCurrentMediaRestore();
    clearPendingPlaylistSelection();
    pendingPlaylistNaturalOrderSources_.clear();
    clearPendingPlaylistViewportRestore();
    firstRunPromptShown_ = false;

    resetWindowLayout();
    applyPlaybackProfile(false);
    setRepeatMode(configuredRepeatMode(), false);
    applyRuntimePreferences();
    applyUiPreferences();
    reloadHistoryPanel();
    reloadBookmarks();
    rebuildPinnedCourseTabs();
    refreshPlaylistPresentationData();
    reloadHomeDashboard();
    updateHomeDashboardVisibility();
    showIdleOverlay(defaultIdleOverlayText());
    updateWindowTitle();
    updateActionStates();

    QString statusMessage = uiText("Reva Player was reset to factory defaults.");
    if (!cacheError.trimmed().isEmpty()) {
        statusMessage.append(QStringLiteral(" %1").arg(uiText("Some cached files could not be removed: %1").arg(cacheError)));
    }
    statusBar()->showMessage(statusMessage, 6000);
}

void MainWindow::clearHistoryPanel()
{
    if (historyController_ == nullptr || !historyController_->isReady()) {
        statusBar()->showMessage(uiText("History storage is unavailable"), 3000);
        return;
    }

    if (!historyEnabled()) {
        statusBar()->showMessage(uiText("History is disabled in Preferences"), 3000);
        return;
    }

    if (historyEntries_.isEmpty()) {
        statusBar()->showMessage(uiText("History is already empty"), 2500);
        return;
    }

    const auto choice = QMessageBox::question(
        this,
        uiText("Clear History"),
        uiText("Remove all recent playback history entries?"));
    if (choice != QMessageBox::Yes) {
        return;
    }

    if (!historyController_->clearHistory()) {
        statusBar()->showMessage(uiText("Failed to clear playback history"), 3500);
        return;
    }

    reloadHistoryPanel();
    statusBar()->showMessage(uiText("Playback history cleared"), 3000);
}

void MainWindow::reloadHistoryPanel()
{
    if (!historyEnabled()) {
        historyEntries_.clear();
        historyEntriesBySource_.clear();
        resumeStateCache_.clear();
        resumeStateLookupCompleted_.clear();
        if (historyList_ != nullptr) {
            historyList_->clear();
            auto *placeholderItem = new QListWidgetItem(uiText("History is disabled in Preferences."), historyList_);
            placeholderItem->setFlags(placeholderItem->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        }
        if (historySummaryLabel_ != nullptr) {
            historySummaryLabel_->setText(uiText("History disabled"));
        }
        if (historyClearButton_ != nullptr) {
            historyClearButton_->setEnabled(false);
        }
        reloadHomeDashboard();
        refreshPlaylistPresentationData();
        rebuildPinnedCourseTabs();
        reloadFavoritesPanel();
        updateActionStates();
        return;
    }

    if (historyController_ == nullptr || !historyController_->isReady()) {
        historyEntries_.clear();
        historyEntriesBySource_.clear();
        resumeStateCache_.clear();
        resumeStateLookupCompleted_.clear();
        populateHistory({});
        if (historySummaryLabel_ != nullptr) {
            historySummaryLabel_->setText(uiText("History unavailable"));
        }
        if (historyClearButton_ != nullptr) {
            historyClearButton_->setEnabled(false);
        }
        reloadHomeDashboard();
        refreshPlaylistPresentationData();
        rebuildPinnedCourseTabs();
        reloadFavoritesPanel();
        return;
    }

    populateHistory(historyController_->recentHistory(historyLimit()));
    reloadHomeDashboard();
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadFavoritesPanel();
}

void MainWindow::populateHistory(
    const QVector<revaplayer::infrastructure::storage::PlaybackHistoryRecord> &entries)
{
    historyEntries_ = entries;
    rebuildHistoryLookupCache();
    if (historyList_ == nullptr) {
        return;
    }

    historyList_->clear();
    if (entries.isEmpty()) {
        auto *placeholderItem = new QListWidgetItem(QStringLiteral("No recent files or streams yet."), historyList_);
        placeholderItem->setFlags(placeholderItem->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        if (historyClearButton_ != nullptr) {
            historyClearButton_->setEnabled(false);
        }
        filterHistoryPanel();
        updateActionStates();
        return;
    }

    for (const auto &entry : entries) {
        const QString title = displayTitleForHistory(entry.source, entry.title);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(title, formatHistorySummary(entry)),
            historyList_);
        item->setData(Qt::UserRole, entry.source);
        item->setData(Qt::UserRole + 1, QStringLiteral("%1 %2").arg(title, entry.source).toLower());
        item->setToolTip(
            QStringLiteral("%1\nLast opened: %2")
                .arg(entry.source, formatHistoryTimestamp(entry.lastOpenedAt)));
    }

    if (historyClearButton_ != nullptr) {
        historyClearButton_->setEnabled(true);
    }
    filterHistoryPanel();
    updateActionStates();
}

void MainWindow::rebuildHistoryLookupCache()
{
    historyEntriesBySource_.clear();
    for (const auto &entry : historyEntries_) {
        const QString source = entry.source.trimmed();
        if (source.isEmpty()) {
            continue;
        }

        const auto it = historyEntriesBySource_.constFind(source);
        if (it == historyEntriesBySource_.constEnd() || it.value().lastOpenedAt < entry.lastOpenedAt) {
            historyEntriesBySource_.insert(source, entry);
        }
    }
    resumeStateCache_.clear();
    resumeStateLookupCompleted_.clear();
}

void MainWindow::filterHistoryPanel()
{
    if (historyList_ == nullptr || historySummaryLabel_ == nullptr) {
        return;
    }

    const QString needle = historySearchEdit_ != nullptr ? historySearchEdit_->text().trimmed().toLower() : QString {};
    int visibleCount = 0;

    for (int row = 0; row < historyList_->count(); ++row) {
        QListWidgetItem *item = historyList_->item(row);
        if (item == nullptr) {
            continue;
        }

        const bool hasSource = !item->data(Qt::UserRole).toString().trimmed().isEmpty();
        if (!hasSource) {
            item->setHidden(!historyEntries_.isEmpty());
            continue;
        }

        const bool matches = needle.isEmpty() || item->data(Qt::UserRole + 1).toString().contains(needle);
        item->setHidden(!matches);
        if (matches) {
            ++visibleCount;
        }
    }

    historySummaryLabel_->setText(
        historyEntries_.isEmpty()
            ? QStringLiteral("History empty")
            : (needle.isEmpty()
                   ? QStringLiteral("%1 recent").arg(visibleCount)
                   : QStringLiteral("%1 matches").arg(visibleCount)));
}

void MainWindow::reloadFavoritesPanel()
{
    if (favoritesList_ == nullptr) {
        return;
    }

    favoritesList_->clear();

    struct FavoritePanelEntry {
        QString source;
        QString title;
        QString createdAt;
    };

    QVector<FavoritePanelEntry> favorites;
    if (settingsController_ != nullptr) {
        const QStringList keys = settingsController_->customKeys(QString::fromLatin1(kFavoriteMediaPrefix));
        favorites.reserve(keys.size());
        for (const QString &key : keys) {
            const QJsonObject object = QJsonDocument::fromJson(settingsController_->customValue(key).toUtf8()).object();
            const QString source = object.value(QStringLiteral("source")).toString();
            if (!storedMediaSourceIsUsable(source)) {
                settingsController_->removeCustomValue(key);
                continue;
            }

            favorites.push_back(FavoritePanelEntry {
                source,
                object.value(QStringLiteral("title")).toString().trimmed(),
                object.value(QStringLiteral("created_at")).toString().trimmed(),
            });
        }
    }

    std::sort(favorites.begin(), favorites.end(), [](const FavoritePanelEntry &left, const FavoritePanelEntry &right) {
        const QDateTime leftCreatedAt = QDateTime::fromString(left.createdAt, Qt::ISODate);
        const QDateTime rightCreatedAt = QDateTime::fromString(right.createdAt, Qt::ISODate);
        if (leftCreatedAt.isValid() && rightCreatedAt.isValid() && leftCreatedAt != rightCreatedAt) {
            return leftCreatedAt > rightCreatedAt;
        }
        if (leftCreatedAt.isValid() != rightCreatedAt.isValid()) {
            return leftCreatedAt.isValid();
        }
        return left.title.localeAwareCompare(right.title) < 0;
    });

    if (favorites.isEmpty()) {
        auto *placeholderItem = new QListWidgetItem(uiText("No favorites pinned yet"), favoritesList_);
        placeholderItem->setFlags(placeholderItem->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
        refreshFavoritesViewLayout();
        updateActionStates();
        return;
    }

    for (const FavoritePanelEntry &favorite : favorites) {
        const QString source = favorite.source;
        const QString localPath = localMediaPathForSource(source);
        const auto fileInfo = localPath.isEmpty()
            ? std::optional<QFileInfo> {}
            : std::optional<QFileInfo> {QFileInfo(localPath)};
        QString displayTitle = source == currentMediaSource_
            ? displayTitleForHistory(source, currentTitle_)
            : displayTitleForHistory(source, favorite.title);
        double durationSeconds = 0.0;

        const auto historyIt = historyEntriesBySource_.constFind(source);
        if (historyIt != historyEntriesBySource_.constEnd()) {
            durationSeconds = historyIt.value().durationSeconds;
            if (displayTitle.trimmed().isEmpty()) {
                displayTitle = displayTitleForHistory(source, historyIt.value().title);
            }
        }

        if (source == currentMediaSource_) {
            if (currentDurationSeconds_ > 0.0) {
                durationSeconds = currentDurationSeconds_;
            }
            displayTitle = displayTitleForHistory(source, currentTitle_);
        }

        std::optional<revaplayer::services::media::MediaScanResult> scanResult;
        const QString scanKey = mediaScanSourceKey(source);
        if (!scanKey.isEmpty()) {
            const auto scanIt = mediaScanCache_.constFind(scanKey);
            if (scanIt != mediaScanCache_.constEnd()) {
                scanResult = scanIt.value();
            } else if (!mediaScanCacheLookupCompleted_.contains(scanKey)) {
                mediaScanCacheLookupCompleted_.insert(scanKey);
                if (const auto cached = loadCachedMediaScanResult(settingsController_, scanKey); cached.has_value()) {
                    mediaScanCache_.insert(scanKey, *cached);
                    scanResult = cached;
                }
            }
        }

        if (scanResult.has_value() && durationSeconds <= 0.0 && scanResult->durationSeconds > 0.0) {
            durationSeconds = scanResult->durationSeconds;
        }

        QString resolutionText = scanResult.has_value() ? approximateResolutionLabelForScan(*scanResult) : QString {};
        if (resolutionText.isEmpty() && scanResult.has_value()) {
            resolutionText = resolutionLabelForScan(*scanResult);
        }
        QString fileFormatText = scanResult.has_value()
            ? scanResult->fileFormat.trimmed()
            : (fileInfo.has_value() ? fileInfo->suffix().trimmed() : QString {});

        QImage thumbnail;
        const auto thumbnailIt = playlistThumbnailCache_.constFind(source);
        if (thumbnailIt != playlistThumbnailCache_.constEnd()) {
            thumbnail = thumbnailIt.value();
        } else if (fileInfo.has_value()
                   && fileInfo->exists()
                   && fileInfo->isFile()
                   && loadPlaylistThumbnailDiskCacheForFileMetadata(
                       fileInfo->absoluteFilePath(),
                       fileInfo->lastModified().toMSecsSinceEpoch(),
                       fileInfo->size(),
                       durationSeconds,
                       &thumbnail)
                   && !thumbnail.isNull()) {
            playlistThumbnailCache_.insert(source, thumbnail);
        }

        if (thumbnail.isNull()
            && thumbnailService_ != nullptr
            && thumbnailService_->previewEnabled()
            && fileInfo.has_value()
            && fileInfo->exists()
            && fileInfo->isFile()
            && !playlistThumbnailPendingBuckets_.contains(source)) {
            const double requestTime = playlistThumbnailTimeSeconds(durationSeconds);
            const qint64 bucketMilliseconds = revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(requestTime);
            playlistThumbnailPendingBuckets_.insert(source, bucketMilliseconds);
            thumbnailService_->requestThumbnail(source, requestTime);
        }

        auto *item = new QListWidgetItem(displayTitle, favoritesList_);
        item->setData(Qt::UserRole, source);
        item->setData(Qt::UserRole + 1, QStringLiteral("%1 %2").arg(displayTitle, source).toLower());
        item->setData(Qt::UserRole + 2, favorite.createdAt);
        item->setData(revaplayer::application::PlaylistRoles::DurationSecondsRole, durationSeconds);
        item->setData(revaplayer::application::PlaylistRoles::ResolutionRole, resolutionText);
        item->setData(revaplayer::application::PlaylistRoles::FileFormatRole, fileFormatText);
        item->setToolTip(source);
        if (!thumbnail.isNull()) {
            item->setData(Qt::DecorationRole, QIcon(QPixmap::fromImage(thumbnail)));
        }
    }

    filterFavoritesPanel();
    refreshFavoritesViewLayout();
    updateActionStates();
}

void MainWindow::filterFavoritesPanel()
{
    if (favoritesList_ == nullptr) {
        return;
    }

    const QString needle = favoritesSearchEdit_ != nullptr ? favoritesSearchEdit_->text().trimmed().toLower() : QString {};
    QListWidgetItem *firstVisibleItem = nullptr;

    for (int row = 0; row < favoritesList_->count(); ++row) {
        QListWidgetItem *item = favoritesList_->item(row);
        if (item == nullptr) {
            continue;
        }

        const QString source = item->data(Qt::UserRole).toString();
        if (source.isEmpty()) {
            item->setHidden(false);
            continue;
        }

        const bool matches = needle.isEmpty() || item->data(Qt::UserRole + 1).toString().contains(needle);
        item->setHidden(!matches);
        if (matches && firstVisibleItem == nullptr) {
            firstVisibleItem = item;
        }
    }

    if (favoritesList_->currentItem() == nullptr || favoritesList_->currentItem()->isHidden()) {
        if (firstVisibleItem != nullptr) {
            favoritesList_->setCurrentItem(firstVisibleItem);
        } else {
            favoritesList_->clearSelection();
            favoritesList_->setCurrentRow(-1);
        }
    }

    refreshFavoritesViewLayout();
    updateActionStates();
}

void MainWindow::addFavoriteFile()
{
    QString initialDirectory = QDir::homePath();
    if (settingsController_ != nullptr && settingsController_->rememberLastOpenDirectory()) {
        const QString storedDirectory = settingsController_->lastOpenDirectory();
        if (!storedDirectory.isEmpty()) {
            initialDirectory = storedDirectory;
        }
    }

    const QString path = filedialog::getOpenFileName(
        this,
        uiText("Add Favorite Media"),
        initialDirectory,
        uiText("Media Files (*.mkv *.mp4 *.webm *.avi *.mov *.mp3 *.flac *.wav *.m4a *.ogg);;All Files (*)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    const QFileInfo fileInfo(path);
    const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
        ? fileInfo.absoluteFilePath()
        : fileInfo.canonicalFilePath();
    addMediaSourceToFavorites(normalizedPath,
                              fileInfo.completeBaseName().trimmed(),
                              uiText("File added to favorites"),
                              uiText("This file is already in favorites"));
}

void MainWindow::openSelectedFavorite()
{
    if (favoritesList_ == nullptr || favoritesList_->currentItem() == nullptr) {
        return;
    }

    const QString source = favoritesList_->currentItem()->data(Qt::UserRole).toString();
    if (source.isEmpty()) {
        return;
    }

    openMediaSource(source);
}

void MainWindow::removeSelectedFavorite()
{
    if (favoritesList_ == nullptr || favoritesList_->currentItem() == nullptr || settingsController_ == nullptr) {
        return;
    }

    const QString source = favoritesList_->currentItem()->data(Qt::UserRole).toString();
    if (source.isEmpty()) {
        return;
    }

    settingsController_->removeCustomValue(favoriteStorageKey(source));
    if (source == currentMediaSource_) {
        favoriteCurrentMedia_ = false;
    }

    reloadFavoritesPanel();
    refreshPlaylistPresentationData();
    reloadHomeDashboard();
    showActionResult(uiText("Removed selected favorite"),
                     uiText("Removed selected favorite"),
                     2500);
    updateActionStates();
}

bool MainWindow::addMediaSourceToFavorites(const QString &source,
                                           const QString &title,
                                           const QString &addedMessage,
                                           const QString &alreadyFavoriteMessage)
{
    if (settingsController_ == nullptr) {
        return false;
    }

    QString normalizedSource = source;
    if (normalizedSource.isEmpty()) {
        return false;
    }

    const QString localPath = localMediaPathForSource(normalizedSource);
    if (!localPath.isEmpty()) {
        const QFileInfo fileInfo(localPath);
        normalizedSource = fileInfo.canonicalFilePath().isEmpty()
            ? fileInfo.absoluteFilePath()
            : fileInfo.canonicalFilePath();
    }

    if (!storedMediaSourceIsUsable(normalizedSource)) {
        statusBar()->showMessage(uiText("The selected local file does not exist."), 5000);
        return false;
    }

    const QString key = favoriteStorageKey(normalizedSource);
    if (!settingsController_->customValue(key).trimmed().isEmpty()) {
        if (!alreadyFavoriteMessage.isEmpty()) {
            statusBar()->showMessage(alreadyFavoriteMessage, 2500);
        }
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("source"), normalizedSource);
    object.insert(QStringLiteral("title"), title.trimmed().isEmpty() ? displayTitleForHistory(normalizedSource, QString {}) : title.trimmed());
    object.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    settingsController_->setCustomValue(key, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    if (sourcesReferToSameMedia(normalizedSource, currentMediaSource_)) {
        favoriteCurrentMedia_ = true;
    }

    reloadFavoritesPanel();
    if (favoritesList_ != nullptr) {
        for (int row = 0; row < favoritesList_->count(); ++row) {
            QListWidgetItem *item = favoritesList_->item(row);
            if (item != nullptr
                && sourcesReferToSameMedia(item->data(Qt::UserRole).toString(), normalizedSource)) {
                favoritesList_->setCurrentItem(item);
                break;
            }
        }
    }

    refreshPlaylistPresentationData();
    reloadHomeDashboard();
    updateActionStates();
    if (!addedMessage.isEmpty()) {
        showActionResult(addedMessage, addedMessage, 2500);
    }
    return true;
}

void MainWindow::updateFavoriteThumbnailForSource(const QString &source, const QImage &image)
{
    if (favoritesList_ == nullptr || image.isNull()) {
        return;
    }

    bool updated = false;
    for (int row = 0; row < favoritesList_->count(); ++row) {
        QListWidgetItem *item = favoritesList_->item(row);
        if (item == nullptr || item->data(Qt::UserRole).toString().trimmed() != source) {
            continue;
        }

        item->setData(Qt::DecorationRole, QIcon(QPixmap::fromImage(image)));
        updated = true;
    }

    if (updated) {
        refreshFavoritesViewLayout();
    }
}

void MainWindow::refreshFavoritesViewLayout()
{
    if (favoritesList_ == nullptr) {
        return;
    }

    favoritesList_->setIconSize(QSize(128, 72));
    favoritesList_->doItemsLayout();
    favoritesList_->updateGeometry();
    favoritesList_->viewport()->update();
}

void MainWindow::clearSceneBrowser(const QString &message)
{
    sceneThumbnailQueue_.clear();
    sceneRowByBucket_.clear();
    sceneBrowserLoading_ = false;
    sceneBrowserSource_.clear();
    if (sceneList_ != nullptr) {
        sceneList_->clear();
    }
    if (sceneStatusLabel_ != nullptr) {
        sceneStatusLabel_->setText(
            message.trimmed().isEmpty()
                ? QStringLiteral("Load a local video to browse scenes")
                : message.trimmed());
    }
}

void MainWindow::refreshSceneBrowserPrompt(const bool preserveGeneratedScenes)
{
    if (sceneList_ == nullptr || sceneStatusLabel_ == nullptr) {
        return;
    }

    if (preserveGeneratedScenes
        && sceneList_->count() > 0
        && !sceneBrowserSource_.trimmed().isEmpty()) {
        return;
    }

    const QString localSource = localMediaPathForSource(currentMediaSource_);
    QString message;
    const auto fileInfo = localSource.isEmpty()
        ? std::optional<QFileInfo> {}
        : std::optional<QFileInfo> {QFileInfo(localSource)};
    if (!mediaLoaded_
        || !hasVideoTrack_
        || !fileInfo.has_value()
        || !fileInfo->exists()
        || !fileInfo->isFile()) {
        message = uiText("Load a local video to browse scenes");
    } else if (thumbnailService_ == nullptr || !thumbnailService_->previewEnabled()) {
        message = uiText("Enable preview thumbnails in Preferences to use Scene Browser");
    } else if (currentDurationSeconds_ <= 0.0) {
        message = uiText("Scenes will appear once the media duration is known");
    } else {
        message = uiText("Press Refresh to generate scenes");
    }

    clearSceneBrowser(message);
    updateActionStates();
}

void MainWindow::rebuildSceneBrowser(const bool force)
{
    if (sceneList_ == nullptr || sceneStatusLabel_ == nullptr) {
        return;
    }

    const QString localSource = localMediaPathForSource(currentMediaSource_);
    const auto fileInfo = localSource.isEmpty()
        ? std::optional<QFileInfo> {}
        : std::optional<QFileInfo> {QFileInfo(localSource)};
    if (!mediaLoaded_
        || !hasVideoTrack_
        || !fileInfo.has_value()
        || !fileInfo->exists()
        || !fileInfo->isFile()) {
        clearSceneBrowser(uiText("Load a local video to browse scenes"));
        return;
    }

    if (thumbnailService_ == nullptr || !thumbnailService_->previewEnabled()) {
        clearSceneBrowser(uiText("Enable preview thumbnails in Preferences to use Scene Browser"));
        return;
    }

    if (currentDurationSeconds_ <= 0.0) {
        clearSceneBrowser(uiText("Scenes will appear once the media duration is known"));
        return;
    }

    const QString canonicalSource = QDir::cleanPath(fileInfo->absoluteFilePath());
    if (!force && sceneBrowserSource_ == canonicalSource && sceneList_->count() > 0 && sceneThumbnailQueue_.isEmpty()) {
        return;
    }

    sceneList_->clear();
    sceneThumbnailQueue_.clear();
    sceneRowByBucket_.clear();
    sceneBrowserSource_ = canonicalSource;

    const int requestedStep = std::max(1, sceneStepSpinBox_ != nullptr ? sceneStepSpinBox_->value() : kDefaultSceneStepSeconds);

    QVector<double> cueTimes;
    cueTimes.reserve(std::max(1, static_cast<int>(std::ceil(currentDurationSeconds_ / requestedStep)) + 1));
    for (double seconds = 0.0; seconds < currentDurationSeconds_; seconds += requestedStep) {
        cueTimes.push_back(cueTimes.isEmpty() ? std::min(1.0, std::max(0.0, currentDurationSeconds_)) : seconds);
    }
    const double nearEndTime = std::max(0.0, currentDurationSeconds_ - 1.0);
    if (cueTimes.isEmpty() || (nearEndTime - cueTimes.last()) > requestedStep * 0.45) {
        cueTimes.push_back(nearEndTime);
    }

    cueTimes.erase(std::unique(cueTimes.begin(), cueTimes.end(), [](const double left, const double right) {
                       return std::abs(left - right) < 0.5;
                   }),
                   cueTimes.end());

    const QSize iconSize = sceneList_->iconSize();
    for (const double timeSeconds : cueTimes) {
        const QString timeLabel = formatPlaybackTime(timeSeconds);
        auto *item = new QListWidgetItem(buildScenePlaceholderIcon(iconSize, timeLabel), timeLabel, sceneList_);
        const qint64 bucketMilliseconds = revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(timeSeconds);
        item->setData(Qt::UserRole, timeSeconds);
        item->setData(Qt::UserRole + 1, bucketMilliseconds);
        item->setData(Qt::UserRole + 2, 0);
        item->setToolTip(uiText("Jump to %1").arg(timeLabel));
        sceneRowByBucket_.insert(bucketMilliseconds, sceneList_->row(item));
        sceneThumbnailQueue_.push_back(bucketMilliseconds);
    }

    sceneBrowserLoading_ = !sceneThumbnailQueue_.isEmpty();
    sceneStatusLabel_->setText(uiText("Loading %1 scenes").arg(sceneList_->count()));
    filterSceneBrowser();
    requestNextSceneThumbnail();
    updateActionStates();
}

void MainWindow::filterSceneBrowser()
{
    if (sceneList_ == nullptr) {
        return;
    }

    const QString searchText = sceneSearchEdit_ != nullptr ? sceneSearchEdit_->text().trimmed().toLower() : QString {};
    for (int row = 0; row < sceneList_->count(); ++row) {
        QListWidgetItem *item = sceneList_->item(row);
        if (item == nullptr) {
            continue;
        }

        const bool matches = searchText.isEmpty() || item->text().toLower().contains(searchText);
        item->setHidden(!matches);
    }
}

void MainWindow::exportSceneImages()
{
    if (sceneList_ == nullptr || sceneList_->count() == 0) {
        return;
    }

    QString initialDirectory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (snapshotController_ != nullptr) {
        initialDirectory = snapshotController_->screenshotDirectory();
    }
    if (initialDirectory.trimmed().isEmpty()) {
        initialDirectory = QDir::homePath();
    }

    const QString directoryPath = filedialog::getExistingDirectory(
        this,
        uiText("Export Scene Images"),
        initialDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (directoryPath.trimmed().isEmpty()) {
        return;
    }

    QDir directory(directoryPath);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        statusBar()->showMessage(uiText("Could not create the export folder."), 4000);
        return;
    }

    const QString mediaTitle = effectiveCurrentMediaTitle().trimmed().isEmpty()
        ? QStringLiteral("scenes")
        : effectiveCurrentMediaTitle().trimmed();
    int savedCount = 0;
    int skippedCount = 0;

    for (int row = 0; row < sceneList_->count(); ++row) {
        QListWidgetItem *item = sceneList_->item(row);
        if (item == nullptr) {
            continue;
        }

        QImage image = item->data(kSceneThumbnailImageRole).value<QImage>();
        if (image.isNull() && item->data(Qt::UserRole + 2).toInt() == 1) {
            image = item->icon().pixmap(sceneList_->iconSize()).toImage();
        }
        if (image.isNull()) {
            ++skippedCount;
            continue;
        }

        QString timeStem = item->text().trimmed();
        timeStem.replace(QChar(':'), QChar('-'));
        const QString fileStem = revaplayer::application::sanitizeSnapshotFileStem(
            QStringLiteral("%1-scene-%2-%3")
                .arg(mediaTitle)
                .arg(row + 1, 2, 10, QChar('0'))
                .arg(timeStem));
        const QString targetPath = directory.filePath(QStringLiteral("%1.png").arg(fileStem));
        if (image.save(targetPath, "PNG", 95)) {
            ++savedCount;
        } else {
            ++skippedCount;
        }
    }

    if (savedCount == 0) {
        statusBar()->showMessage(uiText("Generate scene thumbnails before exporting images."), 5000);
        return;
    }

    const QString message = skippedCount > 0
        ? uiText("Exported %1 scene images. %2 were unavailable.").arg(savedCount).arg(skippedCount)
        : uiText("Exported %1 scene images.").arg(savedCount);
    showActionResult(message, message, 5000);
}

void MainWindow::bookmarkSelectedScene()
{
    if (sceneList_ == nullptr || sceneList_->currentItem() == nullptr || !ensureBookmarkStorageReady(true)) {
        return;
    }

    QListWidgetItem *item = sceneList_->currentItem();
    const double positionSeconds = item->data(Qt::UserRole).toDouble();
    const auto created = bookmarkController_->createBookmark(
        currentMediaSource_,
        QStringLiteral("Scene %1").arg(item->text()),
        positionSeconds,
        QStringLiteral("Scene browser bookmark"),
        QStringLiteral("Scene"));
    if (created.has_value()) {
        reloadBookmarks();
        const QString message = uiText("Scene bookmark saved at %1").arg(item->text());
        showActionResult(message, message, 3000);
    }
}

void MainWindow::updateBookmarkSelectionPreview()
{
    if (bookmarkJumpButton_ == nullptr) {
        return;
    }

    QListWidgetItem *item = bookmarksList_ != nullptr ? bookmarksList_->currentItem() : nullptr;
    bookmarkJumpButton_->setEnabled(item != nullptr);
    if (bookmarkPreviewImageLabel_ == nullptr
        || bookmarkPreviewTitleLabel_ == nullptr
        || bookmarkPreviewNoteLabel_ == nullptr) {
        return;
    }

    if (item == nullptr) {
        bookmarkPreviewImageLabel_->setPixmap({});
        bookmarkPreviewImageLabel_->setText(uiText("Preview"));
        if (!ensureBookmarkStorageReady(false)) {
            bookmarkPreviewTitleLabel_->setText(uiText("Bookmarks are unavailable right now"));
            const QString reason = bookmarkController_ != nullptr ? bookmarkController_->lastError().trimmed() : QString {};
            bookmarkPreviewNoteLabel_->setText(
                reason.isEmpty()
                    ? uiText("The bookmark database is currently unavailable.")
                    : uiText("The bookmark database is currently unavailable: %1").arg(reason));
        } else if (mediaLoaded_ && !currentMediaSource_.trimmed().isEmpty()) {
            bookmarkPreviewTitleLabel_->setText(uiText("No bookmarks yet for this media"));
            bookmarkPreviewNoteLabel_->setText(uiText("Add your first bookmark from the panel, the playback menu, or with Ctrl+B."));
        } else {
            bookmarkPreviewTitleLabel_->setText(uiText("Select a bookmark to preview it"));
            bookmarkPreviewNoteLabel_->setText(uiText("Bookmark thumbnails and quick jump preview will appear here."));
        }
        return;
    }

    const QString title = item->data(Qt::UserRole + 2).toString().trimmed();
    const QString note = item->data(Qt::UserRole + 3).toString().trimmed();
    const QString category = item->data(Qt::UserRole + 4).toString().trimmed();
    const double positionSeconds = item->data(Qt::UserRole + 1).toDouble();

    const QVariant imageVariant = item->data(Qt::UserRole + 5);
    if (imageVariant.canConvert<QImage>()) {
        const QImage image = qvariant_cast<QImage>(imageVariant);
        bookmarkPreviewImageLabel_->setPixmap(QPixmap::fromImage(image).scaled(
            bookmarkPreviewImageLabel_->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
        bookmarkPreviewImageLabel_->setText(QString {});
    } else {
        bookmarkPreviewImageLabel_->setPixmap({});
        bookmarkPreviewImageLabel_->setText(formatPlaybackTime(positionSeconds));
    }

    bookmarkPreviewTitleLabel_->setText(
        category.isEmpty()
            ? QStringLiteral("%1  •  %2").arg(formatPlaybackTime(positionSeconds), title)
            : QStringLiteral("[%1] %2  •  %3").arg(category, formatPlaybackTime(positionSeconds), title));
    bookmarkPreviewNoteLabel_->setText(note.isEmpty() ? uiText("Jump to this bookmark instantly from here.") : note);
}

void MainWindow::requestNextBookmarkThumbnail()
{
    if (thumbnailService_ == nullptr
        || !thumbnailService_->previewEnabled()
        || bookmarkThumbnailQueue_.isEmpty()) {
        return;
    }

    while (!bookmarkThumbnailQueue_.isEmpty()) {
        const qint64 bucketMilliseconds = bookmarkThumbnailQueue_.takeFirst();
        const QVector<int> rows = bookmarkRowsByBucket_.value(bucketMilliseconds);
        QString requestSource;
        for (const int row : rows) {
            QListWidgetItem *item = row >= 0 && bookmarksList_ != nullptr ? bookmarksList_->item(row) : nullptr;
            if (item == nullptr || item->data(Qt::UserRole + 5).canConvert<QImage>()) {
                continue;
            }

            requestSource = item->data(Qt::UserRole + 6).toString().trimmed();
            if (!requestSource.isEmpty()) {
                break;
            }
        }

        if (requestSource.isEmpty()) {
            continue;
        }

        thumbnailService_->requestThumbnail(requestSource, bucketMilliseconds / 1000.0);
        return;
    }
}

void MainWindow::requestNextSceneThumbnail()
{
    if (sceneList_ == nullptr || sceneStatusLabel_ == nullptr) {
        return;
    }

    const auto updateStatus = [this]() {
        if (sceneList_ == nullptr || sceneStatusLabel_ == nullptr) {
            return;
        }

        int readyCount = 0;
        int failedCount = 0;
        for (int row = 0; row < sceneList_->count(); ++row) {
            QListWidgetItem *item = sceneList_->item(row);
            if (item == nullptr) {
                continue;
            }

            const int state = item->data(Qt::UserRole + 2).toInt();
            if (state == 1) {
                ++readyCount;
            } else if (state == 2) {
                ++failedCount;
            }
        }

        if (sceneThumbnailQueue_.isEmpty()) {
            sceneStatusLabel_->setText(
                failedCount > 0
                    ? uiText("%1 ready • %2 unavailable").arg(readyCount).arg(failedCount)
                    : uiText("%1 scenes ready").arg(readyCount));
            return;
        }

        sceneStatusLabel_->setText(
            uiText("Loading %1 / %2 scenes")
                .arg(readyCount + failedCount)
                .arg(sceneList_->count()));
    };

    updateStatus();
    if (thumbnailService_ == nullptr || sceneBrowserSource_.trimmed().isEmpty()) {
        sceneBrowserLoading_ = false;
        return;
    }

    while (!sceneThumbnailQueue_.isEmpty()) {
        const qint64 bucketMilliseconds = sceneThumbnailQueue_.takeFirst();
        const int row = sceneRowByBucket_.value(bucketMilliseconds, -1);
        QListWidgetItem *item = row >= 0 ? sceneList_->item(row) : nullptr;
        if (item == nullptr || item->data(Qt::UserRole + 2).toInt() != 0) {
            continue;
        }

        sceneBrowserLoading_ = true;
        thumbnailService_->requestThumbnail(sceneBrowserSource_, bucketMilliseconds / 1000.0);
        return;
    }

    sceneBrowserLoading_ = false;
    updateStatus();
}

void MainWindow::populateSecondarySubtitleOptions()
{
    if (secondarySubtitleCombo_ == nullptr) {
        return;
    }

    const QString previousValue = secondarySubtitleCombo_->currentData().toString();
    const QSignalBlocker blocker(secondarySubtitleCombo_);
    secondarySubtitleCombo_->clear();
    secondarySubtitleCombo_->addItem(uiText("Off"), QStringLiteral("no"));

    for (const auto &track : currentTracks_) {
        if (track.type == revaplayer::domain::TrackType::Subtitle) {
            secondarySubtitleCombo_->addItem(buildTrackTitle(track), QString::number(track.id));
        }
    }

    const int selectedIndex = secondarySubtitleCombo_->findData(previousValue);
    secondarySubtitleCombo_->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
}

void MainWindow::applySecondarySubtitleSelection()
{
    if (secondarySubtitleCombo_ == nullptr || playbackController_ == nullptr || !playbackController_->isInitialized() || !mediaLoaded_) {
        return;
    }

    const QString value = secondarySubtitleCombo_->currentData().toString().trimmed();
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("secondary-sid"),
        value.isEmpty() ? QStringLiteral("no") : value,
    });

    if (mediaLabStatusLabel_ != nullptr) {
        mediaLabStatusLabel_->setText(
            value == QStringLiteral("no")
                ? QStringLiteral("Secondary subtitles are off.")
                : QStringLiteral("Secondary subtitle track applied."));
    }
}

void MainWindow::setSubtitleDelayWithFeedback(double delaySeconds, const bool resetRequested)
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    delaySeconds = std::clamp(delaySeconds, -3600.0, 3600.0);
    if (resetRequested || std::abs(delaySeconds) < 0.0005) {
        delaySeconds = 0.0;
        playbackController_->resetSubtitleDelay();
    } else {
        playbackController_->setSubtitleDelay(delaySeconds);
    }
    currentSubtitleDelaySeconds_ = delaySeconds;

    if (subtitleDelayCustomSpinBox_ != nullptr) {
        const QSignalBlocker blocker(subtitleDelayCustomSpinBox_);
        subtitleDelayCustomSpinBox_->setValue(delaySeconds);
    }

    const QString text = uiText("Subtitle delay: %1").arg(subtitleDelayDisplayText(delaySeconds));
    statusBar()->showMessage(text, 2500);
    showPlaybackFeedback(text);
    if (subtitleAutomationStatusLabel_ != nullptr) {
        subtitleAutomationStatusLabel_->setText(resetRequested
                ? uiText("Subtitle delay reset to %1").arg(subtitleDelayDisplayText(0.0))
                : text);
    }
    if (subtitleRememberDelayForMediaCheckBox_ != nullptr && subtitleRememberDelayForMediaCheckBox_->isChecked()) {
        saveRememberedSubtitleDelayForCurrentMedia();
    }
}

void MainWindow::saveRememberedSubtitleDelayForCurrentMedia()
{
    if (settingsController_ == nullptr
        || currentMediaSource_.trimmed().isEmpty()
        || subtitleRememberDelayForMediaCheckBox_ == nullptr
        || !subtitleRememberDelayForMediaCheckBox_->isChecked()) {
        return;
    }

    const QString storageKey = rememberedSubtitleDelayStorageKeyForSource(currentMediaSource_);
    if (storageKey.isEmpty()) {
        return;
    }

    settingsController_->setCustomValue(storageKey, QString::number(currentSubtitleDelaySeconds_, 'f', 3));
}

void MainWindow::removeRememberedSubtitleDelayForCurrentMedia()
{
    if (settingsController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString storageKey = rememberedSubtitleDelayStorageKeyForSource(currentMediaSource_);
    if (!storageKey.isEmpty()) {
        settingsController_->removeCustomValue(storageKey);
    }
}

void MainWindow::applyRememberedSubtitleDelayForCurrentMedia()
{
    if (settingsController_ == nullptr
        || playbackController_ == nullptr
        || !mediaLoaded_
        || currentMediaSource_.trimmed().isEmpty()) {
        if (subtitleRememberDelayForMediaCheckBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleRememberDelayForMediaCheckBox_);
            subtitleRememberDelayForMediaCheckBox_->setChecked(false);
        }
        return;
    }

    const QString storageKey = rememberedSubtitleDelayStorageKeyForSource(currentMediaSource_);
    const QString rawValue = storageKey.isEmpty() ? QString {} : settingsController_->customValue(storageKey).trimmed();
    const bool hasRememberedValue = !rawValue.isEmpty();
    if (subtitleRememberDelayForMediaCheckBox_ != nullptr) {
        const QSignalBlocker blocker(subtitleRememberDelayForMediaCheckBox_);
        subtitleRememberDelayForMediaCheckBox_->setChecked(hasRememberedValue);
    }

    if (!hasRememberedValue) {
        if (subtitleDelayCustomSpinBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleDelayCustomSpinBox_);
            subtitleDelayCustomSpinBox_->setValue(currentSubtitleDelaySeconds_);
        }
        return;
    }

    bool parsed = false;
    const double rememberedDelay = rawValue.toDouble(&parsed);
    if (!parsed) {
        settingsController_->removeCustomValue(storageKey);
        if (subtitleRememberDelayForMediaCheckBox_ != nullptr) {
            const QSignalBlocker blocker(subtitleRememberDelayForMediaCheckBox_);
            subtitleRememberDelayForMediaCheckBox_->setChecked(false);
        }
        return;
    }

    const double clampedDelay = std::clamp(rememberedDelay, -3600.0, 3600.0);
    currentSubtitleDelaySeconds_ = clampedDelay;
    playbackController_->setSubtitleDelay(clampedDelay);
    if (subtitleDelayCustomSpinBox_ != nullptr) {
        const QSignalBlocker blocker(subtitleDelayCustomSpinBox_);
        subtitleDelayCustomSpinBox_->setValue(clampedDelay);
    }
    if (subtitleAutomationStatusLabel_ != nullptr) {
        subtitleAutomationStatusLabel_->setText(uiText("Remembered subtitle delay applied: %1").arg(subtitleDelayDisplayText(clampedDelay)));
    }
}

void MainWindow::adjustSubtitleDelayWithFeedback(const double deltaSeconds)
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    setSubtitleDelayWithFeedback(currentSubtitleDelaySeconds_ + deltaSeconds);
}

void MainWindow::resetSubtitleDelayWithFeedback()
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    setSubtitleDelayWithFeedback(0.0, true);
}

void MainWindow::showManualSubtitleDelayDialog()
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    const std::optional<double> delaySeconds = promptManualDelay(
        this,
        uiText("Set Subtitle Delay"),
        uiText("Subtitle delay in seconds"),
        currentSubtitleDelaySeconds_);
    if (delaySeconds.has_value()) {
        setSubtitleDelayWithFeedback(*delaySeconds);
    }
}

void MainWindow::setAudioDelayWithFeedback(double delaySeconds, const bool resetRequested)
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    delaySeconds = std::clamp(delaySeconds, -3600.0, 3600.0);
    if (resetRequested || std::abs(delaySeconds) < 0.0005) {
        delaySeconds = 0.0;
        playbackController_->resetAudioDelay();
    } else {
        playbackController_->setAudioDelay(delaySeconds);
    }
    currentAudioDelaySeconds_ = delaySeconds;

    const QString text = resetRequested
        ? uiText("Audio delay reset to %1").arg(revaplayer::application::formatSignedSeconds(0.0))
        : uiText("Audio delay: %1").arg(revaplayer::application::formatSignedSeconds(delaySeconds));
    statusBar()->showMessage(text, 2500);
    showPlaybackFeedback(text);
}

void MainWindow::showManualAudioDelayDialog()
{
    if (playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    const std::optional<double> delaySeconds = promptManualDelay(
        this,
        uiText("Set Audio Delay"),
        uiText("Audio delay in seconds"),
        currentAudioDelaySeconds_);
    if (delaySeconds.has_value()) {
        setAudioDelayWithFeedback(*delaySeconds);
    }
}

void MainWindow::maybeAutoLoadMatchingLocalSubtitles()
{
    if (localSubtitlesAutoLoadedForCurrentMedia_
        || currentMediaSource_.trimmed().isEmpty()
        || playbackController_ == nullptr) {
        return;
    }

    const QString autoLoadMode = subtitleAutoLoadModeSetting();
    if (autoLoadMode == QStringLiteral("manual_only")) {
        localSubtitlesAutoLoadedForCurrentMedia_ = true;
        return;
    }

    if (std::any_of(currentTracks_.cbegin(), currentTracks_.cend(), [](const revaplayer::domain::TrackInfo &track) {
            return track.type == revaplayer::domain::TrackType::Subtitle && track.external;
        })) {
        localSubtitlesAutoLoadedForCurrentMedia_ = true;
        return;
    }

    const QString localPath = localMediaPathForSource(currentMediaSource_);
    if (localPath.isEmpty()) {
        localSubtitlesAutoLoadedForCurrentMedia_ = true;
        return;
    }

    const QFileInfo mediaFileInfo(localPath);
    if (!mediaFileInfo.exists() || !mediaFileInfo.isFile() || !mediaFileInfo.dir().exists()) {
        localSubtitlesAutoLoadedForCurrentMedia_ = true;
        return;
    }

    QVector<QPair<int, QString>> rankedFiles;
    const QString baseName = mediaFileInfo.completeBaseName();
    const QSet<QString> allowedExtensions = subtitleAutoLoadExtensionsSetting();
    for (const QFileInfo &candidate : mediaFileInfo.dir().entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!isSupportedSubtitleFile(candidate)) {
            continue;
        }
        if (!allowedExtensions.isEmpty() && !allowedExtensions.contains(candidate.suffix().trimmed().toLower())) {
            continue;
        }

        const int score = subtitleFileScore(candidate, baseName, preferredSubtitleLanguages());
        const bool sameNameOnlyMatch = subtitleMatchesSameNameOnly(candidate, baseName);
        const bool sameNameWithLanguageSuffixMatch = subtitleMatchesSameNameWithLanguageSuffix(candidate, baseName);
        const bool acceptCandidate = autoLoadMode == QStringLiteral("same_folder_any")
            || (autoLoadMode == QStringLiteral("same_name_only") && sameNameOnlyMatch)
            || (autoLoadMode == QStringLiteral("same_name_language") && sameNameWithLanguageSuffixMatch);
        if (acceptCandidate) {
            rankedFiles.push_back({score, candidate.absoluteFilePath()});
        }
    }

    std::sort(rankedFiles.begin(), rankedFiles.end(), [](const auto &left, const auto &right) {
        if (left.first != right.first) {
            return left.first > right.first;
        }
        return left.second.localeAwareCompare(right.second) < 0;
    });

    int maxAutoLoads = 1;
    if (autoLoadMode == QStringLiteral("same_name_only")) {
        maxAutoLoads = 2;
    } else if (autoLoadMode == QStringLiteral("same_name_language")) {
        maxAutoLoads = 3;
    } else if (autoLoadMode == QStringLiteral("same_folder_any")) {
        maxAutoLoads = 2;
    }

    int loadedCount = 0;
    QStringList loadedNames;
    for (const auto &entry : rankedFiles) {
        if (loadedCount >= maxAutoLoads) {
            break;
        }

        playbackController_->loadSubtitleFile(entry.second);
        loadedNames.push_back(QFileInfo(entry.second).fileName());
        ++loadedCount;
    }

    localSubtitlesAutoLoadedForCurrentMedia_ = true;
    if (loadedCount > 0) {
        const QString text = QStringLiteral("Loaded %1 matching subtitle file%2")
            .arg(loadedCount)
            .arg(loadedCount == 1 ? QString {} : QStringLiteral("s"));
        if (subtitleAutomationStatusLabel_ != nullptr) {
            subtitleAutomationStatusLabel_->setText(QStringLiteral("%1: %2").arg(text, loadedNames.join(QStringLiteral(", "))));
        }
        statusBar()->showMessage(text, 3500);
    }
}

void MainWindow::applySmartSubtitleSelection(const bool allowLocalSubtitleLoad, const bool announce)
{
    if (!announce && !subtitleAutoSelectEnabled()) {
        return;
    }

    if (allowLocalSubtitleLoad) {
        maybeAutoLoadMatchingLocalSubtitles();
    }

    QVector<revaplayer::domain::TrackInfo> subtitleTracks;
    subtitleTracks.reserve(currentTracks_.size());
    for (const auto &track : currentTracks_) {
        if (track.type == revaplayer::domain::TrackType::Subtitle) {
            subtitleTracks.push_back(track);
        }
    }

    if (subtitleTracks.isEmpty()) {
        if (subtitleAutomationStatusLabel_ != nullptr && announce) {
            subtitleAutomationStatusLabel_->setText(uiText("No subtitle tracks are available."));
        }
        return;
    }

    const QStringList languages = preferredSubtitleLanguages();
    const bool preferExternal = subtitlePreferExternal();
    const revaplayer::domain::TrackInfo *bestTrack = nullptr;
    int bestScore = std::numeric_limits<int>::min();
    for (const auto &track : subtitleTracks) {
        const int score = subtitleLanguageScore(track, languages, preferExternal);
        if (score > bestScore) {
            bestScore = score;
            bestTrack = &track;
        }
    }

    if (bestTrack == nullptr || playbackController_ == nullptr || !mediaLoaded_) {
        return;
    }

    const auto selectedIt = std::find_if(subtitleTracks.cbegin(), subtitleTracks.cend(), [](const auto &track) {
        return track.selected;
    });
    const int selectedId = selectedIt != subtitleTracks.cend() ? selectedIt->id : -1;
    const QString label = buildTrackTitle(*bestTrack);
    if (selectedId != bestTrack->id) {
        playbackController_->selectTrack(revaplayer::domain::TrackType::Subtitle, bestTrack->id);
        if (announce) {
            statusBar()->showMessage(uiText("Subtitle selected: %1").arg(label), 3500);
        }
    }

    if (subtitleAutomationStatusLabel_ != nullptr) {
        subtitleAutomationStatusLabel_->setText(
            selectedId == bestTrack->id
                ? uiText("Smart subtitle selection kept: %1").arg(label)
                : uiText("Smart subtitle selection chose: %1").arg(label));
    }
}

void MainWindow::triggerSubtitleDownload()
{
    if (currentMediaSource_.trimmed().isEmpty()) {
        statusBar()->showMessage(uiText("Load media before downloading subtitles."), 4000);
        return;
    }

    const QString templateCommand = subtitleDownloadCommandTemplate();
    if (templateCommand.trimmed().isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Subtitle Download"),
            QStringLiteral("Configure a subtitle download command in Preferences first."));
        return;
    }

    const QString localPath = localMediaPathForSource(currentMediaSource_);
    const auto fileInfo = localPath.isEmpty()
        ? std::optional<QFileInfo> {}
        : std::optional<QFileInfo> {QFileInfo(localPath)};
    QString expanded = templateCommand;
    expanded.replace(QStringLiteral("{file}"), localPath);
    expanded.replace(QStringLiteral("{source}"), currentMediaSource_);
    expanded.replace(QStringLiteral("{base_name}"), fileInfo.has_value() ? fileInfo->completeBaseName() : QString {});
    expanded.replace(QStringLiteral("{languages}"), preferredSubtitleLanguages().join(QStringLiteral(",")));

    QStringList commandParts = QProcess::splitCommand(expanded);
    if (commandParts.isEmpty()) {
        statusBar()->showMessage(uiText("The subtitle download command is invalid."), 5000);
        return;
    }

    const QString program = commandParts.takeFirst();
    const QString workingDirectory = fileInfo.has_value() && fileInfo->dir().exists()
        ? fileInfo->absolutePath()
        : QDir::homePath();
    const bool started = QProcess::startDetached(program, commandParts, workingDirectory);
    const QString message = started
        ? QStringLiteral("Subtitle downloader started")
        : QStringLiteral("Failed to launch the subtitle downloader");
    statusBar()->showMessage(message, 4000);
    if (subtitleAutomationStatusLabel_ != nullptr) {
        subtitleAutomationStatusLabel_->setText(message);
    }
}

void MainWindow::applyAudioFilterState()
{
    QStringList audioFilters;
    QStringList equalizerFilters;

    for (qsizetype index = 0; index < equalizerSliders_.size() && index < static_cast<qsizetype>(kEqualizerFrequencies.size()); ++index) {
        QSlider *slider = equalizerSliders_[index];
        if (slider == nullptr || slider->value() == 0) {
            continue;
        }

        equalizerFilters << QStringLiteral("equalizer=f=%1:t=q:w=1:g=%2")
                                 .arg(kEqualizerFrequencies[static_cast<size_t>(index)])
                                 .arg(QString::number(slider->value()));
    }

    if (!equalizerFilters.isEmpty()) {
        audioFilters << QStringLiteral("lavfi=[%1]").arg(equalizerFilters.join(QStringLiteral(",")));
    }
    if (audioNormalizeCheckBox_ != nullptr && audioNormalizeCheckBox_->isChecked()) {
        audioFilters << QStringLiteral("lavfi=[loudnorm=I=-16:TP=-1.5:LRA=11]");
    }
    if (customAudioFilterEdit_ != nullptr && !customAudioFilterEdit_->text().trimmed().isEmpty()) {
        audioFilters << customAudioFilterEdit_->text().trimmed();
    }

    const QString filterChain = audioFilters.join(QStringLiteral(","));
    if (mediaLabStatusLabel_ != nullptr) {
        mediaLabStatusLabel_->setText(
            filterChain.isEmpty()
                ? QStringLiteral("Audio filters cleared.")
                : QStringLiteral("Audio filters %1.").arg(mediaLoaded_ ? QStringLiteral("applied") : QStringLiteral("prepared")));
    }

    if (playbackController_ == nullptr || !playbackController_->isInitialized() || !mediaLoaded_) {
        return;
    }

    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("af"),
        filterChain,
    });
}

void MainWindow::applyVideoFilterState()
{
    QStringList videoFilters;
    if (videoSharpenSlider_ != nullptr && videoSharpenSlider_->value() > 0) {
        const double sharpenAmount = std::clamp(videoSharpenSlider_->value() / 55.0, 0.10, 2.0);
        videoFilters << QStringLiteral("unsharp=5:5:%1:5:5:0")
                            .arg(QString::number(sharpenAmount, 'f', 2));
    }
    if (videoDenoiseSlider_ != nullptr && videoDenoiseSlider_->value() > 0) {
        const double denoiseAmount = std::clamp(videoDenoiseSlider_->value() / 18.0, 0.50, 6.0);
        videoFilters << QStringLiteral("hqdn3d=%1:%2:6:6")
                            .arg(QString::number(denoiseAmount, 'f', 2),
                                 QString::number(std::max(0.50, denoiseAmount * 0.75), 'f', 2));
    }
    if (videoDebandCheckBox_ != nullptr && videoDebandCheckBox_->isChecked()) {
        videoFilters << QStringLiteral("gradfun=radius=16:strength=0.7");
    }
    if (stereo3dFilterEdit_ != nullptr && !stereo3dFilterEdit_->text().trimmed().isEmpty()) {
        videoFilters << stereo3dFilterEdit_->text().trimmed();
    }
    if (customVideoFilterEdit_ != nullptr && !customVideoFilterEdit_->text().trimmed().isEmpty()) {
        videoFilters << customVideoFilterEdit_->text().trimmed();
    }
    const QString filterChain = videoFilters.join(QStringLiteral(","));

    if (mediaLabStatusLabel_ != nullptr) {
        mediaLabStatusLabel_->setText(
            filterChain.isEmpty()
                ? QStringLiteral("Video filters ready.")
                : QStringLiteral("Video filters %1.").arg(mediaLoaded_ ? QStringLiteral("applied") : QStringLiteral("prepared")));
    }

    if (playbackController_ == nullptr || !playbackController_->isInitialized() || !mediaLoaded_ || !hasVideoTrack_) {
        return;
    }

    deinterlaceEnabled_ = videoDeinterlaceCheckBox_ != nullptr && videoDeinterlaceCheckBox_->isChecked();
    playbackController_->setDeinterlace(deinterlaceEnabled_);
    if (deinterlaceAction_ != nullptr) {
        const QSignalBlocker blocker(deinterlaceAction_);
        deinterlaceAction_->setChecked(deinterlaceEnabled_);
    }

    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("brightness"),
        QString::number(videoBrightnessSlider_ != nullptr ? videoBrightnessSlider_->value() : 0),
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("contrast"),
        QString::number(videoContrastSlider_ != nullptr ? videoContrastSlider_->value() : 0),
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("saturation"),
        QString::number(videoSaturationSlider_ != nullptr ? videoSaturationSlider_->value() : 0),
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("gamma"),
        QString::number(videoGammaSlider_ != nullptr ? videoGammaSlider_->value() : 0),
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("hue"),
        QString::number(videoHueSlider_ != nullptr ? videoHueSlider_->value() : 0),
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("vf"),
        filterChain,
    });
    playbackController_->executeMpvCommand({
        QStringLiteral("set"),
        QStringLiteral("glsl-shaders"),
        shaderPathEdit_ != nullptr ? shaderPathEdit_->text().trimmed() : QString {},
    });
}

void MainWindow::dispatchWheelAction(const int steps, const Qt::KeyboardModifiers modifiers)
{
    if (steps == 0 || !mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        return;
    }

    if (hasVideoTrack_) {
        const QString wheelBehavior = videoZoomWheelBehaviorSetting();
        const bool modifierPressed = zoomWheelModifierPressed(modifiers);
        const bool zoomWhileZoomed = wheelBehavior == QStringLiteral("zoom_when_zoomed")
            && (modifierPressed || currentVideoZoomFactor_ > 1.0 + kVideoTransformEpsilon);
        const bool zoomWithModifier = wheelBehavior == QStringLiteral("zoom_with_ctrl") && modifierPressed;
        if (zoomWhileZoomed || zoomWithModifier) {
            adjustVideoZoomFactor(videoZoomStepSetting() * steps);
            return;
        }
    }

    const QString actionId = mouseWheelActionId();
    if (actionId == QStringLiteral("seek")) {
        seekBySecondsWithFeedback(steps * mouseWheelSeekStepSeconds());
        return;
    }
    if (actionId == QStringLiteral("volume")) {
        adjustVolumeWithFeedback(steps * mouseWheelVolumeStep());
    }
}

void MainWindow::dispatchClickAction(const Qt::MouseButton button)
{
    if (button != Qt::LeftButton) {
        return;
    }

    const QString actionId = clickActionId();
    if (actionId == QStringLiteral("play_pause")) {
        if (playbackController_ != nullptr) {
            playbackController_->togglePause();
        }
        return;
    }
    if (actionId == QStringLiteral("playlist")) {
        toggleSidePanel(SidePanel::Playlist);
        return;
    }
    if (actionId == QStringLiteral("details")) {
        toggleSidePanel(SidePanel::Details);
        return;
    }
    if (actionId == QStringLiteral("fullscreen")) {
        toggleFullscreen();
        return;
    }
    if (actionId == QStringLiteral("mute")) {
        if (playbackController_ != nullptr) {
            playbackController_->toggleMuted();
        }
        return;
    }
    if (actionId == QStringLiteral("subtitles")) {
        if (playbackController_ != nullptr) {
            playbackController_->toggleSubtitleVisible();
        }
        return;
    }
    if (playbackController_ == nullptr || !mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        return;
    }
    if (actionId == QStringLiteral("next_playlist")) {
        navigatePlaylistWithResumeGuard(true);
        return;
    }
    if (actionId == QStringLiteral("previous_playlist")) {
        navigatePlaylistWithResumeGuard(false);
        return;
    }
    if (actionId == QStringLiteral("seek_forward_short")) {
        seekBySecondsWithFeedback(shortSeekStepSeconds());
        return;
    }
    if (actionId == QStringLiteral("seek_backward_short")) {
        seekBySecondsWithFeedback(-shortSeekStepSeconds());
        return;
    }
    if (!hasVideoTrack_) {
        return;
    }
    if (actionId == QStringLiteral("zoom_in")) {
        adjustVideoZoomFactor(videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_out")) {
        adjustVideoZoomFactor(-videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_reset")) {
        resetVideoZoomAndPan(true);
    }
}

void MainWindow::dispatchMouseSideButtonAction(const bool forward)
{
    if (!mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        return;
    }

    const QString actionId = mouseSideButtonsActionId();
    if (actionId == QStringLiteral("seek_long")) {
        seekBySecondsWithFeedback(forward ? longSeekStepSeconds() : -longSeekStepSeconds());
        return;
    }
    if (actionId == QStringLiteral("playlist")) {
        navigatePlaylistWithResumeGuard(forward);
        return;
    }
    if (actionId == QStringLiteral("chapter")) {
        if (forward) {
            playbackController_->nextChapter();
        } else {
            playbackController_->previousChapter();
        }
        return;
    }
    if (actionId == QStringLiteral("zoom")) {
        if (hasVideoTrack_) {
            adjustVideoZoomFactor(forward ? videoZoomStepSetting() : -videoZoomStepSetting());
        }
        return;
    }
    if (actionId == QStringLiteral("seek_short")) {
        seekBySecondsWithFeedback(forward ? mouseNavigationSeekStepSeconds() : -mouseNavigationSeekStepSeconds());
    }
}

void MainWindow::dispatchDoubleClickAction(const Qt::MouseButton button)
{
    if (button != Qt::LeftButton) {
        return;
    }

    const QString actionId = doubleClickActionId();
    if (actionId == QStringLiteral("play_pause")) {
        if (playbackController_ != nullptr) {
            playbackController_->togglePause();
        }
        return;
    }
    if (actionId == QStringLiteral("playlist")) {
        toggleSidePanel(SidePanel::Playlist);
        return;
    }
    if (actionId == QStringLiteral("details")) {
        toggleSidePanel(SidePanel::Details);
        return;
    }
    if (actionId == QStringLiteral("reload_folder_playlist")) {
        reloadCurrentFolderPlaylist();
        return;
    }
    if (actionId == QStringLiteral("fullscreen")) {
        toggleFullscreen();
        return;
    }
    if (actionId == QStringLiteral("mute")) {
        if (playbackController_ != nullptr) {
            playbackController_->toggleMuted();
        }
        return;
    }
    if (actionId == QStringLiteral("subtitles")) {
        if (playbackController_ != nullptr) {
            playbackController_->toggleSubtitleVisible();
        }
        return;
    }
    if (playbackController_ == nullptr || !mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        return;
    }
    if (actionId == QStringLiteral("next_playlist")) {
        navigatePlaylistWithResumeGuard(true);
        return;
    }
    if (actionId == QStringLiteral("previous_playlist")) {
        navigatePlaylistWithResumeGuard(false);
        return;
    }
    if (actionId == QStringLiteral("seek_forward_short")) {
        seekBySecondsWithFeedback(shortSeekStepSeconds());
        return;
    }
    if (actionId == QStringLiteral("seek_backward_short")) {
        seekBySecondsWithFeedback(-shortSeekStepSeconds());
        return;
    }
    if (!hasVideoTrack_) {
        return;
    }
    if (actionId == QStringLiteral("zoom_in")) {
        adjustVideoZoomFactor(videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_out")) {
        adjustVideoZoomFactor(-videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_reset")) {
        resetVideoZoomAndPan(true);
        return;
    }
}

void MainWindow::dispatchMiddleClickAction()
{
    if (playbackController_ == nullptr) {
        return;
    }

    const QString actionId = middleClickActionId();
    if (actionId == QStringLiteral("play_pause")) {
        playbackController_->togglePause();
        return;
    }
    if (actionId == QStringLiteral("mute")) {
        playbackController_->toggleMuted();
        return;
    }
    if (actionId == QStringLiteral("subtitles")) {
        playbackController_->toggleSubtitleVisible();
        return;
    }
    if (actionId == QStringLiteral("playlist")) {
        toggleSidePanel(SidePanel::Playlist);
        return;
    }
    if (actionId == QStringLiteral("details")) {
        toggleSidePanel(SidePanel::Details);
        return;
    }
    if (actionId == QStringLiteral("fullscreen")) {
        toggleFullscreen();
        return;
    }
    if (!mediaLoaded_ || loadingMedia_ || errorStateActive_) {
        return;
    }
    if (actionId == QStringLiteral("next_playlist")) {
        navigatePlaylistWithResumeGuard(true);
        return;
    }
    if (actionId == QStringLiteral("previous_playlist")) {
        navigatePlaylistWithResumeGuard(false);
        return;
    }
    if (actionId == QStringLiteral("seek_forward_short")) {
        seekBySecondsWithFeedback(shortSeekStepSeconds());
        return;
    }
    if (actionId == QStringLiteral("seek_backward_short")) {
        seekBySecondsWithFeedback(-shortSeekStepSeconds());
        return;
    }
    if (!hasVideoTrack_) {
        return;
    }
    if (actionId == QStringLiteral("zoom_in")) {
        adjustVideoZoomFactor(videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_out")) {
        adjustVideoZoomFactor(-videoZoomStepSetting());
        return;
    }
    if (actionId == QStringLiteral("zoom_reset")) {
        resetVideoZoomAndPan(true);
        return;
    }
}

void MainWindow::dispatchGestureAction(const QString &directionId)
{
    if (!mediaLoaded_ || playbackController_ == nullptr || loadingMedia_ || errorStateActive_) {
        return;
    }

    const QString actionId = normalizeGestureAction(
        customSettingValue(
            settingsController_,
            gestureActionSettingKey(directionId).toUtf8().constData(),
            defaultGestureAction(directionId)));

    if (actionId == QStringLiteral("seek_backward_short")) {
        seekBySecondsWithFeedback(-shortSeekStepSeconds());
        return;
    } else if (actionId == QStringLiteral("seek_forward_short")) {
        seekBySecondsWithFeedback(shortSeekStepSeconds());
        return;
    } else if (actionId == QStringLiteral("volume_up")) {
        adjustVolumeWithFeedback(volumeStep());
        return;
    } else if (actionId == QStringLiteral("volume_down")) {
        adjustVolumeWithFeedback(-volumeStep());
        return;
    } else if (actionId == QStringLiteral("speed_up")) {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(currentSpeed_ + 0.10);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
        return;
    } else if (actionId == QStringLiteral("speed_down")) {
        const double targetSpeed = revaplayer::application::clampPlaybackSpeed(currentSpeed_ - 0.10);
        playbackController_->setSpeed(targetSpeed);
        if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
            comparePlaybackController_->setSpeed(targetSpeed);
        }
        showPlaybackSpeedFeedback(targetSpeed);
        return;
    } else if (actionId == QStringLiteral("playlist")) {
        setSidePanelVisible(SidePanel::Playlist, !isSidePanelVisible(SidePanel::Playlist), true);
    } else if (actionId == QStringLiteral("details")) {
        setSidePanelVisible(SidePanel::Details, !isSidePanelVisible(SidePanel::Details), true);
    } else if (actionId == QStringLiteral("play_pause")) {
        playbackController_->togglePause();
    } else if (actionId == QStringLiteral("subtitle_delay_up")) {
        adjustSubtitleDelayWithFeedback(subtitleSyncSmallStep());
    } else if (actionId == QStringLiteral("subtitle_delay_down")) {
        adjustSubtitleDelayWithFeedback(-subtitleSyncSmallStep());
    } else {
        return;
    }

    showPlaybackFeedback(
        QStringLiteral("Gesture  •  %1")
            .arg(gestureActionLabel(actionId)));
}

QVector<int> MainWindow::selectedPlaylistIndices() const
{
    QVector<int> indices;
    if (pinnedCourseBrowserActive_
        || playlistView_ == nullptr
        || playlistController_ == nullptr
        || playlistView_->selectionModel() == nullptr) {
        return indices;
    }

    const QModelIndexList selectedRows = playlistView_->selectionModel()->selectedRows();
    indices.reserve(selectedRows.size());
    for (const QModelIndex &index : selectedRows) {
        const int playlistIndex = playlistController_->playlistIndexFor(index);
        if (playlistIndex >= 0 && !indices.contains(playlistIndex)) {
            indices.push_back(playlistIndex);
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void MainWindow::removeSelectedPlaylistItems()
{
    const QVector<int> indices = selectedPlaylistIndices();
    if (indices.isEmpty() || playbackController_ == nullptr) {
        return;
    }

    for (auto it = indices.crbegin(); it != indices.crend(); ++it) {
        playbackController_->executeMpvCommand({QStringLiteral("playlist-remove"), QString::number(*it)});
    }
    statusBar()->showMessage(uiText("Removed %1 item%2 from the list").arg(indices.size()).arg(indices.size() == 1 ? QString {} : QStringLiteral("s")), 3000);
}

void MainWindow::keepOnlySelectedPlaylistItems()
{
    const QVector<int> selected = selectedPlaylistIndices();
    if (selected.isEmpty() || playbackController_ == nullptr) {
        return;
    }

    QVector<int> toRemove;
    for (int index = 0; index < playlistCount_; ++index) {
        if (!selected.contains(index)) {
            toRemove.push_back(index);
        }
    }
    for (auto it = toRemove.crbegin(); it != toRemove.crend(); ++it) {
        playbackController_->executeMpvCommand({QStringLiteral("playlist-remove"), QString::number(*it)});
    }
    statusBar()->showMessage(uiText("Kept %1 selected item%2").arg(selected.size()).arg(selected.size() == 1 ? QString {} : QStringLiteral("s")), 3000);
}

void MainWindow::removeDuplicatePlaylistItems()
{
    if (playbackController_ == nullptr || playlistController_ == nullptr) {
        return;
    }

    QSet<QString> seenSources;
    QVector<int> duplicateIndices;
    const auto entries = playlistController_->entries();
    for (const auto &entry : entries) {
        const QString normalized = entry.source.trimmed().toLower();
        if (normalized.isEmpty()) {
            continue;
        }
        if (seenSources.contains(normalized)) {
            duplicateIndices.push_back(entry.index);
        } else {
            seenSources.insert(normalized);
        }
    }

    if (duplicateIndices.isEmpty()) {
        statusBar()->showMessage(uiText("Playlist has no duplicates"), 2500);
        return;
    }

    std::sort(duplicateIndices.begin(), duplicateIndices.end());
    for (auto it = duplicateIndices.crbegin(); it != duplicateIndices.crend(); ++it) {
        playbackController_->executeMpvCommand({QStringLiteral("playlist-remove"), QString::number(*it)});
    }
    statusBar()->showMessage(uiText("Removed %1 duplicate playlist item%2").arg(duplicateIndices.size()).arg(duplicateIndices.size() == 1 ? QString {} : QStringLiteral("s")), 3000);
}

void MainWindow::openPlaylistSources(const QStringList &sources,
                                     const int selectedIndex,
                                     const bool revealPlaylistPanel,
                                     const bool exitFolderBrowserMode)
{
    if (sources.isEmpty() || playbackController_ == nullptr) {
        return;
    }

    if (exitFolderBrowserMode) {
        pinnedCourseBrowserActive_ = false;
        pinnedCourseBrowserFolderPath_.clear();
        pinnedCourseBrowserRootPath_.clear();
        pinnedCourseBrowserHistory_.clear();
    }
    const int maxIndex = std::max(0, static_cast<int>(sources.size()) - 1);
    const int clampedSelectedIndex = std::clamp(selectedIndex, 0, maxIndex);
    clearPendingPlaylistSelection();
    pendingPlaylistSelectionIndex_ = clampedSelectedIndex;
    pendingPlaylistSelectionExpectedCount_ = sources.size();
    pendingPlaylistSelectionRetryCount_ = 0;
    pendingPlaylistSelectionSource_ = sources.value(clampedSelectedIndex);
    pendingPlaylistNaturalOrderSources_.clear();
    pendingPlaylistRequestedOrderSources_.clear();
    clearPendingPlaylistViewportRestore();

    QStringList loadSources = sources;
    if (clampedSelectedIndex > 0 && clampedSelectedIndex < loadSources.size()) {
        const QString selectedSource = loadSources.takeAt(clampedSelectedIndex);
        loadSources.prepend(selectedSource);
        pendingPlaylistNaturalOrderSources_ = sources;
    }

    Q_UNUSED(revealPlaylistPanel);
    playlistActivationPanelRestorePending_ = false;
    playlistActivationPlaylistVisible_ = false;
    playlistActivationDetailsVisible_ = false;
    temporaryPlaylistHidePending_ = false;
    enforceHiddenSidePanelsAfterMediaOpen(1100, true);
    loadingMediaSource_ = sources.value(clampedSelectedIndex).trimmed();
    playbackController_->openFiles(loadSources);
    schedulePendingPlaylistSelectionRetry(kPendingPlaylistSelectionRetryDelayMs);
}

void MainWindow::saveCurrentPlaylistSnapshot()
{
    if (playlistController_ == nullptr || settingsController_ == nullptr) {
        return;
    }

    const auto entries = playlistController_->entries();
    if (entries.isEmpty()) {
        statusBar()->showMessage(uiText("Playlist is empty"), 2500);
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("Save Playlist Snapshot"),
        QStringLiteral("Snapshot name"),
        QLineEdit::Normal,
        effectiveCurrentMediaTitle().isEmpty() ? QStringLiteral("Playlist Snapshot") : effectiveCurrentMediaTitle(),
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    QJsonArray sourcesArray;
    for (const auto &entry : entries) {
        sourcesArray.push_back(entry.source);
    }

    QJsonObject snapshot;
    snapshot.insert(QStringLiteral("name"), name);
    snapshot.insert(QStringLiteral("current_index"), currentPlaylistIndex_);
    snapshot.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapshot.insert(QStringLiteral("sources"), sourcesArray);
    settingsController_->setCustomValue(
        QStringLiteral("%1%2").arg(QString::fromLatin1(kPlaylistSnapshotPrefix), encodeSettingKeySegment(name)),
        QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Compact)));
    showActionResult(uiText("Playlist snapshot saved"),
                     uiText("Playlist snapshot saved"),
                     3000);
}

void MainWindow::loadPlaylistSnapshot(const QString &snapshotKey)
{
    if (settingsController_ == nullptr) {
        return;
    }

    const QByteArray payload = settingsController_->customValue(snapshotKey).toUtf8();
    const QJsonObject snapshot = QJsonDocument::fromJson(payload).object();
    QStringList sources;
    for (const QJsonValue &value : snapshot.value(QStringLiteral("sources")).toArray()) {
        const QString source = value.toString().trimmed();
        if (!source.isEmpty()) {
            sources.push_back(source);
        }
    }

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("The selected snapshot is empty"), 3000);
        return;
    }

    openPlaylistSources(sources, snapshot.value(QStringLiteral("current_index")).toInt(0));
}

void MainWindow::deletePlaylistSnapshot(const QString &snapshotKey)
{
    if (settingsController_ == nullptr || snapshotKey.trimmed().isEmpty()) {
        return;
    }

    settingsController_->removeCustomValue(snapshotKey);
    statusBar()->showMessage(uiText("Playlist snapshot deleted"), 3000);
}

QStringList MainWindow::playlistSnapshotKeys() const
{
    return settingsController_ != nullptr
        ? settingsController_->customKeys(QString::fromLatin1(kPlaylistSnapshotPrefix))
        : QStringList {};
}

QString MainWindow::playlistSnapshotLabel(const QString &snapshotKey) const
{
    if (settingsController_ == nullptr || snapshotKey.trimmed().isEmpty()) {
        return QStringLiteral("Snapshot");
    }

    const QByteArray payload = settingsController_->customValue(snapshotKey).toUtf8();
    const QJsonObject snapshot = QJsonDocument::fromJson(payload).object();
    const QString name = snapshot.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty()) {
        return name;
    }

    return decodeSettingKeySegment(snapshotKey.mid(QString::fromLatin1(kPlaylistSnapshotPrefix).size()));
}

void MainWindow::buildSmartPlaylistFromSameFolder()
{
    const QString localPath = localMediaPathForSource(currentMediaSource_);
    const QStringList sources = siblingMediaPlaylist(localPath);
    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("No same-folder media items were found"), 3000);
        return;
    }
    openPlaylistSources(sources, 0);
}

void MainWindow::buildSmartPlaylistFromSameExtension()
{
    const QString localPath = localMediaPathForSource(currentMediaSource_);
    const QFileInfo currentFile(localPath);
    if (!currentFile.exists() || !currentFile.isFile()) {
        statusBar()->showMessage(uiText("Current media is not a local file"), 3000);
        return;
    }

    QStringList sameFolder = siblingMediaPlaylist(currentFile.absoluteFilePath());
    const QString suffix = currentFile.suffix().trimmed().toLower();
    sameFolder.erase(std::remove_if(sameFolder.begin(), sameFolder.end(), [&suffix](const QString &source) {
                         return QFileInfo(source).suffix().trimmed().toLower() != suffix;
                     }),
                     sameFolder.end());
    if (sameFolder.isEmpty()) {
        statusBar()->showMessage(uiText("No matching-extension media items were found"), 3000);
        return;
    }
    openPlaylistSources(sameFolder, 0);
}

void MainWindow::buildSmartPlaylistFromIncompleteHistory()
{
    if (historyController_ == nullptr || !historyController_->isReady()) {
        statusBar()->showMessage(uiText("History is unavailable"), 3000);
        return;
    }

    QStringList sources;
    QSet<QString> seen;
    for (const auto &entry : historyController_->recentHistory(250)) {
        if (entry.completed) {
            continue;
        }
        const QString source = entry.source.trimmed();
        if (source.isEmpty() || seen.contains(source)) {
            continue;
        }
        seen.insert(source);
        sources.push_back(source);
    }

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("No incomplete history items were found"), 3000);
        return;
    }
    openPlaylistSources(sources, 0);
}

void MainWindow::buildSmartPlaylistFromRecentHistory()
{
    if (historyController_ == nullptr || !historyController_->isReady()) {
        statusBar()->showMessage(uiText("History is unavailable"), 3000);
        return;
    }

    QStringList sources;
    QSet<QString> seen;
    for (const auto &entry : historyController_->recentHistory(80)) {
        const QString source = entry.source.trimmed();
        if (source.isEmpty() || seen.contains(source)) {
            continue;
        }
        seen.insert(source);
        sources.push_back(source);
    }

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("No recent history items were found"), 3000);
        return;
    }
    openPlaylistSources(sources, 0);
}

void MainWindow::buildSmartPlaylistFromNoSubtitles()
{
    const QString localPath = localMediaPathForSource(currentMediaSource_);
    const QFileInfo currentFile(localPath);
    if (!currentFile.exists() || !currentFile.isFile()) {
        statusBar()->showMessage(uiText("Current media is not a local file"), 3000);
        return;
    }

    QStringList sources = siblingMediaPlaylist(currentFile.absoluteFilePath());
    sources.erase(std::remove_if(sources.begin(), sources.end(), [](const QString &source) {
                     const QFileInfo fileInfo(source);
                     if (!fileInfo.exists() || !fileInfo.isFile()) {
                         return true;
                     }

                     const QDir directory = fileInfo.dir();
                     const QString baseName = fileInfo.completeBaseName();
                     const QFileInfoList siblings = directory.entryInfoList(
                         QDir::Files | QDir::NoDotAndDotDot,
                         QDir::Name);
                     return std::any_of(siblings.cbegin(), siblings.cend(), [&fileInfo, &baseName](const QFileInfo &candidate) {
                         return candidate.absoluteFilePath() != fileInfo.absoluteFilePath()
                             && candidate.completeBaseName() == baseName
                             && isSupportedSubtitleFile(candidate);
                     });
                 }),
                 sources.end());

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("All same-folder items already have sibling subtitles"), 3000);
        return;
    }
    openPlaylistSources(sources, 0);
}

void MainWindow::buildSmartPlaylistFromFavorites()
{
    if (settingsController_ == nullptr) {
        return;
    }

    QStringList sources;
    const QStringList keys = settingsController_->customKeys(QString::fromLatin1(kFavoriteMediaPrefix));
    for (const QString &key : keys) {
        const QJsonObject object = QJsonDocument::fromJson(settingsController_->customValue(key).toUtf8()).object();
        const QString source = object.value(QStringLiteral("source")).toString().trimmed();
        if (!source.isEmpty()) {
            sources.push_back(source);
        }
    }

    if (sources.isEmpty()) {
        statusBar()->showMessage(uiText("Favorites list is empty"), 3000);
        return;
    }
    openPlaylistSources(sources, 0);
}

void MainWindow::loadSmartPlaylistRule(const QString &ruleKey)
{
    if (settingsController_ == nullptr || ruleKey.trimmed().isEmpty()) {
        return;
    }

    const QJsonObject ruleObject = QJsonDocument::fromJson(settingsController_->customValue(ruleKey).toUtf8()).object();
    const QString kind = ruleObject.value(QStringLiteral("kind")).toString().trimmed().toLower();
    if (kind == QStringLiteral("folder")) {
        buildSmartPlaylistFromSameFolder();
    } else if (kind == QStringLiteral("extension")) {
        buildSmartPlaylistFromSameExtension();
    } else if (kind == QStringLiteral("incomplete")) {
        buildSmartPlaylistFromIncompleteHistory();
    } else if (kind == QStringLiteral("recent")) {
        buildSmartPlaylistFromRecentHistory();
    } else if (kind == QStringLiteral("no_subtitles")) {
        buildSmartPlaylistFromNoSubtitles();
    } else if (kind == QStringLiteral("favorites")) {
        buildSmartPlaylistFromFavorites();
    }
}

void MainWindow::saveSmartPlaylistRule(const QString &ruleKind, const QString &defaultName)
{
    if (settingsController_ == nullptr || ruleKind.trimmed().isEmpty()) {
        return;
    }

    bool accepted = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("Save Smart Playlist Rule"),
        QStringLiteral("Rule name"),
        QLineEdit::Normal,
        defaultName.trimmed().isEmpty() ? QStringLiteral("Smart Playlist") : defaultName.trimmed(),
        &accepted).trimmed();
    if (!accepted || name.isEmpty()) {
        return;
    }

    settingsController_->setCustomValue(
        smartPlaylistRuleStorageKey(name),
        QString::fromUtf8(QJsonDocument(smartPlaylistRuleObject(name, ruleKind)).toJson(QJsonDocument::Compact)));
    statusBar()->showMessage(uiText("Smart playlist rule saved"), 2500);
}

void MainWindow::deleteSmartPlaylistRule(const QString &ruleKey)
{
    if (settingsController_ == nullptr || ruleKey.trimmed().isEmpty()) {
        return;
    }

    settingsController_->removeCustomValue(ruleKey);
    statusBar()->showMessage(uiText("Smart playlist rule deleted"), 2500);
}

void MainWindow::toggleFavoriteCurrentMedia()
{
    if (settingsController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString key = favoriteStorageKey(currentMediaSource_);
    if (favoriteCurrentMedia_) {
        settingsController_->removeCustomValue(key);
        favoriteCurrentMedia_ = false;
        showActionResult(uiText("Removed current media from favorites"),
                         uiText("Removed current media from favorites"),
                         2500);
    } else {
        QJsonObject object;
        object.insert(QStringLiteral("source"), currentMediaSource_);
        object.insert(QStringLiteral("title"), effectiveCurrentMediaTitle());
        object.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        settingsController_->setCustomValue(key, QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
        favoriteCurrentMedia_ = true;
        showActionResult(uiText("Current media added to favorites"),
                         uiText("Current media added to favorites"),
                         2500);
    }
    reloadFavoritesPanel();
    refreshPlaylistPresentationData();
    reloadHomeDashboard();
    updateActionStates();
}

void MainWindow::openCompareSource()
{
    if (comparePlaybackController_ == nullptr) {
        return;
    }

    const QString filePath = filedialog::getOpenFileName(
        this,
        uiText("Open Compare Source"),
        settingsController_ != nullptr ? settingsController_->lastOpenDirectory() : QDir::homePath(),
        uiText("Media Files (*.mkv *.mp4 *.webm *.avi *.mov *.mp3 *.flac *.wav *.m4a *.ogg);;All Files (*)"));
    if (filePath.trimmed().isEmpty()) {
        return;
    }

    compareSource_ = QFileInfo(filePath).absoluteFilePath();
    compareSourceLoaded_ = false;
    comparePrimaryVisible_ = true;
    if (compareVideoViewport_ != nullptr) {
        compareVideoViewport_->setOverlayText(uiText("Loading compare source"));
        compareVideoViewport_->setOverlayVisible(true);
    }
    comparePlaybackController_->openFiles(QStringList {compareSource_});
    setCompareMode(QStringLiteral("side_by_side"));
    statusBar()->showMessage(uiText("Opening compare source: %1").arg(QFileInfo(compareSource_).fileName()), 3000);
}

void MainWindow::setCompareMode(const QString &modeId)
{
    QString normalized = modeId.trimmed().toLower();
    if (normalized != QStringLiteral("side_by_side") && normalized != QStringLiteral("ab")) {
        normalized = QStringLiteral("off");
    }

    compareModeId_ = normalized;
    if (compareModeId_ == QStringLiteral("off")) {
        comparePrimaryVisible_ = true;
        if (videoViewport_ != nullptr) {
            videoViewport_->show();
        }
        if (compareVideoViewport_ != nullptr) {
            compareVideoViewport_->setRenderHostVisible(false);
            compareVideoViewport_->hide();
        }
        updateActionStates();
        return;
    }

    if (compareVideoViewport_ == nullptr || compareSource_.trimmed().isEmpty()) {
        compareModeId_ = QStringLiteral("off");
        updateActionStates();
        return;
    }

    compareVideoViewport_->setRenderHostVisible(true);
    compareVideoViewport_->show();
    if (compareModeId_ == QStringLiteral("side_by_side")) {
        comparePrimaryVisible_ = true;
        if (videoViewport_ != nullptr) {
            videoViewport_->show();
        }
        compareVideoViewport_->show();
    } else {
        if (videoViewport_ != nullptr) {
            videoViewport_->setVisible(comparePrimaryVisible_);
        }
        compareVideoViewport_->setVisible(!comparePrimaryVisible_);
    }

    updateVideoOverlayGeometry();
    updateActionStates();
}

void MainWindow::toggleCompareView()
{
    if (compareModeId_ != QStringLiteral("ab")) {
        return;
    }

    comparePrimaryVisible_ = !comparePrimaryVisible_;
    if (videoViewport_ != nullptr) {
        videoViewport_->setVisible(comparePrimaryVisible_);
    }
    if (compareVideoViewport_ != nullptr) {
        compareVideoViewport_->setRenderHostVisible(!comparePrimaryVisible_);
        compareVideoViewport_->setVisible(!comparePrimaryVisible_);
    }
    showPlaybackFeedback(comparePrimaryVisible_ ? uiText("Compare A") : uiText("Compare B"));
    updateActionStates();
}

void MainWindow::showScreenshotSuiteDialog()
{
    if (!mediaLoaded_ || playbackController_ == nullptr || snapshotController_ == nullptr) {
        statusBar()->showMessage(uiText("Load media before using Screenshot Suite."), 3000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(uiText("Screenshot Suite"));
    dialog.resize(480, 260);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *form = new QFormLayout();
    auto *countSpin = new QSpinBox(&dialog);
    countSpin->setRange(1, 25);
    countSpin->setValue(6);
    auto *intervalSpin = new QSpinBox(&dialog);
    intervalSpin->setRange(40, 5000);
    intervalSpin->setSuffix(QStringLiteral(" ms"));
    intervalSpin->setValue(240);
    auto *columnsSpin = new QSpinBox(&dialog);
    columnsSpin->setRange(2, 6);
    columnsSpin->setValue(4);
    auto *rowsSpin = new QSpinBox(&dialog);
    rowsSpin->setRange(1, 4);
    rowsSpin->setValue(2);
    auto *formatCombo = new QComboBox(&dialog);
    formatCombo->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    formatCombo->addItem(QStringLiteral("JPG"), QStringLiteral("jpg"));
    formatCombo->setCurrentIndex(formatCombo->findData(screenshotFormat(settingsController_)));
    auto *templateEdit = new QLineEdit(screenshotTemplate(settingsController_), &dialog);

    form->addRow(uiText("Burst frames"), countSpin);
    form->addRow(uiText("Burst interval"), intervalSpin);
    form->addRow(uiText("Contact sheet columns"), columnsSpin);
    form->addRow(uiText("Contact sheet rows"), rowsSpin);
    form->addRow(uiText("Format"), formatCombo);
    form->addRow(uiText("Filename template"), templateEdit);

    auto *hintLabel = new QLabel(
        uiText("Use {timestamp}, {title}, and {index} inside the filename template."),
        &dialog);
    hintLabel->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(&dialog);
    QPushButton *burstButton = buttons->addButton(uiText("Burst Capture"), QDialogButtonBox::AcceptRole);
    QPushButton *contactSheetButton = buttons->addButton(uiText("Contact Sheet"), QDialogButtonBox::ActionRole);
    QPushButton *closeButton = buttons->addButton(QDialogButtonBox::Close);

    layout->addLayout(form);
    layout->addWidget(hintLabel, 0);
    layout->addWidget(buttons, 0);

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(burstButton, &QPushButton::clicked, &dialog, [this, &dialog, countSpin, intervalSpin, formatCombo, templateEdit]() {
        if (settingsController_ != nullptr) {
            settingsController_->setCustomValue(QString::fromLatin1(kScreenshotFormatSetting), formatCombo->currentData().toString());
            settingsController_->setCustomValue(QString::fromLatin1(kScreenshotTemplateSetting), templateEdit->text().trimmed());
        }
        pendingBurstScreenshots_ = countSpin->value();
        completedBurstScreenshots_ = 0;
        pendingBurstIntervalMs_ = intervalSpin->value();
        pendingBurstMediaLabel_ = effectiveCurrentMediaTitle();
        pendingBurstFormat_ = formatCombo->currentData().toString();
        performBurstScreenshotStep();
        dialog.accept();
    });
    connect(contactSheetButton, &QPushButton::clicked, &dialog, [this, &dialog, columnsSpin, rowsSpin, formatCombo, templateEdit]() {
        if (settingsController_ != nullptr) {
            settingsController_->setCustomValue(QString::fromLatin1(kScreenshotFormatSetting), formatCombo->currentData().toString());
            settingsController_->setCustomValue(QString::fromLatin1(kScreenshotTemplateSetting), templateEdit->text().trimmed());
        }

        QList<QPair<QString, QImage>> frames;
        const int maxFrames = columnsSpin->value() * rowsSpin->value();
        const auto harvestFrames = [&frames, maxFrames](QListWidget *listWidget) {
            if (listWidget == nullptr) {
                return;
            }
            for (int row = 0; row < listWidget->count() && frames.size() < maxFrames; ++row) {
                QListWidgetItem *item = listWidget->item(row);
                if (item == nullptr) {
                    continue;
                }
                const QImage image = item->icon().pixmap(240, 135).toImage();
                if (image.isNull()) {
                    continue;
                }
                frames.push_back(qMakePair(item->text(), image));
            }
        };

        harvestFrames(bookmarksList_);
        if (frames.size() < maxFrames) {
            harvestFrames(sceneList_);
        }

        if (frames.isEmpty()) {
            statusBar()->showMessage(uiText("Generate bookmark or scene thumbnails first to create a contact sheet."), 4000);
            return;
        }

        const int tileWidth = 240;
        const int tileHeight = 150;
        QImage sheet(
            tileWidth * columnsSpin->value(),
            tileHeight * rowsSpin->value(),
            QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(QStringLiteral("#10141a")));

        QPainter painter(&sheet);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QColor(QStringLiteral("#edf1f5")));
        painter.setFont(QFont(QStringLiteral("Noto Sans"), 10, QFont::DemiBold));

        for (int index = 0; index < frames.size(); ++index) {
            const int row = index / columnsSpin->value();
            const int column = index % columnsSpin->value();
            const QRect tileRect(column * tileWidth, row * tileHeight, tileWidth, tileHeight);
            painter.fillRect(tileRect.adjusted(6, 6, -6, -6), QColor(QStringLiteral("#18202b")));
            painter.drawImage(
                tileRect.adjusted(8, 8, -8, -32),
                frames.at(index).second.scaled(tileRect.width() - 16, tileRect.height() - 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            painter.drawText(tileRect.adjusted(10, tileRect.height() - 28, -10, -8), Qt::AlignLeft | Qt::TextWordWrap, frames.at(index).first);
        }
        painter.end();

        const QString directoryPath = snapshotController_->screenshotDirectory();
        QDir directory(directoryPath);
        directory.mkpath(QStringLiteral("."));
        const QString targetPath = buildScreenshotOutputPath(
            directory.absolutePath(),
            QStringLiteral("%1-contact-sheet").arg(effectiveCurrentMediaTitle()),
            formatCombo->currentData().toString(),
            templateEdit->text().trimmed() + QStringLiteral("-contact"),
            1);
        const QByteArray formatName = formatCombo->currentData().toString().toUpper().toUtf8();
        if (!sheet.save(targetPath, formatName.constData(), 92)) {
            statusBar()->showMessage(uiText("Could not save the contact sheet."), 4000);
            return;
        }

        showActionResult(
            uiText("Contact sheet saved: %1").arg(targetPath),
            uiText("Contact sheet saved: %1").arg(QFileInfo(targetPath).fileName()),
            5000);
        dialog.accept();
    });

    dialog.exec();
}

void MainWindow::performBurstScreenshotStep()
{
    if (pendingBurstScreenshots_ <= 0 || playbackController_ == nullptr || snapshotController_ == nullptr) {
        return;
    }

    QDir directory(snapshotController_->screenshotDirectory());
    if (!directory.exists()) {
        directory.mkpath(QStringLiteral("."));
    }

    const QString targetPath = buildScreenshotOutputPath(
        directory.absolutePath(),
        pendingBurstMediaLabel_,
        pendingBurstFormat_,
        screenshotTemplate(settingsController_),
        completedBurstScreenshots_ + 1);

    if (!playbackController_->captureScreenshot(targetPath)) {
        pendingBurstScreenshots_ = 0;
        statusBar()->showMessage(uiText("Burst capture failed."), 4000);
        return;
    }

    ++completedBurstScreenshots_;
    --pendingBurstScreenshots_;
    if (pendingBurstScreenshots_ > 0 && burstScreenshotTimer_ != nullptr) {
        burstScreenshotTimer_->start(std::max(40, pendingBurstIntervalMs_));
    } else {
        const QString message = uiText("Burst capture saved %1 frame%2")
            .arg(completedBurstScreenshots_)
            .arg(completedBurstScreenshots_ == 1 ? QString {} : QStringLiteral("s"));
        showActionResult(message, message, 5000);
    }
}

QStringList MainWindow::preferredSubtitleLanguages() const
{
    QString raw = subtitleLanguageHintEdit_ != nullptr ? subtitleLanguageHintEdit_->text().trimmed() : QString {};
    if (raw.isEmpty() && settingsController_ != nullptr) {
        raw = settingsController_->subtitlePreferredLanguages();
    }

    raw = raw.toLower();
    raw.replace(QChar(';'), QChar(','));
    raw.replace(QChar('|'), QChar(','));
    raw.replace(QChar(' '), QChar(','));
    QStringList languages;
    for (QString part : raw.split(QChar(','), Qt::SkipEmptyParts)) {
        part = part.trimmed();
        if (!part.isEmpty() && !languages.contains(part)) {
            languages.push_back(part);
        }
    }
    return languages;
}

QString MainWindow::currentContentTypeId() const
{
    const QUrl sourceUrl(currentMediaSource_);
    if (sourceUrl.isValid() && !sourceUrl.scheme().isEmpty() && !sourceUrl.isLocalFile()) {
        return QStringLiteral("stream");
    }
    if (currentMediaSource_.startsWith(QStringLiteral("dvd://"), Qt::CaseInsensitive)
        || currentMediaSource_.startsWith(QStringLiteral("bd://"), Qt::CaseInsensitive)) {
        return QStringLiteral("disc");
    }
    return hasVideoTrack_ ? QStringLiteral("video") : QStringLiteral("audio");
}

QJsonObject MainWindow::captureMediaProfileState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("playback_profile"),
                 settingsController_ != nullptr
                     ? revaplayer::domain::playerProfileId(settingsController_->playbackProfile())
                     : QStringLiteral("balanced"));
    state.insert(QStringLiteral("speed"), currentSpeed_);
    state.insert(QStringLiteral("repeat_mode"), currentRepeatMode_);
    state.insert(QStringLiteral("subtitle_visible"), subtitleVisible_);
    state.insert(QStringLiteral("subtitle_delay"), currentSubtitleDelaySeconds_);
    state.insert(QStringLiteral("subtitle_scale"), currentSubtitleScale_);
    state.insert(QStringLiteral("subtitle_position"), currentSubtitlePosition_);
    state.insert(QStringLiteral("subtitle_font"), currentSubtitleFontFamily_);
    state.insert(QStringLiteral("subtitle_font_size"), currentSubtitleFontSize_);
    state.insert(QStringLiteral("subtitle_ass_override"), currentSubtitleAssOverride_);
    state.insert(QStringLiteral("audio_delay"), currentAudioDelaySeconds_);
    state.insert(QStringLiteral("audio_normalize"), audioNormalizeCheckBox_ != nullptr && audioNormalizeCheckBox_->isChecked());
    state.insert(QStringLiteral("custom_audio_filter"), customAudioFilterEdit_ != nullptr ? customAudioFilterEdit_->text().trimmed() : QString {});
    state.insert(QStringLiteral("video_brightness"), videoBrightnessSlider_ != nullptr ? videoBrightnessSlider_->value() : 0);
    state.insert(QStringLiteral("video_contrast"), videoContrastSlider_ != nullptr ? videoContrastSlider_->value() : 0);
    state.insert(QStringLiteral("video_saturation"), videoSaturationSlider_ != nullptr ? videoSaturationSlider_->value() : 0);
    state.insert(QStringLiteral("video_gamma"), videoGammaSlider_ != nullptr ? videoGammaSlider_->value() : 0);
    state.insert(QStringLiteral("video_hue"), videoHueSlider_ != nullptr ? videoHueSlider_->value() : 0);
    state.insert(QStringLiteral("video_sharpen"), videoSharpenSlider_ != nullptr ? videoSharpenSlider_->value() : 0);
    state.insert(QStringLiteral("video_denoise"), videoDenoiseSlider_ != nullptr ? videoDenoiseSlider_->value() : 0);
    state.insert(QStringLiteral("video_deband"), videoDebandCheckBox_ != nullptr && videoDebandCheckBox_->isChecked());
    state.insert(QStringLiteral("video_deinterlace"), videoDeinterlaceCheckBox_ != nullptr && videoDeinterlaceCheckBox_->isChecked());
    state.insert(QStringLiteral("video_zoom_factor"), currentVideoZoomFactor_);
    state.insert(QStringLiteral("video_align_x"), currentVideoAlignX_);
    state.insert(QStringLiteral("video_align_y"), currentVideoAlignY_);
    state.insert(QStringLiteral("stereo3d_filter"), stereo3dFilterEdit_ != nullptr ? stereo3dFilterEdit_->text().trimmed() : QString {});
    state.insert(QStringLiteral("custom_video_filter"), customVideoFilterEdit_ != nullptr ? customVideoFilterEdit_->text().trimmed() : QString {});
    state.insert(QStringLiteral("shader_path"), shaderPathEdit_ != nullptr ? shaderPathEdit_->text().trimmed() : QString {});

    QJsonArray equalizerArray;
    for (QSlider *slider : equalizerSliders_) {
        equalizerArray.push_back(slider != nullptr ? slider->value() : 0);
    }
    state.insert(QStringLiteral("equalizer"), equalizerArray);
    return state;
}

void MainWindow::applyMediaProfileState(const QJsonObject &state, const bool announce)
{
    const QString playbackProfileId = jsonString(state, QStringLiteral("playback_profile"), QStringLiteral("balanced"));
    const auto playbackProfile = revaplayer::domain::playerProfileFromId(playbackProfileId);
    if (thumbnailService_ != nullptr) {
        thumbnailService_->setProfile(playbackProfile);
    }
    if (previewRequestTimer_ != nullptr && thumbnailService_ != nullptr) {
        previewRequestTimer_->setInterval(thumbnailService_->recommendedDebounceIntervalMs());
    }
    if (profileActionGroup_ != nullptr) {
        for (QAction *action : profileActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == playbackProfileId);
        }
    }

    const auto setSliderValue = [](QSlider *slider, const int value) {
        if (slider == nullptr) {
            return;
        }
        const QSignalBlocker blocker(slider);
        slider->setValue(value);
    };

    if (audioNormalizeCheckBox_ != nullptr) {
        const QSignalBlocker blocker(audioNormalizeCheckBox_);
        audioNormalizeCheckBox_->setChecked(state.value(QStringLiteral("audio_normalize")).toBool(false));
    }
    if (customAudioFilterEdit_ != nullptr) {
        customAudioFilterEdit_->setText(jsonString(state, QStringLiteral("custom_audio_filter")));
    }
    const QJsonArray equalizerArray = state.value(QStringLiteral("equalizer")).toArray();
    for (qsizetype index = 0; index < equalizerSliders_.size() && index < equalizerArray.size(); ++index) {
        setSliderValue(equalizerSliders_[index], equalizerArray.at(index).toInt());
    }

    setSliderValue(videoBrightnessSlider_, state.value(QStringLiteral("video_brightness")).toInt(0));
    setSliderValue(videoContrastSlider_, state.value(QStringLiteral("video_contrast")).toInt(0));
    setSliderValue(videoSaturationSlider_, state.value(QStringLiteral("video_saturation")).toInt(0));
    setSliderValue(videoGammaSlider_, state.value(QStringLiteral("video_gamma")).toInt(0));
    setSliderValue(videoHueSlider_, state.value(QStringLiteral("video_hue")).toInt(0));
    setSliderValue(videoSharpenSlider_, state.value(QStringLiteral("video_sharpen")).toInt(0));
    setSliderValue(videoDenoiseSlider_, state.value(QStringLiteral("video_denoise")).toInt(0));
    if (videoDebandCheckBox_ != nullptr) {
        const QSignalBlocker blocker(videoDebandCheckBox_);
        videoDebandCheckBox_->setChecked(state.value(QStringLiteral("video_deband")).toBool(false));
    }
    if (videoDeinterlaceCheckBox_ != nullptr) {
        const QSignalBlocker blocker(videoDeinterlaceCheckBox_);
        videoDeinterlaceCheckBox_->setChecked(state.value(QStringLiteral("video_deinterlace")).toBool(false));
    }
    if (stereo3dFilterEdit_ != nullptr) {
        stereo3dFilterEdit_->setText(jsonString(state, QStringLiteral("stereo3d_filter")));
    }
    if (customVideoFilterEdit_ != nullptr) {
        customVideoFilterEdit_->setText(jsonString(state, QStringLiteral("custom_video_filter")));
    }
    if (shaderPathEdit_ != nullptr) {
        shaderPathEdit_->setText(jsonString(state, QStringLiteral("shader_path")));
    }

    setRepeatMode(jsonString(state, QStringLiteral("repeat_mode"), currentRepeatMode_), false);
    applyAudioFilterState();
    applyVideoFilterState();

    currentSpeed_ = state.value(QStringLiteral("speed")).toDouble(currentSpeed_);
    subtitleVisible_ = state.value(QStringLiteral("subtitle_visible")).toBool(subtitleVisible_);
    currentSubtitleDelaySeconds_ = state.value(QStringLiteral("subtitle_delay")).toDouble(currentSubtitleDelaySeconds_);
    currentSubtitleScale_ = state.value(QStringLiteral("subtitle_scale")).toDouble(currentSubtitleScale_);
    currentSubtitlePosition_ = state.value(QStringLiteral("subtitle_position")).toInt(currentSubtitlePosition_);
    currentSubtitleFontFamily_ = jsonString(state, QStringLiteral("subtitle_font"), currentSubtitleFontFamily_);
    currentSubtitleFontSize_ = state.value(QStringLiteral("subtitle_font_size")).toInt(currentSubtitleFontSize_);
    currentSubtitleAssOverride_ = jsonString(state, QStringLiteral("subtitle_ass_override"), currentSubtitleAssOverride_);
    currentAudioDelaySeconds_ = state.value(QStringLiteral("audio_delay")).toDouble(currentAudioDelaySeconds_);
    currentVideoZoomFactor_ = state.value(QStringLiteral("video_zoom_factor")).toDouble(currentVideoZoomFactor_);
    currentVideoAlignX_ = state.value(QStringLiteral("video_align_x")).toDouble(currentVideoAlignX_);
    currentVideoAlignY_ = state.value(QStringLiteral("video_align_y")).toDouble(currentVideoAlignY_);
    syncSubtitleActionStates();
    updateMediaInformationOverlay();
    applyVideoViewportTransform();

    if (playbackController_ != nullptr && mediaLoaded_) {
        playbackController_->setSpeed(state.value(QStringLiteral("speed")).toDouble(currentSpeed_));
        playbackController_->setSubtitleVisible(state.value(QStringLiteral("subtitle_visible")).toBool(subtitleVisible_));
        playbackController_->setSubtitleDelay(state.value(QStringLiteral("subtitle_delay")).toDouble(currentSubtitleDelaySeconds_));
        playbackController_->setSubtitleScale(state.value(QStringLiteral("subtitle_scale")).toDouble(currentSubtitleScale_));
        playbackController_->setSubtitlePosition(state.value(QStringLiteral("subtitle_position")).toInt(currentSubtitlePosition_));
        playbackController_->setSubtitleFontFamily(jsonString(state, QStringLiteral("subtitle_font"), currentSubtitleFontFamily_));
        playbackController_->setSubtitleFontSize(state.value(QStringLiteral("subtitle_font_size")).toInt(currentSubtitleFontSize_));
        playbackController_->setSubtitleAssOverride(jsonString(state, QStringLiteral("subtitle_ass_override"), currentSubtitleAssOverride_));
        playbackController_->setAudioDelay(state.value(QStringLiteral("audio_delay")).toDouble(currentAudioDelaySeconds_));
    }

    updateActionStates();

    if (announce) {
        statusBar()->showMessage(uiText("Media profile applied"), 3000);
    }
}

void MainWindow::saveMediaProfileForFile()
{
    if (settingsController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    settingsController_->setCustomValue(
        QStringLiteral("%1%2").arg(QString::fromLatin1(kFileProfilePrefix), encodeSettingKeySegment(currentMediaSource_)),
        QString::fromUtf8(QJsonDocument(captureMediaProfileState()).toJson(QJsonDocument::Compact)));
    statusBar()->showMessage(uiText("Saved profile for this file"), 3000);
}

void MainWindow::saveMediaProfileForType()
{
    if (settingsController_ == nullptr) {
        return;
    }

    settingsController_->setCustomValue(
        QStringLiteral("%1%2").arg(QString::fromLatin1(kTypeProfilePrefix), encodeSettingKeySegment(currentContentTypeId())),
        QString::fromUtf8(QJsonDocument(captureMediaProfileState()).toJson(QJsonDocument::Compact)));
    statusBar()->showMessage(uiText("Saved profile for %1").arg(currentContentTypeId()), 3000);
}

void MainWindow::clearMediaProfileForFile()
{
    if (settingsController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    settingsController_->removeCustomValue(
        QStringLiteral("%1%2").arg(QString::fromLatin1(kFileProfilePrefix), encodeSettingKeySegment(currentMediaSource_)));
    statusBar()->showMessage(uiText("Cleared the file-specific profile"), 3000);
}

void MainWindow::clearMediaProfileForType()
{
    if (settingsController_ == nullptr) {
        return;
    }

    settingsController_->removeCustomValue(
        QStringLiteral("%1%2").arg(QString::fromLatin1(kTypeProfilePrefix), encodeSettingKeySegment(currentContentTypeId())));
    statusBar()->showMessage(uiText("Cleared the content-type profile"), 3000);
}

void MainWindow::applyStoredMediaProfiles()
{
    if (settingsController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString typeKey = QStringLiteral("%1%2").arg(QString::fromLatin1(kTypeProfilePrefix), encodeSettingKeySegment(currentContentTypeId()));
    const QString fileKey = QStringLiteral("%1%2").arg(QString::fromLatin1(kFileProfilePrefix), encodeSettingKeySegment(currentMediaSource_));

    const QString typePayload = settingsController_->customValue(typeKey).trimmed();
    if (!typePayload.isEmpty()) {
        applyMediaProfileState(QJsonDocument::fromJson(typePayload.toUtf8()).object(), false);
    }

    const QString filePayload = settingsController_->customValue(fileKey).trimmed();
    if (!filePayload.isEmpty()) {
        applyMediaProfileState(QJsonDocument::fromJson(filePayload.toUtf8()).object(), false);
    }
}

void MainWindow::syncPanelToggleActions()
{
    const bool playlistVisible = isSidePanelVisible(SidePanel::Playlist);
    const bool detailsVisible = isSidePanelVisible(SidePanel::Details);

    if (togglePlaylistAction_ != nullptr) {
        const QSignalBlocker blocker(togglePlaylistAction_);
        togglePlaylistAction_->setChecked(playlistVisible);
    }
    if (fullscreenPlaylistButton_ != nullptr) {
        const QSignalBlocker blocker(fullscreenPlaylistButton_);
        fullscreenPlaylistButton_->setChecked(playlistVisible);
    }
    if (controlBar_ != nullptr) {
        controlBar_->setPanelButtonsChecked(playlistVisible, detailsVisible);
        controlBar_->setPanelButtonsVisible(true);
    }

    if (toggleDetailsAction_ != nullptr) {
        const QSignalBlocker blocker(toggleDetailsAction_);
        toggleDetailsAction_->setChecked(detailsVisible);
    }
    if (fullscreenDetailsButton_ != nullptr) {
        const QSignalBlocker blocker(fullscreenDetailsButton_);
        fullscreenDetailsButton_->setChecked(detailsVisible);
    }

    updateSidePanelSelectorState();
}

void MainWindow::updateSidePanelSelectorState()
{
    if (sidePanelSelector_ == nullptr) {
        return;
    }

    const bool selectorEnabled = isFullScreen() ? fullscreenSideSelectorEnabled() : true;
    const bool showSelector = panelOverlayModeActive_
        && (isSidePanelVisible(SidePanel::Playlist) || isSidePanelVisible(SidePanel::Details))
        && selectorEnabled;
    sidePanelSelector_->setVisible(showSelector);
    if (!showSelector) {
        return;
    }

    if (sidePanelPlaylistButton_ != nullptr) {
        const QSignalBlocker blocker(sidePanelPlaylistButton_);
        sidePanelPlaylistButton_->setChecked(activeSidePanel_ == SidePanel::Playlist);
    }
    if (sidePanelDetailsButton_ != nullptr) {
        const QSignalBlocker blocker(sidePanelDetailsButton_);
        sidePanelDetailsButton_->setChecked(activeSidePanel_ == SidePanel::Details);
    }
    sidePanelSelector_->raise();
}

void MainWindow::toggleFullscreen()
{
    pointerPanelSuppressionElapsed_.restart();
    pointerPanelSuppressionDurationMs_ = kPointerPanelSuppressionDelayMs;
    activeMouseZoneId_.clear();
    pointerNearRightEdge_ = false;

    const bool enteringFullscreen = !isFullScreen();
    enteringFullscreen ? showFullScreen() : showNormal();

    if (mediaLoaded_ && hasVideoTrack_ && videoZoomFullscreenBehaviorSetting() == QStringLiteral("reset_on_toggle")) {
        resetVideoZoomAndPan(false);
    }
    showPlaybackFeedback(enteringFullscreen ? uiText("Fullscreen") : uiText("Windowed"));
}

void MainWindow::updateFullscreenChromeMode()
{
    if (fullscreenChromeTimer_ != nullptr) {
        fullscreenChromeTimer_->stop();
    }
    if (pointerLeaveTimer_ != nullptr) {
        pointerLeaveTimer_->stop();
    }
    pointerNearRightEdge_ = false;

    if (!isFullScreen()) {
        sidePanelEdgeRevealActive_ = false;
        updateControlBarPresentationMode();
        suppressPanelPreferencePersistence_ = true;
        if (menuBar() != nullptr && menuBar()->isVisible() != preFullscreenMenuBarVisible_) {
            menuBar()->setVisible(preFullscreenMenuBarVisible_);
        }
        updateStatusBarVisibility();
        if (controlBar_ != nullptr && controlBar_->isVisible() != preFullscreenControlBarVisible_) {
            controlBar_->setVisible(preFullscreenControlBarVisible_);
        }
        if (fullscreenTopBar_ != nullptr && fullscreenTopBar_->isVisible()) {
            fullscreenTopBar_->hide();
        }
        if (isSidePanelVisible(SidePanel::Playlist) != preFullscreenPlaylistVisible_) {
            setSidePanelVisible(SidePanel::Playlist, preFullscreenPlaylistVisible_, preFullscreenPlaylistVisible_);
        }
        if (isSidePanelVisible(SidePanel::Details) != preFullscreenDetailsVisible_) {
            setSidePanelVisible(SidePanel::Details, preFullscreenDetailsVisible_, preFullscreenDetailsVisible_ && !preFullscreenPlaylistVisible_);
        }
        suppressPanelPreferencePersistence_ = false;
        fullscreenChromeStateCaptured_ = false;
        syncPanelToggleActions();
        updateVideoOverlayGeometry();
        return;
    }

    if (!fullscreenChromeStateCaptured_) {
        preFullscreenMenuBarVisible_ = menuBar() != nullptr ? menuBar()->isVisible() : true;
        preFullscreenStatusBarVisible_ = settingsController_ == nullptr
            ? (statusBar() != nullptr && statusBar()->isVisible())
            : settingsController_->showStatusBarInWindowedMode();
        preFullscreenControlBarVisible_ = controlBar_ != nullptr && controlBar_->isVisible();
        preFullscreenPlaylistVisible_ = isSidePanelVisible(SidePanel::Playlist);
        preFullscreenDetailsVisible_ = isSidePanelVisible(SidePanel::Details);
        fullscreenChromeStateCaptured_ = true;
    }

    suppressPanelPreferencePersistence_ = true;
    if (playlistDock_ != nullptr && playlistDock_->isVisible()) {
        playlistDock_->hide();
    }
    if (detailsDock_ != nullptr && detailsDock_->isVisible()) {
        detailsDock_->hide();
    }
    suppressPanelPreferencePersistence_ = false;
    syncPanelToggleActions();

    if (statusBar() != nullptr && statusBar()->isVisible()) {
        statusBar()->hide();
    }
    if (menuBar() != nullptr && menuBar()->isVisible()) {
        menuBar()->hide();
    }

    updateControlBarPresentationMode();
    if (fullscreenAutoHideEnabled()) {
        setFullscreenChromeVisible(false, false);
    } else {
        setFullscreenChromeVisible(true, true);
    }
    updateVideoOverlayGeometry();
}

void MainWindow::setFullscreenChromeVisible(const bool topVisible, const bool bottomVisible)
{
    if (!isFullScreen()) {
        return;
    }

    if (fullscreenTopBar_ != nullptr) {
        fullscreenTopBar_->setVisible(topVisible);
    }
    if (fullscreenExitFullscreenButton_ != nullptr) {
        fullscreenExitFullscreenButton_->setEnabled(true);
    }
    if (statusBar() != nullptr) {
        statusBar()->setVisible(false);
    }
    if (controlBar_ != nullptr) {
        controlBar_->setVisible(bottomVisible);
    }
    updateVideoOverlayGeometry();
}

void MainWindow::handleFullscreenPointerActivity(const QPoint &localPosition)
{
    if (videoViewport_ == nullptr) {
        return;
    }

    if (pointerPanelSuppressionDurationMs_ > 0
        && pointerPanelSuppressionElapsed_.isValid()
        && pointerPanelSuppressionElapsed_.elapsed() < pointerPanelSuppressionDurationMs_) {
        activeMouseZoneId_.clear();
        return;
    }
    pointerPanelSuppressionDurationMs_ = 0;

    if (pointerLeaveTimer_ != nullptr) {
        pointerLeaveTimer_->stop();
    }

    if (QApplication::mouseButtons() != Qt::NoButton) {
        activeMouseZoneId_.clear();
        if (isFullScreen() && fullscreenAutoHideEnabled() && fullscreenChromeTimer_ != nullptr) {
            fullscreenChromeTimer_->stop();
        }
        return;
    }

    const bool fullscreenContext = isFullScreen();
    const int fullscreenMargin = fullscreenRevealMargin();
    const int rightEdgeMargin = pointerRightEdgeMargin();
    const int rightEdgeReleaseMargin = std::clamp(rightEdgeMargin + std::max(18, rightEdgeMargin / 3), 18, 260);
    const bool nearTop = fullscreenContext && localPosition.y() <= fullscreenMargin;
    const bool nearBottom = fullscreenContext && localPosition.y() >= videoViewport_->height() - fullscreenMargin;
    const bool nearRight = pointerNearRightEdge_
        ? localPosition.x() >= videoViewport_->width() - rightEdgeReleaseMargin
        : localPosition.x() >= videoViewport_->width() - rightEdgeMargin;

    if (nearRight && !pointerNearRightEdge_) {
        const bool edgeRevealEnabled = fullscreenContext
            ? fullscreenEdgePanelRevealEnabled()
            : (panelOverlayModeActive_ && windowedEdgePanelRevealEnabled());
        if (edgeRevealEnabled) {
            applyConfiguredRightEdgeAction(pointerRightEdgeActionId(fullscreenContext));
            if (fullscreenChromeTimer_ != nullptr) {
                fullscreenChromeTimer_->stop();
            }
        }
    } else if (!nearRight && pointerNearRightEdge_) {
        if (fullscreenContext && sidePanelEdgeRevealActive_) {
            applyConfiguredRightEdgeLeaveAction(pointerRightEdgeLeaveActionId());
        }
    }
    pointerNearRightEdge_ = nearRight;

    const int zoneMargin = std::clamp(std::max(fullscreenRevealMargin(), 56), 40, 160);
    const QRect centerRect(
        videoViewport_->width() / 4,
        videoViewport_->height() / 4,
        videoViewport_->width() / 2,
        videoViewport_->height() / 2);
    QString zoneId;
    if (centerRect.contains(localPosition)) {
        zoneId = QStringLiteral("center");
    } else if (localPosition.y() <= zoneMargin) {
        zoneId = QStringLiteral("top");
    } else if (localPosition.y() >= videoViewport_->height() - zoneMargin) {
        zoneId = QStringLiteral("bottom");
    } else if (localPosition.x() <= zoneMargin) {
        zoneId = QStringLiteral("left");
    } else if (localPosition.x() >= videoViewport_->width() - zoneMargin) {
        zoneId = QStringLiteral("right");
    }
    if (zoneId != activeMouseZoneId_) {
        activeMouseZoneId_ = zoneId;
        applyMouseZoneAction(zoneId);
    }

    if (!fullscreenContext || !fullscreenAutoHideEnabled()) {
        return;
    }

    const bool sidePanelVisible = panelOverlayModeActive_
        && (isSidePanelVisible(SidePanel::Playlist) || isSidePanelVisible(SidePanel::Details));
    const bool keepBottomVisible = nearBottom || (sidePanelVisible && pointerKeepControlsVisibleWhilePanelOpen());

    if (nearTop || keepBottomVisible || nearRight) {
        setFullscreenChromeVisible(nearTop, keepBottomVisible);
        if (fullscreenChromeTimer_ != nullptr) {
            fullscreenChromeTimer_->stop();
        }
        return;
    }

    scheduleFullscreenChromeHide();
}

void MainWindow::handleViewportPointerLeave()
{
    activeMouseZoneId_.clear();
    scheduleManagedPointerLeaveCheck();
}

void MainWindow::scheduleManagedPointerLeaveCheck(const int delayMs)
{
    if (pointerLeaveTimer_ == nullptr) {
        evaluateManagedPointerLeave();
        return;
    }

    const int effectiveDelay = delayMs >= 0
        ? delayMs
        : std::clamp(pointerLeaveDelayMs() / 6, 90, 220);
    pointerLeaveTimer_->start(std::max(40, effectiveDelay));
}

bool MainWindow::pointerInsideManagedChrome(const QPoint &globalPosition) const
{
    const auto widgetContainsGlobalPoint = [&globalPosition](const QWidget *widget) {
        if (widget == nullptr || !widget->isVisible()) {
            return false;
        }
        return QRect(widget->mapToGlobal(QPoint(0, 0)), widget->size()).contains(globalPosition);
    };

    return widgetContainsGlobalPoint(videoViewport_)
        || widgetContainsGlobalPoint(homeDashboard_)
        || widgetContainsGlobalPoint(playlistDock_)
        || widgetContainsGlobalPoint(detailsDock_)
        || widgetContainsGlobalPoint(sidePanelSelector_)
        || widgetContainsGlobalPoint(playlistResizeHandle_)
        || widgetContainsGlobalPoint(detailsResizeHandle_)
        || widgetContainsGlobalPoint(controlBar_)
        || widgetContainsGlobalPoint(fullscreenTopBar_);
}

void MainWindow::evaluateManagedPointerLeave()
{
    if (videoViewport_ == nullptr) {
        return;
    }

    const QPoint globalPosition = QCursor::pos();
    if (pointerInsideManagedChrome(globalPosition)) {
        if (fullscreenChromeTimer_ != nullptr) {
            fullscreenChromeTimer_->stop();
        }
        if (isFullScreen() && sidePanelEdgeRevealActive_ && pointerLeaveTimer_ != nullptr) {
            const int pollDelay = std::clamp(pointerLeaveDelayMs() / 6, 90, 220);
            pointerLeaveTimer_->start(std::max(40, pollDelay));
        }
        return;
    }

    pointerNearRightEdge_ = false;
    activeMouseZoneId_.clear();
    if (isFullScreen() && sidePanelEdgeRevealActive_) {
        applyConfiguredRightEdgeLeaveAction(pointerRightEdgeLeaveActionId());
    }
    scheduleFullscreenChromeHide(pointerLeaveDelayMs());
}

void MainWindow::scheduleFullscreenChromeHide(const int delayMs)
{
    if (!isFullScreen() || !fullscreenAutoHideEnabled() || fullscreenChromeTimer_ == nullptr) {
        return;
    }

    fullscreenChromeTimer_->start(std::max(250, delayMs));
}

void MainWindow::updateControlBarPresentationMode()
{
    if (controlBar_ == nullptr || videoViewport_ == nullptr || centralWidget() == nullptr) {
        return;
    }

    QWidget *targetParent = isFullScreen() ? static_cast<QWidget *>(videoViewport_) : centralWidget();
    const bool shouldOverlay = targetParent == videoViewport_;
    const bool wasVisible = controlBar_->isVisible();

    if (controlBar_->parentWidget() != targetParent) {
        if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout()); layout != nullptr) {
            layout->removeWidget(controlBar_);
        }

        controlBar_->hide();
        controlBar_->setParent(targetParent);

        if (targetParent == centralWidget()) {
            if (auto *layout = qobject_cast<QVBoxLayout *>(centralWidget()->layout()); layout != nullptr) {
                layout->addWidget(controlBar_, 0);
            }
        }

        controlBar_->setVisible(wasVisible);
    }

    if (controlBarOverlayModeActive_ != shouldOverlay) {
        controlBarOverlayModeActive_ = shouldOverlay;
        controlBar_->setProperty("overlayMode", controlBarOverlayModeActive_);
        if (QStyle *styleEngine = controlBar_->style(); styleEngine != nullptr) {
            styleEngine->unpolish(controlBar_);
            styleEngine->polish(controlBar_);
        }
        controlBar_->refreshPresentation();
        controlBar_->update();
    }

    controlBar_->ensurePolished();
    if (QLayout *barLayout = controlBar_->layout(); barLayout != nullptr) {
        barLayout->invalidate();
        barLayout->activate();
    }
    controlBar_->updateGeometry();
}

QRect MainWindow::overlaySidePanelGeometry() const
{
    if (videoViewport_ == nullptr) {
        return {};
    }

    const int panelMargin = std::max(12, videoViewport_->width() / 90);
    const int desiredPanelWidth = overlayPanelWidth_ > 0
        ? overlayPanelWidth_
        : std::clamp((videoViewport_->width() * 36) / 100, 400, 560);
    const int panelWidth = std::clamp(
        desiredPanelWidth,
        360,
        std::max(360, videoViewport_->width() - (panelMargin * 2)));
    const int topInset = (fullscreenTopBar_ != nullptr && fullscreenTopBar_->isVisible())
        ? fullscreenTopBar_->geometry().bottom() + panelMargin
        : panelMargin;
    const int bottomInset = (controlBar_ != nullptr && controlBar_->parentWidget() == videoViewport_ && controlBar_->isVisible())
        ? (videoViewport_->height() - controlBar_->geometry().top()) + panelMargin
        : panelMargin;
    const int panelHeight = std::max(260, videoViewport_->height() - topInset - bottomInset);
    return QRect(
        std::max(panelMargin, videoViewport_->width() - panelWidth - panelMargin),
        topInset,
        panelWidth,
        panelHeight);
}

QRect MainWindow::hiddenOverlaySidePanelGeometry(const QRect &targetGeometry) const
{
    if (!targetGeometry.isValid() || videoViewport_ == nullptr) {
        return targetGeometry;
    }

    return QRect(
        videoViewport_->width() + std::max(12, targetGeometry.width() / 12),
        targetGeometry.y(),
        targetGeometry.width(),
        targetGeometry.height());
}

void MainWindow::animateOverlayDock(QDockWidget *dock, const bool visible, const bool raisePanel)
{
    if (dock == nullptr) {
        return;
    }

    if (!panelOverlayModeActive_ || dock->parentWidget() != videoViewport_ || videoViewport_ == nullptr) {
        dock->setProperty("overlayHidePending", false);
        dock->setVisible(visible);
        if (visible && raisePanel) {
            dock->raise();
        }
        return;
    }

    QPropertyAnimation *animation = dock == playlistDock_ ? playlistDockAnimation_ : detailsDockAnimation_;
    if (animation == nullptr) {
        dock->setProperty("overlayHidePending", false);
        dock->setVisible(visible);
        if (visible && raisePanel) {
            dock->raise();
        }
        return;
    }

    const QRect targetGeometry = overlaySidePanelGeometry();
    const QRect hiddenGeometry = hiddenOverlaySidePanelGeometry(targetGeometry);
    if (!targetGeometry.isValid()) {
        dock->setVisible(visible);
        return;
    }

    if (animation->state() != QAbstractAnimation::Stopped) {
        animation->stop();
    }

    const bool wasVisible = dock->isVisible();
    const QRect startGeometry = (wasVisible && dock->geometry().isValid()) ? dock->geometry() : hiddenGeometry;
    dock->setProperty("overlayHidePending", !visible);
    const QString easingId = uiAnimationEasingId();
    const auto easingCurveFor = [&easingId](const bool showing) {
        if (easingId == QStringLiteral("quart")) {
            return showing ? QEasingCurve::OutQuart : QEasingCurve::InQuart;
        }
        if (easingId == QStringLiteral("expo")) {
            return showing ? QEasingCurve::OutExpo : QEasingCurve::InExpo;
        }
        if (easingId == QStringLiteral("sine")) {
            return showing ? QEasingCurve::OutSine : QEasingCurve::InSine;
        }
        return showing ? QEasingCurve::OutCubic : QEasingCurve::InCubic;
    };
    const int speedPercent = uiAnimationSpeedPercent();
    animation->setEasingCurve(easingCurveFor(visible));
    animation->setDuration(std::clamp(((visible ? 190 : 150) * speedPercent) / 100, 70, 520));
    animation->setStartValue(startGeometry);
    animation->setEndValue(visible ? targetGeometry : hiddenGeometry);

    if (visible) {
        dock->show();
        if (!wasVisible || !dock->geometry().isValid()) {
            dock->setGeometry(hiddenGeometry);
        }
        if (raisePanel) {
            dock->raise();
        }
    }

    animation->start();
}

void MainWindow::updateVideoOverlayGeometry()
{
    if (videoViewport_ == nullptr || suppressVideoOverlayUpdates_) {
        return;
    }

    const int horizontalMargin = std::max(16, videoViewport_->width() / 42);
    const int bottomMargin = std::max(14, videoViewport_->height() / 36);

    if (fullscreenTopBar_ != nullptr) {
        fullscreenTopBar_->adjustSize();
        const int maxTopBarWidth = std::max(280, videoViewport_->width() - (horizontalMargin * 2));
        const int topBarWidth = std::clamp(maxTopBarWidth, 280, 980);
        const int topBarHeight = fullscreenTopBar_->sizeHint().height();
        fullscreenTopBar_->setGeometry(
            (videoViewport_->width() - topBarWidth) / 2,
            std::max(10, bottomMargin - 2),
            topBarWidth,
            topBarHeight);
        fullscreenTopBar_->raise();
    }

    if (mediaInformationOverlay_ != nullptr) {
        mediaInformationOverlay_->raise();
    }

    if (controlBar_ != nullptr && controlBar_->parentWidget() == videoViewport_) {
        controlBar_->ensurePolished();
        controlBar_->refreshPresentation();
        if (QLayout *barLayout = controlBar_->layout(); barLayout != nullptr) {
            barLayout->activate();
        }
        const int controlWidth = std::clamp(
            videoViewport_->width() - (horizontalMargin * 2),
            440,
            1360);
        const int controlHeight = controlBar_->sizeHint().height();
        controlBar_->setGeometry(
            (videoViewport_->width() - controlWidth) / 2,
            std::max(12, videoViewport_->height() - controlHeight - bottomMargin),
            controlWidth,
            controlHeight);
        controlBar_->raise();
    }

    if (homeDashboard_ != nullptr) {
        const int dashboardWidth = std::clamp(videoViewport_->width() - (horizontalMargin * 2), 520, 1180);
        const int dashboardHeight = std::clamp(videoViewport_->height() - (bottomMargin * 2), 320, 760);
        homeDashboard_->setGeometry(
            (videoViewport_->width() - dashboardWidth) / 2,
            std::max(18, (videoViewport_->height() - dashboardHeight) / 2),
            dashboardWidth,
            dashboardHeight);
        if (homeDashboard_->isVisible()) {
            homeDashboard_->raise();
        }
    }

    if (sidePanelSelector_ != nullptr && sidePanelSelector_->isVisible()) {
        sidePanelSelector_->adjustSize();
        const int selectorWidth = std::clamp(sidePanelSelector_->sizeHint().width(), 40, 56);
        const int selectorHeight = sidePanelSelector_->sizeHint().height();
        const int selectorX = std::max(8, videoViewport_->width() - selectorWidth - std::max(10, horizontalMargin / 2));
        const int selectorY = std::clamp(
            (videoViewport_->height() - selectorHeight) / 2,
            std::max(20, horizontalMargin),
            std::max(20, videoViewport_->height() - selectorHeight - horizontalMargin));
        sidePanelSelector_->setGeometry(selectorX, selectorY, selectorWidth, selectorHeight);
        sidePanelSelector_->raise();
    }

    if (detailsDock_ != nullptr && detailsDock_->widget() != nullptr) {
        if (auto *detailsLayout = qobject_cast<QVBoxLayout *>(detailsDock_->widget()->layout()); detailsLayout != nullptr) {
            constexpr int kBaseDetailsMargin = 6;
            const int rightMargin = panelOverlayModeActive_ && detailsDock_->parentWidget() == videoViewport_
                ? kBaseDetailsMargin + 2
                : kBaseDetailsMargin;
            if (detailsLayout->contentsMargins().right() != rightMargin) {
                detailsLayout->setContentsMargins(
                    kBaseDetailsMargin,
                    kBaseDetailsMargin,
                    rightMargin,
                    kBaseDetailsMargin);
            }
        }
    }

    if (panelOverlayModeActive_) {
        const QRect panelGeometry = overlaySidePanelGeometry();
        const QRect hiddenGeometry = hiddenOverlaySidePanelGeometry(panelGeometry);

        for (QDockWidget *dock : {playlistDock_, detailsDock_}) {
            if (dock == nullptr || dock->parentWidget() != videoViewport_ || !dock->isVisible()) {
                continue;
            }
            QPropertyAnimation *animation = dock == playlistDock_ ? playlistDockAnimation_ : detailsDockAnimation_;
            if (animation != nullptr && animation->state() != QAbstractAnimation::Stopped) {
                animation->setStartValue(dock->geometry());
                animation->setEndValue(dock->property("overlayHidePending").toBool() ? hiddenGeometry : panelGeometry);
            } else {
                dock->setGeometry(panelGeometry);
            }
        }

        if (QDockWidget *activeDock = sidePanelDock(activeSidePanel_); activeDock != nullptr && activeDock->isVisible()) {
            activeDock->raise();
        }
    }

    const auto updateResizeHandle = [this](QDockWidget *dock, QWidget *handle) {
        if (dock == nullptr || handle == nullptr) {
            return;
        }

        const bool showHandle = panelOverlayModeActive_
            && dock->parentWidget() == videoViewport_
            && dock->isVisible()
            && dock == sidePanelDock(activeSidePanel_);
        handle->setVisible(showHandle);
        if (!showHandle) {
            return;
        }

        handle->setGeometry(0, 24, 12, std::max(80, dock->height() - 48));
        handle->raise();
    };

    updateResizeHandle(playlistDock_, playlistResizeHandle_);
    updateResizeHandle(detailsDock_, detailsResizeHandle_);

    if (sidePanelSelector_ != nullptr && sidePanelSelector_->isVisible()) {
        sidePanelSelector_->raise();
    }
}

void MainWindow::focusCurrentPlaylistItem(const bool ensureVisible)
{
    if (pinnedCourseBrowserActive_ || playlistView_ == nullptr || playlistController_ == nullptr || currentPlaylistIndex_ < 0) {
        return;
    }

    const QModelIndex sourceIndex = playlistController_->model()->index(currentPlaylistIndex_, 0);
    if (!sourceIndex.isValid()) {
        return;
    }

    QModelIndex targetIndex = playlistFilterModel_ != nullptr ? playlistFilterModel_->mapFromSource(sourceIndex) : sourceIndex;
    if (!targetIndex.isValid() && ensureVisible && playlistSearchEdit_ != nullptr && !playlistSearchEdit_->text().trimmed().isEmpty()) {
        playlistSearchEdit_->clear();
        targetIndex = playlistFilterModel_ != nullptr ? playlistFilterModel_->mapFromSource(sourceIndex) : sourceIndex;
    }

    if (!targetIndex.isValid()) {
        return;
    }

    playlistView_->setCurrentIndex(targetIndex);
    if (ensureVisible || (playlistAutoFollowEnabled() && !preservePlaylistViewportAfterReorder_)) {
        playlistView_->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    }
}

void MainWindow::refreshPlaylistSummary()
{
    if (playlistSummaryLabel_ == nullptr
        && playlistProgressRow_ == nullptr
        && playlistProgressTextLabel_ == nullptr
        && playlistAggregateProgressBar_ == nullptr
        && playlistTotalDurationLabel_ == nullptr) {
        return;
    }

    const int visibleItems = playlistView_ != nullptr && playlistView_->model() != nullptr
        ? playlistView_->model()->rowCount()
        : 0;
    const auto playlistEntries = playlistController_ != nullptr ? playlistController_->entries() : QVector<revaplayer::domain::PlaylistEntry> {};
    const int totalItems = pinnedCourseBrowserActive_ ? playlistEntries.size() : playlistCount_;
    const QString filterText = playlistSearchEdit_ != nullptr ? playlistSearchEdit_->text().trimmed() : QString {};
    const QString activeItemText = !pinnedCourseBrowserActive_ && currentPlaylistIndex_ >= 0
        ? uiText(" • #%1 active").arg(currentPlaylistIndex_ + 1)
        : QString {};

    QString summaryText;
    if (!filterText.isEmpty() && visibleItems != totalItems) {
        summaryText = uiText("%1 / %2 items%3").arg(visibleItems).arg(totalItems).arg(activeItemText);
    } else {
        summaryText = uiText("%1 items%2").arg(totalItems).arg(activeItemText);
    }

    int metadataReadyItems = 0;
    int metadataPendingItems = 0;
    int metadataFailedItems = 0;
    int metadataScannableItems = 0;
    int metadataDeferredItems = 0;
    for (const auto &entry : playlistEntries) {
        const QString scanKey = mediaScanSourceKey(entry.source.trimmed());
        if (scanKey.isEmpty()) {
            continue;
        }

        ++metadataScannableItems;
        if (pendingMediaScanSources_.contains(scanKey)) {
            ++metadataPendingItems;
            continue;
        }
        if (failedMediaScanSources_.contains(scanKey)) {
            ++metadataFailedItems;
            continue;
        }
        if (mediaScanCache_.contains(scanKey)) {
            ++metadataReadyItems;
            continue;
        }
        if (!mediaScanCacheLookupCompleted_.contains(scanKey)) {
            mediaScanCacheLookupCompleted_.insert(scanKey);
            if (const auto cached = loadCachedMediaScanResult(settingsController_, scanKey); cached.has_value()) {
                mediaScanCache_.insert(scanKey, *cached);
                ++metadataReadyItems;
                continue;
            }
        }

        if (mediaScanCache_.contains(scanKey)) {
            ++metadataReadyItems;
            continue;
        }

        ++metadataDeferredItems;
    }

    if (metadataScannableItems > 0) {
        summaryText += QStringLiteral(" • ");
        if (metadataReadyItems >= metadataScannableItems
            && metadataPendingItems == 0
            && metadataFailedItems == 0
            && metadataDeferredItems == 0) {
            summaryText += uiText("Metadata Ready");
        } else {
            summaryText += uiText("Metadata %1 / %2").arg(metadataReadyItems).arg(metadataScannableItems);
        }
        if (metadataPendingItems > 0) {
            summaryText += QStringLiteral(" • ") + uiText("Scanning %1").arg(metadataPendingItems);
            const int batchPendingCount = metadataPendingItems;
            const int batchTotalCount = std::max(playlistMetadataScanBatchTotal_, batchPendingCount);
            const int batchCompletedCount = std::max(0, batchTotalCount - batchPendingCount);
            if (playlistMetadataScanElapsed_.isValid() && batchCompletedCount > 0) {
                const double averageMsPerItem = static_cast<double>(playlistMetadataScanElapsed_.elapsed()) / batchCompletedCount;
                const double remainingSeconds = std::max(0.0, (averageMsPerItem * batchPendingCount) / 1000.0);
                summaryText += QStringLiteral(" • ") + uiText("About %1 left").arg(formatPlaybackTime(remainingSeconds));
            }
        }
        if (metadataDeferredItems > 0) {
            summaryText += QStringLiteral(" • ") + uiText("On demand %1").arg(metadataDeferredItems);
        }
        if (metadataFailedItems > 0) {
            summaryText += QStringLiteral(" • ") + uiText("%1 failed").arg(metadataFailedItems);
        }
    }

    if (playlistSummaryLabel_ != nullptr) {
        playlistSummaryLabel_->setText(summaryText);
        playlistSummaryLabel_->setToolTip(summaryText);
    }

    refreshPlaylistProgressIndicators();

    if (playlistRefreshButton_ != nullptr) {
        QString metadataStatusText = uiText("Metadata Ready");
        if (metadataScannableItems <= 0) {
            metadataStatusText = uiText("Playlist metadata is already up to date");
        } else if (metadataReadyItems >= metadataScannableItems
                   && metadataPendingItems == 0
                   && metadataFailedItems == 0
                   && metadataDeferredItems == 0) {
            metadataStatusText = uiText("Metadata Ready");
        } else {
            metadataStatusText = uiText("Metadata %1 / %2").arg(metadataReadyItems).arg(metadataScannableItems);
            if (metadataPendingItems > 0) {
                metadataStatusText += QStringLiteral(" • ") + uiText("Scanning %1").arg(metadataPendingItems);
            }
            if (metadataDeferredItems > 0) {
                metadataStatusText += QStringLiteral(" • ") + uiText("On demand %1").arg(metadataDeferredItems);
            }
            if (metadataFailedItems > 0) {
                metadataStatusText += QStringLiteral(" • ") + uiText("%1 failed").arg(metadataFailedItems);
            }
        }
        playlistRefreshButton_->setToolTip(
            metadataScannableItems > 0
                ? QStringLiteral("%1\n%2")
                      .arg(uiText("Rescan metadata for every playlist item"), metadataStatusText)
                : uiText("Rescan metadata for every playlist item"));
    }
}

void MainWindow::refreshPlaylistInspector()
{
    return;
}

void MainWindow::updatePlaylistReorderAvailability()
{
    if (playlistView_ == nullptr) {
        return;
    }

    const bool filterActive = playlistSearchEdit_ != nullptr && !playlistSearchEdit_->text().trimmed().isEmpty();
    const bool reorderEnabled = !filterActive
        && playlistController_ != nullptr
        && playlistController_->entries().size() > 1;
    playlistView_->setDragEnabled(reorderEnabled);
    playlistView_->setAcceptDrops(reorderEnabled);
    playlistView_->setDropIndicatorShown(false);
    playlistView_->setDragDropMode(reorderEnabled ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
}

void MainWindow::refreshPlaylistViewLayout()
{
    if (playlistView_ == nullptr) {
        return;
    }

    playlistView_->doItemsLayout();
    playlistView_->updateGeometry();
    playlistView_->viewport()->update();
}

void MainWindow::applyPlaylistPanelDisplayPreferences()
{
    if (playlistController_ != nullptr) {
        playlistController_->setSortMode(playlistSortModeId());
    }

    if (playlistView_ != nullptr) {
        playlistView_->setProperty("playlistProgressModeEnabled", progressTrackingModeEnabled());
        playlistView_->setProperty("playlistCardZoom", playlistCardZoomId());
        playlistView_->setProperty("playlistThumbnailShape", playlistThumbnailShapeId());
        playlistView_->setProperty("playlistVisibleColumns", playlistVisibleColumns());
        refreshPlaylistViewLayout();
    }

    if (detailsTabs_ == nullptr || detailsTabs_->tabBar() == nullptr) {
        return;
    }

    QVector<QPair<QString, QString>> detailsTabs;
    detailsTabs.reserve(detailsTabs_->count());
    for (int index = 0; index < detailsTabs_->count(); ++index) {
        QWidget *page = detailsTabs_->widget(index);
        if (page == nullptr) {
            continue;
        }

        const QString tabId = page->property("detailsTabId").toString().trimmed();
        if (!tabId.isEmpty()) {
            detailsTabs.push_back({tabId, detailsTabs_->tabText(index)});
        }
    }

    const QStringList storedVisibleTabIds = playlistVisibleDetailsTabIds();
    const QStringList visibleTabIds = storedVisibleTabIds.isEmpty()
        ? QStringList {}
        : normalizeVisibleDetailsTabs(storedVisibleTabIds, detailsTabs);
    const QString preferredTabId = playlistPreferredDetailsTabId();
    int fallbackIndex = -1;
    int preferredIndex = -1;
    int currentVisibleIndex = -1;

    for (int index = 0; index < detailsTabs_->count(); ++index) {
        QWidget *page = detailsTabs_->widget(index);
        if (page == nullptr) {
            continue;
        }

        const QString tabId = page->property("detailsTabId").toString().trimmed();
        const bool visible = visibleTabIds.isEmpty() || visibleTabIds.contains(tabId);
        page->setProperty("detailsTabHidden", !visible);
        detailsTabs_->tabBar()->setTabVisible(index, visible);

        if (visible && detailsTabs_->isTabEnabled(index) && fallbackIndex < 0) {
            fallbackIndex = index;
        }
        if (visible && detailsTabs_->isTabEnabled(index) && tabId == preferredTabId) {
            preferredIndex = index;
        }
        if (visible && detailsTabs_->currentIndex() == index) {
            currentVisibleIndex = index;
        }
    }

    const int targetIndex = preferredIndex >= 0
        ? preferredIndex
        : (currentVisibleIndex >= 0 ? currentVisibleIndex : fallbackIndex);
    if (targetIndex >= 0) {
        detailsTabs_->setCurrentIndex(targetIndex);
    }
    syncDetailsTabStripState();
}

void MainWindow::rebuildDetailsTabStrip()
{
    if (detailsTabs_ == nullptr
        || detailsTabsStripContainer_ == nullptr
        || detailsTabsStripLayout_ == nullptr) {
        return;
    }

    while (QLayoutItem *item = detailsTabsStripLayout_->takeAt(0)) {
        if (QWidget *widget = item->widget(); widget != nullptr) {
            delete widget;
        }
        delete item;
    }
    detailsTabButtons_.clear();

    for (int index = 0; index < detailsTabs_->count(); ++index) {
        QWidget *page = detailsTabs_->widget(index);
        if (page == nullptr) {
            continue;
        }

        auto *button = new QPushButton(detailsTabs_->tabText(index), detailsTabsStripContainer_);
        button->setObjectName(QStringLiteral("detailsTabStripButton"));
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, page]() {
            if (detailsTabs_ == nullptr || page == nullptr) {
                return;
            }

            const int pageIndex = detailsTabs_->indexOf(page);
            if (pageIndex < 0 || page->property("detailsTabHidden").toBool()) {
                return;
            }

            const bool tabEnabled = detailsTabs_->isTabEnabled(pageIndex);
            if (!tabEnabled) {
                detailsTabs_->setTabEnabled(pageIndex, true);
            }
            detailsTabs_->setCurrentIndex(pageIndex);
            if (!tabEnabled) {
                detailsTabs_->setTabEnabled(pageIndex, false);
            }
        });
        detailsTabsStripLayout_->addWidget(button, 0);
        detailsTabButtons_.push_back(button);
    }
    detailsTabsStripLayout_->addStretch(1);
    detailsTabsStripContainer_->adjustSize();

    if (detailsTabsStripScrollArea_ != nullptr && !detailsTabButtons_.isEmpty()) {
        const int rowHeight = detailsTabButtons_.constFirst() != nullptr
            ? detailsTabButtons_.constFirst()->sizeHint().height()
            : 36;
        const int scrollAreaHeight = rowHeight
            + detailsTabsStripScrollArea_->horizontalScrollBar()->sizeHint().height()
            + 8;
        detailsTabsStripScrollArea_->setFixedHeight(std::max(scrollAreaHeight, 58));
        if (detailsPanelSettingsButton_ != nullptr) {
            detailsPanelSettingsButton_->setFixedHeight(detailsTabsStripScrollArea_->height());
        }
    }

    syncDetailsTabStripState();
}

void MainWindow::syncDetailsTabStripState()
{
    if (detailsTabs_ == nullptr || detailsTabButtons_.isEmpty()) {
        return;
    }

    const int currentIndex = detailsTabs_->currentIndex();
    for (int index = 0; index < detailsTabs_->count() && index < detailsTabButtons_.size(); ++index) {
        QWidget *page = detailsTabs_->widget(index);
        QPushButton *button = detailsTabButtons_.at(index);
        if (page == nullptr || button == nullptr) {
            continue;
        }

        const bool visible = !page->property("detailsTabHidden").toBool();
        button->setText(detailsTabs_->tabText(index));
        button->setVisible(visible);
        button->setEnabled(visible);
        button->setChecked(index == currentIndex);
    }

    if (detailsTabsStripContainer_ != nullptr) {
        detailsTabsStripContainer_->adjustSize();
    }
    scrollDetailsTabButtonIntoView(currentIndex);
}

void MainWindow::scrollDetailsTabButtonIntoView(const int index)
{
    if (detailsTabsStripScrollArea_ == nullptr || index < 0 || index >= detailsTabButtons_.size()) {
        return;
    }

    QPushButton *button = detailsTabButtons_.at(index);
    if (button == nullptr || !button->isVisible()) {
        return;
    }

    QScrollBar *scrollBar = detailsTabsStripScrollArea_->horizontalScrollBar();
    if (scrollBar == nullptr) {
        return;
    }

    const int viewportWidth = detailsTabsStripScrollArea_->viewport()->width();
    const int currentValue = scrollBar->value();
    const int visibleLeft = currentValue;
    const int visibleRight = currentValue + viewportWidth;
    const int leftEdge = button->x();
    const int rightEdge = button->x() + button->width();
    if (leftEdge < visibleLeft) {
        scrollBar->setValue(leftEdge);
    } else if (rightEdge > visibleRight) {
        scrollBar->setValue(std::max(0, rightEdge - viewportWidth));
    }
}

void MainWindow::scrollPinnedCourseTabIntoView(const int index)
{
    if (pinnedCoursesScrollArea_ == nullptr || pinnedCoursesTabBar_ == nullptr || index < 0 || index >= pinnedCoursesTabBar_->count()) {
        return;
    }

    QScrollBar *scrollBar = pinnedCoursesScrollArea_->horizontalScrollBar();
    if (scrollBar == nullptr) {
        return;
    }

    const QRect tabRect = pinnedCoursesTabBar_->tabRect(index);
    if (!tabRect.isValid()) {
        return;
    }

    const int viewportWidth = pinnedCoursesScrollArea_->viewport()->width();
    const int currentValue = scrollBar->value();
    const int visibleLeft = currentValue;
    const int visibleRight = currentValue + viewportWidth;
    const int leftEdge = tabRect.left();
    const int rightEdge = tabRect.right() + 1;
    if (leftEdge < visibleLeft) {
        scrollBar->setValue(leftEdge);
    } else if (rightEdge > visibleRight) {
        scrollBar->setValue(std::max(0, rightEdge - viewportWidth));
    }
}

void MainWindow::updatePlaylistChromeState()
{
    const bool narrow = adaptiveUiEnabled() && width() < adaptiveUiBreakpoint();
    const bool hasSavedLists = pinnedCoursesTabBar_ != nullptr && pinnedCoursesTabBar_->count() > 1;

    if (playlistHeaderBar_ != nullptr) {
        playlistHeaderBar_->setVisible(false);
    }
    if (pinnedCoursesScrollArea_ != nullptr) {
        pinnedCoursesScrollArea_->setVisible(hasSavedLists);
    }
    if (playlistManageCoursesButton_ != nullptr) {
        playlistManageCoursesButton_->setVisible(hasSavedLists);
        playlistManageCoursesButton_->setEnabled(hasSavedLists);
    }
    if (playlistCoursesRow_ != nullptr) {
        playlistCoursesRow_->setProperty("rowHasTabs", hasSavedLists);
        playlistCoursesRow_->setVisible(hasSavedLists || playlistPinCourseButton_ != nullptr);
        refreshWidgetStyle(playlistCoursesRow_);
    }

    if (playlistSummaryLabel_ != nullptr) {
        playlistSummaryLabel_->setVisible(!narrow);
    }
    if (playlistProgressTextLabel_ != nullptr) {
        playlistProgressTextLabel_->setVisible(!narrow);
    }
}

void MainWindow::updateAdaptiveUiLayout()
{
    if (!adaptiveUiEnabled()) {
        updatePlaylistChromeState();
        return;
    }

    const int width = this->width();
    const bool narrow = width < adaptiveUiBreakpoint();

    if (playlistView_ != nullptr) {
        const QString effectiveZoomId = narrow
            ? adaptedPlaylistCardZoomForViewport(playlistCardZoomId(), adaptiveUiBreakpoint() - 1)
            : playlistCardZoomId();
        playlistView_->setProperty("playlistCardZoom", effectiveZoomId);
        playlistView_->setProperty("playlistThumbnailShape", playlistThumbnailShapeId());
        refreshPlaylistViewLayout();
    }

    updatePlaylistChromeState();
}

void MainWindow::applySelectedTheme(const bool announce)
{
    auto *application = qobject_cast<QApplication *>(QApplication::instance());
    if (application == nullptr) {
        return;
    }

    QString errorMessage;
    if (!revaplayer::application::applyApplicationTheme(
            *application,
            selectedThemeId(),
            uiAccentId(),
            uiDensityId(),
            revaplayer::application::ThemeCustomization {
                uiRadiusPx(),
                uiSpacingPx(),
                uiFontScalePercent(),
                uiFontWeightValue(),
                uiLetterSpacingValue(),
                uiBorderContrastPercent(),
                uiShadowStrengthPercent(),
                uiBlurStrengthPercent(),
                uiOverlayOpacityPercent(),
            },
            &errorMessage)) {
        statusBar()->showMessage(
            uiText("Theme could not be applied: %1").arg(errorMessage.isEmpty() ? uiText("Unknown error") : errorMessage),
            4000);
        return;
    }

    if (themeActionGroup_ != nullptr) {
        for (QAction *action : themeActionGroup_->actions()) {
            const QSignalBlocker blocker(action);
            action->setChecked(action->data().toString() == selectedThemeId());
        }
    }

    updateVideoOverlayGeometry();
    if (announce) {
        statusBar()->showMessage(
            uiText("Theme applied: %1").arg(revaplayer::application::themeLabel(selectedThemeId())),
            2500);
    }
}

void MainWindow::setRepeatMode(const QString &mode, const bool announce)
{
    QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("file") && normalized != QStringLiteral("playlist")) {
        normalized = QStringLiteral("off");
    }

    currentRepeatMode_ = normalized;
    playbackController_->setRepeatMode(normalized);
    syncRepeatModeActions();
    if (controlBar_ != nullptr) {
        controlBar_->setRepeatMode(currentRepeatMode_);
    }

    if (announce) {
        const QString label = normalized == QStringLiteral("file")
            ? uiText("Repeat File")
            : (normalized == QStringLiteral("playlist") ? uiText("Repeat Playlist") : uiText("Repeat Off"));
        statusBar()->showMessage(label, 2500);
        showPlaybackFeedback(label);
    }
}

void MainWindow::syncRepeatModeActions()
{
    if (repeatModeActionGroup_ == nullptr) {
        return;
    }

    for (QAction *action : repeatModeActionGroup_->actions()) {
        const QSignalBlocker blocker(action);
        action->setChecked(action->data().toString() == currentRepeatMode_);
    }
}

void MainWindow::setAlwaysOnTopEnabled(const bool enabled, const bool announce)
{
    if (alwaysOnTop_ == enabled) {
        if (alwaysOnTopAction_ != nullptr) {
            const QSignalBlocker blocker(alwaysOnTopAction_);
            alwaysOnTopAction_->setChecked(enabled);
        }
        return;
    }

    alwaysOnTop_ = enabled;
    const bool wasVisible = isVisible();
    const bool wasFullscreen = isFullScreen();
    const bool wasMaximized = isMaximized();

    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    if (wasVisible) {
        show();
        if (wasFullscreen) {
            showFullScreen();
        } else if (wasMaximized) {
            showMaximized();
        }
    }

    if (alwaysOnTopAction_ != nullptr) {
        const QSignalBlocker blocker(alwaysOnTopAction_);
        alwaysOnTopAction_->setChecked(enabled);
    }

    updateVideoOverlayGeometry();
    if (announce) {
        const QString label = enabled ? uiText("Always On Top enabled") : uiText("Always On Top disabled");
        statusBar()->showMessage(label, 2500);
        showPlaybackFeedback(label);
    }
}

void MainWindow::updateActionStates()
{
    const bool playbackAvailable = mediaLoaded_ && !loadingMedia_ && !errorStateActive_;
    const bool videoControlsAvailable = playbackAvailable && hasVideoTrack_;
    const bool hasPreviousPlaylistItem = currentPlaylistIndex_ > 0;
    const bool hasNextPlaylistItem = currentPlaylistIndex_ >= 0 && currentPlaylistIndex_ + 1 < playlistCount_;
    const bool hasPreviousChapter = currentChapterIndex_ > 0;
    const bool hasNextChapter = currentChapterIndex_ >= 0 && currentChapterIndex_ + 1 < chapterCount_;

    controlBar_->setPlaybackAvailable(playbackAvailable);
    controlBar_->setPlaylistNavigationAvailable(hasPreviousPlaylistItem, hasNextPlaylistItem);
    controlBar_->setRepeatMode(currentRepeatMode_);
    controlBar_->setLoopPoints(loopStartSeconds_, loopEndSeconds_);

    if (playPauseAction_ != nullptr) {
        playPauseAction_->setEnabled(playbackAvailable);
        if (!playbackAvailable) {
            playPauseAction_->setText(uiText("Play"));
        }
    }

    if (togglePlaylistAction_ != nullptr) {
        togglePlaylistAction_->setEnabled(true);
    }
    if (fullscreenPlaylistButton_ != nullptr) {
        fullscreenPlaylistButton_->setEnabled(true);
    }

    if (toggleDetailsAction_ != nullptr) {
        toggleDetailsAction_->setEnabled(true);
    }
    if (fullscreenDetailsButton_ != nullptr) {
        fullscreenDetailsButton_->setEnabled(true);
    }
    if (controlBar_ != nullptr) {
        controlBar_->setPanelButtonsEnabled(true);
    }
    if (fullscreenOpenButton_ != nullptr) {
        fullscreenOpenButton_->setEnabled(true);
    }
    if (fullscreenPreferencesButton_ != nullptr) {
        fullscreenPreferencesButton_->setEnabled(true);
    }
    if (fullscreenExitFullscreenButton_ != nullptr) {
        fullscreenExitFullscreenButton_->setEnabled(true);
    }

    if (showMediaInformationOverlayAction_ != nullptr) {
        showMediaInformationOverlayAction_->setEnabled(playbackAvailable);
    }

    if (takeScreenshotAction_ != nullptr) {
        takeScreenshotAction_->setEnabled(playbackAvailable);
    }
    if (favoriteCurrentMediaAction_ != nullptr) {
        favoriteCurrentMediaAction_->setEnabled(playbackAvailable);
        const QSignalBlocker blocker(favoriteCurrentMediaAction_);
        favoriteCurrentMediaAction_->setChecked(favoriteCurrentMedia_);
        favoriteCurrentMediaAction_->setText(favoriteCurrentMedia_
            ? uiText("Remove from Favorites")
            : uiText("Add to Favorites"));
    }

    int visibleFavoriteCount = 0;
    const bool hasFavoriteSelection = favoritesList_ != nullptr
        && favoritesList_->currentItem() != nullptr
        && !favoritesList_->currentItem()->data(Qt::UserRole).toString().trimmed().isEmpty();
    if (favoritesList_ != nullptr) {
        for (int row = 0; row < favoritesList_->count(); ++row) {
            QListWidgetItem *item = favoritesList_->item(row);
            if (item == nullptr || item->isHidden()) {
                continue;
            }
            if (!item->data(Qt::UserRole).toString().trimmed().isEmpty()) {
                ++visibleFavoriteCount;
            }
        }
    }
    if (favoritesCurrentButton_ != nullptr) {
        favoritesCurrentButton_->setEnabled(!currentMediaSource_.trimmed().isEmpty());
        const QSignalBlocker blocker(favoritesCurrentButton_);
        favoritesCurrentButton_->setChecked(favoriteCurrentMedia_);
        favoritesCurrentButton_->setText(favoriteCurrentMedia_
            ? uiText("Remove Current")
            : uiText("Add Current"));
    }
    if (favoritesOpenButton_ != nullptr) {
        favoritesOpenButton_->setEnabled(hasFavoriteSelection);
    }
    if (favoritesOpenAllButton_ != nullptr) {
        favoritesOpenAllButton_->setEnabled(visibleFavoriteCount > 0);
    }
    if (favoritesRemoveButton_ != nullptr) {
        favoritesRemoveButton_->setEnabled(hasFavoriteSelection);
    }
    if (favoritesRefreshButton_ != nullptr) {
        favoritesRefreshButton_->setEnabled(true);
    }
    if (favoritesAddButton_ != nullptr) {
        favoritesAddButton_->setEnabled(true);
    }

    if (addBookmarkAction_ != nullptr) {
        addBookmarkAction_->setEnabled(playbackAvailable);
    }
    if (bookmarkAddButton_ != nullptr) {
        bookmarkAddButton_->setEnabled(playbackAvailable);
    }
    if (bookmarkImportButton_ != nullptr) {
        bookmarkImportButton_->setEnabled(playbackAvailable);
    }
    if (bookmarkExportButton_ != nullptr) {
        bookmarkExportButton_->setEnabled(bookmarksList_ != nullptr && bookmarksList_->count() > 0);
    }
    if (bookmarkQuizCsvButton_ != nullptr) {
        bookmarkQuizCsvButton_->setEnabled(bookmarksList_ != nullptr && bookmarksList_->count() > 0);
    }
    if (bookmarkDeleteButton_ != nullptr) {
        bookmarkDeleteButton_->setEnabled(bookmarksList_ != nullptr && bookmarksList_->currentItem() != nullptr);
    }
    if (bookmarkJumpButton_ != nullptr) {
        bookmarkJumpButton_->setEnabled(bookmarksList_ != nullptr && bookmarksList_->currentItem() != nullptr);
    }
    if (setLoopStartAction_ != nullptr) {
        setLoopStartAction_->setEnabled(playbackAvailable);
    }

    if (setLoopEndAction_ != nullptr) {
        setLoopEndAction_->setEnabled(playbackAvailable && loopStartSeconds_ >= 0.0);
    }

    if (clearLoopAction_ != nullptr) {
        clearLoopAction_->setEnabled(playbackAvailable && (loopStartSeconds_ >= 0.0 || loopEndSeconds_ >= 0.0));
    }
    if (frameStepBackwardAction_ != nullptr) {
        frameStepBackwardAction_->setEnabled(playbackAvailable);
    }
    if (frameStepForwardAction_ != nullptr) {
        frameStepForwardAction_->setEnabled(playbackAvailable);
    }
    if (alwaysOnTopAction_ != nullptr) {
        const QSignalBlocker blocker(alwaysOnTopAction_);
        alwaysOnTopAction_->setChecked(alwaysOnTop_);
    }
    syncRepeatModeActions();

    if (deleteBookmarkAction_ != nullptr) {
        deleteBookmarkAction_->setEnabled(bookmarksList_ != nullptr && bookmarksList_->currentItem() != nullptr);
    }
    if (showMediaInfoAction_ != nullptr) {
        showMediaInfoAction_->setEnabled(playbackAvailable);
    }

    if (stopAction_ != nullptr) {
        stopAction_->setEnabled(playbackAvailable);
    }

    for (QAction *action : {loadSubtitleAction_,
                            toggleSubtitleVisibilityAction_,
                            seekBackwardShortAction_,
                            seekForwardShortAction_,
                            seekBackwardLongAction_,
                            seekForwardLongAction_,
                            volumeDownAction_,
                            volumeUpAction_,
                            toggleMuteAction_,
                            speedDownAction_,
                            speedUpAction_,
                            speedResetAction_,
                            subtitleDelayDownAction_,
                            subtitleDelayUpAction_,
                            subtitleDelayResetAction_,
                            subtitleDelayManualAction_,
                            subtitleScaleDownAction_,
                            subtitleScaleUpAction_,
                            subtitleScaleResetAction_,
                            subtitlePositionUpAction_,
                            subtitlePositionDownAction_,
                            subtitlePositionResetAction_,
                            cycleSubtitleAssOverrideAction_,
                            audioDelayDownAction_,
                            audioDelayUpAction_,
                            audioDelayResetAction_,
                            audioDelayManualAction_}) {
        if (action != nullptr) {
            action->setEnabled(playbackAvailable);
        }
    }

    if (subtitleOverrideActionGroup_ != nullptr) {
        for (QAction *action : subtitleOverrideActionGroup_->actions()) {
            action->setEnabled(playbackAvailable);
        }
    }

    const bool qualityControlsAvailable = videoControlsAvailable && std::any_of(
        currentTracks_.cbegin(),
        currentTracks_.cend(),
        [](const revaplayer::domain::TrackInfo &track) {
            return track.type == revaplayer::domain::TrackType::Video;
        });
    const bool subtitleControlsAvailable = playbackAvailable;

    if (aspectActionGroup_ != nullptr) {
        for (QAction *action : aspectActionGroup_->actions()) {
            action->setEnabled(videoControlsAvailable);
        }
    }
    if (cropActionGroup_ != nullptr) {
        for (QAction *action : cropActionGroup_->actions()) {
            action->setEnabled(videoControlsAvailable);
        }
    }
    if (rotateActionGroup_ != nullptr) {
        for (QAction *action : rotateActionGroup_->actions()) {
            action->setEnabled(videoControlsAvailable);
        }
    }
    if (videoQualityMenu_ != nullptr) {
        videoQualityMenu_->setEnabled(qualityControlsAvailable);
    }
    if (deinterlaceAction_ != nullptr) {
        deinterlaceAction_->setEnabled(videoControlsAvailable);
    }
    const bool videoTransformActive = videoViewportTransformActive(
        currentVideoZoomFactor_,
        currentVideoAlignX_,
        currentVideoAlignY_);
    const double minimumConfiguredZoom = std::max(1.0, videoMinimumZoomSetting());
    const bool videoPanAvailable = videoControlsAvailable && currentVideoZoomFactor_ > 1.0 + kVideoTransformEpsilon;
    if (videoZoomOutAction_ != nullptr) {
        videoZoomOutAction_->setEnabled(videoControlsAvailable && currentVideoZoomFactor_ > minimumConfiguredZoom + kVideoTransformEpsilon);
    }
    if (videoZoomInAction_ != nullptr) {
        videoZoomInAction_->setEnabled(videoControlsAvailable && currentVideoZoomFactor_ < videoMaximumZoomSetting() - kVideoTransformEpsilon);
    }
    if (videoZoomResetAction_ != nullptr) {
        videoZoomResetAction_->setEnabled(videoControlsAvailable && videoTransformActive);
    }
    for (QAction *action : {videoPanLeftAction_, videoPanRightAction_, videoPanUpAction_, videoPanDownAction_}) {
        if (action != nullptr) {
            action->setEnabled(videoPanAvailable);
        }
    }
    if (videoViewport_ != nullptr) {
        videoViewport_->setVideoPanEnabled(videoPanAvailable);
    }
    if (controlBar_ != nullptr) {
        controlBar_->setTrackMenusEnabled(qualityControlsAvailable, subtitleControlsAvailable);
    }

    if (previousPlaylistAction_ != nullptr) {
        previousPlaylistAction_->setEnabled(hasPreviousPlaylistItem);
    }
    if (nextPlaylistAction_ != nullptr) {
        nextPlaylistAction_->setEnabled(hasNextPlaylistItem);
    }
    if (previousChapterAction_ != nullptr) {
        previousChapterAction_->setEnabled(hasPreviousChapter);
    }
    if (nextChapterAction_ != nullptr) {
        nextChapterAction_->setEnabled(hasNextChapter);
    }

    if (historyRefreshButton_ != nullptr) {
        historyRefreshButton_->setEnabled(historyEnabled() && historyController_ != nullptr && historyController_->isReady());
    }
    if (historyClearButton_ != nullptr) {
        historyClearButton_->setEnabled(historyEnabled() && !historyEntries_.isEmpty());
    }
    if (historyList_ != nullptr) {
        historyList_->setEnabled(historyEnabled() && !historyEntries_.isEmpty());
    }
    if (sceneRefreshButton_ != nullptr) {
        sceneRefreshButton_->setEnabled(playbackAvailable && hasVideoTrack_);
    }
    if (sceneExportButton_ != nullptr) {
        sceneExportButton_->setEnabled(playbackAvailable && hasVideoTrack_ && sceneList_ != nullptr && sceneList_->count() > 0);
    }
    if (sceneBookmarkButton_ != nullptr) {
        sceneBookmarkButton_->setEnabled(playbackAvailable && hasVideoTrack_ && sceneList_ != nullptr && sceneList_->currentItem() != nullptr);
    }
    if (sceneStepSpinBox_ != nullptr) {
        sceneStepSpinBox_->setEnabled(playbackAvailable && hasVideoTrack_);
    }
    if (sceneList_ != nullptr) {
        sceneList_->setEnabled(playbackAvailable && hasVideoTrack_ && sceneList_->count() > 0);
    }
    if (secondarySubtitleCombo_ != nullptr) {
        secondarySubtitleCombo_->setEnabled(playbackAvailable && secondarySubtitleCombo_->count() > 1);
    }

    updateMediaInformationOverlay();
}

void MainWindow::restorePersistentState()
{
    if (settingsController_ == nullptr || !settingsController_->rememberWindowState()) {
        const QString mode = startupWindowMode();
        applyFullscreenOnShow_ = mode == QStringLiteral("fullscreen");
        applyMaximizedOnShow_ = mode == QStringLiteral("maximized");
        applyUiPreferences();
        showIdleOverlay(defaultIdleOverlayText());
        updateActionStates();
        return;
    }

    const auto stateRecord = settingsController_->mainWindowState();
    if (!stateRecord.has_value()) {
        applyUiPreferences();
        showIdleOverlay(defaultIdleOverlayText());
        updateActionStates();
        return;
    }

    if (!stateRecord->geometry.isEmpty()) {
        restoreGeometry(stateRecord->geometry);
        setWindowState(windowState() & ~Qt::WindowFullScreen & ~Qt::WindowMaximized);
    }

    if (!stateRecord->state.isEmpty()) {
        restoreState(stateRecord->state);
    }

    applyFullscreenOnShow_ = false;
    applyMaximizedOnShow_ = stateRecord->maximized;
    normalizeWindowGeometryAfterRestore();
    syncPanelToggleActions();
    updateActionStates();
}

void MainWindow::persistWindowState()
{
    if (settingsController_ == nullptr) {
        return;
    }

    if (!settingsController_->rememberWindowState()) {
        return;
    }

    settingsController_->saveMainWindowState(
        saveGeometry(),
        panelOverlayModeActive_ ? defaultWindowState_ : saveState(),
        isMaximized(),
        false);
    settingsController_->setPlaylistOverlayPanelWidth(playlistOverlayPanelWidth_);
    settingsController_->setDetailsOverlayPanelWidth(detailsOverlayPanelWidth_);
}

void MainWindow::normalizeWindowGeometryAfterRestore()
{
    if (isFullScreen()) {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }

    if (applyMaximizedOnShow_) {
        return;
    }

    QRect availableBounds;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen != nullptr) {
            availableBounds = availableBounds.united(screen->availableGeometry());
        }
    }

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QRect primaryBounds = primaryScreen != nullptr ? primaryScreen->availableGeometry() : QRect {};
    if (!availableBounds.isValid()) {
        availableBounds = primaryBounds;
    }
    if (!availableBounds.isValid()) {
        return;
    }

    const int minimumWidth = std::min(availableBounds.width(), std::max(this->minimumWidth(), 720));
    const int minimumHeight = std::min(availableBounds.height(), std::max(this->minimumHeight(), 420));
    resize(
        std::clamp(width(), minimumWidth, availableBounds.width()),
        std::clamp(height(), minimumHeight, availableBounds.height()));

    QRect frame = frameGeometry();
    QPoint targetTopLeft = frame.topLeft();
    if (!availableBounds.intersects(frame) && primaryBounds.isValid()) {
        targetTopLeft = primaryBounds.center() - QPoint(frame.width() / 2, frame.height() / 2);
    }

    const int maxX = std::max(availableBounds.left(), availableBounds.right() - frame.width() + 1);
    const int maxY = std::max(availableBounds.top(), availableBounds.bottom() - frame.height() + 1);
    targetTopLeft.setX(std::clamp(targetTopLeft.x(), availableBounds.left(), maxX));
    targetTopLeft.setY(std::clamp(targetTopLeft.y(), availableBounds.top(), maxY));
    move(targetTopLeft);
}

void MainWindow::updateWindowTitle()
{
    const QString suffix = QStringLiteral("Reva Player");
    const QString effectiveTitle = effectiveCurrentMediaTitle();
    const QString titleText = effectiveTitle.isEmpty() ? suffix : effectiveTitle;
    if (fullscreenTitleLabel_ != nullptr) {
        fullscreenTitleLabel_->setText(titleText);
    }

    if (effectiveTitle.isEmpty()) {
        setWindowTitle(suffix);
        return;
    }

    setWindowTitle(QStringLiteral("%1 - %2").arg(effectiveTitle, suffix));
}

QString MainWindow::effectiveCurrentMediaTitle() const
{
    const QString source = currentMediaSource_.trimmed();
    if (!source.isEmpty()) {
        const QString localPath = localMediaPathForSource(source);
        if (!localPath.isEmpty()) {
            const QString fileName = fileNameFromPathLikeSource(localPath);
            if (!fileName.isEmpty()) {
                return fileName;
            }
        }
    }

    if (!currentTitle_.trimmed().isEmpty()) {
        return currentTitle_.trimmed();
    }

    if (source.isEmpty()) {
        return {};
    }

    const QString scanKey = mediaScanSourceKey(source);
    const auto it = mediaScanCache_.constFind(scanKey);
    if (it != mediaScanCache_.constEnd() && !it->mediaTitle.trimmed().isEmpty()) {
        return it->mediaTitle.trimmed();
    }

    return displayTitleForHistory(source, QString {});
}

void MainWindow::reloadCurrentFolderPlaylist()
{
    const QString localPath = localMediaPathForSource(currentMediaSource_);
    const QFileInfo currentFile(localPath);
    if (!isSupportedMediaFile(currentFile)) {
        statusBar()->showMessage(uiText("Current media is not a local file that can seed a folder playlist."), 4000);
        return;
    }

    const QStringList folderPlaylist = siblingMediaPlaylist(currentFile.absoluteFilePath());
    if (folderPlaylist.isEmpty()) {
        statusBar()->showMessage(uiText("No sibling media files were found in this folder."), 4000);
        return;
    }

    int currentIndex = 0;
    for (int index = 0; index < folderPlaylist.size(); ++index) {
        if (sourcesReferToSameMedia(folderPlaylist.at(index), currentMediaSource_)) {
            currentIndex = index;
            break;
        }
    }

    armPendingCurrentMediaRestore(currentMediaSource_, currentPositionSeconds_);
    openPlaylistSources(folderPlaylist, currentIndex, !isFullScreen());
    statusBar()->showMessage(uiText("Reloaded playlist from %1").arg(currentFile.dir().dirName()), 3000);
}

QStringList MainWindow::resolvedOpenFiles(const QStringList &requestedFiles, bool *expandedFromFolder) const
{
    if (expandedFromFolder != nullptr) {
        *expandedFromFolder = false;
    }

    QStringList cleanedFiles;
    cleanedFiles.reserve(requestedFiles.size());
    for (const QString &path : requestedFiles) {
        const QFileInfo fileInfo(path);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            continue;
        }

        const QString normalizedPath = fileInfo.canonicalFilePath().isEmpty()
            ? fileInfo.absoluteFilePath()
            : fileInfo.canonicalFilePath();
        if (!cleanedFiles.contains(normalizedPath)) {
            cleanedFiles.push_back(normalizedPath);
        }
    }

    if (cleanedFiles.size() != 1 || !autoLoadSiblingMediaEnabled()) {
        return cleanedFiles;
    }

    const QStringList folderPlaylist = siblingMediaPlaylist(cleanedFiles.first());
    if (folderPlaylist.size() <= 1) {
        return cleanedFiles;
    }

    if (expandedFromFolder != nullptr) {
        *expandedFromFolder = true;
    }
    return folderPlaylist;
}

QStringList MainWindow::siblingMediaPlaylist(const QString &selectedFilePath) const
{
    const QFileInfo selectedInfo(selectedFilePath);
    if (!isSupportedMediaFile(selectedInfo)) {
        return {};
    }

    QFileInfoList siblings = selectedInfo.dir().entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    siblings.erase(std::remove_if(siblings.begin(), siblings.end(), [](const QFileInfo &entry) {
                      return !isSupportedMediaFile(entry);
                  }),
                   siblings.end());

    if (siblings.isEmpty()) {
        return QStringList {selectedInfo.absoluteFilePath()};
    }

    if (naturalSortFolderPlaylistEnabled()) {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        collator.setNumericMode(true);
        std::sort(siblings.begin(), siblings.end(), [&collator](const QFileInfo &left, const QFileInfo &right) {
            return collator.compare(left.fileName(), right.fileName()) < 0;
        });
    }

    const QString normalizedSelected = selectedInfo.canonicalFilePath().isEmpty()
        ? selectedInfo.absoluteFilePath()
        : selectedInfo.canonicalFilePath();

    QStringList playlist;
    playlist.reserve(siblings.size());

    int selectedIndex = -1;
    for (qsizetype index = 0; index < siblings.size(); ++index) {
        const QFileInfo &entry = siblings.at(index);
        const QString normalizedEntry = entry.canonicalFilePath().isEmpty()
            ? entry.absoluteFilePath()
            : entry.canonicalFilePath();
        playlist.push_back(normalizedEntry);
        if (normalizedEntry == normalizedSelected) {
            selectedIndex = static_cast<int>(index);
        }
    }

    if (selectedIndex > 0 && rotateFolderPlaylistToCurrentEnabled()) {
        std::rotate(playlist.begin(), playlist.begin() + selectedIndex, playlist.end());
    }

    return playlist;
}

void MainWindow::seekBySecondsWithFeedback(const int seconds)
{
    if (seconds == 0) {
        return;
    }

    playbackController_->seekBySeconds(seconds);
    const double targetPosition = std::clamp(
        currentPositionSeconds_ + static_cast<double>(seconds),
        0.0,
        std::max(0.0, currentDurationSeconds_));
    currentPositionSeconds_ = targetPosition;
    persistPlaybackProgress(false, true);
    if (comparePlaybackController_ != nullptr && compareSourceLoaded_) {
        comparePlaybackController_->seekToSeconds(targetPosition);
    }
    const QString directionText = seconds > 0
        ? QStringLiteral("⏩ +%1s").arg(seconds)
        : QStringLiteral("⏪ %1s").arg(seconds);
    showPlaybackFeedback(QStringLiteral("%1  •  %2").arg(directionText, formatPlaybackTime(targetPosition)));
}

void MainWindow::adjustVolumeWithFeedback(const int delta)
{
    if (delta == 0) {
        return;
    }

    playbackController_->adjustVolume(delta);
    const int targetVolume = revaplayer::application::clampPlaybackVolume(currentVolume_ + delta);
    showVolumeFeedback(targetVolume);
}

void MainWindow::showActionResult(const QString &statusMessage,
                                  const QString &overlayMessage,
                                  const int timeoutMs)
{
    const QString trimmedStatus = statusMessage.trimmed();
    if (!trimmedStatus.isEmpty() && statusBar() != nullptr) {
        statusBar()->showMessage(trimmedStatus, timeoutMs);
    }

    const QString effectiveOverlay = overlayMessage.trimmed().isEmpty()
        ? trimmedStatus
        : overlayMessage.trimmed();
    if (!effectiveOverlay.isEmpty()) {
        showPlaybackFeedback(effectiveOverlay);
    }
}

void MainWindow::showPlaybackFeedback(const QString &message)
{
    if (!actionFeedbackOverlayEnabled_ || videoViewport_ == nullptr || message.trimmed().isEmpty()) {
        return;
    }

    videoViewport_->showActionOverlay(message.trimmed());
}

void MainWindow::showPlaybackSpeedFeedback(const double speed, const bool reset)
{
    const QString formattedSpeed = revaplayer::application::formatPlaybackRate(reset ? 1.0 : speed);
    statusBar()->showMessage(
        reset
            ? uiText("Playback speed reset to %1").arg(formattedSpeed)
            : uiText("Playback speed: %1").arg(formattedSpeed),
        2500);
    showPlaybackFeedback(uiText("Speed %1").arg(formattedSpeed));
}

void MainWindow::showVolumeFeedback(const int volume)
{
    if (!actionFeedbackOverlayEnabled_ || videoViewport_ == nullptr) {
        return;
    }

    videoViewport_->showVolumeOverlay(
        revaplayer::application::clampPlaybackVolume(volume),
        revaplayer::application::kMaximumPlaybackVolume,
        revaplayer::application::kDefaultPlaybackVolume);
}

void MainWindow::applyVideoViewportTransform()
{
    currentVideoZoomFactor_ = revaplayer::application::clampVideoZoomFactor(currentVideoZoomFactor_);
    if (currentVideoZoomFactor_ <= 1.0 + kVideoTransformEpsilon) {
        currentVideoZoomFactor_ = 1.0;
        currentVideoAlignX_ = 0.0;
        currentVideoAlignY_ = 0.0;
    } else {
        currentVideoAlignX_ = clampConfiguredVideoViewportAlignment(
            currentVideoAlignX_,
            videoZoomConstrainPanningEnabled());
        currentVideoAlignY_ = clampConfiguredVideoViewportAlignment(
            currentVideoAlignY_,
            videoZoomConstrainPanningEnabled());
    }

    if (playbackController_ == nullptr || !playbackController_->isInitialized()) {
        return;
    }

    playbackController_->setVideoZoomFactor(currentVideoZoomFactor_);
    playbackController_->setVideoAlignment(currentVideoAlignX_, currentVideoAlignY_);
}

void MainWindow::setVideoZoomFactor(const double factor, const bool showFeedback)
{
    double normalizedFactor = revaplayer::application::clampVideoZoomFactor(factor);
    if (normalizedFactor > 1.0 + kVideoTransformEpsilon) {
        normalizedFactor = std::clamp(
            normalizedFactor,
            std::max(1.0, videoMinimumZoomSetting()),
            videoMaximumZoomSetting());
    }
    const bool transformWasActive = videoViewportTransformActive(
        currentVideoZoomFactor_,
        currentVideoAlignX_,
        currentVideoAlignY_);

    currentVideoZoomFactor_ = normalizedFactor;
    if (currentVideoZoomFactor_ <= 1.0 + kVideoTransformEpsilon) {
        currentVideoZoomFactor_ = 1.0;
        currentVideoAlignX_ = 0.0;
        currentVideoAlignY_ = 0.0;
    }

    applyVideoViewportTransform();
    updateActionStates();
    rememberVideoZoomStateForCurrentMedia();

    if (showFeedback) {
        const bool transformActive = videoViewportTransformActive(
            currentVideoZoomFactor_,
            currentVideoAlignX_,
            currentVideoAlignY_);
        const QString message = !transformActive && transformWasActive
            ? uiText("Zoom reset")
            : uiText("Zoom %1").arg(revaplayer::application::formatScaleFactor(currentVideoZoomFactor_));
        statusBar()->showMessage(message, 2500);
        showPlaybackFeedback(message);
    }
}

void MainWindow::adjustVideoZoomFactor(const double delta)
{
    setVideoZoomFactor(currentVideoZoomFactor_ + delta, true);
}

void MainWindow::resetVideoZoomAndPan(const bool showFeedback)
{
    currentVideoZoomFactor_ = 1.0;
    currentVideoAlignX_ = 0.0;
    currentVideoAlignY_ = 0.0;
    applyVideoViewportTransform();
    updateActionStates();
    rememberVideoZoomStateForCurrentMedia();

    if (showFeedback) {
        statusBar()->showMessage(uiText("Zoom reset"), 2500);
        showPlaybackFeedback(uiText("Zoom reset"));
    }
}

void MainWindow::setVideoAlignment(const double horizontal, const double vertical, const bool showFeedback)
{
    if (currentVideoZoomFactor_ <= 1.0 + kVideoTransformEpsilon) {
        currentVideoAlignX_ = 0.0;
        currentVideoAlignY_ = 0.0;
    } else {
        currentVideoAlignX_ = clampConfiguredVideoViewportAlignment(horizontal, videoZoomConstrainPanningEnabled());
        currentVideoAlignY_ = clampConfiguredVideoViewportAlignment(vertical, videoZoomConstrainPanningEnabled());
    }

    applyVideoViewportTransform();
    updateActionStates();
    rememberVideoZoomStateForCurrentMedia();

    if (showFeedback) {
        statusBar()->showMessage(uiText("Video position updated"), 1800);
    }
}

void MainWindow::adjustVideoAlignment(const double deltaX, const double deltaY, const bool showFeedback)
{
    setVideoAlignment(currentVideoAlignX_ + deltaX, currentVideoAlignY_ + deltaY, false);

    if (!showFeedback) {
        return;
    }

    QString message = uiText("Video position updated");
    if (std::abs(deltaX) >= std::abs(deltaY)) {
        message = deltaX < 0.0 ? uiText("Pan Left") : uiText("Pan Right");
    } else if (std::abs(deltaY) > 0.0) {
        message = deltaY < 0.0 ? uiText("Pan Up") : uiText("Pan Down");
    }
    statusBar()->showMessage(message, 1800);
    showPlaybackFeedback(message);
}

void MainWindow::handleVideoPanDelta(const QPoint &deltaPixels)
{
    if (videoViewport_ == nullptr
        || !hasVideoTrack_
        || currentVideoZoomFactor_ <= 1.0 + kVideoTransformEpsilon
        || deltaPixels.isNull()) {
        return;
    }

    const double deltaX = (-2.0 * static_cast<double>(deltaPixels.x()))
        / std::max(1, videoViewport_->width())
        * std::max(0.25, videoPanSensitivitySetting());
    const double deltaY = (-2.0 * static_cast<double>(deltaPixels.y()))
        / std::max(1, videoViewport_->height())
        * std::max(0.25, videoPanSensitivitySetting());
    adjustVideoAlignment(deltaX, deltaY, false);
}

QJsonObject MainWindow::currentVideoTransformState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("zoom_factor"), currentVideoZoomFactor_);
    state.insert(QStringLiteral("align_x"), currentVideoAlignX_);
    state.insert(QStringLiteral("align_y"), currentVideoAlignY_);
    return state;
}

void MainWindow::rememberVideoZoomStateForCurrentMedia()
{
    const QString source = currentMediaSource_.trimmed();
    const QString mode = videoZoomRememberModeSetting();
    if (source.isEmpty() || mode == QStringLiteral("off")) {
        return;
    }

    const QJsonObject state = currentVideoTransformState();
    if (mode == QStringLiteral("session")) {
        sessionVideoZoomStates_.insert(source, state);
        return;
    }

    if (settingsController_ == nullptr) {
        return;
    }

    const QString storageKey = videoZoomStateStorageKeyForSource(source);
    if (!storageKey.isEmpty()) {
        settingsController_->setCustomValue(
            storageKey,
            QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact)));
    }
}

bool MainWindow::applyRememberedVideoZoomStateForSource(const QString &source)
{
    const QString trimmedSource = source.trimmed();
    const QString mode = videoZoomRememberModeSetting();
    if (trimmedSource.isEmpty() || mode == QStringLiteral("off")) {
        return false;
    }

    QJsonObject state;
    if (mode == QStringLiteral("session")) {
        state = sessionVideoZoomStates_.value(trimmedSource);
    } else if (settingsController_ != nullptr) {
        const QString storageKey = videoZoomStateStorageKeyForSource(trimmedSource);
        const QJsonDocument document = QJsonDocument::fromJson(settingsController_->customValue(storageKey).toUtf8());
        state = document.object();
        if (state.isEmpty()) {
            settingsController_->removeCustomValue(storageKey);
        }
    }

    if (state.isEmpty()) {
        return false;
    }

    currentVideoZoomFactor_ = state.value(QStringLiteral("zoom_factor")).toDouble(1.0);
    if (currentVideoZoomFactor_ > 1.0 + kVideoTransformEpsilon) {
        currentVideoZoomFactor_ = std::clamp(
            revaplayer::application::clampVideoZoomFactor(currentVideoZoomFactor_),
            std::max(1.0, videoMinimumZoomSetting()),
            videoMaximumZoomSetting());
    } else {
        currentVideoZoomFactor_ = 1.0;
    }
    currentVideoAlignX_ = state.value(QStringLiteral("align_x")).toDouble(currentVideoAlignX_);
    currentVideoAlignY_ = state.value(QStringLiteral("align_y")).toDouble(currentVideoAlignY_);
    applyVideoViewportTransform();
    updateActionStates();
    return true;
}

void MainWindow::applyConfiguredZoomStateForCurrentMedia()
{
    const QString source = currentMediaSource_.trimmed();
    if (source.isEmpty() || !hasVideoTrack_) {
        currentVideoZoomFactor_ = 1.0;
        currentVideoAlignX_ = 0.0;
        currentVideoAlignY_ = 0.0;
        applyVideoViewportTransform();
        updateActionStates();
        return;
    }

    if (applyRememberedVideoZoomStateForSource(source)) {
        return;
    }

    if (videoZoomResetOnFileChangeEnabled() || videoZoomDefaultBehaviorSetting() == QStringLiteral("fit_to_frame")) {
        resetVideoZoomAndPan(false);
        return;
    }

    if (currentVideoZoomFactor_ > 1.0 + kVideoTransformEpsilon) {
        currentVideoZoomFactor_ = std::clamp(
            currentVideoZoomFactor_,
            std::max(1.0, videoMinimumZoomSetting()),
            videoMaximumZoomSetting());
    } else {
        currentVideoZoomFactor_ = 1.0;
    }
    applyVideoViewportTransform();
    updateActionStates();
}

void MainWindow::applyAdvancedSubtitlePreferences()
{
    if (playbackController_ == nullptr || !playbackController_->isInitialized() || settingsController_ == nullptr) {
        return;
    }

    const QString autoExtensions = revaplayer::application::normalizeSubtitleAutoExtensions(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
            revaplayer::application::defaultSubtitleAutoExtensions()));
    const QString codepage = revaplayer::application::normalizeSubtitleEncoding(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleCodepageSetting),
            QStringLiteral("auto")));
    const bool fixTiming = customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleFixTimingSetting,
        false);
    const int fontWeight = revaplayer::application::clampSubtitleFontWeight(customSettingInt(
        settingsController_,
        revaplayer::application::kSubtitleFontWeightSetting,
        500,
        300,
        900));
    const bool italic = customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleItalicSetting,
        false);
    const QString textColor = revaplayer::application::normalizeSubtitleColorString(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleTextColorSetting),
            QStringLiteral("#FFFFFFFF")),
        QStringLiteral("#FFFFFFFF"));
    const QString outlineColor = revaplayer::application::normalizeSubtitleColorString(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleOutlineColorSetting),
            QStringLiteral("#FF000000")),
        QStringLiteral("#FF000000"));
    const double outlineSize = revaplayer::application::clampSubtitleOutlineSize(customSettingDouble(
        settingsController_,
        revaplayer::application::kSubtitleOutlineSizeSetting,
        1.65,
        0.0,
        12.0));
    const bool backgroundEnabled = customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleBackgroundEnabledSetting,
        false);
    const QColor backgroundColor = withAlphaPercent(
        QColor(revaplayer::application::normalizeSubtitleColorString(
            settingsController_->customValue(
                QString::fromLatin1(revaplayer::application::kSubtitleBackgroundColorSetting),
                QStringLiteral("#AF000000")),
            QStringLiteral("#AF000000"))),
        revaplayer::application::clampSubtitleBackgroundOpacity(customSettingInt(
            settingsController_,
            revaplayer::application::kSubtitleBackgroundOpacitySetting,
            68,
            0,
            100)));
    const bool shadowEnabled = customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleShadowEnabledSetting,
        true);
    const QColor shadowColor = QColor(revaplayer::application::normalizeSubtitleColorString(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShadowColorSetting),
            QStringLiteral("#AF000000")),
        QStringLiteral("#AF000000")));
    const double shadowOffset = shadowEnabled
        ? revaplayer::application::clampSubtitleShadowOffset(customSettingDouble(
            settingsController_,
            revaplayer::application::kSubtitleShadowOffsetSetting,
            0.0,
            0.0,
            20.0))
        : 0.0;
    const double shadowBlur = shadowEnabled
        ? revaplayer::application::clampSubtitleBlur(customSettingDouble(
            settingsController_,
            revaplayer::application::kSubtitleShadowBlurSetting,
            0.0,
            0.0,
            3.0))
        : 0.0;
    const QString effectiveBorderStyle = backgroundEnabled
        ? revaplayer::application::normalizeSubtitleBorderStyle(
            settingsController_->customValue(
                QString::fromLatin1(revaplayer::application::kSubtitleBorderStyleSetting),
                QStringLiteral("background-box")))
        : QStringLiteral("outline-and-shadow");
    const QColor backColor = backgroundEnabled ? backgroundColor : (shadowEnabled ? shadowColor : QColor(QStringLiteral("#00000000")));

    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-auto"), QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-auto-exts"), autoExtensions});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-codepage"), codepage});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-fix-timing"), fixTiming ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-bold"), fontWeight >= 600 ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-italic"), italic ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-color"), textColor});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-outline-color"), outlineColor});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-outline-size"), QString::number(outlineSize, 'f', 2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-border-style"), effectiveBorderStyle});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-back-color"), backColor.name(QColor::HexArgb)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-shadow-offset"), QString::number(shadowOffset, 'f', 2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-gauss"), QString::number(shadowBlur, 'f', 2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-line-spacing"), QString::number(
        revaplayer::application::clampSubtitleLineSpacing(customSettingDouble(
            settingsController_,
            revaplayer::application::kSubtitleLineSpacingSetting,
            0.0,
            -1000.0,
            1000.0)),
        'f',
        2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-spacing"), QString::number(
        revaplayer::application::clampSubtitleLetterSpacing(customSettingDouble(
            settingsController_,
            revaplayer::application::kSubtitleLetterSpacingSetting,
            0.0,
            -10.0,
            10.0)),
        'f',
        2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-width"), QString::number(
        revaplayer::application::clampSubtitleMaxWidth(customSettingDouble(
            settingsController_,
            revaplayer::application::kSubtitleMaxWidthSetting,
            92.0,
            30.0,
            100.0)),
        'f',
        2)});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-align-x"), revaplayer::application::normalizeSubtitleAlignX(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignXSetting),
            QStringLiteral("center")))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-align-y"), revaplayer::application::normalizeSubtitleAlignY(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAlignYSetting),
            QStringLiteral("bottom")))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-justify"), revaplayer::application::normalizeSubtitleJustify(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleJustifySetting),
            QStringLiteral("auto")))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-margin-x"), QString::number(
        revaplayer::application::clampSubtitleMarginX(customSettingInt(
            settingsController_,
            revaplayer::application::kSubtitleMarginXSetting,
            19,
            0,
            300)))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-margin-y"), QString::number(
        revaplayer::application::clampSubtitleMarginY(customSettingInt(
            settingsController_,
            revaplayer::application::kSubtitleMarginYSetting,
            34,
            0,
            600)))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-use-margins"), customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleUseMarginsSetting,
        true) ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-scale-with-window"), customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleScaleWithWindowSetting,
        true) ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-ass-force-margins"), customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleAssForceMarginsSetting,
        false) ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-ass-justify"), customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleAssJustifySetting,
        false) ? QStringLiteral("yes") : QStringLiteral("no")});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-font-provider"), revaplayer::application::normalizeSubtitleFontProvider(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleFontProviderSetting),
            QStringLiteral("auto")))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-shaper"), revaplayer::application::normalizeSubtitleShaper(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleShaperSetting),
            QStringLiteral("complex")))});
    playbackController_->executeMpvCommand({QStringLiteral("set"), QStringLiteral("sub-hinting"), revaplayer::application::normalizeSubtitleHinting(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleHintingSetting),
            QStringLiteral("none")))});
}

bool MainWindow::rememberSubtitleTrackChoiceEnabled() const
{
    return customSettingFlag(
        settingsController_,
        revaplayer::application::kSubtitleRememberTrackChoiceSetting,
        false);
}

void MainWindow::applyRememberedSubtitleTrackChoice(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    if (settingsController_ == nullptr
        || playbackController_ == nullptr
        || !rememberSubtitleTrackChoiceEnabled()
        || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString desiredSignature = settingsController_->customValue(
        rememberedSubtitleTrackStorageKeyForSource(currentMediaSource_)).trimmed();
    if (desiredSignature.isEmpty()) {
        return;
    }

    const auto alreadySelected = std::find_if(tracks.cbegin(), tracks.cend(), [&desiredSignature](const auto &track) {
        return track.type == revaplayer::domain::TrackType::Subtitle
            && track.selected
            && subtitleTrackChoiceSignature(track) == desiredSignature;
    });
    if (alreadySelected != tracks.cend()) {
        return;
    }

    const auto matchIt = std::find_if(tracks.cbegin(), tracks.cend(), [&desiredSignature](const auto &track) {
        return track.type == revaplayer::domain::TrackType::Subtitle
            && subtitleTrackChoiceSignature(track) == desiredSignature;
    });
    if (matchIt == tracks.cend()) {
        return;
    }

    applyingRememberedSubtitleTrackChoice_ = true;
    playbackController_->setSubtitleVisible(true);
    playbackController_->selectTrack(revaplayer::domain::TrackType::Subtitle, matchIt->id);
}

void MainWindow::rememberSelectedSubtitleTrackChoice(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    if (settingsController_ == nullptr
        || !rememberSubtitleTrackChoiceEnabled()
        || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    const QString storageKey = rememberedSubtitleTrackStorageKeyForSource(currentMediaSource_);
    const bool hasSubtitleTrack = std::any_of(tracks.cbegin(), tracks.cend(), [](const auto &track) {
        return track.type == revaplayer::domain::TrackType::Subtitle;
    });
    const auto selectedIt = std::find_if(tracks.cbegin(), tracks.cend(), [](const auto &track) {
        return track.type == revaplayer::domain::TrackType::Subtitle && track.selected;
    });
    if (selectedIt == tracks.cend()) {
        if (hasSubtitleTrack) {
            settingsController_->removeCustomValue(storageKey);
        }
        return;
    }

    settingsController_->setCustomValue(storageKey, subtitleTrackChoiceSignature(*selectedIt));
}

QVector<revaplayer::ui::ShortcutBinding> MainWindow::buildShortcutBindings() const
{
    QVector<revaplayer::ui::ShortcutBinding> bindings;
    bindings.reserve(static_cast<int>(std::size(kShortcutDefinitions)));

    for (const auto &definition : kShortcutDefinitions) {
        QAction *action = actionForShortcutId(QString::fromLatin1(definition.id));
        if (action == nullptr) {
            continue;
        }

        const QString overrideSequence = settingsController_ != nullptr
            ? settingsController_->shortcutOverride(QString::fromLatin1(definition.id))
            : QString {};
        const QKeySequence defaultSequence = portableShortcut(definition.defaultSequence);
        const QKeySequence effectiveSequence = overrideSequence.trimmed().isEmpty()
            ? defaultSequence
            : QKeySequence(overrideSequence, QKeySequence::PortableText);

        bindings.push_back(revaplayer::ui::ShortcutBinding {
            QString::fromLatin1(definition.id),
            shortcutCategoryForId(QString::fromLatin1(definition.id)),
            revaplayer::application::translateUiText(QString::fromLatin1(definition.label)),
            defaultSequence,
            effectiveSequence,
        });
    }

    return bindings;
}

QAction *MainWindow::actionForShortcutId(const QString &shortcutId) const
{
    if (shortcutId == QStringLiteral("open_file_alt")) {
        return openFileAction_;
    }
    if (shortcutId == QStringLiteral("open_folder_alt")) {
        return openFolderAction_;
    }
    if (shortcutId == QStringLiteral("open_url_alt")) {
        return openUrlAction_;
    }
    if (shortcutId == QStringLiteral("open_preferences_alt")) {
        return preferencesAction_;
    }
    if (shortcutId == QStringLiteral("show_media_info_alt")) {
        return showMediaInfoAction_;
    }
    if (shortcutId == QStringLiteral("play_pause_alt")) {
        return playPauseAction_;
    }
    if (shortcutId == QStringLiteral("stop_alt")) {
        return stopAction_;
    }
    if (shortcutId == QStringLiteral("take_screenshot_alt")) {
        return takeScreenshotAction_;
    }
    if (shortcutId == QStringLiteral("add_bookmark_alt")) {
        return addBookmarkAction_;
    }
    if (shortcutId == QStringLiteral("speed_down_alt")) {
        return speedDownAction_;
    }
    if (shortcutId == QStringLiteral("speed_up_alt")) {
        return speedUpAction_;
    }
    if (shortcutId == QStringLiteral("speed_reset_alt")) {
        return speedResetAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_down_alt")) {
        return subtitleDelayDownAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_up_alt")) {
        return subtitleDelayUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_reset_alt")) {
        return subtitleDelayResetAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_down_alt")) {
        return audioDelayDownAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_up_alt")) {
        return audioDelayUpAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_reset_alt")) {
        return audioDelayResetAction_;
    }
    if (shortcutId == QStringLiteral("toggle_playlist_alt")) {
        return togglePlaylistAction_;
    }
    if (shortcutId == QStringLiteral("toggle_details_alt")) {
        return toggleDetailsAction_;
    }
    if (shortcutId == QStringLiteral("toggle_fullscreen_alt")) {
        return toggleFullscreenAction_;
    }
    if (shortcutId == QStringLiteral("previous_playlist_alt")) {
        return previousPlaylistAction_;
    }
    if (shortcutId == QStringLiteral("next_playlist_alt")) {
        return nextPlaylistAction_;
    }
    if (shortcutId == QStringLiteral("previous_chapter_alt")) {
        return previousChapterAction_;
    }
    if (shortcutId == QStringLiteral("next_chapter_alt")) {
        return nextChapterAction_;
    }
    if (shortcutId == QStringLiteral("seek_backward_short_alt")) {
        return seekBackwardShortAction_;
    }
    if (shortcutId == QStringLiteral("seek_forward_short_alt")) {
        return seekForwardShortAction_;
    }
    if (shortcutId == QStringLiteral("seek_backward_long_alt")) {
        return seekBackwardLongAction_;
    }
    if (shortcutId == QStringLiteral("seek_forward_long_alt")) {
        return seekForwardLongAction_;
    }
    if (shortcutId == QStringLiteral("volume_down_alt")) {
        return volumeDownAction_;
    }
    if (shortcutId == QStringLiteral("volume_up_alt")) {
        return volumeUpAction_;
    }
    if (shortcutId == QStringLiteral("toggle_mute_alt")) {
        return toggleMuteAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_scale_down_alt")) {
        return subtitleScaleDownAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_scale_up_alt")) {
        return subtitleScaleUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_position_up_alt")) {
        return subtitlePositionUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_position_down_alt")) {
        return subtitlePositionDownAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_out_alt")) {
        return videoZoomOutAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_in_alt")) {
        return videoZoomInAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_reset_alt")) {
        return videoZoomResetAction_;
    }

    if (shortcutId == QStringLiteral("open_file")) {
        return openFileAction_;
    }
    if (shortcutId == QStringLiteral("open_folder")) {
        return openFolderAction_;
    }
    if (shortcutId == QStringLiteral("open_url")) {
        return openUrlAction_;
    }
    if (shortcutId == QStringLiteral("load_subtitle")) {
        return loadSubtitleAction_;
    }
    if (shortcutId == QStringLiteral("toggle_subtitle_visibility")) {
        return toggleSubtitleVisibilityAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_scale_down")) {
        return subtitleScaleDownAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_scale_up")) {
        return subtitleScaleUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_scale_reset")) {
        return subtitleScaleResetAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_position_up")) {
        return subtitlePositionUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_position_down")) {
        return subtitlePositionDownAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_position_reset")) {
        return subtitlePositionResetAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_style_cycle")) {
        return cycleSubtitleAssOverrideAction_;
    }
    if (shortcutId == QStringLiteral("show_media_info")) {
        return showMediaInfoAction_;
    }
    if (shortcutId == QStringLiteral("open_preferences")) {
        return preferencesAction_;
    }
    if (shortcutId == QStringLiteral("close_application")) {
        return closeApplicationAction_;
    }
    if (shortcutId == QStringLiteral("play_pause")) {
        return playPauseAction_;
    }
    if (shortcutId == QStringLiteral("stop")) {
        return stopAction_;
    }
    if (shortcutId == QStringLiteral("take_screenshot")) {
        return takeScreenshotAction_;
    }
    if (shortcutId == QStringLiteral("add_bookmark")) {
        return addBookmarkAction_;
    }
    if (shortcutId == QStringLiteral("favorite_current_media")) {
        return favoriteCurrentMediaAction_;
    }
    if (shortcutId == QStringLiteral("delete_bookmark")) {
        return deleteBookmarkAction_;
    }
    if (shortcutId == QStringLiteral("speed_down")) {
        return speedDownAction_;
    }
    if (shortcutId == QStringLiteral("speed_up")) {
        return speedUpAction_;
    }
    if (shortcutId == QStringLiteral("speed_reset")) {
        return speedResetAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_down")) {
        return subtitleDelayDownAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_up")) {
        return subtitleDelayUpAction_;
    }
    if (shortcutId == QStringLiteral("subtitle_delay_reset")) {
        return subtitleDelayResetAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_down")) {
        return audioDelayDownAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_up")) {
        return audioDelayUpAction_;
    }
    if (shortcutId == QStringLiteral("audio_delay_reset")) {
        return audioDelayResetAction_;
    }
    if (shortcutId == QStringLiteral("set_loop_start")) {
        return setLoopStartAction_;
    }
    if (shortcutId == QStringLiteral("set_loop_end")) {
        return setLoopEndAction_;
    }
    if (shortcutId == QStringLiteral("clear_loop")) {
        return clearLoopAction_;
    }
    if (shortcutId == QStringLiteral("frame_step_backward")) {
        return frameStepBackwardAction_;
    }
    if (shortcutId == QStringLiteral("frame_step_forward")) {
        return frameStepForwardAction_;
    }
    if (shortcutId == QStringLiteral("toggle_always_on_top")) {
        return alwaysOnTopAction_;
    }
    if (shortcutId == QStringLiteral("show_media_information_overlay")) {
        return showMediaInformationOverlayAction_;
    }
    if (shortcutId == QStringLiteral("toggle_playlist")) {
        return togglePlaylistAction_;
    }
    if (shortcutId == QStringLiteral("toggle_details")) {
        return toggleDetailsAction_;
    }
    if (shortcutId == QStringLiteral("toggle_fullscreen")) {
        return toggleFullscreenAction_;
    }
    if (shortcutId == QStringLiteral("previous_playlist")) {
        return previousPlaylistAction_;
    }
    if (shortcutId == QStringLiteral("next_playlist")) {
        return nextPlaylistAction_;
    }
    if (shortcutId == QStringLiteral("previous_chapter")) {
        return previousChapterAction_;
    }
    if (shortcutId == QStringLiteral("next_chapter")) {
        return nextChapterAction_;
    }
    if (shortcutId == QStringLiteral("seek_backward_short")) {
        return seekBackwardShortAction_;
    }
    if (shortcutId == QStringLiteral("seek_forward_short")) {
        return seekForwardShortAction_;
    }
    if (shortcutId == QStringLiteral("seek_backward_long")) {
        return seekBackwardLongAction_;
    }
    if (shortcutId == QStringLiteral("seek_forward_long")) {
        return seekForwardLongAction_;
    }
    if (shortcutId == QStringLiteral("volume_down")) {
        return volumeDownAction_;
    }
    if (shortcutId == QStringLiteral("volume_up")) {
        return volumeUpAction_;
    }
    if (shortcutId == QStringLiteral("toggle_mute")) {
        return toggleMuteAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_out")) {
        return videoZoomOutAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_in")) {
        return videoZoomInAction_;
    }
    if (shortcutId == QStringLiteral("video_zoom_reset")) {
        return videoZoomResetAction_;
    }
    if (shortcutId == QStringLiteral("video_pan_left")) {
        return videoPanLeftAction_;
    }
    if (shortcutId == QStringLiteral("video_pan_right")) {
        return videoPanRightAction_;
    }
    if (shortcutId == QStringLiteral("video_pan_up")) {
        return videoPanUpAction_;
    }
    if (shortcutId == QStringLiteral("video_pan_down")) {
        return videoPanDownAction_;
    }
    if (shortcutId == QStringLiteral("toggle_deinterlace")) {
        return deinterlaceAction_;
    }
    if (shortcutId == QStringLiteral("aspect_default")) {
        return aspectDefaultAction_;
    }
    if (shortcutId == QStringLiteral("aspect_16_9")) {
        return aspect16x9Action_;
    }
    if (shortcutId == QStringLiteral("aspect_4_3")) {
        return aspect4x3Action_;
    }
    if (shortcutId == QStringLiteral("aspect_1_85")) {
        return aspect185Action_;
    }
    if (shortcutId == QStringLiteral("aspect_2_35")) {
        return aspect235Action_;
    }
    if (shortcutId == QStringLiteral("crop_default")) {
        return cropDefaultAction_;
    }
    if (shortcutId == QStringLiteral("crop_16_9")) {
        return crop16x9Action_;
    }
    if (shortcutId == QStringLiteral("crop_1_85")) {
        return crop185Action_;
    }
    if (shortcutId == QStringLiteral("crop_2_35")) {
        return crop235Action_;
    }
    if (shortcutId == QStringLiteral("crop_disable")) {
        return cropDisableAction_;
    }
    if (shortcutId == QStringLiteral("rotate_default")) {
        return rotateDefaultAction_;
    }
    if (shortcutId == QStringLiteral("rotate_90")) {
        return rotate90Action_;
    }
    if (shortcutId == QStringLiteral("rotate_180")) {
        return rotate180Action_;
    }
    if (shortcutId == QStringLiteral("rotate_270")) {
        return rotate270Action_;
    }

    return nullptr;
}

bool MainWindow::historyEnabled() const
{
    return settingsController_ == nullptr || settingsController_->historyEnabled();
}

bool MainWindow::clearHistoryOnExitEnabled() const
{
    return settingsController_ != nullptr && settingsController_->clearHistoryOnExit();
}

bool MainWindow::resumeEnabled() const
{
    return settingsController_ == nullptr || settingsController_->resumeEnabled();
}

int MainWindow::shortSeekStepSeconds() const
{
    return settingsController_ != nullptr ? settingsController_->shortSeekStepSeconds() : 5;
}

int MainWindow::longSeekStepSeconds() const
{
    return settingsController_ != nullptr ? settingsController_->longSeekStepSeconds() : 30;
}

int MainWindow::volumeStep() const
{
    return settingsController_ != nullptr ? settingsController_->volumeStep() : 5;
}

bool MainWindow::mouseWheelVolumeEnabled() const
{
    return settingsController_ == nullptr || settingsController_->mouseWheelVolumeEnabled();
}

int MainWindow::mouseWheelVolumeStep() const
{
    return settingsController_ != nullptr ? settingsController_->mouseWheelVolumeStep() : 4;
}

QString MainWindow::mouseWheelActionId() const
{
    return settingsController_ != nullptr ? settingsController_->mouseWheelAction() : QStringLiteral("volume");
}

int MainWindow::mouseWheelSeekStepSeconds() const
{
    return settingsController_ != nullptr ? settingsController_->mouseWheelSeekStepSeconds() : 10;
}

bool MainWindow::mouseNavigationSeekEnabled() const
{
    return settingsController_ == nullptr || settingsController_->mouseNavigationSeekEnabled();
}

int MainWindow::mouseNavigationSeekStepSeconds() const
{
    return settingsController_ != nullptr ? settingsController_->mouseNavigationSeekStepSeconds() : 5;
}

QString MainWindow::mouseSideButtonsActionId() const
{
    return settingsController_ != nullptr ? settingsController_->mouseSideButtonsAction() : QStringLiteral("seek_short");
}

QString MainWindow::clickActionId() const
{
    return settingsController_ != nullptr ? settingsController_->clickAction() : QStringLiteral("none");
}

QString MainWindow::doubleClickActionId() const
{
    return settingsController_ != nullptr ? settingsController_->doubleClickAction() : QStringLiteral("play_pause");
}

QString MainWindow::middleClickActionId() const
{
    return settingsController_ != nullptr ? settingsController_->middleClickAction() : QStringLiteral("mute");
}

bool MainWindow::actionFeedbackOverlayEnabled() const
{
    return settingsController_ == nullptr || settingsController_->actionFeedbackOverlayEnabled();
}

bool MainWindow::expressiveControlLabelsEnabled() const
{
    return settingsController_ == nullptr || settingsController_->expressiveControlLabelsEnabled();
}

bool MainWindow::fullscreenAutoHideEnabled() const
{
    return settingsController_ == nullptr || settingsController_->fullscreenAutoHideEnabled();
}

int MainWindow::fullscreenRevealMargin() const
{
    return settingsController_ != nullptr ? settingsController_->fullscreenRevealMargin() : 72;
}

bool MainWindow::fullscreenEdgePanelRevealEnabled() const
{
    return settingsController_ == nullptr || settingsController_->fullscreenEdgePanelRevealEnabled();
}

bool MainWindow::fullscreenSideSelectorEnabled() const
{
    return settingsController_ == nullptr || settingsController_->fullscreenSideSelectorEnabled();
}

bool MainWindow::windowedEdgePanelRevealEnabled() const
{
    return customSettingFlag(settingsController_, kPointerRightEdgeWindowedEnabledSetting, false);
}

QString MainWindow::defaultSidePanelId() const
{
    return settingsController_ != nullptr ? settingsController_->defaultSidePanel() : QStringLiteral("last_opened");
}

bool MainWindow::doubleClickFullscreenEnabled() const
{
    return settingsController_ == nullptr || settingsController_->doubleClickFullscreenEnabled();
}

QString MainWindow::pointerRightEdgeActionId(const bool fullscreenContext) const
{
    return customSettingValue(
        settingsController_,
        fullscreenContext ? kPointerRightEdgeActionFullscreenSetting : kPointerRightEdgeActionWindowedSetting,
        QStringLiteral("default"));
}

QString MainWindow::pointerRightEdgeLeaveActionId() const
{
    return customSettingValue(
        settingsController_,
        kPointerRightEdgeLeaveActionSetting,
        QStringLiteral("hide_panel"));
}

int MainWindow::pointerRightEdgeMargin() const
{
    return customSettingInt(settingsController_, kPointerRightEdgeMarginSetting, 72, 8, 220);
}

int MainWindow::pointerLeaveDelayMs() const
{
    return customSettingInt(settingsController_, kPointerLeaveDelaySetting, 900, 0, 4000);
}

bool MainWindow::pointerKeepControlsVisibleWhilePanelOpen() const
{
    return customSettingFlag(settingsController_, kPointerKeepControlsVisibleSetting, true);
}

QString MainWindow::mouseZoneActionId(const QString &zoneId) const
{
    const QString settingKey = mouseZoneSettingKey(zoneId);
    if (settingKey.isEmpty()) {
        return QStringLiteral("none");
    }
    return normalizeMouseZoneAction(
        customSettingValue(
            settingsController_,
            settingKey.toUtf8().constData(),
            defaultMouseZoneAction(zoneId)));
}

void MainWindow::applyMouseZoneAction(const QString &zoneId)
{
    const QString actionId = mouseZoneActionId(zoneId);
    if (actionId == QStringLiteral("none")) {
        return;
    }

    const bool windowedRightPanelRevealBlocked = !isFullScreen()
        && !windowedEdgePanelRevealEnabled()
        && zoneId == QStringLiteral("right")
        && (actionId == QStringLiteral("playlist") || actionId == QStringLiteral("details"));
    if (windowedRightPanelRevealBlocked) {
        return;
    }

    if (actionId == QStringLiteral("playlist")) {
        setSidePanelVisible(SidePanel::Playlist, true, true);
        return;
    }
    if (actionId == QStringLiteral("details")) {
        setSidePanelVisible(SidePanel::Details, true, true);
        return;
    }
    if (actionId == QStringLiteral("controls")) {
        if (isFullScreen() && fullscreenAutoHideEnabled()) {
            setFullscreenChromeVisible(false, true);
            if (fullscreenChromeTimer_ != nullptr) {
                fullscreenChromeTimer_->stop();
            }
        }
        return;
    }
    if (actionId == QStringLiteral("top_bar")) {
        if (isFullScreen() && fullscreenAutoHideEnabled()) {
            const bool keepBottom = controlBar_ != nullptr && controlBar_->isVisible();
            setFullscreenChromeVisible(true, keepBottom);
            if (fullscreenChromeTimer_ != nullptr) {
                fullscreenChromeTimer_->stop();
            }
        }
        return;
    }
    if (actionId == QStringLiteral("dashboard") && !mediaLoaded_) {
        updateHomeDashboardVisibility();
        if (homeDashboard_ != nullptr && homeDashboard_->isVisible()) {
            homeDashboard_->raise();
        }
    }
}

void MainWindow::applyConfiguredRightEdgeAction(const QString &actionId)
{
    const QString normalized = actionId.trimmed().toLower();
    const bool fullscreenEdgeReveal = isFullScreen();
    const auto markEdgeRevealOpen = [this, fullscreenEdgeReveal]() {
        if (!fullscreenEdgeReveal) {
            return;
        }
        sidePanelEdgeRevealActive_ = isSidePanelVisible(SidePanel::Playlist) || isSidePanelVisible(SidePanel::Details);
        if (sidePanelEdgeRevealActive_) {
            scheduleManagedPointerLeaveCheck();
        }
    };
    if (normalized == QStringLiteral("playlist")) {
        setSidePanelVisible(SidePanel::Playlist, true, true);
        markEdgeRevealOpen();
        return;
    }
    if (normalized == QStringLiteral("details")) {
        setSidePanelVisible(SidePanel::Details, true, true);
        markEdgeRevealOpen();
        return;
    }
    if (normalized == QStringLiteral("default")) {
        const QString preferredPanelId = defaultSidePanelId();
        const SidePanel targetPanel = preferredPanelId == QStringLiteral("details")
            ? SidePanel::Details
            : (preferredPanelId == QStringLiteral("playlist")
                   ? SidePanel::Playlist
                   : activeSidePanel_);
        setSidePanelVisible(targetPanel, true, true);
        markEdgeRevealOpen();
    }
}

void MainWindow::applyConfiguredRightEdgeLeaveAction(const QString &actionId)
{
    const QString normalized = actionId.trimmed().toLower();
    if (normalized == QStringLiteral("hide_panel") || normalized == QStringLiteral("hide_all")) {
        setSidePanelVisible(SidePanel::Playlist, false, false);
        setSidePanelVisible(SidePanel::Details, false, false);
        sidePanelEdgeRevealActive_ = false;
    }

    if (normalized == QStringLiteral("hide_all") && isFullScreen()) {
        setFullscreenChromeVisible(false, false);
    }
}

int MainWindow::thumbnailPopupWidth() const
{
    return settingsController_ != nullptr ? settingsController_->thumbnailPopupWidth() : 0;
}

int MainWindow::thumbnailPopupVerticalOffset() const
{
    return settingsController_ != nullptr ? settingsController_->thumbnailPopupVerticalOffset() : 18;
}

int MainWindow::thumbnailPopupScreenPadding() const
{
    return settingsController_ != nullptr ? settingsController_->thumbnailPopupScreenPadding() : 8;
}

int MainWindow::historyLimit() const
{
    return settingsController_ != nullptr ? settingsController_->historyLimit() : 120;
}

bool MainWindow::autoLoadSiblingMediaEnabled() const
{
    return settingsController_ == nullptr || settingsController_->autoLoadSiblingMediaEnabled();
}

bool MainWindow::showPlaylistPanelOnFolderLoad() const
{
    return settingsController_ == nullptr || settingsController_->showPlaylistPanelOnFolderLoad();
}

bool MainWindow::naturalSortFolderPlaylistEnabled() const
{
    return settingsController_ == nullptr || settingsController_->naturalSortFolderPlaylistEnabled();
}

bool MainWindow::playlistShowFullPathsEnabled() const
{
    return settingsController_ != nullptr && settingsController_->playlistShowFullPaths();
}

bool MainWindow::playlistShowIndexPrefixesEnabled() const
{
    return settingsController_ == nullptr || settingsController_->playlistShowIndexPrefixes();
}

bool MainWindow::playlistAutoFollowEnabled() const
{
    return settingsController_ == nullptr || settingsController_->playlistAutoFollowCurrent();
}

bool MainWindow::rotateFolderPlaylistToCurrentEnabled() const
{
    return settingsController_ == nullptr || settingsController_->rotateFolderPlaylistToCurrent();
}

double MainWindow::videoZoomStepSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoZoomStep() : 0.20;
}

double MainWindow::videoMinimumZoomSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoMinimumZoom() : 1.0;
}

double MainWindow::videoMaximumZoomSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoMaximumZoom() : 6.0;
}

QString MainWindow::videoZoomDefaultBehaviorSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoZoomDefaultBehavior() : QStringLiteral("fit_to_frame");
}

bool MainWindow::videoZoomResetOnFileChangeEnabled() const
{
    return settingsController_ == nullptr || settingsController_->videoZoomResetOnFileChange();
}

QString MainWindow::videoZoomRememberModeSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoZoomRememberMode() : QStringLiteral("off");
}

double MainWindow::videoPanSensitivitySetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoPanSensitivity() : 1.0;
}

bool MainWindow::videoZoomConstrainPanningEnabled() const
{
    return settingsController_ == nullptr || settingsController_->videoZoomConstrainPanning();
}

QString MainWindow::videoZoomWheelBehaviorSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoZoomWheelBehavior() : QStringLiteral("global");
}

QString MainWindow::videoZoomFullscreenBehaviorSetting() const
{
    return settingsController_ != nullptr ? settingsController_->videoZoomFullscreenBehavior() : QStringLiteral("keep");
}

QString MainWindow::selectedThemeId() const
{
    return settingsController_ != nullptr ? settingsController_->uiTheme() : QStringLiteral("gray");
}

QString MainWindow::startupWindowMode() const
{
    return settingsController_ != nullptr ? settingsController_->startupWindowMode() : QStringLiteral("normal");
}

QString MainWindow::startupCanvasStyleId() const
{
    return normalizedStartupCanvasStyleId(customSettingValue(
        settingsController_,
        kStartupCanvasStyleSetting,
        QStringLiteral("theme")));
}

QString MainWindow::configuredRepeatMode() const
{
    return settingsController_ != nullptr ? settingsController_->defaultRepeatMode() : QStringLiteral("off");
}

double MainWindow::configuredStartupPlaybackSpeed() const
{
    return settingsController_ != nullptr ? settingsController_->startupPlaybackSpeed() : 1.0;
}

bool MainWindow::sessionWidePlaybackSpeedEnabled() const
{
    return customSettingFlag(settingsController_, kSessionWidePlaybackSpeedSetting, false);
}

bool MainWindow::subtitleAutoSelectEnabled() const
{
    return settingsController_ == nullptr || settingsController_->subtitleAutoSelectEnabled();
}

bool MainWindow::subtitlePreferExternal() const
{
    return settingsController_ == nullptr || settingsController_->subtitlePreferExternal();
}

bool MainWindow::subtitleAutoLoadLocalMatchesEnabled() const
{
    return subtitleAutoLoadModeSetting() != QStringLiteral("manual_only");
}

QString MainWindow::subtitleAutoLoadModeSetting() const
{
    if (settingsController_ == nullptr) {
        return QStringLiteral("same_name_only");
    }

    const QString configuredMode = revaplayer::application::normalizeSubtitleAutoLoadMode(
        settingsController_->customValue(
            QString::fromLatin1(revaplayer::application::kSubtitleAutoLoadModeSetting),
            settingsController_->subtitleAutoLoadLocalMatches()
                ? QStringLiteral("same_name_only")
                : QStringLiteral("manual_only")));
    if (!settingsController_->subtitleAutoLoadLocalMatches()) {
        return QStringLiteral("manual_only");
    }
    return configuredMode;
}

QSet<QString> MainWindow::subtitleAutoLoadExtensionsSetting() const
{
    const QString normalized = revaplayer::application::normalizeSubtitleAutoExtensions(
        settingsController_ != nullptr
            ? settingsController_->customValue(
                QString::fromLatin1(revaplayer::application::kSubtitleAutoExtensionsSetting),
                revaplayer::application::defaultSubtitleAutoExtensions())
            : revaplayer::application::defaultSubtitleAutoExtensions());

    QSet<QString> extensions;
    for (QString token : normalized.split(QChar(','), Qt::SkipEmptyParts)) {
        token = token.trimmed().toLower();
        while (token.startsWith(QChar('.'))) {
            token.remove(0, 1);
        }
        if (!token.isEmpty()) {
            extensions.insert(token);
        }
    }
    return extensions;
}

double MainWindow::subtitleSyncSmallStep() const
{
    return settingsController_ != nullptr ? settingsController_->subtitleSyncSmallStep() : 0.25;
}

QString MainWindow::subtitleDownloadCommandTemplate() const
{
    return settingsController_ != nullptr ? settingsController_->subtitleDownloadCommand() : QString {};
}

int MainWindow::sceneBrowserStepSeconds() const
{
    return settingsController_ != nullptr ? settingsController_->sceneBrowserStepSeconds() : kDefaultSceneStepSeconds;
}

int MainWindow::sceneBrowserMaxItems() const
{
    return settingsController_ != nullptr ? settingsController_->sceneBrowserMaxItems() : kMaxSceneItems;
}

QString MainWindow::uiDensityId() const
{
    return customSettingValue(settingsController_, kUiDensitySetting, QStringLiteral("normal")).trimmed().toLower();
}

QString MainWindow::uiAccentId() const
{
    return customSettingValue(settingsController_, kUiAccentSetting, QStringLiteral("blue")).trimmed().toLower();
}

QString MainWindow::uiModeId() const
{
    return QStringLiteral("simple");
}

bool MainWindow::dashboardEnabled() const
{
    return customSettingFlag(settingsController_, kDashboardEnabledSetting, true);
}

bool MainWindow::dashboardShowWhenIdle() const
{
    return customSettingFlag(settingsController_, kDashboardShowOnIdleSetting, true);
}

bool MainWindow::dashboardSectionEnabled(const char *settingKey) const
{
    return customSettingFlag(settingsController_, settingKey, true);
}

bool MainWindow::progressTrackingModeEnabled() const
{
    return customSettingFlag(settingsController_, kProgressTrackingModeSetting, true);
}

int MainWindow::progressCompletionThreshold() const
{
    return customSettingInt(settingsController_, kProgressCompletionThresholdSetting, 92, 60, 100);
}

bool MainWindow::progressBadgesEnabled() const
{
    return customSettingFlag(settingsController_, kProgressShowBadgesSetting, true);
}

QString MainWindow::playlistViewModeId() const
{
    return normalizePlaylistViewMode(customSettingValue(settingsController_, kPlaylistViewModeSetting, QStringLiteral("list")));
}

QString MainWindow::playlistViewDensityId() const
{
    return normalizePlaylistViewDensity(customSettingValue(settingsController_, kPlaylistViewDensitySetting, QStringLiteral("normal")));
}

QString MainWindow::playlistLayoutFitId() const
{
    return normalizePlaylistLayoutFit(customSettingValue(settingsController_, kPlaylistLayoutFitSetting, QStringLiteral("balanced")));
}

QString MainWindow::playlistCardZoomId() const
{
    const QString stored = settingsController_ != nullptr
        ? settingsController_->customValue(QString::fromLatin1(kPlaylistCardZoomSetting)).trimmed()
        : QString {};
    if (!stored.isEmpty()) {
        return normalizePlaylistCardZoom(stored);
    }

    return QStringLiteral("100");
}

QString MainWindow::playlistThumbnailShapeId() const
{
    const QString stored = settingsController_ != nullptr
        ? settingsController_->customValue(QString::fromLatin1(kPlaylistThumbnailShapeSetting)).trimmed()
        : QString {};
    if (!stored.isEmpty()) {
        return normalizePlaylistThumbnailShape(stored);
    }

    return QStringLiteral("rectangle");
}

QStringList MainWindow::playlistVisibleColumns() const
{
    return normalizePlaylistColumns(
        customSettingValue(
            settingsController_,
            kPlaylistViewColumnsSetting,
            defaultPlaylistVisibleColumns().join(QChar(',')))
            .split(QChar(','), Qt::SkipEmptyParts));
}

QStringList MainWindow::playlistVisibleDetailsTabIds() const
{
    QStringList visibleTabs = customSettingValue(settingsController_, kPlaylistDetailsVisibleTabsSetting)
                                  .split(QChar(','), Qt::SkipEmptyParts);
    for (QString &tabId : visibleTabs) {
        tabId = tabId.trimmed();
    }
    visibleTabs.removeAll(QString {});
    visibleTabs.removeDuplicates();
    if (!visibleTabs.isEmpty()) {
        return visibleTabs;
    }

    if (settingsController_ == nullptr) {
        return {};
    }

    const QString activePresetKey = settingsController_->customValue(QString::fromLatin1(kPlaylistViewActiveSetting)).trimmed();
    if (activePresetKey.isEmpty()) {
        return {};
    }

    for (const auto &preset : loadPlaylistViewPresets(settingsController_)) {
        if (preset.key == activePresetKey) {
            return preset.visibleDetailsTabs.isEmpty()
                ? defaultVisibleDetailsTabIds()
                : preset.visibleDetailsTabs;
        }
    }
    return defaultVisibleDetailsTabIds();
}

QString MainWindow::playlistPreferredDetailsTabId() const
{
    const QString stored = customSettingValue(settingsController_, kPlaylistDetailsCurrentTabSetting).trimmed();
    if (!stored.isEmpty()) {
        return stored;
    }

    if (settingsController_ == nullptr) {
        return {};
    }

    const QString activePresetKey = settingsController_->customValue(QString::fromLatin1(kPlaylistViewActiveSetting)).trimmed();
    if (activePresetKey.isEmpty()) {
        return {};
    }

    for (const auto &preset : loadPlaylistViewPresets(settingsController_)) {
        if (preset.key == activePresetKey) {
            return preset.currentDetailsTabId;
        }
    }
    return {};
}

QString MainWindow::playlistSortModeId() const
{
    return normalizePlaylistSortMode(customSettingValue(settingsController_, kPlaylistSortModeSetting, QStringLiteral("natural")));
}

bool MainWindow::adaptiveUiEnabled() const
{
    return customSettingFlag(settingsController_, kUiAdaptiveEnabledSetting, true);
}

int MainWindow::adaptiveUiBreakpoint() const
{
    return customSettingInt(settingsController_, kUiAdaptiveBreakpointSetting, 1220, 760, 2200);
}

int MainWindow::uiRadiusPx() const
{
    return customSettingInt(settingsController_, kUiRadiusSetting, 10, 4, 28);
}

int MainWindow::uiSpacingPx() const
{
    return customSettingInt(settingsController_, kUiSpacingSetting, 8, 4, 20);
}

int MainWindow::uiFontScalePercent() const
{
    return customSettingInt(settingsController_, kUiFontScaleSetting, 100, 85, 140);
}

int MainWindow::uiFontWeightValue() const
{
    return customSettingInt(settingsController_, kUiFontWeightSetting, 500, 350, 800);
}

double MainWindow::uiLetterSpacingValue() const
{
    return customSettingDouble(settingsController_, kUiLetterSpacingSetting, 0.0, -0.4, 2.0);
}

int MainWindow::uiBorderContrastPercent() const
{
    return customSettingInt(settingsController_, kUiBorderContrastSetting, 100, 65, 160);
}

int MainWindow::uiShadowStrengthPercent() const
{
    return customSettingInt(settingsController_, kUiShadowStrengthSetting, 60, 40, 160);
}

int MainWindow::uiBlurStrengthPercent() const
{
    return customSettingInt(settingsController_, kUiBlurStrengthSetting, 0, 0, 100);
}

int MainWindow::uiAnimationSpeedPercent() const
{
    return customSettingInt(settingsController_, kUiAnimationSpeedSetting, 100, 40, 220);
}

QString MainWindow::uiAnimationEasingId() const
{
    const QString easing = customSettingValue(settingsController_, kUiAnimationEasingSetting, QStringLiteral("cubic")).trimmed().toLower();
    static const QSet<QString> allowed {
        QStringLiteral("cubic"),
        QStringLiteral("quart"),
        QStringLiteral("expo"),
        QStringLiteral("sine"),
    };
    return allowed.contains(easing) ? easing : QStringLiteral("cubic");
}

int MainWindow::uiOverlayOpacityPercent() const
{
    return customSettingInt(settingsController_, kUiOverlayOpacitySetting, 82, 55, 98);
}

void MainWindow::refreshPlaylistProgressIndicators()
{
    if (playlistProgressRow_ == nullptr
        && playlistProgressTextLabel_ == nullptr
        && playlistAggregateProgressBar_ == nullptr
        && playlistTotalDurationLabel_ == nullptr) {
        return;
    }

    const int playlistItems = cachedPlaylistItemCount_;
    const int completedItems = cachedPlaylistCompletedItemCount_;
    const int totalProgress = cachedPlaylistTotalProgress_;
    const double totalDuration = cachedPlaylistTotalDurationSeconds_;
    const double watchedDuration = cachedPlaylistWatchedDurationSeconds_;
    const int knownDurations = cachedPlaylistKnownDurationCount_;

    const int playlistProgress = totalDuration > 0.0
        ? std::clamp(static_cast<int>(std::round((watchedDuration / totalDuration) * 100.0)), 0, 100)
        : (playlistItems > 0 ? totalProgress / playlistItems : 0);
    QString progressToolTip = uiText("Playlist • %1 / %2 completed • %3%")
        .arg(completedItems)
        .arg(playlistItems)
        .arg(playlistProgress);
    if (knownDurations > 0) {
        progressToolTip += QStringLiteral("\n")
            + uiText("Watched: %1 / %2").arg(formatPlaybackTime(watchedDuration), formatPlaybackTime(totalDuration));
    }
    const QString durationText = knownDurations > 0
        ? uiText("Watched %1 • Total %2").arg(formatPlaybackTime(watchedDuration), formatPlaybackTime(totalDuration))
        : uiText("Duration: %1").arg(uiText("Unknown"));

    if (playlistProgressRow_ != nullptr) {
        playlistProgressRow_->setVisible(playlistItems > 0);
    }
    if (playlistProgressTextLabel_ != nullptr) {
        playlistProgressTextLabel_->setText(
            uiText("Completed %1 / %2 • Watch %3%")
                .arg(completedItems)
                .arg(playlistItems)
                .arg(playlistProgress));
        playlistProgressTextLabel_->setToolTip(progressToolTip);
    }
    if (playlistAggregateProgressBar_ != nullptr) {
        playlistAggregateProgressBar_->setValue(playlistProgress);
        playlistAggregateProgressBar_->setToolTip(progressToolTip);
    }
    if (playlistTotalDurationLabel_ != nullptr) {
        playlistTotalDurationLabel_->setText(durationText);
        playlistTotalDurationLabel_->setToolTip(durationText);
    }
}

void MainWindow::refreshPlaylistPlaybackProgress()
{
    if (!mediaLoaded_ || playlistController_ == nullptr || currentMediaSource_.trimmed().isEmpty()) {
        return;
    }

    QAbstractItemModel *model = playlistController_->model();
    if (model == nullptr || model->rowCount() <= 0) {
        return;
    }

    const QString source = currentMediaSource_.trimmed();
    const double safeDuration = std::max(0.0, currentDurationSeconds_);
    const double safePosition = std::max(0.0, currentPositionSeconds_);
    const int completionThreshold = progressCompletionThreshold();
    const int watchedPercent = safeDuration > 0.0
        ? std::clamp(static_cast<int>(std::round((safePosition / safeDuration) * 100.0)), 0, 100)
        : -1;
    const QStringList visibleColumns = playlistVisibleColumns();
    bool changed = false;

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (!index.isValid()
            || index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed() != source) {
            continue;
        }

        const int previousProgress = index.data(revaplayer::application::PlaylistRoles::ProgressRole).toInt();
        const bool previousCompleted = index.data(revaplayer::application::PlaylistRoles::CompletedRole).toBool();
        const double previousDuration = std::max(0.0, index.data(revaplayer::application::PlaylistRoles::DurationSecondsRole).toDouble());
        const double previousPosition = std::max(0.0, index.data(revaplayer::application::PlaylistRoles::LastPositionSecondsRole).toDouble());
        const PlaylistProgressContribution before = playlistProgressContributionFor(
            previousProgress,
            previousCompleted,
            previousDuration,
            previousPosition);

        const double nextDuration = safeDuration > 0.0 ? safeDuration : previousDuration;
        const double nextPosition = safePosition;
        const int nextProgress = watchedPercent >= 0 ? watchedPercent : previousProgress;
        const bool nextCompleted = nextProgress >= completionThreshold;
        const PlaylistProgressContribution after = playlistProgressContributionFor(
            nextProgress,
            nextCompleted,
            nextDuration,
            nextPosition);

        cachedPlaylistTotalProgress_ = std::max(
            0,
            cachedPlaylistTotalProgress_ + after.itemProgress - before.itemProgress);
        const bool beforeCountsCompleted = before.itemProgress >= completionThreshold;
        const bool afterCountsCompleted = after.itemProgress >= completionThreshold;
        if (beforeCountsCompleted != afterCountsCompleted) {
            cachedPlaylistCompletedItemCount_ = std::max(
                0,
                cachedPlaylistCompletedItemCount_ + (afterCountsCompleted ? 1 : -1));
        }
        if (before.knownDuration != after.knownDuration) {
            cachedPlaylistKnownDurationCount_ = std::max(
                0,
                cachedPlaylistKnownDurationCount_ + (after.knownDuration ? 1 : -1));
        }
        cachedPlaylistTotalDurationSeconds_ = std::max(
            0.0,
            cachedPlaylistTotalDurationSeconds_ + after.durationSeconds - before.durationSeconds);
        cachedPlaylistWatchedDurationSeconds_ = std::max(
            0.0,
            cachedPlaylistWatchedDurationSeconds_ + after.watchedSeconds - before.watchedSeconds);

        model->setData(index, nextProgress, revaplayer::application::PlaylistRoles::ProgressRole);
        model->setData(index, nextCompleted, revaplayer::application::PlaylistRoles::CompletedRole);
        model->setData(index, nextDuration, revaplayer::application::PlaylistRoles::DurationSecondsRole);
        model->setData(index, nextPosition, revaplayer::application::PlaylistRoles::LastPositionSecondsRole);
        model->setData(
            index,
            playlistBadgesWithPlaybackProgress(
                index.data(revaplayer::application::PlaylistRoles::BadgeListRole).toStringList(),
                nextProgress,
                nextCompleted),
            revaplayer::application::PlaylistRoles::BadgeListRole);
        model->setData(
            index,
            playlistSecondaryBadgesWithPlaybackProgress(
                index.data(revaplayer::application::PlaylistRoles::SecondaryBadgeListRole).toStringList(),
                visibleColumns,
                nextProgress,
                nextPosition),
            revaplayer::application::PlaylistRoles::SecondaryBadgeListRole);
        changed = true;
    }

    if (changed) {
        refreshPlaylistProgressIndicators();
    }
}

void MainWindow::clearFileSystemCache()
{
    mediaFileSystemCache_.clear();
    mediaScanCacheLookupCompleted_.clear();
}

void MainWindow::refreshPlaylistPresentationData()
{
    if (playlistController_ == nullptr) {
        return;
    }

    QHash<QString, QString> savedListBadgeByPath;
    for (const auto &course : loadPinnedCourses(settingsController_)) {
        const QString absolutePath = cleanAbsolutePathWithoutFilesystemLookup(course.path);
        if (absolutePath.isEmpty()) {
            continue;
        }
        savedListBadgeByPath.insert(
            absolutePath,
            course.category.trimmed().isEmpty() ? uiText("Saved") : course.category.trimmed());
    }

    QHash<QString, revaplayer::application::PlaylistPresentationData> presentationData;
    const auto entries = playlistController_->entries();
    presentationData.reserve(entries.size());
    const int completionThreshold = progressCompletionThreshold();
    const QStringList visibleColumns = playlistVisibleColumns();
    const bool badgesEnabled = progressBadgesEnabled();
    const bool canScanMetadata = metadataScanService_ != nullptr && metadataScanService_->isInitialized();
    const QSet<QString> autoScanTargets = playlistMetadataAutoScanTargets();
    int cachedPlaylistItems = 0;
    int cachedCompletedItems = 0;
    int cachedTotalProgress = 0;
    int cachedKnownDurations = 0;
    double cachedTotalDuration = 0.0;
    double cachedWatchedDuration = 0.0;
    const bool hadPendingBeforeRefresh = !pendingMediaScanSources_.isEmpty();
    QStringList sourcesToScan;
    sourcesToScan.reserve(entries.size());

    for (const auto &entry : entries) {
        revaplayer::application::PlaylistPresentationData data;
        const QString source = entry.source.trimmed();
        data.displayTitle = entry.title.trimmed();
        const bool folderEntry = isFolderBrowserSource(source);
        const bool backEntry = isFolderBrowserBackSource(source);
        const QString scanKey = mediaScanSourceKey(source);
        const QString localPath = localMediaPathForSource(source);

        if (folderEntry || backEntry) {
            const QString folderPath = folderBrowserPathFromSource(source);
            data.displayTitle = data.displayTitle.isEmpty() ? folderDisplayName(folderPath) : data.displayTitle;
            data.mediaKind = backEntry ? uiText("Back") : uiText("Folder");
            data.secondaryText = QDir::toNativeSeparators(folderPath);
            data.secondaryBadges = QStringList {data.mediaKind};
            presentationData.insert(source, data);
            continue;
        }

        if (!localPath.isEmpty() && !mediaFileSystemCache_.contains(source)) {
            const QFileInfo fileInfo(localPath);
            auto &fsMeta = mediaFileSystemCache_[source];
            fsMeta.exists = fileInfo.exists();
            fsMeta.isFile = fileInfo.isFile();
            fsMeta.absoluteFilePath = fileInfo.absoluteFilePath();
            fsMeta.dirPath = fileInfo.dir().absolutePath();
            fsMeta.completeBaseName = fileInfo.completeBaseName();
            fsMeta.suffix = fileInfo.suffix();
            fsMeta.size = fsMeta.exists && fsMeta.isFile ? fileInfo.size() : -1;
            fsMeta.lastModifiedMs = fsMeta.exists && fsMeta.isFile ? fileInfo.lastModified().toMSecsSinceEpoch() : -1;
            fsMeta.hasSubtitleSibling = false;

            if (badgesEnabled && fsMeta.exists && fsMeta.isFile) {
                const QString baseName = fsMeta.completeBaseName;
                const QFileInfoList siblings = QDir(fsMeta.dirPath).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
                fsMeta.hasSubtitleSibling = std::any_of(siblings.cbegin(), siblings.cend(), [&fsMeta, &baseName](const QFileInfo &candidate) {
                    return candidate.absoluteFilePath() != fsMeta.absoluteFilePath
                        && candidate.completeBaseName() == baseName
                        && isSupportedSubtitleFile(candidate);
                });
            }
        }
        const auto fsMeta = mediaFileSystemCache_.value(source);

        const MediaMetadata metadata = loadMediaMetadata(settingsController_, source);
        std::optional<revaplayer::services::media::MediaScanResult> scanResult;

        if (!scanKey.isEmpty()) {
            const auto scanIt = mediaScanCache_.constFind(scanKey);
            if (scanIt != mediaScanCache_.constEnd()) {
                scanResult = scanIt.value();
            } else if (!mediaScanCacheLookupCompleted_.contains(scanKey)) {
                mediaScanCacheLookupCompleted_.insert(scanKey);
                if (const auto cached = loadCachedMediaScanResult(settingsController_, scanKey); cached.has_value()) {
                    mediaScanCache_.insert(scanKey, *cached);
                    scanResult = cached;
                }
            }
        }

        data.tags = metadata.tags;
        data.notesPreview = metadata.notes.left(140);
        data.difficulty = metadata.difficulty;

        auto historyIt = historyEntriesBySource_.constFind(source);
        if (historyIt == historyEntriesBySource_.constEnd()) {
            for (auto candidate = historyEntriesBySource_.cbegin(); candidate != historyEntriesBySource_.cend(); ++candidate) {
                if (sourcesReferToSameMedia(candidate.key(), source)) {
                    historyIt = candidate;
                    break;
                }
            }
        }
        if (historyIt != historyEntriesBySource_.constEnd()) {
            const auto &historyEntry = historyIt.value();
            data.durationSeconds = historyEntry.durationSeconds;
            data.lastPositionSeconds = historyEntry.positionSeconds;
            if (historyEntry.durationSeconds > 0.0) {
                data.watchedPercent = std::clamp(
                    static_cast<int>(std::round((historyEntry.positionSeconds / historyEntry.durationSeconds) * 100.0)),
                    0,
                    100);
            } else if (historyEntry.completed) {
                data.watchedPercent = 100;
            }
            data.completed = historyEntry.completed || data.watchedPercent >= completionThreshold;
            if (data.completed) {
                data.watchedPercent = 100;
            }
        } else if (historyController_ != nullptr && historyController_->isReady()) {
            if (!resumeStateLookupCompleted_.contains(source)) {
                resumeStateLookupCompleted_.insert(source);
                if (const auto resume = historyController_->resumeStateFor(source); resume.has_value()) {
                    resumeStateCache_.insert(source, *resume);
                }
            }

            const auto resumeIt = resumeStateCache_.constFind(source);
            if (resumeIt != resumeStateCache_.constEnd()) {
                const auto &resume = resumeIt.value();
                data.durationSeconds = resume.durationSeconds;
                data.lastPositionSeconds = resume.positionSeconds;
                if (resume.durationSeconds > 0.0) {
                    data.watchedPercent = std::clamp(
                        static_cast<int>(std::round((resume.positionSeconds / resume.durationSeconds) * 100.0)),
                        0,
                        100);
                }
                data.completed = data.watchedPercent >= completionThreshold;
                if (data.completed) {
                    data.watchedPercent = 100;
                }
            }
        }

        if (source == currentMediaSource_) {
            if (currentDurationSeconds_ > 0.0) {
                data.durationSeconds = currentDurationSeconds_;
            }
            if (currentPositionSeconds_ > 0.0) {
                data.lastPositionSeconds = currentPositionSeconds_;
                if (data.completed) {
                    data.watchedPercent = 100;
                } else if (data.durationSeconds > 0.0) {
                    data.watchedPercent = std::clamp(
                        static_cast<int>(std::round((currentPositionSeconds_ / data.durationSeconds) * 100.0)),
                        0,
                        100);
                    data.completed = data.watchedPercent >= completionThreshold;
                }
            }
        }

        if (scanResult.has_value()) {
            if (data.durationSeconds <= 0.0 && scanResult->durationSeconds > 0.0) {
                data.durationSeconds = scanResult->durationSeconds;
            }
            const bool audioOnly = scanResult->hasAudioTrack && !scanResult->hasVideoTrack;
            if (audioOnly && fsMeta.exists && fsMeta.isFile) {
                const QString cleanFileLabel = fsMeta.completeBaseName.trimmed();
                if (!cleanFileLabel.isEmpty()) {
                    data.displayTitle = cleanFileLabel;
                }
            }
            if (!audioOnly && !scanResult->mediaTitle.trimmed().isEmpty() && data.displayTitle.isEmpty()) {
                data.displayTitle = scanResult->mediaTitle.trimmed();
            }
            data.width = scanResult->width;
            data.height = scanResult->height;
            data.fileFormat = scanResult->fileFormat.trimmed();
            data.mediaKind = mediaKindLabelForScan(*scanResult);
        } else {
            data.metadataPending = canScanMetadata && pendingMediaScanSources_.contains(scanKey);
            const bool shouldAutoScan = playlistMetadataFullScanRequested_ || autoScanTargets.contains(scanKey);
            if (canScanMetadata
                && shouldAutoScan
                && !scanKey.isEmpty()
                && fsMeta.exists
                && fsMeta.isFile
                && !pendingMediaScanSources_.contains(scanKey)
                && !failedMediaScanSources_.contains(scanKey)) {
                sourcesToScan.push_back(scanKey);
                pendingMediaScanSources_.insert(scanKey);
                data.metadataPending = true;
            }
        }

        data.favorite = badgesEnabled
            && settingsController_ != nullptr
            && !settingsController_->customValue(favoriteStorageKey(source)).trimmed().isEmpty();
        if (badgesEnabled && fsMeta.exists) {
            data.savedListBadge = savedListBadgeByPath.value(fsMeta.dirPath);
        }

        data.hasSubtitleSibling = fsMeta.hasSubtitleSibling;

        const bool hasEmbeddedSubtitle = (source == currentMediaSource_
            && std::any_of(currentTracks_.cbegin(), currentTracks_.cend(), [](const revaplayer::domain::TrackInfo &track) {
                return track.type == revaplayer::domain::TrackType::Subtitle;
            }))
            || (scanResult.has_value() && scanResult->subtitleTrackCount > 0);
        data.subtitleState = data.hasSubtitleSibling && hasEmbeddedSubtitle
            ? uiText("Local + Embedded")
            : (data.hasSubtitleSibling
                   ? uiText("Local")
                   : (hasEmbeddedSubtitle
                          ? uiText("Embedded")
                          : (data.metadataPending ? uiText("Scanning") : uiText("None"))));

        const auto thumbnailIt = playlistThumbnailCache_.constFind(source);
        if (thumbnailIt != playlistThumbnailCache_.constEnd()) {
            data.thumbnail = thumbnailIt.value();
        } else {
            QImage cachedThumbnail;
            if (fsMeta.exists
                && fsMeta.isFile
                && loadPlaylistThumbnailDiskCacheForFileMetadata(
                    fsMeta.absoluteFilePath,
                    fsMeta.lastModifiedMs,
                    fsMeta.size,
                    data.durationSeconds,
                    &cachedThumbnail)
                && !cachedThumbnail.isNull()) {
                playlistThumbnailCache_.insert(source, cachedThumbnail);
                data.thumbnail = cachedThumbnail;
            }
        }

        if (data.thumbnail.isNull() && source == currentMediaSource_) {
            warmPlaylistThumbnail(source, data.durationSeconds);
        }

        QStringList secondaryParts;
        QStringList secondaryBadges;
        const bool audioOnlyLabel = scanResult.has_value() && scanResult->hasAudioTrack && !scanResult->hasVideoTrack;
        if (audioOnlyLabel) {
            secondaryBadges.push_back(uiText("Audio"));
        }
        QString fileExtensionLabel;
        if (fsMeta.exists) {
            const QString suffix = fsMeta.suffix.trimmed().toLower();
            if (!suffix.isEmpty()) {
                fileExtensionLabel = QStringLiteral(".%1").arg(suffix);
            }
        } else {
            const QUrl sourceUrl(source);
            const QString sourcePath = sourceUrl.isValid() && !sourceUrl.path().isEmpty() ? sourceUrl.path() : source;
            const qsizetype lastSlash = sourcePath.lastIndexOf(QLatin1Char('/'));
            const qsizetype lastDot = sourcePath.lastIndexOf(QLatin1Char('.'));
            const QString suffix = lastDot > lastSlash && lastDot + 1 < sourcePath.size()
                ? sourcePath.mid(lastDot + 1).trimmed().toLower()
                : QString {};
            if (!suffix.isEmpty()) {
                fileExtensionLabel = QStringLiteral(".%1").arg(suffix);
            }
        }
        for (const QString &columnId : visibleColumns) {
            if (columnId == QStringLiteral("watched") && data.watchedPercent >= 0) {
                secondaryBadges.push_back(QStringLiteral("%1%").arg(data.watchedPercent));
            } else if (columnId == QStringLiteral("last_position") && data.lastPositionSeconds > 0.0) {
                secondaryBadges.push_back(formatPlaybackTime(data.lastPositionSeconds));
            } else if (columnId == QStringLiteral("file_extension") && !fileExtensionLabel.isEmpty()) {
                secondaryBadges.push_back(fileExtensionLabel);
            } else if (columnId == QStringLiteral("resolution_approx") && scanResult.has_value()) {
                const QString resolutionEstimate = approximateResolutionLabelForScan(*scanResult);
                if (!resolutionEstimate.isEmpty()) {
                    secondaryBadges.push_back(resolutionEstimate);
                }
            } else if (columnId == QStringLiteral("resolution_exact") && scanResult.has_value()) {
                const QString exactResolution = resolutionLabelForScan(*scanResult);
                if (!exactResolution.isEmpty()) {
                    secondaryBadges.push_back(exactResolution);
                }
            } else if (columnId == QStringLiteral("tags") && !data.tags.isEmpty()) {
                secondaryParts.push_back(data.tags.join(QStringLiteral(", ")));
            } else if (columnId == QStringLiteral("notes") && !data.notesPreview.trimmed().isEmpty()) {
                secondaryParts.push_back(data.notesPreview.simplified());
            } else if (columnId == QStringLiteral("subtitle_state") && !data.subtitleState.trimmed().isEmpty()) {
                secondaryBadges.push_back(data.subtitleState.trimmed());
            } else if (columnId == QStringLiteral("difficulty") && !data.difficulty.trimmed().isEmpty()) {
                secondaryBadges.push_back(data.difficulty.trimmed());
            }
        }
        if (data.metadataPending && playlistColumnsProduceSecondaryText(visibleColumns)) {
            secondaryBadges.push_back(uiText("Scanning"));
        }
        secondaryParts.removeDuplicates();
        secondaryBadges.removeDuplicates();
        data.secondaryText = secondaryParts.join(QStringLiteral("  •  "));
        data.secondaryBadges = secondaryBadges;

        const int itemProgress = data.completed ? 100 : std::max(0, data.watchedPercent);
        cachedTotalProgress += itemProgress;
        ++cachedPlaylistItems;
        if (itemProgress >= completionThreshold) {
            ++cachedCompletedItems;
        }
        if (data.durationSeconds > 0.0) {
            cachedTotalDuration += data.durationSeconds;
            cachedWatchedDuration += data.completed
                ? data.durationSeconds
                : std::clamp(data.lastPositionSeconds, 0.0, data.durationSeconds);
            ++cachedKnownDurations;
        }

        presentationData.insert(source, data);
    }

    cachedPlaylistItemCount_ = cachedPlaylistItems;
    cachedPlaylistCompletedItemCount_ = cachedCompletedItems;
    cachedPlaylistKnownDurationCount_ = cachedKnownDurations;
    cachedPlaylistTotalProgress_ = cachedTotalProgress;
    cachedPlaylistTotalDurationSeconds_ = cachedTotalDuration;
    cachedPlaylistWatchedDurationSeconds_ = cachedWatchedDuration;

    playlistController_->setPresentationData(std::move(presentationData));
    if (canScanMetadata && !sourcesToScan.isEmpty()) {
        sourcesToScan.removeDuplicates();
        if (!hadPendingBeforeRefresh || !playlistMetadataScanElapsed_.isValid()) {
            playlistMetadataScanBatchTotal_ = sourcesToScan.size();
            playlistMetadataScanElapsed_.restart();
        } else {
            playlistMetadataScanBatchTotal_ = std::max(
                playlistMetadataScanBatchTotal_,
                static_cast<int>(pendingMediaScanSources_.size()));
        }
        metadataScanService_->enqueueSources(sourcesToScan);
    } else if (pendingMediaScanSources_.isEmpty()) {
        playlistMetadataScanBatchTotal_ = 0;
        playlistMetadataScanElapsed_.invalidate();
    }
    if (playlistView_ != nullptr) {
        playlistView_->setProperty("playlistProgressModeEnabled", progressTrackingModeEnabled());
        playlistView_->setProperty("playlistCardZoom", playlistCardZoomId());
        playlistView_->setProperty("playlistThumbnailShape", playlistThumbnailShapeId());
        playlistView_->setProperty("playlistVisibleColumns", visibleColumns);
        refreshPlaylistViewLayout();
    }
    requestPlaylistThumbnailBatch(true);
}

void MainWindow::rebuildPinnedCourseTabs(const bool includeProgress)
{
    if (pinnedCoursesTabBar_ == nullptr) {
        return;
    }

    const QString activeLocalPath = localMediaPathForSource(currentMediaSource_);
    const QString activeFolder = activeLocalPath.isEmpty() ? QString {} : QFileInfo(activeLocalPath).dir().absolutePath();
    const QString browserFolder = pinnedCourseBrowserActive_
        ? QFileInfo(pinnedCourseBrowserRootPath_.trimmed().isEmpty() ? pinnedCourseBrowserFolderPath_ : pinnedCourseBrowserRootPath_).absoluteFilePath()
        : QString {};
    const QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
    if (settingsController_ != nullptr
        && settingsController_->customKeys(QString::fromLatin1(kPinnedCoursePrefix)).size() != courses.size()) {
        persistPinnedCourses(settingsController_, courses);
    }
    const QSignalBlocker blocker(pinnedCoursesTabBar_);
    while (pinnedCoursesTabBar_->count() > 0) {
        pinnedCoursesTabBar_->removeTab(pinnedCoursesTabBar_->count() - 1);
    }
    pinnedCoursesTabBar_->addTab(uiText("Current"));
    pinnedCoursesTabBar_->setTabData(0, QString {});

    int activeIndex = 0;
    for (const auto &course : courses) {
        QString tabText = course.label;
        QString progressText;
        if (includeProgress && progressTrackingModeEnabled()) {
            const QStringList sources = playlistSourcesForPinnedCourse(settingsController_, course.path, naturalSortFolderPlaylistEnabled());
            int counted = 0;
            int totalProgress = 0;
            for (const QString &source : sources) {
                const auto it = historyEntriesBySource_.constFind(source);
                if (it == historyEntriesBySource_.constEnd()) {
                    continue;
                }
                const auto &entry = it.value();
                const int progress = entry.completed || entry.durationSeconds <= 0.0
                    ? (entry.completed ? 100 : 0)
                    : std::clamp(static_cast<int>(std::round((entry.positionSeconds / entry.durationSeconds) * 100.0)), 0, 100);
                totalProgress += progress;
                ++counted;
            }
            if (counted > 0) {
                progressText = uiText("%1% complete").arg(totalProgress / counted);
                tabText = uiText("%1 %2%").arg(course.label).arg(totalProgress / counted);
            }
        }

        const int tabIndex = pinnedCoursesTabBar_->addTab(
            playlistViewIcon(pinnedCoursesTabBar_, course.iconId.trimmed().isEmpty() ? QStringLiteral("folder") : course.iconId),
            tabText);
        pinnedCoursesTabBar_->setTabData(tabIndex, course.path);
        pinnedCoursesTabBar_->setTabTextColor(tabIndex, accentColorForId(course.colorId));
        QStringList toolTipLines {savedListDisplayLabel(course, true)};
        if (!course.description.trimmed().isEmpty()) {
            toolTipLines.push_back(course.description.trimmed());
        }
        if (!progressText.isEmpty()) {
            toolTipLines.push_back(progressText);
        }
        toolTipLines.push_back(course.path);
        const QString toolTip = toolTipLines.join(QChar('\n'));
        pinnedCoursesTabBar_->setTabToolTip(tabIndex, toolTip);
        if (!browserFolder.isEmpty() && QFileInfo(course.path).absoluteFilePath() == browserFolder) {
            activeIndex = tabIndex;
        } else if (!activeFolder.isEmpty() && QFileInfo(course.path).absoluteFilePath() == activeFolder) {
            activeIndex = tabIndex;
        }
    }

    pinnedCoursesTabBar_->adjustSize();
    pinnedCoursesTabBar_->updateGeometry();
    if (pinnedCoursesScrollArea_ != nullptr) {
        const int rowHeight = pinnedCoursesTabBar_->sizeHint().height();
        const int scrollAreaHeight = rowHeight
            + pinnedCoursesScrollArea_->horizontalScrollBar()->sizeHint().height()
            + 6;
        pinnedCoursesScrollArea_->setFixedHeight(std::max(scrollAreaHeight, 54));
    }

    pinnedCoursesTabBar_->setCurrentIndex(activeIndex);
    scrollPinnedCourseTabIntoView(activeIndex);
    if (playlistPinCourseButton_ != nullptr) {
        const QString savableBrowserFolder = pinnedCourseBrowserActive_
            ? QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath()
            : QString {};
        const bool canSaveBrowserFolder = !savableBrowserFolder.isEmpty()
            && QFileInfo(savableBrowserFolder).exists()
            && QFileInfo(savableBrowserFolder).isDir();
        playlistPinCourseButton_->setEnabled(canSaveBrowserFolder || !activeFolder.isEmpty());
        playlistPinCourseButton_->setToolTip(
            canSaveBrowserFolder
                ? uiText("Save the current browsed folder as a reusable named list")
                : uiText("Save the current media folder as a reusable named list"));
    }
    updatePlaylistChromeState();
}

void MainWindow::pinCurrentMediaFolder()
{
    if (settingsController_ == nullptr) {
        return;
    }

    QString folderPath;
    QFileInfo folderInfo;
    if (pinnedCourseBrowserActive_ && !pinnedCourseBrowserFolderPath_.trimmed().isEmpty()) {
        folderInfo = QFileInfo(pinnedCourseBrowserFolderPath_);
        if (folderInfo.exists() && folderInfo.isDir()) {
            folderPath = folderInfo.absoluteFilePath();
        }
    }

    if (folderPath.isEmpty()) {
        const QString localPath = localMediaPathForSource(currentMediaSource_);
        const QFileInfo fileInfo(localPath);
        if (fileInfo.exists() && fileInfo.isFile()) {
            folderInfo = QFileInfo(fileInfo.dir().absolutePath());
            folderPath = folderInfo.absoluteFilePath();
        }
    }

    if (folderPath.isEmpty() || !folderInfo.exists() || !folderInfo.isDir()) {
        statusBar()->showMessage(uiText("Open a local media file or browse a folder before saving a folder list."), 3500);
        return;
    }

    const QVector<PinnedCourseFolder> existingLists = loadPinnedCourses(settingsController_);
    const auto existingIt = std::find_if(existingLists.cbegin(), existingLists.cend(), [&folderPath](const PinnedCourseFolder &course) {
        return QFileInfo(course.path).absoluteFilePath() == folderPath;
    });

    PinnedCourseFolder course;
    if (existingIt == existingLists.cend()) {
        course.path = folderPath;
        course.label = folderInfo.fileName().trimmed().isEmpty()
            ? QDir::toNativeSeparators(folderPath)
            : folderInfo.fileName().trimmed();
        course.iconId = QStringLiteral("folder");
        course.colorId = uiAccentId();
        course.order = existingLists.size();
    } else {
        course = *existingIt;
    }

    if (!editSavedFolderDetails(
            this,
            existingIt == existingLists.cend() ? uiText("Save Folder as List") : uiText("Edit Saved List"),
            uiText("Choose a name and customize how this saved folder should appear."),
            course,
            uiAccentId(),
            &course)) {
        return;
    }

    QVector<PinnedCourseFolder> updatedCourses = existingLists;
    if (existingIt == existingLists.cend()) {
        updatedCourses.push_back(course);
    } else {
        updatedCourses[std::distance(existingLists.cbegin(), existingIt)] = course;
    }
    if (!persistPinnedCourses(settingsController_, updatedCourses)) {
        const QString failureMessage = settingsController_->lastError().trimmed().isEmpty()
            ? uiText("Could not save the saved folder.")
            : uiText("Could not save the saved folder: %1").arg(settingsController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }
    const QVector<PinnedCourseFolder> reloadedCourses = loadPinnedCourses(settingsController_);
    const bool savedFolderVisible = std::any_of(
        reloadedCourses.cbegin(),
        reloadedCourses.cend(),
        [&folderPath](const PinnedCourseFolder &candidate) {
            return QFileInfo(candidate.path).absoluteFilePath() == folderPath;
        });
    if (!savedFolderVisible) {
        statusBar()->showMessage(uiText("Could not show the saved folder after saving. Check that the folder still exists."), 5000);
        rebuildPinnedCourseTabs();
        reloadHomeDashboard();
        return;
    }

    if (pinnedCourseBrowserActive_
        && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() == folderPath
        && playlistController_ != nullptr) {
        persistPinnedCourseMediaOrder(
            settingsController_,
            folderPath,
            orderedLocalMediaSourcesForFolderBrowserEntries(playlistController_->entries(), folderPath));
    } else if (playlistFolderPathForEntries(playbackPlaylistEntriesCache_) == folderPath) {
        persistPinnedCourseMediaOrder(
            settingsController_,
            folderPath,
            orderedLocalSourcesForEntries(playbackPlaylistEntriesCache_, folderPath));
    }
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    refreshPlaylistPresentationData();
    showActionResult(uiText("Saved list updated: %1").arg(savedListDisplayLabel(course, true)),
                     uiText("Saved list updated: %1").arg(savedListDisplayLabel(course, true)),
                     3000);
}

void MainWindow::updatePlaylistMetadataScanButtonState()
{
    if (playlistRefreshButton_ == nullptr || playlistController_ == nullptr) {
        return;
    }

    playlistRefreshButton_->setEnabled(
        metadataScanService_ != nullptr
        && metadataScanService_->isInitialized()
        && !playlistController_->entries().isEmpty());
}

void MainWindow::schedulePlaylistMetadataRefresh(const int delayMs)
{
    if (fullscreenTransitionActive_) {
        pendingPlaylistMetadataRefresh_ = true;
        return;
    }

    if (metadataRefreshTimer_ == nullptr) {
        refreshPlaylistPresentationData();
        refreshPlaylistSummary();
        refreshPlaylistInspector();
        return;
    }

    const int delayFloor = (!isVisible() || isMinimized())
        ? 1000
        : std::clamp(playlistPlaybackRefreshIntervalMs_ / 2, 120, 1000);
    const int normalizedDelay = std::max(delayFloor, delayMs);
    if (!metadataRefreshTimer_->isActive() || metadataRefreshTimer_->remainingTime() > normalizedDelay) {
        metadataRefreshTimer_->start(normalizedDelay);
    }
}

QSet<QString> MainWindow::playlistMetadataAutoScanTargets() const
{
    QSet<QString> targets;
    if (playlistController_ == nullptr) {
        return targets;
    }

    const auto addScanKey = [&targets](const QString &source) {
        const QString scanKey = mediaScanSourceKey(source);
        if (!scanKey.isEmpty()) {
            targets.insert(scanKey);
        }
    };

    const auto entries = playlistController_->entries();
    if (playlistMetadataFullScanRequested_) {
        for (const auto &entry : entries) {
            addScanKey(entry.source);
        }
        return targets;
    }

    QAbstractItemModel *model = playlistView_ != nullptr ? playlistView_->model() : nullptr;
    if (model != nullptr && model->rowCount() > 0) {
        const int rowCount = model->rowCount();
        for (int row = 0; row < std::min(rowCount, kPlaylistInitialMetadataScanBatch); ++row) {
            addScanKey(model->index(row, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString());
        }

        const int topRow = playlistVisibleRowFromTop(playlistView_);
        int bottomRow = playlistVisibleRowFromBottom(playlistView_);
        if (topRow >= 0) {
            if (bottomRow < topRow) {
                bottomRow = topRow;
            }
            const int startRow = std::max(0, topRow - kPlaylistMetadataScanLookbehind);
            const int endRow = std::min(rowCount - 1, bottomRow + kPlaylistMetadataScanLookahead);
            for (int row = startRow; row <= endRow; ++row) {
                addScanKey(model->index(row, 0).data(revaplayer::application::PlaylistRoles::SourceRole).toString());
            }
        }

        if (playlistView_->currentIndex().isValid()) {
            addScanKey(playlistView_->currentIndex().data(revaplayer::application::PlaylistRoles::SourceRole).toString());
        }
    } else {
        for (int index = 0;
             index < std::min(static_cast<int>(entries.size()), kPlaylistInitialMetadataScanBatch);
             ++index) {
            addScanKey(entries.at(index).source);
        }
    }

    if (!currentMediaSource_.trimmed().isEmpty()) {
        addScanKey(currentMediaSource_);
    }

    return targets;
}

void MainWindow::resetPlaylistMetadataScanState(const bool cancelActiveScan)
{
    if (cancelActiveScan && metadataScanService_ != nullptr && metadataScanService_->isInitialized()) {
        metadataScanService_->cancel();
    }

    playlistMetadataFullScanRequested_ = false;
    pendingMediaScanSources_.clear();
    failedMediaScanSources_.clear();
    mediaScanFailureCounts_.clear();
    mediaScanFailureReasons_.clear();
    playlistMetadataScanBatchTotal_ = 0;
    playlistMetadataScanElapsed_.invalidate();
}

int MainWindow::enqueuePlaylistMetadataScan(const bool forceRescan)
{
    if (playlistController_ == nullptr || metadataScanService_ == nullptr || !metadataScanService_->isInitialized()) {
        updatePlaylistMetadataScanButtonState();
        return 0;
    }

    const auto entries = playlistController_->entries();
    if (entries.isEmpty()) {
        updatePlaylistMetadataScanButtonState();
        return 0;
    }

    int candidateCount = 0;
    for (const auto &entry : entries) {
        if (!mediaScanSourceKey(entry.source.trimmed()).isEmpty()) {
            ++candidateCount;
        }
    }

    if (forceRescan) {
        playlistMetadataFullScanRequested_ = true;
        for (const auto &entry : entries) {
            const QString scanKey = mediaScanSourceKey(entry.source.trimmed());
            if (scanKey.isEmpty()) {
                continue;
            }
            mediaScanCache_.remove(scanKey);
            mediaScanCacheLookupCompleted_.remove(scanKey);
            if (settingsController_ != nullptr) {
                settingsController_->removeCustomValue(mediaScanStorageKey(scanKey));
            }
        }
        resetPlaylistMetadataScanState(true);
        playlistMetadataFullScanRequested_ = true;
    }

    schedulePlaylistMetadataRefresh(0);
    updatePlaylistMetadataScanButtonState();
    return candidateCount;
}

void MainWindow::applyVisiblePlaylistEntries()
{
    if (playlistController_ == nullptr) {
        return;
    }

    clearFileSystemCache();
    resetPlaylistMetadataScanState(true);

    if (pinnedCourseBrowserActive_ && !pinnedCourseBrowserFolderPath_.trimmed().isEmpty()) {
        const QStringList orderedBrowserMediaSources = playlistSourcesForPinnedCourse(
            settingsController_,
            pinnedCourseBrowserFolderPath_,
            naturalSortFolderPlaylistEnabled());
        const QVector<revaplayer::domain::PlaylistEntry> browserEntries = folderBrowserEntriesForDirectory(
            pinnedCourseBrowserFolderPath_,
            pinnedCourseBrowserRootPath_.isEmpty() ? pinnedCourseBrowserFolderPath_ : pinnedCourseBrowserRootPath_,
            naturalSortFolderPlaylistEnabled(),
            currentMediaSource_,
            orderedBrowserMediaSources);
        int browserCurrentIndex = -1;
        for (int index = 0; index < browserEntries.size(); ++index) {
            if (sourcesReferToSameMedia(browserEntries.at(index).source, currentMediaSource_)) {
                browserCurrentIndex = index;
                break;
            }
        }
        playlistController_->setEntries(browserEntries, browserCurrentIndex);
        updatePlaylistReorderAvailability();
        updatePlaylistMetadataScanButtonState();
        return;
    }

    playlistController_->setEntries(playbackPlaylistEntriesCache_, playbackPlaylistCurrentIndexCache_);
    updatePlaylistReorderAvailability();
    updatePlaylistMetadataScanButtonState();
}

void MainWindow::previewPinnedCourseTab(const int index)
{
    if (pinnedCoursesTabBar_ == nullptr || index <= 0) {
        exitPinnedCourseBrowserMode();
        return;
    }

    const QString folderPath = pinnedCoursesTabBar_->tabData(index).toString();
    if (folderPath.isEmpty()) {
        exitPinnedCourseBrowserMode();
        return;
    }

    const bool containsMedia = !supportedMediaFilesInDirectory(QDir(folderPath), naturalSortFolderPlaylistEnabled()).isEmpty();
    const bool containsSubfolders = !supportedSubdirectoriesInDirectory(QDir(folderPath), naturalSortFolderPlaylistEnabled()).isEmpty();
    if (!containsMedia && !containsSubfolders) {
        statusBar()->showMessage(uiText("This saved list folder does not contain playable media files."), 3500);
        return;
    }

    browseFolderPath(QFileInfo(folderPath).absoluteFilePath(), true);
}

void MainWindow::browseFolderPath(const QString &folderPath, const bool resetHistory)
{
    const QFileInfo folderInfo(folderPath);
    if (!folderInfo.exists() || !folderInfo.isDir()) {
        statusBar()->showMessage(uiText("The selected folder is not available."), 3500);
        return;
    }

    const QString absoluteFolderPath = folderInfo.absoluteFilePath();
    const bool containsMedia = !supportedMediaFilesInDirectory(QDir(absoluteFolderPath), naturalSortFolderPlaylistEnabled()).isEmpty();
    const bool containsSubfolders = !supportedSubdirectoriesInDirectory(QDir(absoluteFolderPath), naturalSortFolderPlaylistEnabled()).isEmpty();
    if (!containsMedia && !containsSubfolders) {
        statusBar()->showMessage(uiText("This folder does not contain playable media files or subfolders."), 3500);
        return;
    }

    if (resetHistory || !pinnedCourseBrowserActive_) {
        pinnedCourseBrowserHistory_.clear();
        pinnedCourseBrowserRootPath_ = absoluteFolderPath;
    } else if (!pinnedCourseBrowserFolderPath_.isEmpty()
               && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() != absoluteFolderPath) {
        pinnedCourseBrowserHistory_.push_back(QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath());
    }

    pinnedCourseBrowserActive_ = true;
    pinnedCourseBrowserFolderPath_ = absoluteFolderPath;
    applyVisiblePlaylistEntries();
    refreshPlaylistPresentationData();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    updateHomeDashboardVisibility();
    if (playlistView_ != nullptr) {
        playlistView_->clearSelection();
        if (playlistView_->selectionModel() != nullptr) {
            playlistView_->selectionModel()->clearCurrentIndex();
        }
        playlistView_->scrollToTop();
    }
    setSidePanelVisible(SidePanel::Playlist, true, true);
    statusBar()->showMessage(uiText("Browsing folder: %1").arg(QDir::toNativeSeparators(absoluteFolderPath)), 3000);
}

void MainWindow::browseBackFolder()
{
    if (!pinnedCourseBrowserActive_) {
        return;
    }

    while (!pinnedCourseBrowserHistory_.isEmpty()) {
        const QString targetFolderPath = pinnedCourseBrowserHistory_.takeLast();
        if (targetFolderPath.isEmpty()) {
            continue;
        }

        const QFileInfo targetInfo(targetFolderPath);
        if (!targetInfo.exists() || !targetInfo.isDir()) {
            continue;
        }

        const QString absoluteTargetPath = targetInfo.absoluteFilePath();
        if (absoluteTargetPath.isEmpty()
            || absoluteTargetPath == QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath()) {
            continue;
        }
        if (supportedMediaFilesInDirectory(QDir(absoluteTargetPath), naturalSortFolderPlaylistEnabled()).isEmpty()
            && supportedSubdirectoriesInDirectory(QDir(absoluteTargetPath), naturalSortFolderPlaylistEnabled()).isEmpty()) {
            continue;
        }

        browseFolderPath(absoluteTargetPath, false);
        return;
    }

    const QString rootFolderPath = pinnedCourseBrowserRootPath_;
    const QFileInfo rootInfo(rootFolderPath);
    if (rootFolderPath.isEmpty()
        || !rootInfo.exists()
        || !rootInfo.isDir()
        || rootInfo.absoluteFilePath() == QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath()
        || (supportedMediaFilesInDirectory(QDir(rootInfo.absoluteFilePath()), naturalSortFolderPlaylistEnabled()).isEmpty()
            && supportedSubdirectoriesInDirectory(QDir(rootInfo.absoluteFilePath()), naturalSortFolderPlaylistEnabled()).isEmpty())) {
        exitPinnedCourseBrowserMode();
        return;
    }

    browseFolderPath(rootInfo.absoluteFilePath(), true);
}

void MainWindow::exitPinnedCourseBrowserMode()
{
    if (!pinnedCourseBrowserActive_ && pinnedCourseBrowserFolderPath_.isEmpty()) {
        return;
    }

    pinnedCourseBrowserActive_ = false;
    pinnedCourseBrowserFolderPath_.clear();
    pinnedCourseBrowserRootPath_.clear();
    pinnedCourseBrowserHistory_.clear();
    applyVisiblePlaylistEntries();
    refreshPlaylistPresentationData();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    updateHomeDashboardVisibility();
    if (!pinnedCourseBrowserActive_) {
        focusCurrentPlaylistItem(false);
    }
}

void MainWindow::openPinnedCourseTab(const int index)
{
    if (pinnedCoursesTabBar_ == nullptr || index <= 0) {
        return;
    }

    const QString folderPath = pinnedCoursesTabBar_->tabData(index).toString();
    if (folderPath.isEmpty()) {
        return;
    }

    const QStringList sources = playlistSourcesForPinnedCourse(settingsController_, folderPath, naturalSortFolderPlaylistEnabled());
    if (sources.isEmpty()) {
        if (!supportedSubdirectoriesInDirectory(QDir(folderPath), naturalSortFolderPlaylistEnabled()).isEmpty()) {
            browseFolderPath(QFileInfo(folderPath).absoluteFilePath(), true);
            return;
        }
        statusBar()->showMessage(uiText("This saved list folder does not contain playable media files."), 3500);
        return;
    }

    const QString currentLocalPath = localMediaPathForSource(currentMediaSource_);
    const int selectedIndex = !currentLocalPath.isEmpty() ? sources.indexOf(QFileInfo(currentLocalPath).absoluteFilePath()) : -1;
    openPlaylistSources(sources, selectedIndex >= 0 ? selectedIndex : 0);
}

void MainWindow::managePinnedCourses()
{
    if (settingsController_ == nullptr) {
        return;
    }

    const QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
    QVector<PinnedCourseFolder> updatedCourses;
    if (!manageSavedFoldersDialog(this, courses, uiAccentId(), &updatedCourses)) {
        return;
    }

    if (!persistPinnedCourses(settingsController_, updatedCourses)) {
        const QString failureMessage = settingsController_->lastError().trimmed().isEmpty()
            ? uiText("Could not save the saved folders.")
            : uiText("Could not save the saved folders: %1").arg(settingsController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    refreshPlaylistPresentationData();
    statusBar()->showMessage(uiText("Saved lists updated"), 2500);
}

void MainWindow::showPinnedCourseContextMenu(const QPoint &position)
{
    if (pinnedCoursesTabBar_ == nullptr) {
        return;
    }

    const int index = pinnedCoursesTabBar_->tabAt(position);
    if (index <= 0) {
        return;
    }

    const QString folderPath = pinnedCoursesTabBar_->tabData(index).toString();
    if (folderPath.isEmpty()) {
        return;
    }

    QMenu menu(this);
    QAction *openAction = menu.addAction(uiText("Open"));
    QAction *editAction = menu.addAction(uiText("Edit Details"));
    QAction *removeAction = menu.addAction(uiText("Remove"));
    forceMenuLeftToRight(&menu);
    QAction *chosenAction = menu.exec(pinnedCoursesTabBar_->mapToGlobal(position));
    if (chosenAction == nullptr) {
        return;
    }

    if (chosenAction == openAction) {
        if (pinnedCoursesTabBar_->currentIndex() != index) {
            pinnedCoursesTabBar_->setCurrentIndex(index);
        } else {
            openPinnedCourseTab(index);
        }
        return;
    }

    if (chosenAction == editAction) {
        editPinnedCourseByPath(folderPath);
        return;
    }

    if (chosenAction == removeAction) {
        removePinnedCourseByPath(folderPath);
    }
}

void MainWindow::editPinnedCourseByPath(const QString &folderPath)
{
    if (settingsController_ == nullptr || folderPath.isEmpty()) {
        return;
    }

    QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
    const auto courseIt = std::find_if(courses.begin(), courses.end(), [&folderPath](const PinnedCourseFolder &course) {
        return QFileInfo(course.path).absoluteFilePath() == QFileInfo(folderPath).absoluteFilePath();
    });
    if (courseIt == courses.end()) {
        return;
    }

    PinnedCourseFolder updatedCourse = *courseIt;
    if (!editSavedFolderDetails(
            this,
            uiText("Edit Saved List"),
            uiText("Update this saved folder and how it appears inside the playlist."),
            updatedCourse,
            uiAccentId(),
            &updatedCourse)) {
        return;
    }

    courses[std::distance(courses.begin(), courseIt)] = updatedCourse;
    if (!persistPinnedCourses(settingsController_, courses)) {
        const QString failureMessage = settingsController_->lastError().trimmed().isEmpty()
            ? uiText("Could not save the saved folder.")
            : uiText("Could not save the saved folder: %1").arg(settingsController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    refreshPlaylistPresentationData();
    showActionResult(uiText("Saved list updated"),
                     uiText("Saved list updated"),
                     2500);
}

void MainWindow::removePinnedCourseByPath(const QString &folderPath)
{
    if (settingsController_ == nullptr || folderPath.isEmpty()) {
        return;
    }

    QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
    const auto courseIt = std::find_if(courses.begin(), courses.end(), [&folderPath](const PinnedCourseFolder &course) {
        return QFileInfo(course.path).absoluteFilePath() == QFileInfo(folderPath).absoluteFilePath();
    });
    if (courseIt == courses.end()) {
        return;
    }

    if (QMessageBox::question(
            this,
            uiText("Remove Saved List"),
            uiText("Remove \"%1\" from the saved folders?").arg(courseIt->label))
        != QMessageBox::Yes) {
        return;
    }

    courses.erase(courseIt);
    if (pinnedCourseBrowserActive_ && QFileInfo(pinnedCourseBrowserFolderPath_).absoluteFilePath() == QFileInfo(folderPath).absoluteFilePath()) {
        pinnedCourseBrowserActive_ = false;
        pinnedCourseBrowserFolderPath_.clear();
    }
    if (!persistPinnedCourses(settingsController_, courses)) {
        const QString failureMessage = settingsController_->lastError().trimmed().isEmpty()
            ? uiText("Could not remove the saved folder.")
            : uiText("Could not remove the saved folder: %1").arg(settingsController_->lastError());
        statusBar()->showMessage(failureMessage, 5000);
        return;
    }
    rebuildPinnedCourseTabs();
    reloadHomeDashboard();
    applyVisiblePlaylistEntries();
    refreshPlaylistPresentationData();
    showActionResult(uiText("Saved list removed"),
                     uiText("Saved list removed"),
                     2500);
}

void MainWindow::showPlaylistPanelSettingsDialog()
{
    if (settingsController_ == nullptr) {
        return;
    }

    PlaylistPanelSettingsState state;
    state.cardZoomId = playlistCardZoomId();
    state.thumbnailShapeId = playlistThumbnailShapeId();
    state.secondaryTextVisible = playlistColumnsProduceSecondaryText(playlistVisibleColumns());
    state.sortModeId = playlistSortModeId();
    state.showFullPaths = playlistShowFullPathsEnabled();
    state.showIndexPrefixes = playlistShowIndexPrefixesEnabled();
    state.columns = playlistVisibleColumns();

    if (!editPlaylistPanelSettingsDialog(this, state, &state)) {
        return;
    }

    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistCardZoomSetting), state.cardZoomId);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistThumbnailShapeSetting), state.thumbnailShapeId);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistSortModeSetting), state.sortModeId);
    settingsController_->setPlaylistShowFullPaths(state.showFullPaths);
    settingsController_->setPlaylistShowIndexPrefixes(state.showIndexPrefixes);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistViewColumnsSetting), state.columns.join(QStringLiteral(",")));
    applyPlaylistPanelDisplayPreferences();
    refreshPlaylistPresentationData();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    updateAdaptiveUiLayout();
}

void MainWindow::showDetailsPanelSettingsDialog()
{
    if (settingsController_ == nullptr || detailsTabs_ == nullptr) {
        return;
    }

    QVector<QPair<QString, QString>> detailsTabs;
    detailsTabs.reserve(detailsTabs_->count());
    for (int index = 0; index < detailsTabs_->count(); ++index) {
        QWidget *page = detailsTabs_->widget(index);
        if (page == nullptr) {
            continue;
        }

        const QString tabId = page->property("detailsTabId").toString().trimmed();
        if (!tabId.isEmpty()) {
            detailsTabs.push_back({tabId, detailsTabs_->tabText(index)});
        }
    }
    if (detailsTabs.isEmpty()) {
        return;
    }

    QStringList visibleTabs = playlistVisibleDetailsTabIds();
    QString currentTabId = playlistPreferredDetailsTabId();
    if (!editDetailsPanelSettingsDialog(this, detailsTabs, visibleTabs, currentTabId, &visibleTabs, &currentTabId)) {
        return;
    }

    const QStringList normalizedVisibleTabs = normalizeVisibleDetailsTabs(visibleTabs, detailsTabs);
    const bool allDetailsTabsVisible = normalizedVisibleTabs.size() == detailsTabs.size();
    if (allDetailsTabsVisible || normalizedVisibleTabs.isEmpty()) {
        settingsController_->removeCustomValue(QString::fromLatin1(kPlaylistDetailsVisibleTabsSetting));
    } else {
        settingsController_->setCustomValue(
            QString::fromLatin1(kPlaylistDetailsVisibleTabsSetting),
            normalizedVisibleTabs.join(QStringLiteral(",")));
    }
    if (currentTabId.isEmpty()) {
        settingsController_->removeCustomValue(QString::fromLatin1(kPlaylistDetailsCurrentTabSetting));
    } else {
        settingsController_->setCustomValue(QString::fromLatin1(kPlaylistDetailsCurrentTabSetting), currentTabId);
    }

    applyPlaylistPanelDisplayPreferences();
}

void MainWindow::rebuildPlaylistViewTabs()
{
    if (playlistViewPresetsTabBar_ == nullptr) {
        return;
    }

    const QVector<PlaylistViewPreset> presets = loadPlaylistViewPresets(settingsController_);
    const QString activeKey = settingsController_ != nullptr
        ? settingsController_->customValue(QString::fromLatin1(kPlaylistViewActiveSetting))
        : QString {};

    playlistViewPresetApplyInProgress_ = true;
    const QSignalBlocker blocker(playlistViewPresetsTabBar_);
    while (playlistViewPresetsTabBar_->count() > 0) {
        playlistViewPresetsTabBar_->removeTab(playlistViewPresetsTabBar_->count() - 1);
    }

    int activeIndex = -1;
    for (const auto &preset : presets) {
        if (!preset.showInTabs) {
            continue;
        }
        const int tabIndex = playlistViewPresetsTabBar_->addTab(playlistViewIcon(playlistViewPresetsTabBar_, preset.iconId), preset.name);
        playlistViewPresetsTabBar_->setTabData(tabIndex, preset.key);
        playlistViewPresetsTabBar_->setTabTextColor(tabIndex, accentColorForId(preset.colorId));
        playlistViewPresetsTabBar_->setTabToolTip(
            tabIndex,
            uiText("%1 • %2").arg(playlistCardZoomLabel(preset.cardZoomId), preset.name));
        if (preset.key == activeKey) {
            activeIndex = tabIndex;
        }
    }

    if (activeIndex >= 0) {
        playlistViewPresetsTabBar_->setCurrentIndex(activeIndex);
    }
    playlistViewPresetApplyInProgress_ = false;

    if (playlistEditViewButton_ != nullptr) {
        playlistEditViewButton_->setEnabled(!activeKey.trimmed().isEmpty());
    }
    updatePlaylistChromeState();
}

void MainWindow::saveCurrentPlaylistView()
{
    if (settingsController_ == nullptr) {
        return;
    }

    PlaylistViewPreset preset;
    preset.key = activePlaylistViewPresetKey_;
    preset.name = uiText("My View");
    preset.iconId = QStringLiteral("play");
    preset.colorId = uiAccentId();
    preset.cardZoomId = playlistCardZoomId();
    preset.thumbnailShapeId = playlistThumbnailShapeId();
    preset.sortModeId = playlistSortModeId();
    preset.filterText = playlistSearchEdit_ != nullptr ? playlistSearchEdit_->text().trimmed() : QString {};
    preset.columns = playlistVisibleColumns();
    preset.secondaryTextVisible = playlistColumnsProduceSecondaryText(preset.columns);
    preset.showInTabs = true;

    QVector<QPair<QString, QString>> detailsTabs;
    if (detailsTabs_ != nullptr) {
        for (int index = 0; index < detailsTabs_->count(); ++index) {
            QWidget *page = detailsTabs_->widget(index);
            if (page == nullptr) {
                continue;
            }
            const QString tabId = page->property("detailsTabId").toString();
            if (!tabId.isEmpty()) {
                detailsTabs.push_back({tabId, detailsTabs_->tabText(index)});
                if (!page->property("detailsTabHidden").toBool()) {
                    preset.visibleDetailsTabs.push_back(tabId);
                }
            }
        }
        if (QWidget *currentPage = detailsTabs_->currentWidget(); currentPage != nullptr) {
            preset.currentDetailsTabId = currentPage->property("detailsTabId").toString();
        }
    }

    if (!activePlaylistViewPresetKey_.isEmpty()) {
        for (const auto &existing : loadPlaylistViewPresets(settingsController_)) {
            if (existing.key == activePlaylistViewPresetKey_) {
                preset = existing;
                preset.cardZoomId = playlistCardZoomId();
                preset.thumbnailShapeId = playlistThumbnailShapeId();
                preset.sortModeId = playlistSortModeId();
                preset.filterText = playlistSearchEdit_ != nullptr ? playlistSearchEdit_->text().trimmed() : QString {};
                preset.columns = playlistVisibleColumns();
                preset.secondaryTextVisible = playlistColumnsProduceSecondaryText(preset.columns);
                preset.visibleDetailsTabs.clear();
                for (const auto &tab : detailsTabs) {
                    if (detailsTabs_ != nullptr) {
                        for (int index = 0; index < detailsTabs_->count(); ++index) {
                            QWidget *page = detailsTabs_->widget(index);
                            if (page != nullptr && page->property("detailsTabId").toString() == tab.first && !page->property("detailsTabHidden").toBool()) {
                                preset.visibleDetailsTabs.push_back(tab.first);
                            }
                        }
                    }
                }
                if (QWidget *currentPage = detailsTabs_ != nullptr ? detailsTabs_->currentWidget() : nullptr; currentPage != nullptr) {
                    preset.currentDetailsTabId = currentPage->property("detailsTabId").toString();
                }
                break;
            }
        }
    }

    if (!editPlaylistViewPresetDialog(
            this,
            uiText("Save Playlist View"),
            uiText("Save the current playlist, details tabs, columns, sorting, and filter as a reusable view."),
            preset,
            detailsTabs,
            &preset)) {
        return;
    }

    const QString presetKey = playlistViewPresetStorageKey(preset.name);
    preset.key = presetKey;
    preset.order = loadPlaylistViewPresets(settingsController_).size();
    settingsController_->setCustomValue(
        presetKey,
        QString::fromUtf8(QJsonDocument(playlistViewPresetObject(preset)).toJson(QJsonDocument::Compact)));
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistViewActiveSetting), presetKey);
    activePlaylistViewPresetKey_ = presetKey;
    rebuildPlaylistViewTabs();
    applyPlaylistViewPreset(presetKey, true);
}

void MainWindow::editCurrentPlaylistView()
{
    if (settingsController_ == nullptr) {
        return;
    }

    if (activePlaylistViewPresetKey_.trimmed().isEmpty()) {
        saveCurrentPlaylistView();
        return;
    }

    PlaylistViewPreset preset;
    bool found = false;
    for (const auto &existing : loadPlaylistViewPresets(settingsController_)) {
        if (existing.key == activePlaylistViewPresetKey_) {
            preset = existing;
            found = true;
            break;
        }
    }
    if (!found) {
        saveCurrentPlaylistView();
        return;
    }

    QVector<QPair<QString, QString>> detailsTabs;
    if (detailsTabs_ != nullptr) {
        for (int index = 0; index < detailsTabs_->count(); ++index) {
            QWidget *page = detailsTabs_->widget(index);
            if (page != nullptr) {
                detailsTabs.push_back({page->property("detailsTabId").toString(), detailsTabs_->tabText(index)});
            }
        }
    }

    const QString oldKey = preset.key;
    if (!editPlaylistViewPresetDialog(
            this,
            uiText("Edit Playlist View"),
            uiText("Adjust the name, appearance, columns, and tab behavior for this saved view."),
            preset,
            detailsTabs,
            &preset)) {
        return;
    }

    const QString newKey = playlistViewPresetStorageKey(preset.name);
    preset.key = newKey;
    settingsController_->setCustomValue(
        newKey,
        QString::fromUtf8(QJsonDocument(playlistViewPresetObject(preset)).toJson(QJsonDocument::Compact)));
    if (newKey != oldKey) {
        settingsController_->removeCustomValue(oldKey);
    }
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistViewActiveSetting), newKey);
    activePlaylistViewPresetKey_ = newKey;
    rebuildPlaylistViewTabs();
    applyPlaylistViewPreset(newKey, true);
}

void MainWindow::applyPlaylistViewPreset(const QString &presetKey, const bool announce)
{
    if (settingsController_ == nullptr || presetKey.trimmed().isEmpty()) {
        return;
    }

    PlaylistViewPreset preset;
    bool found = false;
    for (const auto &candidate : loadPlaylistViewPresets(settingsController_)) {
        if (candidate.key == presetKey) {
            preset = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }

    playlistViewPresetApplyInProgress_ = true;
    activePlaylistViewPresetKey_ = preset.key;
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistViewActiveSetting), preset.key);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistCardZoomSetting), preset.cardZoomId);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistThumbnailShapeSetting), preset.thumbnailShapeId);
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistViewColumnsSetting), preset.columns.join(QStringLiteral(",")));
    settingsController_->setCustomValue(QString::fromLatin1(kPlaylistSortModeSetting), preset.sortModeId);

    if (playlistSearchEdit_ != nullptr) {
        playlistSearchEdit_->setText(preset.filterText);
    }
    if (playlistController_ != nullptr) {
        playlistController_->setSortMode(preset.sortModeId);
    }
    if (playlistView_ != nullptr) {
        playlistView_->setProperty("playlistCardZoom", preset.cardZoomId);
        playlistView_->setProperty("playlistThumbnailShape", preset.thumbnailShapeId);
    playlistView_->setProperty("playlistVisibleColumns", preset.columns);
        refreshPlaylistViewLayout();
    }
    if (detailsTabs_ != nullptr) {
        for (int targetIndex = 0; targetIndex < preset.visibleDetailsTabs.size(); ++targetIndex) {
            const QString desiredId = preset.visibleDetailsTabs.at(targetIndex);
            for (int currentIndex = 0; currentIndex < detailsTabs_->count(); ++currentIndex) {
                QWidget *page = detailsTabs_->widget(currentIndex);
                if (page != nullptr && page->property("detailsTabId").toString() == desiredId) {
                    detailsTabs_->tabBar()->moveTab(currentIndex, targetIndex);
                    break;
                }
            }
        }
        for (int index = 0; index < detailsTabs_->count(); ++index) {
            QWidget *page = detailsTabs_->widget(index);
            if (page == nullptr) {
                continue;
            }
            const QString tabId = page->property("detailsTabId").toString();
            const bool visible = preset.visibleDetailsTabs.isEmpty() || preset.visibleDetailsTabs.contains(tabId);
            page->setProperty("detailsTabHidden", !visible);
            if (detailsTabs_->tabBar() != nullptr) {
                detailsTabs_->tabBar()->setTabVisible(index, visible);
            }
            if (visible && !preset.currentDetailsTabId.isEmpty() && tabId == preset.currentDetailsTabId) {
                detailsTabs_->setCurrentIndex(index);
            }
        }
    }
    rebuildDetailsTabStrip();
    rebuildPlaylistViewTabs();
    playlistViewPresetApplyInProgress_ = false;
    refreshPlaylistPresentationData();
    refreshPlaylistSummary();
    refreshPlaylistInspector();
    updateAdaptiveUiLayout();
    if (announce) {
        statusBar()->showMessage(uiText("Applied playlist view: %1").arg(preset.name), 2500);
    }
}

void MainWindow::editSelectedPlaylistItemMetadata()
{
    if (playlistView_ == nullptr || settingsController_ == nullptr) {
        return;
    }

    QModelIndex index = playlistView_->currentIndex();
    if (!index.isValid() && playlistFilterModel_ != nullptr && currentPlaylistIndex_ >= 0) {
        for (int row = 0; row < playlistFilterModel_->rowCount(); ++row) {
            const QModelIndex candidate = playlistFilterModel_->index(row, 0);
            if (candidate.data(revaplayer::application::PlaylistRoles::IndexRole).toInt() == currentPlaylistIndex_) {
                index = candidate;
                break;
            }
        }
    }
    if (!index.isValid()) {
        return;
    }

    const QString source = index.data(revaplayer::application::PlaylistRoles::SourceRole).toString().trimmed();
    if (source.isEmpty()) {
        return;
    }

    MediaMetadata metadata = loadMediaMetadata(settingsController_, source);
    QDialog dialog(this);
    dialog.setWindowTitle(uiText("Edit Playlist Item Details"));
    dialog.setModal(true);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(12);
    auto *formLayout = new QFormLayout();
    auto *tagsEdit = new QLineEdit(metadata.tags.join(QStringLiteral(", ")), &dialog);
    auto *difficultyEdit = new QLineEdit(metadata.difficulty, &dialog);
    auto *notesEdit = new QPlainTextEdit(&dialog);
    notesEdit->setPlainText(metadata.notes);
    notesEdit->setPlaceholderText(uiText("Private notes, study reminders, or what to revise next"));
    formLayout->addRow(uiText("Tags"), tagsEdit);
    formLayout->addRow(uiText("Difficulty"), difficultyEdit);
    formLayout->addRow(uiText("Notes"), notesEdit);
    layout->addLayout(formLayout, 1);
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox, 0);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    MediaMetadata updated;
    for (QString tag : tagsEdit->text().split(QChar(','), Qt::SkipEmptyParts)) {
        tag = tag.trimmed();
        if (!tag.isEmpty() && !updated.tags.contains(tag)) {
            updated.tags.push_back(tag);
        }
    }
    updated.difficulty = difficultyEdit->text().trimmed();
    updated.notes = notesEdit->toPlainText().trimmed();

    if (updated.tags.isEmpty() && updated.difficulty.isEmpty() && updated.notes.isEmpty()) {
        settingsController_->removeCustomValue(mediaMetadataStorageKey(source));
    } else {
        settingsController_->setCustomValue(
            mediaMetadataStorageKey(source),
            QString::fromUtf8(QJsonDocument(mediaMetadataObject(updated)).toJson(QJsonDocument::Compact)));
    }
    refreshPlaylistPresentationData();
    refreshPlaylistInspector();
    statusBar()->showMessage(uiText("Playlist item details updated"), 2500);
}

void MainWindow::exportBookmarksAsQuizCsv()
{
    if (currentBookmarks_.isEmpty()) {
        statusBar()->showMessage(uiText("Add bookmarks before exporting a quiz file"), 3000);
        return;
    }

    const QString path = filedialog::getSaveFileName(
        this,
        uiText("Export Bookmark Quiz CSV"),
        QStringLiteral("bookmark-quiz.csv"),
        QStringLiteral("CSV Files (*.csv)"));
    if (path.trimmed().isEmpty()) {
        return;
    }

    auto csvEscape = [](QString text) {
        text.replace(QChar('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(text);
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusBar()->showMessage(uiText("Could not create the quiz export file"), 4000);
        return;
    }

    file.write("title,note,time,source,category\n");
    for (const auto &bookmark : currentBookmarks_) {
        const QString line = QStringLiteral("%1,%2,%3,%4,%5\n")
                                 .arg(csvEscape(bookmark.title))
                                 .arg(csvEscape(bookmark.note))
                                 .arg(csvEscape(formatPlaybackTime(bookmark.positionSeconds)))
                                 .arg(csvEscape(bookmark.source))
                                 .arg(csvEscape(bookmark.category));
        file.write(line.toUtf8());
    }
    file.commit();
    statusBar()->showMessage(uiText("Bookmark quiz CSV exported"), 3000);
}

void MainWindow::reloadHomeDashboard(const bool includeSavedListProgress)
{
    const auto populatePlaceholder = [](QListWidget *listWidget, const QString &text) {
        if (listWidget == nullptr) {
            return;
        }
        listWidget->clear();
        auto *item = new QListWidgetItem(text, listWidget);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    };

    if (homeDashboard_ == nullptr) {
        return;
    }

    updateHomeDashboardSectionLayout();

    const QString currentDisplayTitle = effectiveCurrentMediaTitle();
    if (homeDashboardTitleLabel_ != nullptr) {
        homeDashboardTitleLabel_->setText(mediaLoaded_ && !currentDisplayTitle.isEmpty() ? uiText("Now Playing") : uiText("Welcome back"));
    }
    if (homeDashboardSubtitleLabel_ != nullptr) {
        homeDashboardSubtitleLabel_->setText(
            mediaLoaded_ && !currentDisplayTitle.isEmpty()
                ? uiText("Current media: %1").arg(currentDisplayTitle)
                : uiText("Continue where you stopped, jump into favorites, or reopen a saved list."));
    }

    QVector<revaplayer::infrastructure::storage::PlaybackHistoryRecord> history;
    if (historyController_ != nullptr && historyController_->isReady()) {
        history = historyController_->recentHistory(60);
    }

    if (dashboardContinueList_ != nullptr && dashboardSectionEnabled(kDashboardContinueSectionSetting)) {
        dashboardContinueList_->clear();
        QSet<QString> seen;
        int added = 0;
        for (const auto &entry : history) {
            const QString source = entry.source.trimmed();
            if (source.isEmpty() || entry.completed || seen.contains(source)) {
                continue;
            }
            seen.insert(source);
            auto *item = new QListWidgetItem(
                QStringLiteral("%1\n%2").arg(displayTitleForHistory(source, entry.title), formatHistorySummary(entry)),
                dashboardContinueList_);
            item->setData(Qt::UserRole, source);
            item->setData(Qt::UserRole + 1, std::max(0.0, entry.positionSeconds));
            ++added;
            if (added >= 8) {
                break;
            }
        }
        if (added == 0) {
            populatePlaceholder(dashboardContinueList_, uiText("No unfinished media yet"));
        }
    }

    if (dashboardRecentList_ != nullptr && dashboardSectionEnabled(kDashboardRecentSectionSetting)) {
        dashboardRecentList_->clear();
        int added = 0;
        for (const auto &entry : history) {
            const QString source = entry.source.trimmed();
            if (source.isEmpty()) {
                continue;
            }
            auto *item = new QListWidgetItem(
                QStringLiteral("%1\n%2").arg(displayTitleForHistory(source, entry.title), formatHistoryTimestamp(entry.lastOpenedAt)),
                dashboardRecentList_);
            item->setData(Qt::UserRole, source);
            ++added;
            if (added >= 8) {
                break;
            }
        }
        if (added == 0) {
            populatePlaceholder(dashboardRecentList_, uiText("No recent media yet"));
        }
    }

    if (dashboardFavoritesList_ != nullptr && dashboardSectionEnabled(kDashboardFavoritesSectionSetting)) {
        dashboardFavoritesList_->clear();
        int added = 0;
        if (settingsController_ != nullptr) {
            const QStringList keys = settingsController_->customKeys(QString::fromLatin1(kFavoriteMediaPrefix));
            for (const QString &key : keys) {
                const QJsonObject object = QJsonDocument::fromJson(settingsController_->customValue(key).toUtf8()).object();
                const QString source = object.value(QStringLiteral("source")).toString().trimmed();
                if (source.isEmpty()) {
                    continue;
                }
                auto *item = new QListWidgetItem(
                    displayTitleForHistory(source, object.value(QStringLiteral("title")).toString()),
                    dashboardFavoritesList_);
                item->setData(Qt::UserRole, source);
                ++added;
            }
        }
        if (added == 0) {
            populatePlaceholder(dashboardFavoritesList_, uiText("No favorites pinned yet"));
        }
    }

    if (dashboardPinnedCoursesList_ != nullptr && dashboardSectionEnabled(kDashboardSavedListsSectionSetting)) {
        dashboardPinnedCoursesList_->clear();
        const QVector<PinnedCourseFolder> courses = loadPinnedCourses(settingsController_);
        for (const auto &course : courses) {
            QStringList detailParts;
            if (!course.category.trimmed().isEmpty()) {
                detailParts.push_back(course.category.trimmed());
            }
            if (!course.description.trimmed().isEmpty()) {
                detailParts.push_back(course.description.trimmed());
            }
            if (includeSavedListProgress && progressTrackingModeEnabled()) {
                const QStringList sources = supportedMediaFilesInDirectory(QDir(course.path), naturalSortFolderPlaylistEnabled());
                int totalProgress = 0;
                int counted = 0;
                for (const QString &source : sources) {
                    const auto entryIt = historyEntriesBySource_.constFind(source);
                    if (entryIt == historyEntriesBySource_.constEnd()) {
                        continue;
                    }
                    const auto &entry = entryIt.value();
                    const int progress = entry.completed || entry.durationSeconds <= 0.0
                        ? (entry.completed ? 100 : 0)
                        : std::clamp(static_cast<int>(std::round((entry.positionSeconds / entry.durationSeconds) * 100.0)), 0, 100);
                    totalProgress += progress;
                    ++counted;
                }
                if (counted > 0) {
                    detailParts.push_back(uiText("%1% complete").arg(totalProgress / counted));
                }
            }
            const QString text = detailParts.isEmpty()
                ? course.label
                : QStringLiteral("%1\n%2").arg(course.label, detailParts.join(QStringLiteral("  •  ")));
            auto *item = new QListWidgetItem(text, dashboardPinnedCoursesList_);
            item->setData(Qt::UserRole, course.path);
            item->setIcon(playlistViewIcon(
                dashboardPinnedCoursesList_,
                course.iconId.trimmed().isEmpty() ? QStringLiteral("folder") : course.iconId));
            item->setForeground(accentColorForId(course.colorId));
            item->setToolTip(
                detailParts.isEmpty()
                    ? course.path
                    : QStringLiteral("%1\n%2").arg(course.path, detailParts.join(QStringLiteral("  •  "))));
        }
        if (dashboardPinnedCoursesList_->count() == 0) {
            populatePlaceholder(dashboardPinnedCoursesList_, uiText("Save folders from the playlist panel"));
        }
    }
}

void MainWindow::applyStartupCanvasAppearance()
{
    QWidget *mainCanvas = centralWidget();
    if (mainCanvas == nullptr || videoViewportRow_ == nullptr || videoViewport_ == nullptr) {
        return;
    }

    const QString themeId = selectedThemeId();
    const QString accentId = uiAccentId();
    const QString canvasStyleId = startupCanvasStyleId();
    const QColor appBg = revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("APP_BG"));
    const QColor appBgAlt = revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("APP_BG_ALT"));
    const QColor appBgElevated = revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("APP_BG_ELEVATED"));
    const QColor surfaceBg = revaplayer::application::resolvedThemeColor(themeId, accentId, QStringLiteral("SURFACE_BG"));
    const bool lightCanvas = surfaceBg.isValid() && surfaceBg.lightness() >= 190;

    const QColor mainStart = appBg;
    const QColor mainEnd = appBgAlt.isValid() ? appBgAlt : appBg;
    const QColor rowInner = appBgAlt.isValid() ? appBgAlt : mainEnd;
    const QColor rowMid = appBg;
    const QColor rowOuter = appBg;
    QColor viewportStart = appBgElevated.isValid() ? appBgElevated : appBg;
    QColor viewportEnd = mainEnd;

    if (canvasStyleId == QStringLiteral("black")) {
        viewportStart = QColor(QStringLiteral("#020202"));
        viewportEnd = QColor(QStringLiteral("#000000"));
    } else if (canvasStyleId == QStringLiteral("warm")) {
        if (lightCanvas) {
            viewportStart = blendColors(
                appBgElevated.isValid() ? appBgElevated : surfaceBg,
                QColor(QStringLiteral("#fff0d8")),
                168);
            viewportEnd = blendColors(
                surfaceBg,
                QColor(QStringLiteral("#dfb06a")),
                118);
        } else {
            viewportStart = blendColors(
                appBgElevated.isValid() ? appBgElevated : appBg,
                QColor(QStringLiteral("#4a2b11")),
                146);
            viewportEnd = blendColors(
                appBg,
                QColor(QStringLiteral("#120804")),
                84);
        }
    }

    const QString mainBackground = QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:0.55 %1, stop:1 %2)")
        .arg(cssColor(mainStart), cssColor(mainEnd));
    const QString rowBackground = QStringLiteral(
        "qradialgradient(cx:0.52, cy:0.38, radius:0.92, fx:0.52, fy:0.38, stop:0 %1, stop:0.48 %2, stop:1 %3)")
        .arg(cssColor(rowInner), cssColor(rowMid), cssColor(rowOuter));
    const QString viewportBackground = QStringLiteral(
        "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2)")
        .arg(cssColor(viewportStart), cssColor(viewportEnd));

    setScopedBackgroundStyle(mainCanvas, mainBackground);
    setScopedBackgroundStyle(videoViewportRow_, rowBackground);
    setScopedBackgroundStyle(videoViewport_, viewportBackground);
    setScopedBackgroundStyle(compareVideoViewport_, viewportBackground);
}

void MainWindow::applyCustomPlaylistThemeColors()
{
    const QString themeId = selectedThemeId();
    const QString accentId = uiAccentId();
    for (QWidget *widget : {static_cast<QWidget *>(playlistView_),
                            static_cast<QWidget *>(favoritesList_),
                            static_cast<QWidget *>(bookmarksList_)}) {
        applyPlaylistThemeProperties(widget, themeId, accentId);
    }
    if (playlistView_ != nullptr && playlistView_->viewport() != nullptr) {
        playlistView_->viewport()->update();
    }
    if (favoritesList_ != nullptr && favoritesList_->viewport() != nullptr) {
        favoritesList_->viewport()->update();
    }
    if (bookmarksList_ != nullptr && bookmarksList_->viewport() != nullptr) {
        bookmarksList_->viewport()->update();
    }
}

void MainWindow::updateHomeDashboardSectionLayout()
{
    if (homeDashboardGridLayout_ == nullptr) {
        return;
    }

    QVector<QWidget *> visibleSections;
    const auto collectSection = [&visibleSections](QListWidget *listWidget, const bool visible) {
        QWidget *section = listWidget != nullptr ? listWidget->parentWidget() : nullptr;
        if (section == nullptr) {
            return;
        }

        section->setVisible(visible);
        if (visible) {
            visibleSections.push_back(section);
        }
    };

    collectSection(dashboardContinueList_, dashboardSectionEnabled(kDashboardContinueSectionSetting));
    collectSection(dashboardRecentList_, dashboardSectionEnabled(kDashboardRecentSectionSetting));
    collectSection(dashboardFavoritesList_, dashboardSectionEnabled(kDashboardFavoritesSectionSetting));
    collectSection(dashboardPinnedCoursesList_, dashboardSectionEnabled(kDashboardSavedListsSectionSetting));

    while (QLayoutItem *item = homeDashboardGridLayout_->takeAt(0)) {
        delete item;
    }

    for (int index = 0; index < visibleSections.size(); ++index) {
        homeDashboardGridLayout_->addWidget(visibleSections.at(index), index / 2, index % 2);
    }
    homeDashboardGridLayout_->setColumnStretch(0, 1);
    homeDashboardGridLayout_->setColumnStretch(1, 1);
}

void MainWindow::updateHomeDashboardVisibility()
{
    if (homeDashboard_ == nullptr || videoViewport_ == nullptr) {
        return;
    }

    const bool idleCanvasVisible = !mediaLoaded_
        && !loadingMedia_
        && !errorStateActive_;
    videoViewport_->setRenderHostVisible(!idleCanvasVisible);
    if (compareVideoViewport_ != nullptr) {
        compareVideoViewport_->setRenderHostVisible(compareVideoViewport_->isVisible());
    }

    const bool visible = dashboardEnabled()
        && dashboardShowWhenIdle()
        && idleCanvasVisible
        && !pinnedCourseBrowserActive_;
    homeDashboard_->setVisible(visible);
    if (visible) {
        videoViewport_->setOverlayVisible(false);
        homeDashboard_->raise();
    }
}

void MainWindow::showFirstRunWizardIfNeeded()
{
    if (firstRunPromptShown_ || settingsController_ == nullptr) {
        return;
    }

    firstRunPromptShown_ = true;
    if (customSettingFlag(settingsController_, kFirstRunCompletedSetting, false)) {
        updateHomeDashboardVisibility();
        return;
    }

    const QString previousLanguage = revaplayer::application::currentUiLanguage();
    const Qt::LayoutDirection previousDirection = QApplication::layoutDirection();
    revaplayer::ui::FirstRunDialog dialog(settingsController_, this);

    if (dialog.exec() != QDialog::Accepted) {
        revaplayer::application::setCurrentUiLanguage(previousLanguage);
        QApplication::setLayoutDirection(previousDirection);
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        settingsController_->setCustomValue(QString::fromLatin1(kFirstRunCompletedSetting), QStringLiteral("1"));
        updateHomeDashboardVisibility();
        return;
    }

    dialog.applySelections();
    settingsController_->setCustomValue(QString::fromLatin1(kFirstRunCompletedSetting), QStringLiteral("1"));

    const QString appliedLanguage = settingsController_->interfaceLanguage();
    revaplayer::application::setCurrentUiLanguage(appliedLanguage);
    QApplication::setLayoutDirection(revaplayer::application::currentUiLanguageDirection());
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    applySelectedTheme(false);
    applyUiPreferences();
    if (menuBar() != nullptr) {
        menuBar()->clear();
        setupMenuBar();
    }
    refreshPlaylistPresentationData();
    rebuildPinnedCourseTabs();
    reloadHomeDashboard(true);
    updateHomeDashboardVisibility();
}

void MainWindow::trimHistoryToLimit()
{
    if (historyController_ == nullptr || !historyController_->isReady() || !historyEnabled()) {
        return;
    }

    historyController_->trimHistory(historyLimit());
}

void MainWindow::populateChapters(const QVector<revaplayer::domain::ChapterInfo> &chapters, const int currentIndex)
{
    chaptersList_->clear();

    for (const auto &chapter : chapters) {
        auto *item = new QListWidgetItem(chapter.title, chaptersList_);
        item->setData(Qt::UserRole, chapter.index);
        if (chapter.index == currentIndex) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            chaptersList_->setCurrentItem(item);
        }
    }
}

void MainWindow::populateBookmarks(const QVector<revaplayer::domain::Bookmark> &bookmarks)
{
    currentBookmarks_ = bookmarks;
    bookmarksList_->clear();
    bookmarkThumbnailQueue_.clear();
    bookmarkRowsByBucket_.clear();

    QVector<revaplayer::ui::TimelineMarker> timelineMarkers;
    timelineMarkers.reserve(bookmarks.size());

    for (const auto &bookmark : bookmarks) {
        const QString title = bookmark.title.trimmed().isEmpty()
            ? uiText("Bookmark")
            : bookmark.title.trimmed();
        const QString category = bookmark.category.trimmed();
        const QString label = category.isEmpty()
            ? QStringLiteral("%1  %2").arg(formatPlaybackTime(bookmark.positionSeconds), title)
            : QStringLiteral("[%1] %2  %3").arg(category, formatPlaybackTime(bookmark.positionSeconds), title);
        auto *item = new QListWidgetItem(label, bookmarksList_);
        item->setData(Qt::UserRole, bookmark.id);
        item->setData(Qt::UserRole + 1, bookmark.positionSeconds);
        item->setData(Qt::UserRole + 2, title);
        item->setData(Qt::UserRole + 3, bookmark.note.trimmed());
        item->setData(Qt::UserRole + 4, category);
        item->setData(Qt::UserRole + 6, bookmark.source.trimmed());
        QImage cachedThumbnail;
        if (thumbnailService_ != nullptr
            && thumbnailService_->loadCachedThumbnail(bookmark.source, bookmark.positionSeconds, &cachedThumbnail)
            && !cachedThumbnail.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(cachedThumbnail)));
            item->setData(Qt::UserRole + 5, cachedThumbnail);
        } else {
            item->setIcon(buildScenePlaceholderIcon(QSize(160, 90), formatPlaybackTime(bookmark.positionSeconds)));
        }
        if (!bookmark.note.trimmed().isEmpty()) {
            item->setToolTip(
                category.isEmpty()
                    ? bookmark.note.trimmed()
                    : QStringLiteral("%1\n%2").arg(category, bookmark.note.trimmed()));
        }

        const qint64 bucketMilliseconds = revaplayer::services::media::ThumbnailService::bucketMillisecondsFor(bookmark.positionSeconds);
        const int row = bookmarksList_->count() - 1;
        bookmarkRowsByBucket_[bucketMilliseconds].push_back(row);
        if (!item->data(Qt::UserRole + 5).canConvert<QImage>()) {
            bookmarkThumbnailQueue_.push_back(bucketMilliseconds);
        }
        timelineMarkers.push_back(revaplayer::ui::TimelineMarker {
            bookmark.positionSeconds,
            title,
        });
    }

    if (controlBar_ != nullptr) {
        controlBar_->setTimelineMarkers(timelineMarkers);
    }
    refreshBookmarkFilters();
    updateBookmarkSelectionPreview();
    requestNextBookmarkThumbnail();
    updateActionStates();
}

void MainWindow::populateTracks(const QVector<revaplayer::domain::TrackInfo> &tracks)
{
    tracksTree_->clear();

    auto *videoRoot = new QTreeWidgetItem(QStringList {uiText("Video")});
    auto *audioRoot = new QTreeWidgetItem(QStringList {uiText("Audio")});
    auto *subtitleRoot = new QTreeWidgetItem(QStringList {uiText("Subtitles")});

    tracksTree_->addTopLevelItem(videoRoot);
    tracksTree_->addTopLevelItem(audioRoot);
    tracksTree_->addTopLevelItem(subtitleRoot);

    QVector<revaplayer::domain::TrackInfo> orderedTracks = tracks;
    std::stable_sort(orderedTracks.begin(), orderedTracks.end(), [](const auto &left, const auto &right) {
        if (left.type != right.type) {
            return static_cast<int>(left.type) < static_cast<int>(right.type);
        }
        if (left.type == revaplayer::domain::TrackType::Video) {
            if (left.height != right.height) {
                return left.height > right.height;
            }
            if (std::lround(left.fps) != std::lround(right.fps)) {
                return std::lround(left.fps) > std::lround(right.fps);
            }
            if (left.width != right.width) {
                return left.width > right.width;
            }
        }
        return left.id < right.id;
    });

    for (const auto &track : orderedTracks) {
        QTreeWidgetItem *parentItem = nullptr;
        switch (track.type) {
        case revaplayer::domain::TrackType::Video:
            parentItem = videoRoot;
            break;
        case revaplayer::domain::TrackType::Audio:
            parentItem = audioRoot;
            break;
        case revaplayer::domain::TrackType::Subtitle:
            parentItem = subtitleRoot;
            break;
        case revaplayer::domain::TrackType::Unknown:
        default:
            continue;
        }

        auto *item = new QTreeWidgetItem(parentItem, QStringList {buildTrackTitle(track)});
        item->setData(0, Qt::UserRole, track.id);
        item->setData(0, Qt::UserRole + 1, static_cast<int>(track.type));
        if (track.selected) {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
            tracksTree_->setCurrentItem(item);
        }
    }

    videoRoot->setExpanded(true);
    audioRoot->setExpanded(true);
    subtitleRoot->setExpanded(true);
}

}  // namespace revaplayer::ui
