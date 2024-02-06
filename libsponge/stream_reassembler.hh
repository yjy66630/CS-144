#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <functional>
#include <queue>
#include <set>
#include <string>
#include <vector>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler
{
public:
    // Your code here -- add private members as necessary.
    class stream
    {
    public:
        stream(std::string str, uint64_t index, bool eof) :
            m_data(str), m_begin(index), m_len(m_data.size()), m_eof(eof)
        {
        }
        std::string m_data;
        uint64_t m_begin;
        uint64_t m_len;
        bool m_eof;
    };

private:
    // streamCmp想用function包裹，但是失败了。。。
    // std::function<bool(stream, stream)> streamCmp = [](stream a, stream b) {
    //     return a.m_begin > b.m_begin;
    // };

    struct streamCmp
    {
        bool
        operator()(stream a, stream b)
        {
            return a.m_begin > b.m_begin;
        }
    };

    ByteStream _output;   //!< The reassembled in-order byte stream
    size_t _capacity;     //!< The maximum number of bytes
    bool m_eof = false;
    uint64_t m_reorderedFlag = 0;     // the flag of HAS NOT been reassembled in output byte stream
    uint64_t m_assembledBytes = 0;    // the number of bytes that have been assembled
    std::set<char> m_inputSet = {};   // the set of all bytes that have buffered
    // the queue of stashing unordered byte stream
    std::priority_queue<stream, std::vector<stream>, streamCmp> m_streamBuffer = {};

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

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif   // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
