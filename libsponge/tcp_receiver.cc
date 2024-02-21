#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstddef>
#include <cstdint>
#include <unistd.h>

using namespace std;

bool
TCPReceiver::segment_received(const TCPSegment& seg)
{
    uint64_t abs_seqno = 0;
    if (seg.header().syn) {
        if (_syn_flag) {   // 以前已经收到SYN，应该丢掉此分组
            return false;
        }
        _syn_flag = true;   // 已经收到SYN

        _ack.emplace(seg.header().seqno + 1);
        _isn = seg.header().seqno;
    } else if (!_syn_flag) {
        return false;   // 还没收到SYN，但是已经收到ACK等分组
    }

    // TCP随机生成的相对序列号（32位）转换为绝对序列号（64位）
    abs_seqno = unwrap(seg.header().seqno, _isn, abs_seqno);

    // 判断接受窗口是否足够大
    if (seg.payload().size() > 0) {
        if (
            // 新到来的分组由于某种原因序列号大于接受窗口，-1是因为收到SYN分组
            static_cast<int64_t>(abs_seqno - _reassembler.assembled_bytes() - 1) >=
                static_cast<int64_t>(window_size())
            // 拒绝接受旧片段
            || abs_seqno + seg.payload().size() - 1 <= _reassembler.assembled_bytes()) {
            return false;
        }
    }

    // 收到数据分组
    if (seg.payload().size() > 0) {
        _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
        // +1是因为当收到数据分组时，说明之前必定已经收到SYN分组
        _ack.emplace(wrap(_reassembler.stream_out().bytes_written() + 1, _isn));
    }

    if (seg.header().fin) {
        if (_fin_flag) {
            return false;
        }
        _fin_flag = true;

        _ack.emplace(_ack.value() + 1);
        _reassembler.stream_out().end_input();
    }
    return true;
}

optional<WrappingInt32>
TCPReceiver::ackno() const
{
    return _ack;
}

size_t
TCPReceiver::window_size() const
{
    return _capacity - _reassembler.stream_out().buffer_size();
}
