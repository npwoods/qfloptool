/***************************************************************************

	imageitemmodel.cpp

	QAbstractItemModel implementation for an image

***************************************************************************/

// qfloptool headers
#include "imageitemmodel.h"

// MAME headers
#include "formats/fsmgr.h"

// Qt headers
#include <QDir>
#include <QFont>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

struct ImageItemModel::DirectoryEntry
{
	EntryType					m_type;
	std::string					m_name;
	int							m_directoryIndex;
	fs::meta_data				m_metadata;
	bool						m_isExpanded;
};

struct ImageItemModel::Directory
{
	int							m_parentIndex;
	int							m_parentEntryIndex;
	std::vector<DirectoryEntry>	m_children;
};

struct ImageItemModel::Info
{
	std::vector<fs::meta_name>	m_metaNames;
	std::vector<Directory>		m_directories;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  loadIcon
//-------------------------------------------------

static QPixmap loadIcon(const QString &resourceName)
{
	// instantiate the QPixmap
	QPixmap pixmap(resourceName);

	// size it accordingly
	QSize size(16, 16);
	qreal xScaleFactor = pixmap.width() / (qreal)size.width();
	qreal yScaleFactor = pixmap.height() / (qreal)size.height();
	qreal scaleFactor = std::max(xScaleFactor, yScaleFactor);
	pixmap.setDevicePixelRatio(scaleFactor);

	// and return it
	return pixmap;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImageItemModel::ImageItemModel(Floptool::Image::ptr &&image, QObject *parent)
	: QAbstractItemModel(parent)
	, m_image(std::move(image))
	, m_fileIcon(loadIcon(":/resources/file.png"))
	, m_folderIcon(loadIcon(":/resources/folder.png"))
	, m_folderOpenIcon(loadIcon(":/resources/folder_open.png"))
{
	// allocate internal info
	m_info = std::make_unique<Info>();

	// load the directory
	loadDirectory(-1, -1);

	// identify all metadata
	m_info->m_metaNames.reserve(64);
	for (const fs::meta_description &desc : m_image->fileSystem().mameManager().file_meta_description())
	{
		m_info->m_metaNames.push_back(desc.m_name);
	}
	for (const fs::meta_description &desc : m_image->fileSystem().mameManager().directory_meta_description())
	{
		if (std::ranges::find(m_info->m_metaNames, desc.m_name) == m_info->m_metaNames.end())
			m_info->m_metaNames.push_back(desc.m_name);
	}
	m_info->m_metaNames.shrink_to_fit();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ImageItemModel::~ImageItemModel()
{
}


//-------------------------------------------------
//  loadDirectory
//-------------------------------------------------

int ImageItemModel::loadDirectory(int parentIndex, int parentEntryIndex)
{
	// sanity checks
	assert(m_image);

	int newDirectoryIndex = m_info->m_directories.size();
	Directory &newDirectory = m_info->m_directories.emplace_back();
	newDirectory.m_parentIndex = parentIndex;
	newDirectory.m_parentEntryIndex = parentEntryIndex;

	std::vector<std::string> path;
	appendDirectoryPath(path, newDirectoryIndex);
	auto dirContents = m_image->mameFileSystem().directory_contents(path);

	newDirectory.m_children.reserve(dirContents.second.size());
	for (auto &mameChild : dirContents.second)
	{
		// load metadata
		std::vector<std::string> childPath = path;
		childPath.push_back(std::move(mameChild.m_name));
		auto metadataResult = m_image->mameFileSystem().metadata(childPath);

		// create child entry
		DirectoryEntry &child = newDirectory.m_children.emplace_back();
		switch (mameChild.m_type)
		{
		case fs::dir_entry_type::file:
			child.m_type = EntryType::File;
			break;
		case fs::dir_entry_type::dir:
			child.m_type = EntryType::Directory;
			break;
		default:
			throw false;
		}

		// set up child
		child.m_name = std::move(childPath[childPath.size() - 1]);
		child.m_directoryIndex = -1;
		child.m_metadata = std::move(metadataResult.second);
		child.m_isExpanded = false;
	}

	if (parentIndex >= 0 && parentEntryIndex >= 0)
		m_info->m_directories[parentIndex].m_children[parentEntryIndex].m_directoryIndex = newDirectoryIndex;
	return newDirectoryIndex;
}


//-------------------------------------------------
//  appendDirectoryPath
//-------------------------------------------------

void ImageItemModel::appendDirectoryPath(std::vector<std::string> &path, int directoryIndex) const
{
	if (directoryIndex > 0)
	{
		int parentIndex = m_info->m_directories[directoryIndex].m_parentIndex;
		int parentEntryIndex = m_info->m_directories[directoryIndex].m_parentEntryIndex;
		appendDirectoryPath(path, parentIndex);
		path.push_back(m_info->m_directories[parentIndex].m_children[parentEntryIndex].m_name);
	}
}


//-------------------------------------------------
//  loadItem
//-------------------------------------------------

void ImageItemModel::loadItem(const QModelIndex &index)
{
	DirectoryEntry *parentDirectoryEntry = findDirectoryEntry(index);
	if (parentDirectoryEntry && parentDirectoryEntry->m_type == EntryType::Directory && parentDirectoryEntry->m_directoryIndex < 0)
	{
		parentDirectoryEntry->m_directoryIndex = loadDirectory(index.internalId(), index.row());
		emit dataChanged(index, index);
	}
}


//-------------------------------------------------
//  setExpanded
//-------------------------------------------------

void ImageItemModel::setExpanded(const QModelIndex &index, bool expanded)
{
	DirectoryEntry *directoryEntry = findDirectoryEntry(index);
	if (directoryEntry && expanded != directoryEntry->m_isExpanded)
	{
		directoryEntry->m_isExpanded = expanded;
		emit dataChanged(index, index, { Qt::DecorationRole });
	}
}


//-------------------------------------------------
//  detachImage
//-------------------------------------------------

Floptool::Image::ptr ImageItemModel::detachImage()
{
	assert(m_image);
	return std::move(m_image);
}


//-------------------------------------------------
//  findDirectoryEntry
//-------------------------------------------------

const ImageItemModel::DirectoryEntry *ImageItemModel::findDirectoryEntry(const QModelIndex &index) const
{
	const DirectoryEntry *result = nullptr;
	if (index.isValid())
	{
		const Directory &parentDirectory = m_info->m_directories[index.internalId()];
		const DirectoryEntry &directoryEntry = parentDirectory.m_children[index.row()];
		result = &directoryEntry;
	}
	return result;
}


//-------------------------------------------------
//  findDirectoryEntry
//-------------------------------------------------

ImageItemModel::DirectoryEntry *ImageItemModel::findDirectoryEntry(const QModelIndex &index)
{
	const ImageItemModel &constThis = *this;
	return const_cast<DirectoryEntry *>(constThis.findDirectoryEntry(index));
}


//-------------------------------------------------
//  convert
//-------------------------------------------------

QString ImageItemModel::convert(std::string_view s) const
{
	return m_image->convert(s);
}


//-------------------------------------------------
//  textAlignmentFromMetaType
//-------------------------------------------------

static Qt::AlignmentFlag textAlignmentFromMetaType(fs::meta_type type)
{
	Qt::AlignmentFlag result;
	switch (type)
	{
	case fs::meta_type::number:
		result = Qt::AlignmentFlag::AlignRight;
		break;
	case fs::meta_type::date:
	case fs::meta_type::flag:
	case fs::meta_type::string:
	default:
		result = Qt::AlignmentFlag::AlignLeft;
		break;
	}
	return result;
}


//-------------------------------------------------
//  iconFromDirectoryEntry
//-------------------------------------------------

QPixmap ImageItemModel::iconFromDirectoryEntry(const DirectoryEntry &directoryEntry) const
{
	QPixmap result;
	switch (directoryEntry.m_type)
	{
	case EntryType::File:
		result = m_fileIcon;
		break;
	case EntryType::Directory:
		result = directoryEntry.m_isExpanded ? m_folderOpenIcon : m_folderIcon;
		break;
	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  fileName
//-------------------------------------------------

QString ImageItemModel::fileName(const QModelIndex &index) const
{
	const DirectoryEntry *entry = findDirectoryEntry(index);
	return entry
		? convert(entry->m_name)
		: "";
}


//-------------------------------------------------
//  extract
//-------------------------------------------------

void ImageItemModel::extract(const QModelIndex &index, const QString &path, bool appendImageFileName)
{
	// look up the directory entry
	if (!index.isValid())
		return;
	int directoryIndex = index.internalId();
	int directoryEntryIndex = index.row();
	const DirectoryEntry &directoryEntry = m_info->m_directories[directoryIndex].m_children[directoryEntryIndex];

	// determine the path
	std::vector<std::string> pathOnImage;
	appendDirectoryPath(pathOnImage, index.internalId());
	pathOnImage.push_back(directoryEntry.m_name);

	// recursively extract the image
	internalExtract(pathOnImage, directoryIndex, directoryEntryIndex,
		appendImageFileName ? QString("%1/%2").arg(path, convert(directoryEntry.m_name)) : path);
}


//-------------------------------------------------
//  internalExtract
//-------------------------------------------------

void ImageItemModel::internalExtract(std::vector<std::string> &pathOnImage, int directoryIndex, int directoryEntryIndex, const QString &path)
{
	const DirectoryEntry &directoryEntry = m_info->m_directories[directoryIndex].m_children[directoryEntryIndex];
	switch (directoryEntry.m_type)
	{
	case EntryType::File:
		{
			// this a file; get the bytes off the image
			auto [err, bytes] = m_image->mameFileSystem().file_read(pathOnImage);
			if (err)
				return;

			// and write it locally
			QFile file(path);
			if (file.open(QIODevice::WriteOnly))
				file.write((const char *) &bytes[0], bytes.size());
		}
		break;

	case EntryType::Directory:
		// this is a directory; first thing we need to do is create the local directory
		if (!QDir(path).exists() && !QDir().mkdir(path))
			return;

		// the entry needs to be loaded
		if (directoryEntry.m_directoryIndex < 0)
			loadDirectory(directoryIndex, directoryEntryIndex);
		assert(directoryEntry.m_directoryIndex >= 0);

		// recursively extract
		for (int childIndex = 0; childIndex < m_info->m_directories[directoryEntry.m_directoryIndex].m_children.size(); childIndex++)
		{
			const DirectoryEntry &childEntry = m_info->m_directories[directoryEntry.m_directoryIndex].m_children[childIndex];
			pathOnImage.push_back(childEntry.m_name);
			internalExtract(pathOnImage, directoryEntry.m_directoryIndex, childIndex, path + "/" + convert(childEntry.m_name));
			pathOnImage.resize(pathOnImage.size() - 1);
		}
		break;

	default:
		throw false;
	}
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex ImageItemModel::index(int row, int column, const QModelIndex &parent) const
{
	const DirectoryEntry *parentDirectoryEntry = findDirectoryEntry(parent);
	return createIndex(row, column, parentDirectoryEntry ? parentDirectoryEntry->m_directoryIndex : 0);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex ImageItemModel::parent(const QModelIndex &child) const
{
	const Directory &directory = m_info->m_directories[child.internalId()];
	return directory.m_parentIndex >= 0 && directory.m_parentEntryIndex >= 0
		? createIndex(directory.m_parentEntryIndex, child.column(), directory.m_parentIndex)
		: QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int ImageItemModel::rowCount(const QModelIndex &parent) const
{
	const DirectoryEntry *parentDirectoryEntry = findDirectoryEntry(parent);
	int parentDirectoryIndex = parentDirectoryEntry ? parentDirectoryEntry->m_directoryIndex : 0;
	return parentDirectoryIndex >= 0
		? m_info->m_directories[parentDirectoryIndex].m_children.size()
		: 0;
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int ImageItemModel::columnCount(const QModelIndex &parent) const
{
	return m_info->m_metaNames.size();
}


//-------------------------------------------------
//  hasChildren
//-------------------------------------------------

bool ImageItemModel::hasChildren(const QModelIndex &parent) const
{
	return rowCount(parent) > 0;
}


//-------------------------------------------------
//  flags
//-------------------------------------------------

Qt::ItemFlags ImageItemModel::flags(const QModelIndex &index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant ImageItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant result;
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			{
				fs::meta_name name = m_info->m_metaNames[section];
				const char *name_str = fs::meta_data::entry_name(name);
				result = convert(name_str);
			}
			break;

		case Qt::FontRole:
			{
				QFont font;
				font.setUnderline(true);
				result = std::move(font);
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant ImageItemModel::data(const QModelIndex &index, int role) const
{
	QVariant result;
	const DirectoryEntry *directoryEntry = findDirectoryEntry(index);
	if (directoryEntry)
	{
		fs::meta_name name = m_info->m_metaNames[index.column()];
		switch (role)
		{
		case Qt::DisplayRole:
			{
				std::string value = directoryEntry->m_metadata.get_string(name);
				result = convert(value);
			}
			break;

		case Qt::TextAlignmentRole:
			result = textAlignmentFromMetaType(directoryEntry->m_metadata.get(name).type());
			break;

		case Qt::DecorationRole:
			if (index.column() == 0)
				result = iconFromDirectoryEntry(*directoryEntry);
			break;
		}
	}
	return result;
}
