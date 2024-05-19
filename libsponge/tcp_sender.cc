#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cstdint>
#include <random>
#include <unistd.h>

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest
//! outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise
//! uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout,
                     const std::optional<WrappingInt32> fixed_isn) :
    _isn(fixed_isn.value_or(WrappingInt32{random_device()()})),
    _initial_retransmission_timeout{retx_timeout},
    _stream(capacity),
    _retransmission_timeout(retx_timeout)
{
}

uint64_t
TCPSender::bytes_in_flight() const
{
    return _bytes_in_flight;
}

void
TCPSender::fill_window()
{
    // 发送SYN
    if (!_syn_flag) {
        TCPSegment seg;
        seg.header().syn = true;
        make_segment_and_send(seg);
        _syn_flag = true;
        return;
    }
    // 已经发送完SYN，传输数据
    uint64_t copy_window_size = _window_size ? _window_size : 1;
    while (copy_window_size - (_next_seqno - _received_seqno) > 0 && !_fin_flag) {
        uint64_t seg_size =
            min(copy_window_size - (_next_seqno - _received_seqno), TCPConfig::MAX_PAYLOAD_SIZE);
        seg_size = min(seg_size, _stream.buffer_size());
        TCPSegment seg;
        seg.payload() = _stream.peek_back_output(seg_size);
        _stream.pop_output(seg_size);
        // 如果字节流为eof，增加FIN
        if (seg.length_in_sequence_space() < copy_window_size && _stream.eof()) {
            seg.header().fin = true;
            _fin_flag = true;
        }
        // 如果字节流已经为空，但是接受窗口仍然大于0，退出
        if (seg.length_in_sequence_space() == 0) {
            return;
        }

        make_segment_and_send(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent
//! yet)
bool
TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size)
{
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (abs_ackno > _next_seqno) {
        return false;
    }

    // NOTE: ACK报文合法，需要在返回前修改 window_size
    _window_size = window_size;

    // NOTE: 这是合法的，根据测试用例得出。因为存在接收端多次发送ACK报文的情况
    if (abs_ackno <= _received_seqno) {
        return true;
    }

    // 收到的序列号小于等于 `_next_seqno` ，将 `_segment_outgoing` 的一部分或全部 pop 出去
    while (!_segments_outgoing.empty()) {
        TCPSegment seg = _segments_outgoing.front();
        uint64_t seg_abs_ackno = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (seg_abs_ackno + seg.length_in_sequence_space() <= _next_seqno) {
            _segments_outgoing.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
            _received_seqno = seg_abs_ackno + seg.length_in_sequence_space();
        } else {
            break;
        }
    }

    // 收到ACK以后也要在报文中附上数据
    fill_window();

    _consecutive_retransmissions = 0;
    _retransmission_timeout = _initial_retransmission_timeout;

    if (_segments_outgoing.empty()) {
        _time = 0;
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void
TCPSender::tick(const size_t ms_since_last_tick)
{
    _time += ms_since_last_tick;
    if (_time >= _retransmission_timeout) {
        _consecutive_retransmissions++;
        _retransmission_timeout *= 2;
        // 需要重置计时器 _time!
        _time = 0;

        _segments_out.push(_segments_outgoing.front());
    }
}

unsigned int
TCPSender::consecutive_retransmissions() const
{
    return _consecutive_retransmissions;
}

void
TCPSender::send_empty_segment()
{
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void
TCPSender::make_segment_and_send(TCPSegment seg)
{
    WrappingInt32 segno = wrap(_next_seqno, _isn);
    seg.header().seqno = segno;
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outgoing.push(seg);
}
