/***************************************************************************

	floptool.h

	Interface with floptool command line program

***************************************************************************/

// qfloptool headers
#include "floptool.h"
#include "utility.h"

// MAME headers
#include "formats/all.h"
#include "formats/fsblk_vec.h"
#include "ioprocs.h"
#include "ioprocsfill.h"
#include "ioprocsvec.h"

// Qt headers
#include <QProcess>

// C++ headers
#include <algorithm>
#include <ranges>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class MameFileSystemFormatEnumeratorImpl : public fs::manager_t::floppy_enumerator
	{
	public:
		struct FileSystemFormat
		{
			FileSystemFormat(const floppy_image_format_t &type, uint32_t image_size, const char *name, const char *description);

			const floppy_image_format_t &	m_type;
			uint32_t						m_imageSize;
			const char *					m_name;
			const char *					m_description;
		};

		// ctor
		MameFileSystemFormatEnumeratorImpl();

		// accessors
		const std::vector<FileSystemFormat> &fileSystemFormats() const { return m_fileSystemFormats;  }

		// virtuals
		virtual void add_raw(const char *name, uint32_t key, const char *description) override final;

	protected:
		// virtuals
		virtual void add_format(const floppy_image_format_t &type, uint32_t image_size, const char *name, const char *description) override final;

	private:
		std::vector<FileSystemFormat>	m_fileSystemFormats;
		std::vector<uint32_t>			m_variants;
	};
};


// ======================> MameFormatsEnumeratorImpl

class Floptool::MameFormatsEnumeratorImpl : public mame_formats_enumerator
{
public:
	// ctor
	MameFormatsEnumeratorImpl(Floptool &host);

	// methods
	virtual void category(const char *name);
	virtual void add(const cassette_image::Format *const *formats);
	virtual void add(const floppy_image_format_t &mameFormat);
	virtual void add(const fs::manager_t &fs);

private:
	Floptool &	m_host;
	QString		m_currentCategory;
};


// ======================> MameRandomRead

class MameRandomRead : public util::random_read
{
public:
	// ctor
	MameRandomRead(QIODevice &inner);

	// virtuals
	virtual std::error_condition read(void *buffer, std::size_t length, std::size_t &actual) noexcept;
	virtual std::error_condition seek(std::int64_t offset, int whence) noexcept;
	virtual std::error_condition tell(std::uint64_t &result) noexcept;
	virtual std::error_condition length(std::uint64_t &result) noexcept;
	virtual std::error_condition read_at(std::uint64_t offset, void *buffer, std::size_t length, std::size_t &actual) noexcept;

private:
	QIODevice &		m_inner;
}
;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

Floptool *Floptool::s_instance;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

