#include "tcp_connection.hh"

#include <iostream>
#include <limits>

using namespace std;

size_t
TCPConnection::remaining_outbound_capacity() const
{
    return _sender.stream_in().remaining_capacity();
}

size_t
TCPConnection::bytes_in_flight() const
{
    return _sender.bytes_in_flight();
}

size_t
TCPConnection::unassembled_bytes() const
{
    return _receiver.unassembled_bytes();
}

size_t
TCPConnection::time_since_last_segment_received() const
{
    return _time_since_last_segment_received;
}

void
TCPConnection::set_ack_and_window(TCPSegment& seg)
{
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    }

    if (_receiver.window_size() < numeric_limits<uint16_t>::max()) {
        seg.header().win = _receiver.window_size();
    } else {
        seg.header().win = numeric_limits<uint16_t>::max();
    }
}

void
TCPConnection::segment_received(const TCPSegment& seg)
{
    // 收到报文，计时器置零
    _time_since_last_segment_received = 0;

    // 收到报文，需要将其转交给receiver程序，更新ACK
    _receiver.segment_received(seg);

    // 如果是RST报文
    if (seg.header().rst) {
        // 特殊情况1判断
        // 在发送 SYN 之前收到 RST 报文，忽略
        if (!_sender.syn_sent()) {
            return;
        }
        // 特殊情况2判断
        // 在 SYN_SENT 中，发送端只发送了 SYN 报文
        // 必须要先接收 ACK 报文，然后接收 RST 报文才能拒绝通信，如同三次握手，否则忽略该报文
        // SYN -->
        //      <-- ACK  -->    SYN -->
        //      <-- SYN              <-- SYN/ACK
        // ACK -->              ACK -->
        if (!seg.header().ack && _sender.next_seqno_absolute() == 1 &&
            _sender.bytes_in_flight() == _sender.next_seqno_absolute()) {
            // 后两个条件是判断是否在 SYN_SENT 中，即看包含 SYN 和 FIN 的绝对序列号是否为1
            return;
        }
        // 特殊情况3判断
        // 在 SYN_SENT 中，收到 RST/ACK 报文，协议状态转入 RESET 并且不发送任何报文
        if (seg.header().ack && _sender.next_seqno_absolute() == 1 &&
            _sender.bytes_in_flight() == _sender.next_seqno_absolute()) {
            // 出现RST信号，丢弃所有报文
            while (!_sender.segments_out().empty()) {
                _sender.segments_out().pop();
            }
            _if_active = false;
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            return;
        }
        // 正常情况，通知下层发送 RST 报文
        send_reset_segment();
        return;
    }

    // If the inbound stream ends before the `TCPConnection` has reached EOF
    // on its outbound stream, `_linger_after_streams_finish` should be false
    // 当接收端主动关闭连接会出现这种情况，此时接收端出现
    // EOF，但是发送端缓冲区中可能还有数据没有发送。此时再调用 tick 就会直接关闭半连接。
    // 如果数据已在客户端的缓冲区中（内核级，等待应用程序将其读取到用户空间内存中），
    // 则服务器无法阻止它被读取。这就像蜗牛邮件一样：一旦你把它寄出去，你就无法撤消它。
    if (_receiver.stream_out().eof() && !_sender.stream_in().eof() && _sender.syn_sent()) {
        _linger_after_streams_finish = false;
    }

    // 将其转交给sender程序，发送新的报文
    if (seg.header().ack) {
        // 未发送 SYN 时就收到 ACK 报文，
        // 对应测试用例 fsm_ack_rst_relaxed: ACKs in LISTEN
        if (!_sender.syn_sent()) {
            return;
        }
        if (_sender.ack_received(seg.header().ackno, seg.header().win)) {
            _sender.fill_window();
            while (!_sender.segments_out().empty()) {
                TCPSegment segment = _sender.segments_out().front();
                _sender.segments_out().pop();
                send_segment(segment);
            }
        } else {
            // 根据测试用例：fsm_ack_rst_relaxed: ack/rst in SYN_SENT
            if (!_sender.get_old_syn()) {
                return;
            }
            // 根据测试用例：fsm_ack_rst_relaxed: ack in the future -> sent ack back
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_and_window(segment);
            _segments_out.push(segment);
        }
    }

    if (seg.length_in_sequence_space() > 0) {
        // 告诉 sender 发送数据，设置 SYN
        _sender.fill_window();

        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
            TCPSegment segment = _sender.segments_out().front();
            _sender.segments_out().pop();
            set_ack_and_window(segment);
            _segments_out.push(segment);
        } else {
            while (!_sender.segments_out().empty()) {
                TCPSegment segment = _sender.segments_out().front();
                _sender.segments_out().pop();
                send_segment(segment);
            }
        }
    }
}

bool
TCPConnection::active() const
{
    return _if_active;
}

size_t
TCPConnection::write(const string& data)
{
    size_t length = _sender.stream_in().write(data);
    _sender.fill_window();
    // 能取的都取出去
    while (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        send_segment(segment);
    }
    return length;
}

void
TCPConnection::send_segment(const TCPSegment& segment)
{
    TCPSegment seg = segment;
    set_ack_and_window(seg);
    _segments_out.push(seg);
}

void
TCPConnection::send_reset_segment()
{
    // 出现RST信号，丢弃所有报文
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    // 取出空的报文
    auto segment = _sender.segments_out().front();
    _sender.segments_out().pop();

    segment.header().rst = true;

    send_segment(segment);
    _if_active = false;
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
}


//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void
TCPConnection::tick(const size_t ms_since_last_tick)
{
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // 超时重传
    if (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        set_ack_and_window(segment);
        if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
            _if_active = false;
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            segment.header().rst = true;
        }
        _segments_out.push(segment);
    }

    // 半连接关闭，需要满足以下条件：
    // 1. 接收端先关闭（半连接，由 _linger_after_streams_finish 负责）
    // 2. 发送端已经发送 FIN
    // 3. 发送端缓冲区为0
    if (!_linger_after_streams_finish && _sender.fin_sent() && _sender.bytes_in_flight() == 0) {
        _if_active = false;
    }

    // 需要满足以下三个条件才能结束：
    // 1. receiver 中字节流已经被全部整流并且收到了 EOF
    // 2. sender 已经被关闭并且全部字节流发送给了另一个端点
    // 3. sender 中所有字节被确认
    if (_receiver.stream_out().eof() && _sender.stream_in().eof() && _sender.fin_sent() &&
        _sender.bytes_in_flight() == 0) {
        if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _if_active = false;
        }
    }
}

void
TCPConnection::end_input_stream()
{
    _sender.stream_in().end_input();
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        send_segment(segment);
    }
}

void
TCPConnection::connect()
{
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        send_segment(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

TCPConnection::~TCPConnection()
{
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_reset_segment();
        }
    } catch (const exception& e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
