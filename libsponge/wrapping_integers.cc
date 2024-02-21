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
    uint32_t offset = n.raw_value() - wrap(checkpoint, isn).raw_value();
    uint64_t result = checkpoint + offset;
    if (offset > (1u << 31) && result >= (1ul << 32)) result -= (1ul << 32);
    return result;
}
