/***************************************************************************

	viewfile.cpp

	View File dialog

***************************************************************************/

// qfloptool includes
#include "viewfile.h"
#include "ui_viewfile.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class ViewFileDialog::Storage : public QHexView::DataStorage
{
public:
	Storage(std::vector<uint8_t> &&bytes)
		: m_bytes(std::move(bytes))
	{
	}

	virtual QByteArray getData(std::size_t position, std::size_t length) override;
	virtual std::size_t size() override;

private:
	std::vector<uint8_t> m_bytes;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ViewFileDialog::ViewFileDialog(QWidget *parent)
	: QDialog(parent)
{
	m_ui = std::make_unique<Ui::ViewFileDialog>();
	m_ui->setupUi(this);
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

ViewFileDialog::~ViewFileDialog()
{
}


//-------------------------------------------------
//  setFileBytes
//-------------------------------------------------

void ViewFileDialog::setFileBytes(std::vector<uint8_t> &&bytes)
{
	m_ui->hexView->setData(new Storage(std::move(bytes)));
}


//-------------------------------------------------
//  Storage::getData
//-------------------------------------------------

QByteArray ViewFileDialog::Storage::getData(std::size_t position, std::size_t length)
{
	position = std::min(position, m_bytes.size());
	length = std::min(length, m_bytes.size() - position);
	return QByteArray((const char *) &m_bytes[position], length);
}


//-------------------------------------------------
//  Storage::size
//-------------------------------------------------

std::size_t ViewFileDialog::Storage::size()
{
	return m_bytes.size();
}

