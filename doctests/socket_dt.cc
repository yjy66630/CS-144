#include "socket.hh"

#include "address.hh"
#include "util.hh"

#include <array>
#include <cstdlib>
#include <random>
#include <stdexcept>
#include <sys/socket.h>

int
main()
{
    try {
        {
            const uint16_t portnum = ((std::random_device()()) % 50000) + 1025;

            // create a UDP socket and bind it to a local address
            UDPSocket sock1{};
            sock1.bind(Address("127.0.0.1", portnum));

            // create another UDP socket and send a datagram to the first socket without connecting
            UDPSocket sock2{};
            sock2.sendto(Address("127.0.0.1", portnum), "hi there");

            // receive sent datagram, connect the socket to the peer's address, and send a response
            auto recvd = sock1.recv();
            sock1.connect(recvd.source_address);
            sock1.send("hi yourself");

            auto recvd2 = sock2.recv();

            if (recvd.payload != "hi there" || recvd2.payload != "hi yourself") {
                throw std::runtime_error("wrong data received");
            }
        }
        {
            const uint16_t portnum = ((std::random_device()()) % 50000) + 1025;

            // create a TCP socket, bind it to a local address, and listen
            TCPSocket sock1{};
            sock1.bind(Address("127.0.0.1", portnum));
            sock1.listen(1);

            // create another socket and connect to the first one
            TCPSocket sock2{};
            sock2.connect(Address("127.0.0.1", portnum));

            // accept the connection
            auto sock3 = sock1.accept();
            sock3.write("hi there");

            auto recvd = sock2.read();
            sock2.write("hi yourself");

            auto recvd2 = sock3.read();

            sock1.close();               // don't need to accept any more connections
            sock2.close();               // you can call close(2) on a socket
            sock3.shutdown(SHUT_RDWR);   // you can also shutdown(2) a socket
            if (recvd != "hi there" || recvd2 != "hi yourself") {
                throw std::runtime_error("wrong data received");
            }
        }
        {
            // create a pair of stream sockets
            std::array<int, 2> fds{};
            SystemCall("socketpair", ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds.data()));
            LocalStreamSocket pipe1{FileDescriptor(fds[0])}, pipe2{FileDescriptor(fds[1])};

            pipe1.write("hi there");
            auto recvd = pipe2.read();

            pipe2.write("hi yourself");
            auto recvd2 = pipe1.read();

            if (recvd != "hi there" || recvd2 != "hi yourself") {
                throw std::runtime_error("wrong data received");
            }
        }
    } catch (...) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
