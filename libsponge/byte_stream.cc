#include "byte_stream.hh"

#include <algorithm>

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
    _size(0), _capacity(capacity), _num_write(0), _num_read(0), _if_end(false)
{
}

size_t
ByteStream::write(const string& data)
{
    // 如果超出容量限制，直接截取字符串
    size_t l = min(data.size(), _capacity - _size);
    std::string tmp = data.substr(0, l);
    _stream_buffer.append(BufferList(std::move(tmp)));
    _size += l;
    _num_write += l;
    return l;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string
ByteStream::peek_output(const size_t len) const
{
    return _stream_buffer.concatenate(std::move(min(len, _size)));
}

//! \param[in] len bytes will be removed from the output side of the buffer
void
ByteStream::pop_output(const size_t len)
{
    size_t l = min(len, _size);
    _stream_buffer.remove_prefix(l);
    _size -= l;
    _num_read += l;
}

void
ByteStream::end_input()
{
    _if_end = true;
}

bool
ByteStream::input_ended() const
{
    return _if_end;
}

size_t
ByteStream::buffer_size() const
{
    return _size;
}

bool
ByteStream::buffer_empty() const
{
    return _size == 0;
}

bool
ByteStream::eof() const
{
    // 需要无输入并且双端队列内部为空
    return _if_end && (_size == 0);
}

size_t
ByteStream::bytes_written() const
{
    return _num_write;
}

size_t
ByteStream::bytes_read() const
{
    return _num_read;
}

size_t
ByteStream::remaining_capacity() const
{
    return _capacity - _size;
}
