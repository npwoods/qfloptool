/***************************************************************************

	mainwindow.cpp

	Main qfloptool window

***************************************************************************/

// qfloptool includes
#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "floptool.h"
#include "imageitemmodel.h"
#include "dialogs/identify.h"
#include "dialogs/viewfile.h"

// Qt includes
#include <QFileDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QMenu>
#include <QSettings>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
	m_ui = std::make_unique<Ui::MainWindow>();
    m_ui->setupUi(this);

	// cosmetic (why can't this be done in mainwindow.ui?)
	QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
	m_ui->mainTree->setFont(font);

	// set up recent actions
	for (auto i = 0; i < std::size(m_recentActions); i++)
	{	
		m_recentActions[i] = new QAction(this);
		m_ui->menuFile->insertAction(m_ui->actionClose, m_recentActions[i]);
		connect(m_recentActions[i], &QAction::triggered, this, [this, i]()
		{
			loadRecent(i);
		});
	}

	// set up the separator after recent actions
	QAction &separator = *m_ui->menuFile->insertSeparator(m_ui->actionClose);
	connect(&separator, &QAction::visibleChanged, this, [this, &separator]()
	{
		separator.setVisible(m_recentActions[0]->isVisible());
	});

	// handle folder expansion events
	connect(m_ui->mainTree, &QTreeView::doubleClicked, this, [this](const QModelIndex &index)
	{
		ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
		if (model)
			model->loadItem(index);
	});
	connect(m_ui->mainTree, &QTreeView::expanded, this, [this](const QModelIndex &index)
	{
		ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
		if (model)
			model->setExpanded(index, true);
	});
	connect(m_ui->mainTree, &QTreeView::collapsed, this, [this](const QModelIndex &index)
	{
		ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
		if (model)
			model->setExpanded(index, false);
	});

	// update the title
	setTitleFromImageInfo();
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MainWindow::~MainWindow()
{
}


//-------------------------------------------------
//  startNewWindow
//-------------------------------------------------

MainWindow &MainWindow::startNewWindow()
{
	MainWindow &newWindow = *new MainWindow();
	newWindow.setAttribute(Qt::WA_DeleteOnClose);
	newWindow.show();
	return newWindow;
}


//-------------------------------------------------
//  buildNameFilters
//-------------------------------------------------

QStringList MainWindow::buildNameFilters()
{
	QStringList results;
	results << "All files(*)";

	for (const auto &floppyFormatCategory : Floptool::instance().floppyFormats())
	{
		for (const Floptool::FloppyFormat &floppyFormat : floppyFormatCategory)
		{
			QString extensions;
			for (const QString &ext : floppyFormat.fileExtensions())
			{
				if (!extensions.isEmpty())
					extensions.append(" ");
				extensions.append("*.");
				extensions.append(ext);
			}
			results << QString("%1: %2 (%3)").arg(floppyFormatCategory.categoryName(), floppyFormat.description(), extensions);
		}
	}
	return results;
}


//-------------------------------------------------
//  updateRecents
//-------------------------------------------------

void MainWindow::updateRecents()
{
	QSettings settings;

	bool atEnd = false;
	for (int i = 0; i < std::size(m_recentActions); i++)
	{
		if (!atEnd)
		{
			QString settingName = recentFileSettingName(i);
			QString settingValue = settings.value(settingName).toString();
			QString thisFileName, thisFloppyFormatName, thisFileSystemName;
			atEnd = !parseRecentFileSettingValue(settingValue, thisFileName, thisFloppyFormatName, thisFileSystemName);
			if (!atEnd)
				m_recentActions[i]->setText(QFileInfo(thisFileName).fileName());
		}
		m_recentActions[i]->setVisible(!atEnd);
	}
}


//-------------------------------------------------
//  loadRecent
//-------------------------------------------------

bool MainWindow::loadRecent(int i)
{
	// find the recent file out of settings
	QSettings settings;
	QString settingName = recentFileSettingName(i);
	QString settingValue = settings.value(settingName).toString();
	QString fileName, floppyFormatName, fileSystemName;
	if (!parseRecentFileSettingValue(settingValue, fileName, floppyFormatName, fileSystemName))
		return false;

	// find the actual format and file system
	const Floptool::FloppyFormat *floppyFormat = Floptool::instance().findFloppyFormat(floppyFormatName);
	if (!floppyFormat)
		return false;
	const Floptool::FileSystem *fileSystem = Floptool::instance().findFileSystem(fileSystemName);
	if (!fileSystem)
		return false;

	// open the file
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// mount the image
	Floptool::Image::ptr image = Floptool::instance().mount(file, *floppyFormat, *fileSystem);
	if (!image)
		return false;

	// and load it!
	return loadImage(std::move(image), std::move(fileName), std::move(floppyFormatName), std::move(fileSystemName));
}