Floptool::Floptool()
{
	assert(!s_instance);
	s_instance = this;
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

Floptool::~Floptool()
{
	assert(s_instance);
	s_instance = nullptr;
}


//-------------------------------------------------
//  instance
//-------------------------------------------------

Floptool &Floptool::instance()
{
	assert(s_instance);
	return *s_instance;
}


//-------------------------------------------------
//  initializeMameFormats
//-------------------------------------------------

void Floptool::initializeMameFormats()
{
	// clear out our data
	m_floppyFormats.clear();
	m_floppyFormats.reserve(256);
	m_fileSystems.clear();
	m_fileSystems.reserve(32);

	// get data out of MAME
	MameFormatsEnumeratorImpl en(*this);
	mame_formats_full_list(en);

	// shrink to fit
	m_floppyFormats.shrink_to_fit();
	m_fileSystems.shrink_to_fit();

	// and sort the data by categories
	std::ranges::sort(m_floppyFormats, [](const auto &a, const auto &b)
	{
		return a.categoryName() < b.categoryName();
	});
	std::ranges::sort(m_fileSystems, [](const auto &a, const auto &b)
	{
		return a.categoryName() < b.categoryName();
	});
}


//-------------------------------------------------
//  identify
//-------------------------------------------------

std::vector<Floptool::IdentifyResultCategory> Floptool::identify(QIODevice &file) const
{
	std::vector<IdentifyResultCategory> resultCategories;

	QString fileExtension = QFileInfo().suffix();

	MameRandomRead randomRead(file);
	std::vector<uint32_t> variants;
	for (const auto &floppyFormatCategory : m_floppyFormats)
	{
		for (const FloppyFormat &floppyFormat : floppyFormatCategory)
		{
			// try to identify the image
			uint8_t score = floppyFormat.m_mameFormat.identify(randomRead, floppy_image::FF_UNKNOWN, variants);
			if (score)
			{
				// the image was successfully identified - the onus is on us to check the file extension
				if (std::ranges::find(floppyFormat.fileExtensions(), fileExtension) != floppyFormat.fileExtensions().end())
					score |= floppy_image_format_t::FIFID_EXT;

				// try to find a result in this category
				auto iter = std::ranges::find_if(resultCategories, [&floppyFormatCategory](const auto &x)
				{
					return x.categoryName() == floppyFormatCategory.categoryName();
				});
				
				auto &resultsCategory = iter != resultCategories.end()
					? *iter
					: resultCategories.emplace_back(floppyFormatCategory.categoryName());

				resultsCategory.emplace_back(score, std::reference_wrapper<const FloppyFormat>(floppyFormat));
			}
		}
	}

	// sort the results - this requires two levels of sorts
	for (IdentifyResultCategory &cat : resultCategories)
	{
		std::ranges::sort(cat, [](const auto &x, const auto &y)
		{
			return std::get<0>(x) > std::get<0>(y);
		});
	}
	std::ranges::sort(resultCategories, [](const auto &x, const auto &y)
	{
		return std::get<0>(x[0]) > std::get<0>(y[0]);
	});

	return resultCategories;
}


//-------------------------------------------------
//  findFloppyFormat
//-------------------------------------------------

const Floptool::FloppyFormat *Floptool::findFloppyFormat(const QString &name) const
{
	auto view = std::ranges::join_view(m_floppyFormats);
	auto iter = std::ranges::find_if(view, [&name](const FloppyFormat &floppyFormat)
	{
		return floppyFormat.name() == name;
	});
	return iter != view.end() ? &*iter : nullptr;
}


//-------------------------------------------------
//  findFileSystem
//-------------------------------------------------

const Floptool::FileSystem *Floptool::findFileSystem(const QString &name) const
{
	auto view = std::ranges::join_view(m_fileSystems);
	auto iter = std::ranges::find_if(view, [&name](const FileSystem &fileSystem)
	{
		return fileSystem.name() == name;
	});
	return iter != view.end() ? &*iter : nullptr;
}


//-------------------------------------------------
//  mount
//-------------------------------------------------

Floptool::Image::ptr Floptool::mount(QIODevice &file, const Floptool::FloppyFormat &format, const Floptool::FileSystem &fileSystem) const
{
	MameRandomRead randomRead(file);

	// try to load the image
	std::vector<uint32_t> variants;
	floppy_image mameFloppyImage(84, 2, floppy_image::FF_UNKNOWN);
	if (!format.m_mameFormat.load(randomRead, floppy_image::FF_UNKNOWN, variants, &mameFloppyImage))
		return {};

	// figure out what file system formats can be used
	MameFileSystemFormatEnumeratorImpl fsEnum;
	fileSystem.m_mameFsManager.enumerate_f(fsEnum);

	// convert the image to something more 
	const floppy_image_format_t *floppyFsConverter = nullptr;
	std::vector<uint8_t> sectorImage;
	std::optional<floppy_image> floppyImage;
	bool success = false;
	for (const auto &ci : fsEnum.fileSystemFormats())
	{
		if (&ci.m_type != floppyFsConverter)
		{
			// clear out sectorImage and target it with a stream
			sectorImage.clear();
			util::random_read_write_fill_wrapper<util::vector_read_write_adapter<uint8_t>, 0xff> io(sectorImage);

			// and convert to the right format
			std::vector<uint32_t> variants;
			floppyFsConverter = &ci.m_type;
			floppyFsConverter->save(io, variants, &mameFloppyImage);
		}

		success = ci.m_imageSize == sectorImage.size();
		if (success)		
			break;
	}
	if (!success)
		return {};

	return std::make_unique<Image>(format, fileSystem, std::move(sectorImage));
}


//-------------------------------------------------
//  FloppyFormat ctor
//-------------------------------------------------

Floptool::FloppyFormat::FloppyFormat(const floppy_image_format_t &mameFormat)
	: m_mameFormat(mameFormat)
{
	// build file extensions
	const char *extensions = m_mameFormat.extensions();
	while (extensions && *extensions)
	{
		const char *s = strchr(extensions, ',');

		QString ext = QString::fromUtf8(extensions, s ? s - extensions : -1);
		m_fileExtensions.push_back(std::move(ext));

		extensions = s ? s + 1 : nullptr;
	}
	m_fileExtensions.shrink_to_fit();
}


//-------------------------------------------------
//  FloppyFormat::name
//-------------------------------------------------

QString Floptool::FloppyFormat::name() const
{
	return m_mameFormat.name();
}


//-------------------------------------------------
//  FloppyFormat::description
//-------------------------------------------------

QString Floptool::FloppyFormat::description() const
{
	return QString::fromUtf8(m_mameFormat.description());
}


//-------------------------------------------------
//  FileSystem ctor
//-------------------------------------------------

Floptool::FileSystem::FileSystem(const fs::manager_t &mameFsManager)
	: m_mameFsManager(mameFsManager)
{
}


//-------------------------------------------------
//  FileSystem::name
//-------------------------------------------------

QString Floptool::FileSystem::name() const
{
	return m_mameFsManager.name();
}


//-------------------------------------------------
//  FileSystem::description
//-------------------------------------------------

QString Floptool::FileSystem::description() const
{
	return QString::fromUtf8(m_mameFsManager.description());
}


//-------------------------------------------------
//  FileSystem::canRead
//-------------------------------------------------

bool Floptool::FileSystem::canRead() const
{
	return m_mameFsManager.can_read();
}


//-------------------------------------------------
//  Category ctor
//-------------------------------------------------

template<class T>
Floptool::Category<T>::Category(const QString &categoryName)
	: m_categoryName(categoryName)
{
}


//-------------------------------------------------
//  Image ctor
//-------------------------------------------------

Floptool::Image::Image(const Floptool::FloppyFormat &format, const Floptool::FileSystem &fileSystem, std::vector<uint8_t> &&sectorImage)
	: m_format(format)
	, m_fileSystem(fileSystem)
	, m_sectorImage(std::move(sectorImage))
{
	m_mameFsBlk.reset(new fs::fsblk_vec_t(m_sectorImage));
	m_mameFs = m_fileSystem.m_mameFsManager.mount(*m_mameFsBlk);
}


//-------------------------------------------------
//  Image dtor
//-------------------------------------------------

Floptool::Image::~Image()
{
}


//-------------------------------------------------
//  Image::convert
//-------------------------------------------------

QString Floptool::Image::convert(std::string_view s) const
{
	return QString::fromLocal8Bit(s);
}


//-------------------------------------------------
//  Image::volumeName
//-------------------------------------------------

std::optional<QString> Floptool::Image::volumeName() const
{
	fs::meta_data meta = m_mameFs->volume_metadata();
	std::optional<QString> result;
	if (meta.has(fs::meta_name::name))
		result = convert(meta.get(fs::meta_name::name).as_string());
	return result;	
}


//-------------------------------------------------
//  MameFormatsEnumeratorImpl ctor
//-------------------------------------------------

Floptool::MameFormatsEnumeratorImpl::MameFormatsEnumeratorImpl(Floptool &host)
	: m_host(host)
{
}


//-------------------------------------------------
//  MameFormatsEnumeratorImpl::category
//-------------------------------------------------

void Floptool::MameFormatsEnumeratorImpl::category(const char *name)
{
	m_currentCategory = QString::fromUtf8(name);
}


//-------------------------------------------------
//  MameFormatsEnumeratorImpl::add(const cassette_image::Format *const *)
//-------------------------------------------------

void Floptool::MameFormatsEnumeratorImpl::add(const cassette_image::Format *const *formats)
{
}


//-------------------------------------------------
//  MameFormatsEnumeratorImpl::add(const floppy_image_format_t &)
//-------------------------------------------------

void Floptool::MameFormatsEnumeratorImpl::add(const floppy_image_format_t &mameFormat)
{
	if (m_host.m_floppyFormats.empty() || util::last(m_host.m_floppyFormats).categoryName() != m_currentCategory)
		m_host.m_floppyFormats.emplace_back(m_currentCategory);
	util::last(m_host.m_floppyFormats).emplace_back(mameFormat);
}


//-------------------------------------------------
//  MameFormatsEnumeratorImpl::add(const fs::manager_t &)
//-------------------------------------------------

void Floptool::MameFormatsEnumeratorImpl::add(const fs::manager_t &mameFsManager)
{
	if (m_host.m_fileSystems.empty() || util::last(m_host.m_fileSystems).categoryName() != m_currentCategory)
		m_host.m_fileSystems.emplace_back(m_currentCategory);
	util::last(m_host.m_fileSystems).emplace_back(mameFsManager);
}


//-------------------------------------------------
//  MameFileSystemFormatEnumeratorImpl ctor
//-------------------------------------------------

MameFileSystemFormatEnumeratorImpl::MameFileSystemFormatEnumeratorImpl()
	: fs::manager_t::floppy_enumerator(floppy_image::FF_UNKNOWN, m_variants)
{
}


//-------------------------------------------------
//  MameFileSystemFormatEnumeratorImpl::add_raw
//-------------------------------------------------

void MameFileSystemFormatEnumeratorImpl::add_raw(const char *name, uint32_t key, const char *description)
{
}


//-------------------------------------------------
//  MameFileSystemFormatEnumeratorImpl::add_format
//-------------------------------------------------

void MameFileSystemFormatEnumeratorImpl::add_format(const floppy_image_format_t &type, uint32_t image_size, const char *name, const char *description)
{
	m_fileSystemFormats.emplace_back(type, image_size, name, description);
}


//-------------------------------------------------
//  MameFileSystemFormatEnumeratorImpl::FileSystemFormat ctor
//-------------------------------------------------

MameFileSystemFormatEnumeratorImpl::FileSystemFormat::FileSystemFormat(const floppy_image_format_t &type, uint32_t image_size, const char *name, const char *description)
	: m_type(type)
	, m_imageSize(image_size)
	, m_name(name)
	, m_description(description)
{
}


//-------------------------------------------------
//  MameRandomRead ctor
//-------------------------------------------------

MameRandomRead::MameRandomRead(QIODevice &inner)
	: m_inner(inner)
{
}


std::error_condition MameRandomRead::read(void *buffer, std::size_t length, std::size_t &actual) noexcept
{
	actual = m_inner.read((char *)buffer, length);
	return std::error_condition();
}

std::error_condition MameRandomRead::seek(std::int64_t offset, int whence) noexcept
{
	throw false;
}

std::error_condition MameRandomRead::tell(std::uint64_t &result) noexcept
{
	throw false;
}

//-------------------------------------------------
//  MameRandomRead::length
//-------------------------------------------------

std::error_condition MameRandomRead::length(std::uint64_t &result) noexcept
{
	result = m_inner.size();
	return std::error_condition();
}


//-------------------------------------------------
//  MameRandomRead::read_at
//-------------------------------------------------

std::error_condition MameRandomRead::read_at(std::uint64_t offset, void *buffer, std::size_t length, std::size_t &actual) noexcept
{
	m_inner.seek(offset);
	return read(buffer, length, actual);
}
