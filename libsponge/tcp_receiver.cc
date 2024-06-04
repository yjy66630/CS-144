#include "tcp_receiver.hh"

using namespace std;

bool
TCPReceiver::segment_received(const TCPSegment& seg)
{
    const bool old_syn_received = _syn_received, old_fin_received = _fin_received;

    // 收到一个分组，但不是SYN，以前也没有收到过SYN
    if (!seg.header().syn && !_syn_received) {
        return false;
    }
    if (_reassembler.eof() && seg.header().fin) {
        return false;
    }

    if (seg.header().syn) {
        if (_syn_received) {   // 以前已经收到SYN，应该丢掉此分组
            return false;
        }
        _isn = seg.header().seqno;
        _syn_received = true;   // 已经收到SYN
    }

    uint64_t win_start = unwrap(_ackno, _isn, _checkpoint);
    uint64_t win_size = window_size() ? window_size() : 1;
    uint64_t win_end = win_start + win_size - 1;

    // TCP随机生成的相对序列号（32位）转换为绝对序列号（64位）
    uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _checkpoint);
    uint64_t abs_seqno_size = seg.length_in_sequence_space();
    abs_seqno_size = (abs_seqno_size) ? abs_seqno_size : 1;
    uint64_t abs_seqno_end = abs_seqno + abs_seqno_size - 1;

    bool inbound = (abs_seqno >= win_start && abs_seqno <= win_end)   // 整个都在窗口中
                   ||
                   (abs_seqno_end >= win_start && abs_seqno_end <= win_end);   // 后半部分进入窗口

    if (inbound) {
        _reassembler.push_substring(
            seg.payload().copy(), abs_seqno - 1, seg.header().fin);   // 忽视syn，所以减1
        _checkpoint = _reassembler.first_unassembled_byte();
    }

    if (seg.header().fin && !_fin_received) {
        _fin_received = true;
        // if flags = SF and payload_size = 0, we need to end_input() the stream manually
        if (seg.header().syn && seg.length_in_sequence_space() == 2) {
            stream_out().end_input();
        }
    }

    _ackno = wrap(_reassembler.first_unassembled_byte() + 1 +
                      (_fin_received && (_reassembler.unassembled_bytes() == 0)),
                  _isn);   //+1因为bytestream不给syn标号

    // second syn or fin will be rejected
    if (inbound || (seg.header().fin && !old_fin_received) ||
        (seg.header().syn && !old_syn_received)) {
        return true;
    }
    return false;
}

optional<WrappingInt32>
TCPReceiver::ackno() const
{
    if (!_syn_received) {
        return nullopt;
    } else {
        return {_ackno};
    }
}

size_t
TCPReceiver::window_size() const
{
    return stream_out().remaining_capacity();
}
