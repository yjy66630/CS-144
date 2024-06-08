#include "network_interface.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address,
                                   const Address& ip_address) :
    _ethernet_address(ethernet_address), _ip_address(ip_address)
{
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address)
         << " and IP address " << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default
//! gateway, but may also be another host if directly connected to the same network as the
//! destination) (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with
//! the Address::ipv4_numeric() method.)
void
NetworkInterface::send_datagram(const InternetDatagram& dgram, const Address& next_hop)
{
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto iter = _arp_cache.find(next_hop_ip);
    if (iter != _arp_cache.end()) {
        // 找到缓存，直接发送以太网帧
        EthernetFrame frame;
        frame.header().src = _ethernet_address;
        frame.header().dst = iter->second.eth_addr;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(std::move(frame));
    } else {
        auto arp_iter = _frames_cannot_find_arp.find(next_hop_ip);
        // 如果在正在等待 ARP 请求的 map 中没找到，则发送一个新的 ARP 请求
        if (arp_iter == _frames_cannot_find_arp.end()) {
            // 发送 ARP 请求
            ARPMessage seg;
            seg.opcode = ARPMessage::OPCODE_REQUEST;
            seg.sender_ethernet_address = _ethernet_address;
            seg.sender_ip_address = _ip_address.ipv4_numeric();
            seg.target_ip_address = next_hop_ip;
            EthernetFrame frame;
            frame.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
            frame.payload() = seg.serialize();
            _frames_out.push(std::move(frame));
            _frames_cannot_find_arp[next_hop_ip].first.push_back(dgram);
            _frames_cannot_find_arp[next_hop_ip].second = ARP_CONSTANT::ARP_PENDING_TTL;
        } else {
            // 对于该 dgram，过去已经发送了 ARP 报文，不需要重新发送
            if (arp_iter->second.first.size() == 0) {
                std::runtime_error("ARP请求队列异常");
            }
            arp_iter->second.first.push_back(dgram);
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram>
NetworkInterface::recv_frame(const EthernetFrame& frame)
{
    if (frame.header().dst != EthernetAddress{0xff, 0xff, 0xff, 0xff, 0xff, 0xff} &&
        frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    // 收到的是 ARP 报文
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage seg;
        seg.parse(frame.payload());
        // 收到 ARP 回复
        if (seg.opcode == ARPMessage::OPCODE_REPLY) {
            if (seg.target_ip_address == _ip_address.ipv4_numeric()) {
                _arp_cache[seg.sender_ip_address] =
                    ARPEntry{seg.sender_ethernet_address, ARP_CONSTANT::ARP_CACHE_TTL};
                // 该发送的 dgram 全部都要发送出去
                auto dgram_iter = _frames_cannot_find_arp.find(seg.sender_ip_address);
                for (auto dgram : dgram_iter->second.first) {
                    EthernetFrame tmp;
                    tmp.header() = {
                        seg.sender_ethernet_address, _ethernet_address, EthernetHeader::TYPE_IPv4};
                    tmp.payload() = dgram.serialize();
                    _frames_out.push(std::move(tmp));
                }
                _frames_cannot_find_arp.erase(dgram_iter);
            }
        } else {
            // 发送 ARP 回复
            if (seg.target_ip_address == _ip_address.ipv4_numeric()) {
                // 即使只是收到了 ARP 请求消息，也要更新 ARP 缓存
                _arp_cache[seg.sender_ip_address] =
                    ARPEntry{seg.sender_ethernet_address, ARP_CONSTANT::ARP_CACHE_TTL};
                ARPMessage arp_reply;
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply.sender_ethernet_address = _ethernet_address;
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply.target_ip_address = seg.sender_ip_address;
                arp_reply.target_ethernet_address = seg.sender_ethernet_address;
                EthernetFrame tmp;
                tmp.header() = {
                    arp_reply.target_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP};
                tmp.payload() = arp_reply.serialize();
                _frames_out.push(std::move(tmp));
            }
        }
    }

    // 收到的是以太网帧，说明这是 IP 报文
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        dgram.parse(frame.payload());
        return dgram;
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void
NetworkInterface::tick(const size_t ms_since_last_tick)
{
    auto arp_iter = _arp_cache.begin();
    while (arp_iter != _arp_cache.end()) {
        arp_iter->second.ttl -= ms_since_last_tick;
        if (arp_iter->second.ttl <= 0) {
            _arp_cache.erase(arp_iter++);
        } else {
            // 必须要这样写，否则会有bug
            arp_iter++;
        }
    }
    auto dgram_iter = _frames_cannot_find_arp.begin();
    while (dgram_iter != _frames_cannot_find_arp.end()) {
        dgram_iter->second.second -= ms_since_last_tick;
        if (dgram_iter->second.second <= 0) {
            _frames_cannot_find_arp.erase(dgram_iter++);
        } else {
            // 必须要这样写，否则会有bug
            dgram_iter++;
        }
    }
}
