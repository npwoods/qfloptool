/***************************************************************************

	identify.h

	Image identification dialog

***************************************************************************/

#ifndef IDENTIFY_H
#define IDENTIFY_H

// qfloptool includes
#include "../floptool.h"

// Qt includes
#include <QDialog>


QT_BEGIN_NAMESPACE
namespace Ui { class IdentifyDialog; }
QT_END_NAMESPACE

// ======================> IdentifyDialog

class IdentifyDialog : public QDialog
{
	Q_OBJECT

public:
	// ctor/dtor
	IdentifyDialog(QIODevice &file, const std::vector<Floptool::IdentifyResultCategory> &ident, QWidget *parent = nullptr);
	~IdentifyDialog();

	// methods
	Floptool::Image::ptr detachImage();

private:
	std::unique_ptr<Ui::IdentifyDialog>						m_ui;
	QIODevice &												m_file;
	const std::vector<Floptool::IdentifyResultCategory> &	m_ident;

	void updatePreview();
};

#endif // IDENTIFY_H
