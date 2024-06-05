#include "stream_reassembler.hh"
#include <string>

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _output(capacity),
    _unassembled(),
    _first_unassemble_byte(0),
    _num_unassembled_byte(0),
    _capacity(capacity),
    _eof(false)
{
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.

void
StreamReassembler::push_substring(const std::string& data, const size_t index, const bool eof)
{
    _eof |= eof;

    // 如果 data 为空或收到的数据已经属于已经被排好序的
    if (data.empty() || index + data.size() <= _first_unassemble_byte) {
        // 如果所有数据已经被排好序 且 输入结束，告诉 byteStream 结束
        if (_num_unassembled_byte == 0 && _eof) {
            _output.end_input();
        }
        return;
    }

    // 第一个需要被直接丢弃的 index
    size_t first_unacceptable_byte = _first_unassemble_byte + _capacity - _output.buffer_size();

    // 相对于0，尚未排序的第一个序号
    size_t resIndex = index;
    // 相对于字符串的起始，字符串中能够被接受的第一个字符的下标
    size_t beginIndex = 0;
    // 字符串中能够被接受的最后一个字符的下标
    size_t endIndex = data.size();
    if (index < _first_unassemble_byte) {
        resIndex = _first_unassemble_byte;
        beginIndex = _first_unassemble_byte - index;
    }
    if (index + data.size() >= first_unacceptable_byte) {
        endIndex = first_unacceptable_byte - index;
    }
    std::string resData(data.begin() + beginIndex, data.begin() + endIndex);
    //           | resData |
    //        <---|iter|
    auto iter = _unassembled.lower_bound(typeUnassembled(resIndex, std::move("")));
    // 可能出现如下情况，因此需要用 while
    // 后面的大量数据因乱序到达这里，需要用 set 保存起来
    while (iter != _unassembled.begin()) {
        // resIndex > _firstUnassembled
        if (iter == _unassembled.end()) {
            iter--;
        }
        if (merge_substring(
                resIndex, resData, (*iter).index, (*iter).data)) {   // 返回值是删掉重合的bytes数
            _num_unassembled_byte -= (*iter).data.size();
            // 还多余一个字符
            if (iter != _unassembled.begin()) {
                _unassembled.erase(iter--);
            } else {
                _unassembled.erase(iter);
                break;
            }
        } else {
            // 没有重叠，说明前方的数据还未到达，直接退出
            break;
        }
    }

    //         ｜resData |
    //          | iter ... | --->
    iter = _unassembled.lower_bound(typeUnassembled(resIndex, std::move("")));
    // 集合中即使序号最小的元素也比当前元素 data 的序号要大
    // 向后遍历，看哪些字符串能被合并
    while (iter != _unassembled.end()) {
        if (merge_substring(resIndex, resData, (*iter).index, (*iter).data)) {
            _num_unassembled_byte -= (*iter).data.size();
            _unassembled.erase(iter++);
        } else {
            break;
        }
    }

    // 合并完了所有set中的元素后写入
    if (resIndex == _first_unassemble_byte) {
        size_t wSize = _output.write(resData);
        if ((wSize == resData.size()) && eof) {
            _eof = true;
            _output.end_input();
        }
        _first_unassemble_byte += wSize;
    }
    if (!resData.empty() && resIndex > _first_unassemble_byte) {
        _unassembled.insert(typeUnassembled(resIndex, resData));
        _num_unassembled_byte += resData.size();
    }

    if (empty() && _eof) {
        _output.end_input();
    }
    return;
}

bool
StreamReassembler::merge_substring(size_t& index, std::string& data, size_t index2,
                                   const std::string& data2)
{
    size_t l1 = index, r1 = l1 + data.size() - 1;
    size_t l2 = index2, r2 = l2 + data2.size() - 1;
    if (l2 > r1 + 1 || l1 > r2 + 1) {
        return false;
    }
    index = min(l1, l2);
    if (l1 <= l2) {
        if (r2 > r1) {
            data += std::string(data2.begin() + r1 - l2 + 1, data2.end());
        }
    } else {
        if (r1 > r2) {
            data = data2 + std::string(data.begin() + r2 - l1 + 1, data.end());
        } else {
            data = data2;
        }
    }
    return true;
}

size_t
StreamReassembler::unassembled_bytes() const
{
    return _num_unassembled_byte;
}

bool
StreamReassembler::empty() const
{
    return _num_unassembled_byte == 0;
}