//-------------------------------------------------
//  loadImage
//-------------------------------------------------

bool MainWindow::loadImage(Floptool::Image::ptr &&image, QString &&fileName, QString &&floppyFormatName, QString &&fileSystemName)
{
	// set the model
	ImageItemModel &previewModel = *new ImageItemModel(std::move(image), this);
	m_ui->mainTree->setModel(&previewModel);

	// for some reason, the signal needs to be set up here
	connect(m_ui->mainTree->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected)
	{
		m_ui->actionView->setEnabled(!selected.indexes().isEmpty());
		m_ui->actionExtract->setEnabled(!selected.indexes().isEmpty());
	});

	// update the title
	setTitleFromImageInfo(fileName);

	// resize all columns
	int columnCount = previewModel.columnCount();
	for (int column = 0; column < columnCount; column++)
		m_ui->mainTree->resizeColumnToContents(column);

	// and add this to recent files
	addRecent(fileName, std::move(floppyFormatName), std::move(fileSystemName));
	return true;
}


//-------------------------------------------------
//  addRecent
//-------------------------------------------------

void MainWindow::addRecent(const QString &fileName, QString &&floppyFormatName, QString &&fileSystemName)
{
	QSettings settings;
	QString lastFileName = fileName;
	for (int i = 0; i < std::size(m_recentActions); i++)
	{
		// identify the setting name and get the current value
		QString settingName = recentFileSettingName(i);
		QString settingValue = settings.value(settingName).toString();
		QString thisFileName, thisFloppyFormatName, thisFileSystemName;
		parseRecentFileSettingValue(settingValue, thisFileName, thisFloppyFormatName, thisFileSystemName);

		// set the new value
		QString newSettingValue = QString("%1,%2,%3").arg(floppyFormatName, fileSystemName, lastFileName);
		settings.setValue(settingName, newSettingValue);

		// if its time to bail, do so
		if (thisFileName == fileName)
			break;

		// advance
		lastFileName = std::move(thisFileName);
		floppyFormatName = std::move(thisFloppyFormatName);
		fileSystemName = std::move(thisFileSystemName);
	}
}


//-------------------------------------------------
//  parseRecentFileSettingValue
//-------------------------------------------------

bool MainWindow::parseRecentFileSettingValue(const QString &settingValue, QString &fileName, QString &floppyFormatName, QString &fileSystemName)
{
	qsizetype firstCommaPos = settingValue.indexOf(',', 0);
	if (firstCommaPos < 0)
		return false;
	qsizetype secondCommaPos = settingValue.indexOf(',', firstCommaPos + 1);
	if (secondCommaPos < 0 || secondCommaPos >= settingValue.size() - 1)
		return false;

	floppyFormatName = settingValue.left(firstCommaPos);
	fileSystemName = settingValue.mid(firstCommaPos + 1, secondCommaPos - firstCommaPos - 1);
	fileName = settingValue.right(settingValue.size() - secondCommaPos - 1);
	return true;
}


//-------------------------------------------------
//  recentFileSettingName
//-------------------------------------------------

QString MainWindow::recentFileSettingName(int i)
{
	return QString("recentfiles/%1").arg(i);
}


//-------------------------------------------------
//  setTitleFromImageInfo
//-------------------------------------------------

void MainWindow::setTitleFromImageInfo(const QString &fileName)
{
	QString title = QCoreApplication::applicationName();
	if (!fileName.isEmpty())
		title += QString(" - %1").arg(QFileInfo(fileName).fileName());

	ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
	if (model && model->image())
	{
		std::optional<QString> volumeName = model->image()->volumeName();
		if (volumeName)
			title += QString(" (\"%1\")").arg(*volumeName);
	}
	setWindowTitle(title);
}


//-------------------------------------------------
//  extractSingle
//-------------------------------------------------

void MainWindow::extractSingle(ImageItemModel &model, const QModelIndex &index)
{
	QString defaultFileName = model.fileName(index);
	if (defaultFileName.isEmpty())
		return;

	QFileDialog fileDialog;
	fileDialog.setWindowTitle("Extract");
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.selectFile(defaultFileName);
	if (fileDialog.exec() != QDialog::Accepted)
		return;

	QString saveFileName = fileDialog.selectedFiles()[0];
	model.extract(index, saveFileName, false);
}


//-------------------------------------------------
//  extractMultiple
//-------------------------------------------------

