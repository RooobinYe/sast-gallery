#pragma once

#include "../model/Media.h"
#include <QAbstractItemModel>
#include <QImage>
#include <QPersistentModelIndex>
#include <QVBoxLayout>
#include <QWheelEvent>

class MediaViewer;

class MediaViewerDelegate : public QObject {
    Q_OBJECT

public:
    explicit MediaViewerDelegate(QAbstractItemModel* model,
                                 int index,
                                 MediaViewer* viewer,
                                 QObject* parent = nullptr);

    [[nodiscard]] auto getFilePath() const { return filepath; }
    [[nodiscard]] auto getImage() const { return this->image; }
    void init();
    void initConnections();

signals:
    void scaledByWheel();
    void mediaChanged(bool fadeAnimation = true);

public slots:
    void onModelRowsToBeRemoved(const QModelIndex& parent, int first, int last);
    void onMediaChanged(bool fadeAnimation = true);
    void onWheelScrolled(int delta);
    bool copyImageToClipboard();
    void openImageFileDialog();
    void saveImageFileDialog();
    void onLikeButtonClicked();
    void onFileInfoClicked();
    void adaptiveResize();
    void deleteMedia();
    void prevMedia();
    void nextMedia();
    void rotateMedia();
    void openInFileExplorer();

private:
    QAbstractItemModel* mediaListModel;
    QPersistentModelIndex mediaIndex;
    QImage image;
    QString filepath;
    MediaViewer* view;
    QVBoxLayout* layout;

    bool loadImage(const QString& path, bool fadeAnimation = true);
    bool loadImage(const QImage& image, bool fadeAnimation = true);
    bool loadVideo(const QString& path, bool fadeAnimation = true);
    bool loadMedia(const QString& path, bool fadeAnimation = true);
    void scaleTo(int percent);
    [[nodiscard]] int getScale() const;
    void updateLikeButtonState();
};