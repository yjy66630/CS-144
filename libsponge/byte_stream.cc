#include "byte_stream.hh"
#include <cstddef>
#include <string>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
    m_capacity(capacity), m_deque(), m_error(false), m_if_end(false), m_num_read(0), m_num_write(0)
{
}

size_t
ByteStream::write(const string& data)
{
    size_t len = min(data.size(), m_capacity - this->buffer_size());
    size_t i = 0;
    for (; i < len; i++) {
        m_deque.push_back(data[i]);
    }
    m_num_write += i;
    return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string
ByteStream::peek_output(const size_t len) const
{
    string str = "";
    for (size_t i = 0; i < len && !m_deque.empty(); i++) {
        str += m_deque.at(this->buffer_size() - len + i);
    }
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void
ByteStream::pop_output(const size_t len)
{
    for (size_t i = 0; i < len; i++) {
        m_deque.pop_front();
    }
    m_num_read += len;
    return;
}

void
ByteStream::end_input()
{
    m_if_end = true;
    return;
}

bool
ByteStream::input_ended() const
{
    return m_if_end;
}

size_t
ByteStream::buffer_size() const
{
    return m_deque.size();
}

bool
ByteStream::buffer_empty() const
{
    return m_deque.empty();
}

bool
ByteStream::eof() const
{
    // 需要无输入并且双端队列内部为空
    return m_if_end && m_deque.empty();
}

size_t
ByteStream::bytes_written() const
{
    return m_num_write;
}

size_t
ByteStream::bytes_read() const
{
    return m_num_read;
}

size_t
ByteStream::remaining_capacity() const
{
    return m_capacity - m_deque.size();
}
