#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <queue>

enum class TcpState
{
    running,
    stop
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender
{
private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out;

    //! outstanding segments that the TCPSender may resend
    std::queue<TCPSegment> _segments_outgoing;

    //! bytes in flight
    uint64_t _bytes_in_flight;

    // ！ last ackno
    uint64_t _recv_ackno;

    //! notify the window size
    uint16_t _window_size;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno;

    //! the flag of SYN sent
    bool _syn_flag;

    //! 用于记录 SYN 是否第二次发送
    bool _old_syn_flag;

    //! the flag of FIN sent
    bool _fin_sent;

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! 重传次数
    unsigned int _consecutive_retransmissions;

    //! 连接状态（是否正在运行）
    TcpState state{TcpState::stop};

    //! 内部时钟
    uint64_t _time;

    //! 超时重传时间
    uint64_t _retransmission_timeout;

    //! 设置报文序列号，并且推入发送队列
    void make_segment_and_send(TCPSegment& seg);

public:
    //! Initialize a TCPSender
    explicit TCPSender(size_t capacity = TCPConfig::DEFAULT_CAPACITY,
                       uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
                       std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream&
    stream_in()
    {
        return _stream;
    }
    const ByteStream&
    stream_in() const
    {
        return _stream;
    }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    bool ack_received(const WrappingInt32& ackno, const uint16_t& window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment>&
    segments_out()
    {
        return _segments_out;
    }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t
    next_seqno_absolute() const
    {
        return _next_seqno;
    }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32
    next_seqno() const
    {
        return wrap(_next_seqno, _isn);
    }
    //!@}

    bool
    syn_sent() const
    {
        return _syn_flag;
    }

    bool
    get_old_syn() const
    {
        return _old_syn_flag;
    }

    bool
    fin_sent() const
    {
        return _fin_sent;
    }
};

#endif   // SPONGE_LIBSPONGE_TCP_SENDER_HH
