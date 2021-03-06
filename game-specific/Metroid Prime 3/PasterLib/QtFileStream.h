#pragma once
#include <Stream.h>
#include <QFile>

// Wrapper class for possibility to use Qt resource file system
class QtFileStream : public Consolgames::Stream
{
public:
	QtFileStream(const QString& filename, QIODevice::OpenMode mode = QIODevice::ReadOnly);

	virtual largesize_t read(void* buf, largesize_t size) override;
	virtual largesize_t write(const void* buf, largesize_t size) override;
	virtual offset_t seek(offset_t offset, SeekOrigin origin) override;
	virtual offset_t position() const override;
	virtual void flush() override;
	virtual offset_t size() const override;
	virtual bool opened() const override;
	virtual bool atEnd() const override;
	void close();

private:
	QFile m_file;
};