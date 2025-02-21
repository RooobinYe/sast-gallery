#include "DiskScanner.h"
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

DiskScanner::DiskScanner(QObject* parent)
    : QObject(parent) {
    connect(&diskWatcher, &QFileSystemWatcher::directoryChanged, [this](const QString& path) {
        scanPath(path);
        submitChange();
    });

    // monitor file change
    connect(&diskWatcher, &QFileSystemWatcher::fileChanged, [this](const QString& path) {
        QFileInfo fileInfo(path);
        QString dirPath = fileInfo.dir().absolutePath();

        // if the file still exists, add it to the modified list
        if (fileInfo.exists()) {
            pendingModified.append(path);
            // add the file to the watcher again (some systems delete the watcher after file modification)
            if (!diskWatcher.files().contains(path)) {
                diskWatcher.addPath(path);
            }
        }
        submitChange();
    });

    addPaths(QStandardPaths::standardLocations(QStandardPaths::PicturesLocation));
    addPaths(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation));
}

void DiskScanner::addPath(const QString& path) {
    if (searchPath.contains(path)) {
        return;
    }

    searchPath.append(path);

    QStringList pendingPath;
    pendingPath.append(path);

    // recursively traverse the directory
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        pendingPath.append(it.next());
    }

    // add paths to monitor
    for (const auto& path : pendingPath) {
        if (diskWatcher.directories().contains(path)) {
            continue;
        }
        diskWatcher.addPath(path);
        qInfo() << "DiskScanner: path added to disk watcher: " << path;
    }
}

void DiskScanner::addPaths(const QStringList& paths) {
    for (const auto& path : paths) {
        addPath(path);
    }
}

void DiskScanner::removePath(const QString& path) {
    if (!searchPath.removeOne(path)) {
        return;
    }

    QStringList pendingPath;
    pendingPath.append(path);

    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        pendingPath.append(it.next());
    }

    for (auto& path : pendingPath) {
        if (!diskWatcher.directories().contains(path)) {
            continue;
        }
        diskWatcher.removePath(path);
        scanPath(path);

        qInfo() << "DiskScanner: path removed from disk watcher: " << path;
    }
    submitChange();
}

void DiskScanner::removePaths(const QStringList& paths) {
    for (const auto& path : paths) {
        removePath(path);
    }
}

QStringList DiskScanner::path() {
    return searchPath;
}

void DiskScanner::scan(bool fullScan) {
    static bool isInitScan = true;
    fullScan = fullScan || isInitScan;
    isInitScan = false;
    for (auto& path : diskWatcher.directories()) {
        scanPath(path, fullScan);
    }
    submitChange(fullScan);
}

void DiskScanner::scanPath(const QString& path, bool fullScan) {
    if (!diskWatcher.directories().contains(path)) {
        // run remove
        pendingDeleted += cache.take(path);
        // Clean up modCache for removed path
        modCache.remove(path);
        return;
    }

    qDebug() << "DiskScanner: scanning " << path;
    QStringList oldCache = fullScan ? QStringList{} : cache.value(path);
    QMap<QString, QDateTime> oldModMap = fullScan ? QMap<QString, QDateTime>{}
                                                  : modCache.value(path);

    QStringList newCache;
    QMap<QString, QDateTime> newModMap;

    auto&& entryInfoList = QDir(path).entryInfoList(mediaFileFilter,
                                                    QDir::Files | QDir::NoDotAndDotDot);
    for (auto& entry : entryInfoList) {
        QString filePath = entry.absoluteFilePath();
        newCache += filePath;
        newModMap.insert(filePath, entry.lastModified());

        // ensure the file is monitored
        if (!diskWatcher.files().contains(filePath)) {
            diskWatcher.addPath(filePath);
            qDebug() << "DiskScanner: file added to watcher during scan:" << filePath;
        }

        // Check for modified files
        if (!fullScan && oldModMap.contains(filePath)) {
            if (oldModMap.value(filePath) != entry.lastModified()) {
                pendingModified += filePath;
            }
        }
    }

    cache.insert(path, newCache);
    modCache.insert(path, newModMap);

    auto&& [added, removed] = diff(oldCache, newCache);
    pendingCreated += added;
    pendingDeleted += removed;

    // remove the monitor of the non-existent files
    for (const auto& removedFile : removed) {
        if (diskWatcher.files().contains(removedFile)) {
            diskWatcher.removePath(removedFile);
        }
    }
}

void DiskScanner::submitChange(bool fullScan) {
    if (fullScan) {
        emit DiskScanner::fullScan(pendingCreated);
        pendingCreated.clear();
        pendingDeleted.clear();
        pendingModified.clear();
        return;
    }
    if (pendingCreated.size() != 0) {
        emit fileCreated(pendingCreated);
        pendingCreated.clear();
    }
    if (pendingDeleted.size() != 0) {
        emit fileDeleted(pendingDeleted);
        pendingDeleted.clear();
    }
    if (pendingModified.size() != 0) {
        emit fileModified(pendingModified);
        pendingModified.clear();
    }
}

DiskScanner::DiffResult DiskScanner::diff(const QStringList& oldv, const QStringList& newv) {
    QSet olds(oldv.begin(), oldv.end());
    QSet news(newv.begin(), newv.end());
    DiffResult res;
    res.added = [=]() {
        auto res = news - olds;
        return QStringList(res.begin(), res.end());
    }();
    res.removed = [=]() {
        auto res = olds - news;
        return QStringList(res.begin(), res.end());
    }();
    return res;
}
