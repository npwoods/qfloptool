/***************************************************************************

	imageitemmodel.h

	QAbstractItemModel implementation for an image

***************************************************************************/

#ifndef IMAGEITEMMODEL_H
#define IMAGEITEMMODEL_H

// qfloptool headers
#include "floptool.h"

// Qt headers
#include <QAbstractItemModel>
#include <QPixmap>


QT_BEGIN_NAMESPACE
class QDir;
QT_END_NAMESPACE


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> ImageItemModel

class ImageItemModel : public QAbstractItemModel
{
public:
	// ctor/dtor
	ImageItemModel(Floptool::Image::ptr &&image, QObject *parent = nullptr);
	~ImageItemModel();

	// accessors
	Floptool::Image::ptr &image() { return m_image; }

	// methods
	void loadItem(const QModelIndex &index);
	void setExpanded(const QModelIndex &index, bool expanded);
	Floptool::Image::ptr detachImage();
	QString fileName(const QModelIndex &index) const;
	void extract(const QModelIndex &index, const QString &path, bool appendImageFileName);

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override final;
	virtual QModelIndex parent(const QModelIndex &child) const override final;
	virtual int rowCount(const QModelIndex &parent) const override final;
	virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override final;
	virtual bool hasChildren(const QModelIndex &parent) const final;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override final;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override final;
	virtual QVariant data(const QModelIndex &index, int role) const override final;

private:
	enum class EntryType
	{
		Directory,
		File
	};

	struct DirectoryEntry;
	struct Directory;
	struct Info;

	// members
	Floptool::Image::ptr			m_image;
	std::unique_ptr<Info>			m_info;
	QPixmap							m_fileIcon;
	QPixmap							m_folderIcon;
	QPixmap							m_folderOpenIcon;

	// private methods
	int loadDirectory(int parentIndex, int parentEntryIndex);
	void appendDirectoryPath(std::vector<std::string> &path, int directoryIndex) const;
	const DirectoryEntry *findDirectoryEntry(const QModelIndex &index) const; 
	DirectoryEntry *findDirectoryEntry(const QModelIndex &index);
	QString convert(std::string_view s) const;
	QPixmap iconFromDirectoryEntry(const DirectoryEntry &directoryEntry) const;
	void internalExtract(std::vector<std::string> &pathOnImage, int directoryIndex, int directoryEntryIndex, const QString &path);
};


#endif // IMAGEITEMMODEL_H
