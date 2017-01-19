/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_MODEL
#define QGBA_LIBRARY_MODEL

#include <QAbstractItemModel>
#include <QStringList>
#include <QThread>

#include <mgba/core/library.h>

struct VDir;
struct VFile;

namespace QGBA {

class LibraryLoader;
class LibraryModel : public QAbstractItemModel {
Q_OBJECT

public:
	LibraryModel(const QString& path, QObject* parent = nullptr);
	virtual ~LibraryModel();

	bool entryAt(int row, mLibraryEntry* out) const;
	VFile* openVFile(const QModelIndex& index) const;

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

signals:
	void doneLoading();

public slots:
	void loadDirectory(const QString& path);

	void constrainBase(const QString& path);
	void clearConstraints();

private slots:
	void directoryLoaded(const QString& path);

private:
	mLibrary* m_library;
	mLibraryEntry m_constraints;
	LibraryLoader* m_loader;
	QThread m_loaderThread;
	QStringList m_queue;
};

class LibraryLoader : public QObject {
Q_OBJECT

public:
	LibraryLoader(mLibrary* library, QObject* parent = nullptr);

public slots:
	void loadDirectory(const QString& path);

signals:
	void directoryLoaded(const QString& path);

private:
	mLibrary* m_library;
};

}

#endif
