/***************************************************************************

	identify.cpp

	Image identification dialog

***************************************************************************/

// qfloptool headers
#include "identify.h"
#include "ui_identify.h"
#include "../floptool.h"
#include "../imageitemmodel.h"

// Qt headers
#include <QFontDatabase>
#include <QStringListModel>

// C++ headers
#include <algorithm>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> CategoryItemListModel
	template<class T>
	class CategoryItemListModel : public QAbstractItemModel
	{
	public:
		CategoryItemListModel(const std::vector<Floptool::Category<T>> &items, QObject *parent = nullptr);

		// methods
		QModelIndex findFirstModelIndexForCategory(const QString *categoryName = nullptr) const;
		const T *getItem(const QModelIndex &index) const;

		// virtuals
		virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override final;
		virtual QModelIndex parent(const QModelIndex &child) const override final;
		virtual int rowCount(const QModelIndex &parent) const override final;
		virtual int columnCount(const QModelIndex &parent) const override final;
		virtual bool hasChildren(const QModelIndex &parent) const final;
		virtual Qt::ItemFlags flags(const QModelIndex &index) const override final;
		virtual QVariant data(const QModelIndex &index, int role) const override final;

	protected:
		virtual QString displayData(const T &item) const = 0;
		virtual bool itemEnabled(const T &item) const = 0;

	private:
		const std::vector<Floptool::Category<T>> &	m_items;
	};


	// ======================> IdentifyResultsListModel
	class IdentifyResultsListModel : public CategoryItemListModel<Floptool::IdentifyResult>
	{
	public:
		using CategoryItemListModel::CategoryItemListModel;

	protected:
		virtual QString displayData(const Floptool::IdentifyResult &item) const override final;
		virtual bool itemEnabled(const Floptool::IdentifyResult &item) const override final;
	};


	// ======================> FileSystemsListModel
	class FileSystemsListModel : public CategoryItemListModel<Floptool::FileSystem>
	{
	public:
		using CategoryItemListModel::CategoryItemListModel;

	protected:
		virtual QString displayData(const Floptool::FileSystem &item) const override final;
		virtual bool itemEnabled(const Floptool::FileSystem &item) const override final;
	};
};

//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

IdentifyDialog::IdentifyDialog(QIODevice &file, const std::vector<Floptool::IdentifyResultCategory> &ident, QWidget *parent)
	: m_file(file)
	, m_ident(ident)
{
	// set up UI
	m_ui = std::make_unique<Ui::IdentifyDialog>();
	m_ui->setupUi(this);

	// cosmetic (why can't this be done in identify.ui?)
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	m_ui->previewTreeView->setFont(font);

	// set up the format list model
	QStringList formatDescs;
	IdentifyResultsListModel &identifyResultsModel = *new IdentifyResultsListModel(m_ident, this);
	m_ui->identifyResultsTreeView->setModel(&identifyResultsModel);
	m_ui->identifyResultsTreeView->expandAll();

	// set up the file systems list model
	FileSystemsListModel &fileSystemsModel = *new FileSystemsListModel(Floptool::instance().fileSystems(), this);
	m_ui->fileSystemsTreeView->setModel(&fileSystemsModel);
	m_ui->fileSystemsTreeView->expandAll();

	// set the proper selections
	QModelIndex identifyResultsSelection = identifyResultsModel.findFirstModelIndexForCategory();
	m_ui->identifyResultsTreeView->selectionModel()->select(identifyResultsSelection, QItemSelectionModel::SelectCurrent);
	QModelIndex fileSystemsSelection = fileSystemsModel.findFirstModelIndexForCategory(&m_ident[0].categoryName());
	m_ui->fileSystemsTreeView->selectionModel()->select(fileSystemsSelection, QItemSelectionModel::SelectCurrent);

	// listen to selection events
	connect(m_ui->identifyResultsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected)
	{
		updatePreview();
	});
	connect(m_ui->fileSystemsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected)
	{
		updatePreview();
	});
	updatePreview();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

IdentifyDialog::~IdentifyDialog()
{
}


//-------------------------------------------------
//  updatePreview
//-------------------------------------------------

void IdentifyDialog::updatePreview()
{
	IdentifyResultsListModel &identifyResultsModel = *dynamic_cast<IdentifyResultsListModel *>(m_ui->identifyResultsTreeView->model());
	FileSystemsListModel &fileSystemsModel = *dynamic_cast<FileSystemsListModel *>(m_ui->fileSystemsTreeView->model());

	QModelIndexList identifyResultsSelectedIndexes = m_ui->identifyResultsTreeView->selectionModel()->selectedIndexes();
	QModelIndexList fileSystemsSelectedIndexes = m_ui->fileSystemsTreeView->selectionModel()->selectedIndexes();
	const Floptool::IdentifyResult *identifyResult = !identifyResultsSelectedIndexes.empty()
		? identifyResultsModel.getItem(identifyResultsSelectedIndexes[0])
		: nullptr;
	const Floptool::FileSystem *fileSystem = !fileSystemsSelectedIndexes.empty()
		? fileSystemsModel.getItem(fileSystemsSelectedIndexes[0])
		: nullptr;

	// delete the old model, if necessary
	QAbstractItemModel *oldModel = m_ui->previewTreeView->model();
	if (oldModel)
		delete oldModel;
	if (!identifyResult || !fileSystem)
		return;

	// try to load the image
	Floptool::Image::ptr image = Floptool::instance().mount(m_file, std::get<1>(*identifyResult), *fileSystem);
	if (!image)
		return;

	// set up a preview model
	ImageItemModel &previewModel = *new ImageItemModel(std::move(image), this);
	m_ui->previewTreeView->setModel(&previewModel);
}