void MainWindow::extractMultiple(ImageItemModel &model, const QModelIndexList &indexes)
{
	QString path = QFileDialog::getExistingDirectory(this, "Extract");
	if (path.isEmpty())
		return;

	for (const QModelIndex &index : indexes)
		model.extract(index, path, true);
}


//-------------------------------------------------
//  addReplicatedAction
//-------------------------------------------------

QAction &MainWindow::addReplicatedAction(QMenu &menu, QAction &existingAction)
{
	QAction &newAction = *menu.addAction(existingAction.text(), [&existingAction] { existingAction.triggered(); });
	newAction.setEnabled(existingAction.isEnabled());
	return newAction;
}


//-------------------------------------------------
//  on_menuFile_aboutToShow
//-------------------------------------------------

void MainWindow::on_menuFile_aboutToShow()
{
	updateRecents();
}


//-------------------------------------------------
//  on_actionNew_triggered
//-------------------------------------------------

void MainWindow::on_actionNew_triggered()
{
	startNewWindow();
}


//-------------------------------------------------
//  on_actionOpen_triggered
//-------------------------------------------------

void MainWindow::on_actionOpen_triggered()
{
	// show the "Open File..." dialog
	QFileDialog dialog(this);
	dialog.setFileMode(QFileDialog::ExistingFile);
	dialog.setNameFilters(buildNameFilters());
	dialog.exec();
	if (dialog.result() != QDialog::DialogCode::Accepted)
		return;

	// try to open the file
	QString path = dialog.selectedFiles().first();
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return;

	// and identify it
	auto identifyResults = Floptool::instance().identify(file);
	if (identifyResults.empty())
	{
		QMessageBox msgBox;
		msgBox.setText("Unable to open image");
		msgBox.exec();
		return;
	}

	// show the dialog
	QFileInfo fileInfo(file.fileName());
	IdentifyDialog identifyDialog(file, identifyResults, this);
	identifyDialog.setWindowTitle(fileInfo.fileName());
	if (identifyDialog.exec() != QDialog::Accepted)
		return;

	// detach the image that we already loaded
	Floptool::Image::ptr image = identifyDialog.detachImage();
	if (!image)
		return;

	// get the format and file system names
	QString floppyFormatName = image->floppyFormat().name();
	QString fileSystemName = image->fileSystem().name();

	// and load!
	loadImage(std::move(image), std::move(path), std::move(floppyFormatName), std::move(fileSystemName));
}


//-------------------------------------------------
//  on_actionClose_triggered
//-------------------------------------------------

void MainWindow::on_actionClose_triggered()
{
	close();
}


//-------------------------------------------------
//  on_actionView_triggered
//-------------------------------------------------

void MainWindow::on_actionView_triggered()
{
	ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
	if (!model)
		return;

	QModelIndexList indexes = m_ui->mainTree->selectionModel()->selectedRows();
	for (const QModelIndex &index : indexes)
	{
		std::optional<std::vector<uint8_t>> bytes = model->readFile(index);
		if (bytes)
		{
			ViewFileDialog &viewFileDialog = *new ViewFileDialog(this);
			viewFileDialog.setAttribute(Qt::WA_DeleteOnClose);
			viewFileDialog.setWindowTitle(model->fileName(index));
			viewFileDialog.setFileBytes(std::move(*bytes));
			viewFileDialog.show();
		}
	}
}


//-------------------------------------------------
//  on_actionExtract_triggered
//-------------------------------------------------

void MainWindow::on_actionExtract_triggered()
{
	ImageItemModel *model = dynamic_cast<ImageItemModel *>(m_ui->mainTree->model());
	if (!model)
		return;

	QModelIndexList indexes = m_ui->mainTree->selectionModel()->selectedRows();
	switch (indexes.size())
	{
	case 0:
		// do nothing; should not happen
		break;

	case 1:
		// single selection; extract one item
		extractSingle(*model, indexes[0]);
		break;

	default:
		// multiple selection; extract multiple items to a directory
		extractMultiple(*model, indexes);
		break;
	}
}


//-------------------------------------------------
//  on_actionAbout_triggered
//-------------------------------------------------

void MainWindow::on_actionAbout_triggered()
{
	QMessageBox msgBox;
	msgBox.setText("qfloptool");
	msgBox.exec();
}


//-------------------------------------------------
//  on_mainTree_customContextMenuRequested
//-------------------------------------------------

void MainWindow::on_mainTree_customContextMenuRequested(const QPoint &pos)
{
	QMenu popupMenu;
	addReplicatedAction(popupMenu, *m_ui->actionView);
	addReplicatedAction(popupMenu, *m_ui->actionExtract);

	// show the popup menu
	QPoint globalPos = m_ui->mainTree->mapToGlobal(pos);
	popupMenu.exec(globalPos);
}
