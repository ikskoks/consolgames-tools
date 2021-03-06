#include "Archive.h"
#include "DecompressionStream.h"
#include "CompressionStream.h"
#include "DirectoriesFileSource.h"
#include <Hash.h>
#include <PartStream.h>
#include <FileStream.h>
#include <memory>
#include <functional>

using namespace std;
using namespace tr1;
using namespace Consolgames;

LOG_CATEGORY("ShatteredMemories.Archive");

namespace ShatteredMemories
{

const uint32 Archive::s_defaultAlignment = 0x800;

uint32 Archive::FileRecord::originalSize() const
{
	return (decompressedSize == 0 ? storedSize : decompressedSize);
}

bool Archive::FileRecord::isPacked() const
{
	return (decompressedSize != 0);
}

static std::wstring strToWStr(const std::string& str)
{
	return std::wstring(str.begin(), str.end());
}

Archive::Archive(const std::string& filename)
	: m_opened(false)
	, m_alignment(s_defaultAlignment)
	, m_originsMode(false)
	, m_magicMarker1(0)
	, m_magicMarker2(0)
	, m_hasMagicMarker(false)
{
	m_fileStreamHolder.reset(new FileStream(strToWStr(filename), Stream::modeRead));
	m_stream = m_fileStreamHolder.get();
}

Archive::Archive(const wstring& filename)
	: m_opened(false)
	, m_alignment(s_defaultAlignment)
{
	m_fileStreamHolder.reset(new FileStream(filename, Stream::modeRead));
	m_stream = m_fileStreamHolder.get();
}

Archive::Archive(Stream* stream)
	: m_stream(stream)
	, m_opened(false)
	, m_alignment(s_defaultAlignment)
{
}

bool Archive::open()
{
	ASSERT(!m_opened);

	if (!m_stream->opened())
	{
		return false;
	}

	m_header.signature = m_stream->readInt();

	m_originsMode = (m_header.signature == 0x302E3241); // "A2.0"

	m_header.fileCount = m_stream->readInt();
	m_header.headerSize = m_stream->readInt();

	if (m_originsMode)
	{
		DLOG << "Silent Hill Origins archive detected.";
		m_header.namesPosition = m_stream->readInt();
		m_header.namesSize = m_stream->readInt();
	}
	else
	{
		m_header.reserved = m_stream->readInt();
	}
	
	ASSERT(m_header.fileCount > 0);
	m_fileRecords.resize(m_header.fileCount);
	m_fileRecordsMap.clear();

	std::vector<char> namesBuf;
	if (m_originsMode)
	{
		m_names.clear();
		namesBuf.resize(m_header.namesSize);

		const offset_t pos = m_stream->position();
		m_stream->seek(m_header.namesPosition, Stream::seekSet);
		m_stream->read(&namesBuf[0], namesBuf.size());
		m_stream->seek(pos, Stream::seekSet);
	}

	for (vector<FileRecord>::iterator fileRecord = m_fileRecords.begin(); fileRecord != m_fileRecords.end(); fileRecord++)
	{
		if (m_originsMode)
		{
			const uint32 nameOffset = m_stream->readUInt();

			ASSERT(nameOffset < namesBuf.size());
			const char* name = &namesBuf[nameOffset];
			fileRecord->hash = Hash::calc(name);
			ASSERT(m_names.find(fileRecord->hash) == m_names.end());
			m_names[fileRecord->hash] = name;
		}
		else
		{
			fileRecord->hash = m_stream->readUInt();
		}
		fileRecord->offset = m_stream->readUInt();
		fileRecord->storedSize = m_stream->readUInt();
		fileRecord->decompressedSize = m_stream->readUInt();

		m_fileRecordsMap[fileRecord->hash] = &(*fileRecord);

		if (fileRecord->offset % 0x800 != 0)
		{
			m_alignment = 0x20;
		}
	}

	if (m_fileRecords.front().offset >= m_stream->position() + 16)
	{
		m_magicMarker1 = m_stream->readUInt();
		m_magicMarker2 = m_stream->readUInt();
		m_hasMagicMarker = true;
	}

	m_opened = true;
	return true;
}

bool Archive::extractFiles(const std::string& outDir, const std::set<uint32>& fileList)
{
	return extractFiles(strToWStr(outDir), fileList);	
}

bool Archive::extractFiles(const std::wstring& outDir, const std::set<uint32>& fileList)
{
	ProgressGuard guard(*this, fileList.empty() ? m_fileRecords.size() : fileList.size());

	ASSERT(m_opened);
	if (!m_opened)
	{
		return false;
	}

	if (!FileStream::fileExists(outDir))
	{
		DLOG << "Directory does not exists: " << outDir;
		return false;
	}

	int processed = 0;
	for (std::vector<FileRecord>::const_iterator record = m_fileRecords.begin(); record != m_fileRecords.end(); record++)
	{
		std::vector<FileRecord>::const_iterator next = record + 1;
		if (next != m_fileRecords.end() && !record->isValid(*next))
		{
			DLOG << "Skipping bad file record: " << Hash::toString(record->hash);
			processed++;
			continue;
		}

		if (!fileList.empty() && (fileList.find(record->hash) == fileList.end()))
		{
			continue;
		}

		const std::string filename = (m_names.find(record->hash) == m_names.end() ? Hash::toString(record->hash) : m_names[record->hash]);
		const std::wstring outFile = outDir + PATH_SEPARATOR_L + strToWStr(filename);

		notifyProgress(processed, filename);

		if (!extractFile(*record, outFile))
		{
			return false;
		}

		processed++;
	}
	return true;
}

bool Archive::extractFile(const FileRecord& record, Consolgames::Stream* stream)
{
	VERIFY(m_stream->seek(record.offset, Stream::seekSet) == record.offset);

	if (!record.isPacked())
	{
		m_stream->seek(record.offset, Stream::seekSet);
		stream->writeStream(m_stream, record.storedSize);
		return true;
	}

	DecompressionStream zlibStream(m_stream, record.decompressedSize);
	if (!zlibStream.opened())
	{
		DLOG << "Unable to open zlib stream!";
		return false;
	}

	const largesize_t unpacked = stream->writeStream(&zlibStream, record.decompressedSize);
	if (unpacked != record.decompressedSize)
	{
		DLOG << "Error occured during zlib stream unpacking!";
		return false;
	}

	return true;
}

bool Archive::extractFile(const FileRecord& record, const std::wstring& path)
{
	FileStream stream(path, Stream::modeWrite);
	if (!stream.opened())
	{
		DLOG << "Unable to open output file!";
		return false;
	}

	return extractFile(record, &stream);
}

void Archive::setNames(const std::list<std::string>& names)
{
	if (m_originsMode)
	{
		return;
	}

	m_names.clear();
	for (std::list<std::string>::const_iterator name = names.begin(); name != names.end(); name++)
	{
		m_names[Hash::calc(name->c_str())] = *name;
	}
}

uint32 Archive::alignSize(uint32 size) const
{
	size += m_alignment - 1;
	return size - (size % m_alignment);
}

bool Archive::rebuild(const std::wstring& outFile, FileSource& fileSource, const MergeMap& mergeMap)
{
	ProgressGuard guard(*this, m_fileRecords.size());

	FileStream file(outFile, FileStream::modeWrite);
	if (!file.opened())
	{
		return false;
	}

	std::vector<FileRecord> entryList;
	entryList.reserve(m_fileRecords.size());

	const int recordSize = 16;
	const int headerSize = m_originsMode ? 20 : 16;
	const int magicMarkerSize = m_hasMagicMarker ? 16 : 0;
	uint32 position = alignSize(m_fileRecords.size() * recordSize + headerSize + magicMarkerSize);

	int processed = 0;
	std::map<uint32, FileRecord*> recordsMap;
	std::set<uint32> badRecords;
	for (std::vector<FileRecord>::const_iterator record = m_fileRecords.begin(); record != m_fileRecords.end(); record++)
	{
		//DLOG << "[" << (processed + 1) << "/" << m_fileRecords.size() << "] " << Hash::toString(record->hash);

		notifyProgress(processed, Hash::toString(record->hash));

		if (stopRequested())
		{
			return false;
		}

		FileRecord newRecord = *record;
		newRecord.offset = position;

		if (!mergeMap.empty() && mergeMap.find(record->hash) != mergeMap.end())
		{
			const uint32 targetHash = mergeMap.find(record->hash)->second;
			if (m_fileRecordsMap.find(targetHash) == m_fileRecordsMap.end())
			{
				DLOG << "Archive does not contain target file for mapping: " << Hash::toString(targetHash);
				return false;
			}
			if (mergeMap.find(targetHash) != mergeMap.end())
			{
				DLOG << "Found recursive mapping: " << Hash::toString(record->hash) << "->" << Hash::toString(targetHash) << "!";
				return false;
			}

			entryList.push_back(newRecord);
			recordsMap[record->hash] = &entryList.back();
			processed++;
			continue;
		}

		const std::vector<FileRecord>::const_iterator next = record + 1;
		if (next != m_fileRecords.end() && !record->isValid(*next))
		{
			badRecords.insert(record->hash);
			entryList.push_back(newRecord);
			recordsMap[record->hash] = &entryList.back();
			processed++;
			continue;
		}

		file.seek(position, Stream::seekSet);

		shared_ptr<Stream> stream;
		
		if (m_names.find(record->hash) != m_names.end())
		{
			stream = fileSource.fileByName(m_names.find(record->hash)->second, FileAccessor(m_stream, *record));
		}
		if (stream.get() == NULL)
		{
			stream = fileSource.file(record->hash, FileAccessor(m_stream, *record));
		}

		if (stream.get() == NULL)
		{
			m_stream->seek(record->offset, Stream::seekSet);
			file.writeStream(m_stream, record->storedSize);
		}
		else
		{
			if (record->decompressedSize == 0)
			{
				newRecord.storedSize = 0;
				newRecord.decompressedSize = 0;
				m_stream->seek(record->offset, Stream::seekSet);
				while (!stream->atEnd())
				{
					newRecord.storedSize += static_cast<uint32>(file.writeStream(stream.get(), 0x80000));
					if (stopRequested())
					{
						return false;
					}
				}				
			}
			else
			{
				CompressionStream zlibStream(&file);
				ASSERT(zlibStream.opened());				
				while (!stream->atEnd())
				{
					zlibStream.writeStream(stream.get(), 0x80000);
					if (stopRequested())
					{
						return false;
					}
				}
				zlibStream.finish();
				newRecord.storedSize = static_cast<uint32>(zlibStream.size());
				newRecord.decompressedSize = static_cast<uint32>(zlibStream.processedSize());
			}
		}

		position = alignSize(position + newRecord.storedSize);
		entryList.push_back(newRecord);
		recordsMap[record->hash] = &entryList.back();

		processed++;
	}

	// Write SH0 names
	const uint32 namesPosition = position;
	uint32 namesSize = 0;
	if (m_originsMode)
	{
		file.seek(namesPosition, Stream::seekSet);
		for (std::vector<FileRecord>::const_iterator record = entryList.begin(); record != entryList.end(); record++)
		{
			ASSERT(m_names.find(record->hash) != m_names.end());
			const std::string& name = m_names.find(record->hash)->second;
			file.write(name.c_str(), name.size() + 1);
			namesSize += name.size() + 1;
		}

		position = alignSize(position + namesSize);
	}

	if (!m_originsMode && file.size() != file.size() % m_alignment)
	{
		file.seek(alignSize(static_cast<uint32>(file.size())) - 1, Stream::seekSet);
		file.writeUInt8(0);
	}

	// Resolve mapping
	for (MergeMap::const_iterator it = mergeMap.begin(); it != mergeMap.end(); it++)
	{
		const uint32 sourceHash = it->first;
		if (m_fileRecordsMap.find(sourceHash) != m_fileRecordsMap.end())
		{
			const uint32 targetHash = it->second;
			FileRecord* sourceRecord = recordsMap[sourceHash];
			const FileRecord* targetRecord = recordsMap[targetHash];
			sourceRecord->offset = targetRecord->offset;
			sourceRecord->decompressedSize = targetRecord->decompressedSize;
			sourceRecord->storedSize = targetRecord->storedSize;
		}
	}

	// Process bad records
	for (std::vector<FileRecord>::iterator record = entryList.begin(); record != entryList.end() - 1; record++)
	{
		if (badRecords.find(record->hash) != badRecords.end() && mergeMap.find(record->hash) == mergeMap.end())
		{
			const std::vector<FileRecord>::const_iterator next = record + 1;
			record->offset = next->offset;
		}
	}

	file.seek(0, Stream::seekSet);
	file.writeUInt32(m_header.signature);
	file.writeUInt32(m_header.fileCount);
	file.writeUInt32(m_header.headerSize);

	if (m_originsMode)
	{
		file.writeUInt32(namesPosition);
		file.writeUInt32(namesSize);
	}
	else
	{
		file.writeUInt32(m_header.reserved);
	}

	uint32 nameOffset = 0;
	for (std::vector<FileRecord>::const_iterator record = entryList.begin(); record != entryList.end(); record++)
	{
		if (m_originsMode)
		{
			ASSERT(m_names.find(record->hash) != m_names.end());
			const std::string& name = m_names.find(record->hash)->second;

			file.writeUInt32(nameOffset);

			nameOffset += name.size() + 1;
		}
		else
		{
			file.writeUInt32(record->hash);
		}
		file.writeUInt32(record->offset);
		file.writeUInt32(record->storedSize);
		file.writeUInt32(record->decompressedSize);
	}

	if (m_hasMagicMarker)
	{
		file.writeUInt32(m_magicMarker1);
		file.writeUInt32(m_magicMarker2);
	}

	return true;
}

bool Archive::rebuild(const std::wstring& outFile, const std::vector<std::wstring>& dirList, const MergeMap& mergeMap)
{
	return rebuild(outFile, DirectoriesFileSource(dirList), mergeMap);
}

shared_ptr<Stream> Archive::openFile(const std::string& filename)
{
	return openFile(Hash::calc(filename.c_str()));
}

shared_ptr<Stream> Archive::openFile(uint32 fileHash)
{
	if (m_fileRecordsMap.find(fileHash) == m_fileRecordsMap.end())
	{
		return shared_ptr<Stream>();
	}

	const FileRecord& record = *m_fileRecordsMap[fileHash];
	return FileAccessor(m_stream, record).open();
}

void Archive::notifyProgressStart(int size)
{
	std::for_each(m_progressListeners.begin(), m_progressListeners.end(), std::bind2nd(std::mem_fun(&IProgressListener::startProgress), size));
}

void Archive::notifyProgress(int progressValue, const std::string& name)
{
	for (std::list<IProgressListener*>::iterator listener = m_progressListeners.begin(); listener != m_progressListeners.end(); listener++)
	{
		(*listener)->progress(progressValue, name);
	}
}

void Archive::notifyProgressEnd()
{
	std::for_each(m_progressListeners.begin(), m_progressListeners.end(), std::mem_fun(&IProgressListener::finishProgress));
}

void Archive::addProgressListener(IProgressListener* listener)
{
	m_progressListeners.push_back(listener);
}

bool Archive::stopRequested()
{
	for (std::list<IProgressListener*>::iterator listener = m_progressListeners.begin(); listener != m_progressListeners.end(); listener++)
	{
		if ((*listener)->stopRequested())
		{
			return true;
		}
	}
	return false;
}

uint32 Archive::alignment() const
{
	return m_alignment;
}

Archive::FileInfo Archive::fileInfo(uint32 hash) const
{
	if (m_fileRecordsMap.find(hash) == m_fileRecordsMap.end())
	{
		return FileInfo();
	}

	const FileRecord& record = *m_fileRecordsMap.find(hash)->second;
	FileInfo info;
	info.offset = record.offset;
	info.packed = record.isPacked();
	info.size = record.originalSize();
	info.index = std::distance(m_fileRecords.begin(), std::find(m_fileRecords.begin(), m_fileRecords.end(), record));
	return info;
}

Archive::FileInfo Archive::fileInfo(const std::string& filename) const
{
	return fileInfo(Hash::calc(filename.c_str()));
}

//////////////////////////////////////////////////////////////////////////

Archive::FileAccessor::FileAccessor(Stream* stream, const Archive::FileRecord& record)
	: m_stream(stream)
	, m_record(record)
{
}

shared_ptr<Stream> Archive::FileAccessor::open()
{
	if (m_fileStream.get() != NULL)
	{
		return m_fileStream;
	}

	auto_ptr<PartStream> pakStream(new PartStream(m_stream, m_record.offset, m_record.storedSize));
	ASSERT(pakStream->opened());

	VERIFY(pakStream->seek(0, Stream::seekSet) == 0);

	if (m_record.isPacked())
	{
		m_fileStream.reset(new DecompressionStream(pakStream.get(), m_record.originalSize()));
		pakStream.release();
		dynamic_cast<DecompressionStream*>(m_fileStream.get())->holdStream();
		return m_fileStream;
	}

	m_fileStream.reset(pakStream.release());
	return m_fileStream;
}

Archive::ProgressGuard::ProgressGuard(Archive& archive, int size)
	: m_archive(archive)
{
	m_archive.notifyProgressStart(size);
}

Archive::ProgressGuard::~ProgressGuard()
{
	m_archive.notifyProgressEnd();
}

}