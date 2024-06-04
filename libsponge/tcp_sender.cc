#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest
//! outstanding segment \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise
//! uses a random ISN)
TCPSender::
TCPSender(const size_t capacity, const uint16_t retx_timeout,
          const std::optional<WrappingInt32> fixed_isn) :
    _isn(fixed_isn.value_or(WrappingInt32{random_device()()})),
    _segments_out{},
    _segments_outgoing{},
    _bytes_in_flight(0),
    _recv_ackno(0),
    _window_size(1),
    _stream(capacity),
    _next_seqno(0),
    _syn_flag(false),
    _old_syn_flag(false),
    _fin_sent(false),
    _initial_retransmission_timeout(retx_timeout),
    _consecutive_retransmissions{0},
    _time(0),
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
        // 开始发送 SYN 了，说明协议启动
        state = TcpState::running;
        return;
    }
    // 已经发送过 SYN，拒绝第二次发送
    if (!_old_syn_flag) {
        return;
    }

    // 防止接收窗口为0
    uint16_t copy_window_size = _window_size ? _window_size : 1;
    while ((copy_window_size - (_next_seqno - _recv_ackno)) > 0) {
        if (_stream.eof() && _fin_sent) {
            return;
        }
        TCPSegment seg;
        size_t size =
            min(copy_window_size - (_next_seqno - _recv_ackno), TCPConfig::MAX_PAYLOAD_SIZE);
        seg.payload() = Buffer(_stream.read(size));
        // 字节流为eof且过去没有发送过 FIN，需要增加FIN
        if (_stream.eof() && !_fin_sent) {
            seg.header().fin = true;
            _fin_sent = true;
        }
        // 如果字节流已经为空，但是接受窗口仍然大于0，退出
        if (seg.length_in_sequence_space() == 0) {
            return;
        }
        make_segment_and_send(seg);
        state = TcpState::running;
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
    if (abs_ackno <= _recv_ackno) {
        return true;
    }

    // 接收到任意合法 ACK，说明发送的 SYN 有效
    _old_syn_flag = true;

    // 收到的序列号小于等于 `abs_ackno` ，将 `_segment_outgoing` 的一部分或全部 pop 出去
    while (!_segments_outgoing.empty()) {
        TCPSegment seg = _segments_outgoing.front();
        uint64_t seg_abs_ackno = unwrap(seg.header().seqno, _isn, _next_seqno);
        if (seg_abs_ackno + seg.length_in_sequence_space() <= abs_ackno) {
            _segments_outgoing.pop();
            _bytes_in_flight -= seg.length_in_sequence_space();
            _recv_ackno = seg_abs_ackno + seg.length_in_sequence_space();
        } else {
            break;
        }
    }

    // 收到ACK以后也要在报文中附上数据
    fill_window();

    // 收到 ACK 以后需要重置计时器
    _time = 0;
    _consecutive_retransmissions = 0;
    _retransmission_timeout = _initial_retransmission_timeout;

    if (_segments_outgoing.empty()) {
        // 当没有尚未回复的报文的时候，需要停止TCP
        state = TcpState::stop;
    }
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void
TCPSender::tick(const size_t ms_since_last_tick)
{
    if (state == TcpState::stop) {
        return;
    }
    _time += ms_since_last_tick;
    if (_time >= _retransmission_timeout) {
        _consecutive_retransmissions++;
        _retransmission_timeout *= 2;
        // 需要重置计时器 _time!
        _time = 0;
        if (!_segments_outgoing.empty()) {
            _segments_out.push(_segments_outgoing.front());
        } else {
            state = TcpState::stop;
        }
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
