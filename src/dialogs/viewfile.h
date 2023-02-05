/***************************************************************************

	viewfile.h

	File viewing dialog

***************************************************************************/

#ifndef VIEWFILE_H
#define VIEWFILE_H

// Qt includes
#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui { class ViewFileDialog; }
QT_END_NAMESPACE

// ======================> ViewFileDialog

class ViewFileDialog : public QDialog
{
	Q_OBJECT

public:
	// ctor/dtor
	ViewFileDialog(QWidget *parent = nullptr);
	~ViewFileDialog();

	// methods
	void setFileBytes(std::vector<uint8_t> &&bytes);

private:
	class Storage;

	std::unique_ptr<Ui::ViewFileDialog>		m_ui;
};


#endif // VIEWFILE_H

