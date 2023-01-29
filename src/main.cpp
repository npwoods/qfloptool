/***************************************************************************

	main.cpp

	Entry point

***************************************************************************/

// qfloptool headers
#include "floptool.h"
#include "mainwindow.h"

// Qt headers
#include <QApplication>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  main
//-------------------------------------------------

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("BletchMAME");
	QCoreApplication::setApplicationName("qfloptool");

	// instantiate the QApplication
    QApplication a(argc, argv);

	// instantiate the floptool interface
	Floptool floptoolInstance;
	floptoolInstance.initializeMameFormats();

	// show the main window, and execute!
	MainWindow::startNewWindow();
    return a.exec();
}