//-------------------------------------------------
//  detachImage
//-------------------------------------------------

Floptool::Image::ptr IdentifyDialog::detachImage()
{
	ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->previewTreeView->model());
	return model ? model->detachImage() : nullptr;
}


//-------------------------------------------------
//  CategoryItemListModel ctor
//-------------------------------------------------

template<class T>
CategoryItemListModel<T>::CategoryItemListModel(const std::vector<Floptool::Category<T>> &items, QObject *parent)
	: QAbstractItemModel(parent)
	, m_items(items)
{
}


//-------------------------------------------------
//  CategoryItemListModel::findFirstModelIndexForCategory
//-------------------------------------------------

template<class T>
QModelIndex CategoryItemListModel<T>::findFirstModelIndexForCategory(const QString *categoryName) const
{
	for (int row = 0; row < m_items.size(); row++)
	{
		if (!categoryName || m_items[row].categoryName() == *categoryName)
		{
			QModelIndex parentIndex = index(row, 0);
			return index(0, 0, parentIndex);
		}
	}
	return QModelIndex();
}


//-------------------------------------------------
//  CategoryItemListModel::getItem
//-------------------------------------------------

template<class T>
const T *CategoryItemListModel<T>::getItem(const QModelIndex &index) const
{
	return index.internalId() != ~0
		? &m_items[index.internalId()][index.row()]
		: nullptr;
}


//-------------------------------------------------
//  CategoryItemListModel::index
//-------------------------------------------------

template<class T>
QModelIndex CategoryItemListModel<T>::index(int row, int column, const QModelIndex &parent) const
{
	QModelIndex result = createIndex(row, column, parent.isValid() ? parent.row() : ~0);
	assert(result.isValid());
	return result;
}


//-------------------------------------------------
//  CategoryItemListModel::parent
//-------------------------------------------------

template<class T>
QModelIndex CategoryItemListModel<T>::parent(const QModelIndex &child) const
{
	return child.internalId() != ~0
		? index(child.internalId(), child.column())
		: QModelIndex();
}


//-------------------------------------------------
//  CategoryItemListModel::rowCount
//-------------------------------------------------

template<class T>
int CategoryItemListModel<T>::rowCount(const QModelIndex &parent) const
{
	int result;
	if (parent.isValid())
	{
		// filesystem entry
		result = m_items[parent.row()].size();
	}
	else
	{
		// filesystem category
		result = m_items.size();
	}
	return result;
}


//-------------------------------------------------
//  CategoryItemListModel::columnCount
//-------------------------------------------------

template<class T>
int CategoryItemListModel<T>::columnCount(const QModelIndex &parent) const
{
	return 1;
}


//-------------------------------------------------
//  CategoryItemListModel::hasChildren
//-------------------------------------------------

template<class T>
bool CategoryItemListModel<T>::hasChildren(const QModelIndex &parent) const
{
	return !parent.isValid() || parent.internalId() == ~0;
}


//-------------------------------------------------
//  CategoryItemListModel::flags
//-------------------------------------------------

template<class T>
Qt::ItemFlags CategoryItemListModel<T>::flags(const QModelIndex &index) const
{
	bool isEnabled = index.internalId() != ~0
		? itemEnabled(m_items[index.internalId()][index.row()])
		: true;

	Qt::ItemFlags result = isEnabled ? Qt::ItemIsEnabled : (Qt::ItemFlags)0;
	if (index.internalId() != ~0)
		result |= Qt::ItemIsSelectable | Qt::ItemNeverHasChildren;
	return result;
}


//-------------------------------------------------
//  CategoryItemListModel::data
//-------------------------------------------------

template<class T>
QVariant CategoryItemListModel<T>::data(const QModelIndex &index, int role) const
{
	QVariant result;
	const T *item = getItem(index);
	if (item)
	{
		// item entry
		switch (role)
		{
		case Qt::DisplayRole:
			result = displayData(*item);
			break;
		}
	}
	else
	{
		// item category
		switch (role)
		{
		case Qt::DisplayRole:
			result = m_items[index.row()].categoryName();
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
//  IdentifyResultsListModel::displayData
//-------------------------------------------------

QString IdentifyResultsListModel::displayData(const Floptool::IdentifyResult &item) const
{
	const Floptool::FloppyFormat &floppyFormat = std::get<1>(item);
	return floppyFormat.description();
}


//-------------------------------------------------
//  FileSystemsListModel::itemEnabled
//-------------------------------------------------

bool IdentifyResultsListModel::itemEnabled(const Floptool::IdentifyResult &item) const
{
	return true;
}


//-------------------------------------------------
//  FileSystemsListModel::displayData
//-------------------------------------------------

QString FileSystemsListModel::displayData(const Floptool::FileSystem &item) const
{
	return item.description();
}


//-------------------------------------------------
//  FileSystemsListModel::itemEnabled
//-------------------------------------------------

bool FileSystemsListModel::itemEnabled(const Floptool::FileSystem &item) const
{
	return item.canRead();
}
