/***************************************************************************

	mainwindow.h

	Main qfloptool window

***************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// qfloptool headers
#include "floptool.h"

// Qt headers
#include <QMainWindow>
#include <QModelIndex>


class ImageItemModel;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QSettings;
QT_END_NAMESPACE


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> MainWindow

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
	// ctor/dtor
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

	// methods
	static MainWindow &startNewWindow();

private slots:
	void on_menuFile_aboutToShow();
	void on_actionNew_triggered();
	void on_actionOpen_triggered();
	void on_actionClose_triggered();
	void on_actionExtract_triggered();
	void on_actionAbout_triggered();
	void on_mainTree_customContextMenuRequested(const QPoint &pos);

private:
	std::unique_ptr<Ui::MainWindow> m_ui;
	std::array<QAction *, 10>		m_recentActions;

	QStringList buildNameFilters();
	void updateRecents();
	bool loadRecent(int i);
	bool loadImage(Floptool::Image::ptr &&image, QString &&fileName, QString &&floppyFormatName, QString &&fileSystemName);
	void addRecent(const QString &fileName, QString &&floppyFormatName, QString &&fileSystemName);
	static bool parseRecentFileSettingValue(const QString &settingValue, QString &fileName, QString &floppyFormatName, QString &fileSystemName);
	static QString recentFileSettingName(int i);
	void setTitleFromImageInfo(const QString &fileName = "");
	void extractSingle(ImageItemModel &model, const QModelIndex &index);
	void extractMultiple(ImageItemModel &model, const QModelIndexList &indexes);
	void extractSelection();
};

#endif // MAINWINDOW_H
