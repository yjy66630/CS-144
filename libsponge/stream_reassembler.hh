#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <queue>
#include <set>
#include <string>
class typeUnassembled
{
public:
    size_t index;
    std::string data;
    typeUnassembled(size_t _index, std::string _data) : index(_index), data(_data) {}
    bool
    operator<(const typeUnassembled& t1) const
    {
        return index < t1.index;
    }
};

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler
{
private:
    ByteStream _output;   //!< The reassembled in-order byte stream
    // 所有未排序的字符串的集合
    std::set<typeUnassembled> _unassembled;
    size_t _first_unassemble_byte;
    // 未被排序的字符个数，注意：集合 _unassembled 中有多少个字符这就是多少
    size_t _num_unassembled_byte;
    size_t _capacity;   //!< The maximum number of bytes
    bool _eof;

    // 合并两个_Unassembled的子串
    /*
            l1 |      | r1
            l2 |      | r2    --->  l |           | r
    */
    // \return 如果合并失败，返回 false，成功则返回 true
    bool merge_substring(size_t& index, std::string& data, size_t index2, const std::string& data2);

public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receives a substring and writes any newly contiguous bytes into the stream.
    //!
    //! If accepting all the data would overflow the `capacity` of this
    //! `StreamReassembler`, then only the part of the data that fits will be
    //! accepted. If the substring is only partially accepted, then the `eof`
    //! will be disregarded.
    //!
    //! \param data the string being added
    //! \param index the index of the first byte in `data`
    //! \param eof whether or not this segment ends with the end of the stream
    void push_substring(const std::string& data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream&
    stream_out() const
    {
        return _output;
    }
    ByteStream&
    stream_out()
    {
        return _output;
    }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been submitted twice, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;
    size_t
    first_unassembled_byte() const
    {
        return _first_unassemble_byte;
    }
    bool
    eof() const
    {
        return _eof;
    }

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif   // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
