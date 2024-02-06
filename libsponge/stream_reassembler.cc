#include "stream_reassembler.hh"
#include <string>
#include <sys/socket.h>
#include <utility>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`
using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity)
{
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void
StreamReassembler::push_substring(const string& data, const size_t index, const bool eof)
{
    if (_output.remaining_capacity() == 0) {
        return;
    }
    stream substring(data, index, eof);
    m_streamBuffer.push(substring);
    // 储存到目前为止所有输入的数据集合
    for (char i : data) {
        m_inputSet.insert(i);
    }

    while (m_reorderedFlag >= m_streamBuffer.top().m_begin && !m_streamBuffer.empty()) {
        stream topSubstring = m_streamBuffer.top();
        if (m_reorderedFlag == topSubstring.m_begin) {
            // 到来的字符串处于正常状态
            m_streamBuffer.pop();
            // 注意有可能插入的字符串长度大于capacity
            m_reorderedFlag += min(topSubstring.m_len, _output.remaining_capacity());
            m_assembledBytes += _output.write(topSubstring.m_data);
            if (topSubstring.m_eof) {
                _output.end_input();
            }
        } else {
            if (m_reorderedFlag >= topSubstring.m_begin + topSubstring.m_len) {
                // 说明到来的字符串是多余的
                m_streamBuffer.pop();
            } else {
                // 到来的字符串与前面的重叠
                m_streamBuffer.pop();
                std::string unoverlapSubstring =
                    topSubstring.m_data.substr(m_reorderedFlag - topSubstring.m_begin);
                // 注意有可能插入的字符串长度大于capacity
                m_reorderedFlag += min(unoverlapSubstring.size(), _output.remaining_capacity());
                m_assembledBytes += _output.write(unoverlapSubstring);
                if (topSubstring.m_eof) {
                    _output.end_input();
                }
            }
        }
    }
}

size_t
StreamReassembler::unassembled_bytes() const
{
    return m_inputSet.size() - m_assembledBytes;
}

bool
StreamReassembler::empty() const
{
    return m_streamBuffer.empty();
}
