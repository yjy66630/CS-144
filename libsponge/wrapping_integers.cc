#include "wrapping_integers.hh"
#include <cstdint>

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32
wrap(uint64_t n, WrappingInt32 isn)
{
    // n 为64位无符号整型，要强制转为32位，或者取低32位数字
    return WrappingInt32(static_cast<uint32_t>(n) + isn.raw_value());
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number (ISN)
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t
unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint)
{
    // 需要寻找离 checkpoint 最近的64位序列号
    // change the checkpoint to a seqno
    uint32_t offset = n - isn;   // 当前字节的偏移量
    if (!(checkpoint & 0xFFFFFFFF) && checkpoint <= offset) {
        return offset;
    }
    // find the closest offset t checkpoint
    uint64_t overflow_part = (checkpoint - offset) >> 32;
    uint64_t left = offset + overflow_part * (1ul << 32);
    uint64_t right = left + (1ul << 32);
    if (checkpoint - left < right - checkpoint) {
        return left;
    }
    return right;
}
