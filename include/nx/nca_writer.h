/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include <functional>
#include <switch.h>
#include <vector>
#include "nx/ncm.hpp"
#include <memory>

class CloseableWriter
{
public:
    // subclasses that override close() must call it from their destructor.
	virtual ~CloseableWriter() { CloseableWriter::close(); }

	virtual void write(const u8* data, u64 size) = 0;

	// overrides must be idempotent.
	// overrides must call the parent close().
	virtual void close() { m_isClosed = true; }

	bool isClosed() const { return m_isClosed; }

private:
	bool m_isClosed = false;
};

// simple call-back Writer with no life-cycle methods
using WriterFn = void(const u8* data, u64 size);

class NcaBodyWriter : public CloseableWriter, public std::enable_shared_from_this<NcaBodyWriter>
{
public:
	static constexpr u64 CONTENT_BUFFER_SIZE = 0x800000; // 8MB

	NcaBodyWriter(const NcmContentId& ncaId, u64 offset, std::shared_ptr<nx::ncm::ContentStorage>& contentStorage);
	~NcaBodyWriter() override;

	void write(const  u8* ptr, u64 sz) override;

	// subclasses should override doBeforeClose() and doClose() instead.
	void close() final;

	// returns a write function that calls NcaBodyWriter::write() directly.
	// the caller must hold this object via shared_ptr.
	std::function<WriterFn> getDirectWriterFn();

protected:

	// called before the final flush. subclasses should flush their writers here.
	virtual void doBeforeClose() {}

	// called before freeing resources. subclasses should clean up here.
	virtual void doClose() {}

	// overrides must call the parent flushContentBuffer() to flush to storage.
	virtual void flushContentBuffer();

	std::vector<u8> m_contentBuffer;
	std::shared_ptr<nx::ncm::ContentStorage> m_contentStorage;
	NcmContentId m_ncaId;

	u64 m_offset;
};

class NcaWriter : public CloseableWriter
{
public:
	NcaWriter(const NcmContentId& ncaId, std::shared_ptr<nx::ncm::ContentStorage>& contentStorage);
	~NcaWriter() override;

	void close() override;
	void write(const  u8* ptr, u64 sz) override;
	void flushHeader();

protected:
	NcmContentId m_ncaId;
	std::shared_ptr<nx::ncm::ContentStorage> m_contentStorage;
	std::vector<u8> m_buffer;
	std::shared_ptr<NcaBodyWriter> m_writer;
	bool m_headerFlushed = false;
};
