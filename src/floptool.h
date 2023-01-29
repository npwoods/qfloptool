/***************************************************************************

	floptool.h

	Interface with floptool command line program

***************************************************************************/

#ifndef FLOPTOOL_H
#define FLOPTOOL_H

// Qt headers
#include <QObject>


// MAME forward declarations
class floppy_image_format_t;
namespace fs
{
	class manager_t;
	class fsblk_t;
	class filesystem_t;
};


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> Floptool

class Floptool : public QObject
{
	Q_OBJECT
public:
	// ======================> Floptool
	class FloppyFormat
	{
		friend class Floptool;
	public:
		// ctor
		FloppyFormat(const floppy_image_format_t &mameFormat);

		// methods
		QString name() const;
		QString description() const;
		const std::vector<QString> &fileExtensions() const { return m_fileExtensions; }

	private:
		const floppy_image_format_t &	m_mameFormat;
		std::vector<QString>			m_fileExtensions;
	};


	// ======================> FileSystem
	class FileSystem
	{
		friend class Floptool;
	public:
		// ctor
		FileSystem(const fs::manager_t &mameFsManager);

		// accessor
		const fs::manager_t &mameManager() const { return m_mameFsManager; }

		// methods
		QString name() const;
		QString description() const;
		bool canRead() const;

	private:
		const fs::manager_t &			m_mameFsManager;
	};


	// ======================> Category
	template<class T>
	class Category : public std::vector<T>
	{
	public:
		// ctor
		Category(const QString &categoryName);

		// accessor
		const QString &categoryName() const { return m_categoryName;  }

	private:
		QString	m_categoryName;
	};


	// ======================> Image
	class Image
	{
	public:
		typedef std::unique_ptr<Image> ptr;

		// ctor/dtor
		Image(const FloppyFormat &format, const FileSystem &fileSystem, std::vector<uint8_t> &&sectorImage);
		Image(const Image &) = delete;
		Image(Image &&) = delete;
		~Image();

		// accessors
		const FloppyFormat &floppyFormat() const { return m_format; }
		const FileSystem &fileSystem() const { return m_fileSystem; }
		fs::filesystem_t &mameFileSystem() const { return *m_mameFs; }

		// methods
		QString convert(std::string_view s) const;
		std::optional<QString> volumeName() const;

	private:
		const FloppyFormat &				m_format;
		const FileSystem &					m_fileSystem;
		std::vector<uint8_t>				m_sectorImage;
		std::unique_ptr<fs::fsblk_t>		m_mameFsBlk;
		std::unique_ptr<fs::filesystem_t>	m_mameFs;
	};

	// typedefs
	typedef std::tuple<uint8_t, std::reference_wrapper<const FloppyFormat>> IdentifyResult;
	typedef Category<IdentifyResult> IdentifyResultCategory;

	// ctor/dtor
	Floptool();
	~Floptool();

	// statics
	static Floptool &instance();

	// methods
	void initializeMameFormats();

	// accessors
	const std::vector<Category<FloppyFormat>> &floppyFormats() const	{ return m_floppyFormats; }
	const std::vector<Category<FileSystem>> &fileSystems() const		{ return m_fileSystems; }

	// methods
	std::vector<IdentifyResultCategory> identify(QIODevice &file) const;
	Image::ptr mount(QIODevice &file, const Floptool::FloppyFormat &format, const Floptool::FileSystem &fileSystem) const;
	const FloppyFormat *findFloppyFormat(const QString &name) const;
	const FileSystem *findFileSystem(const QString &name) const;

private:
	class MameFormatsEnumeratorImpl;

	// static members
	static Floptool *s_instance;

	// members
	std::vector<Category<FloppyFormat>>		m_floppyFormats;
	std::vector<Category<FileSystem>>		m_fileSystems;
};


#endif // FLOPTOOL_H
