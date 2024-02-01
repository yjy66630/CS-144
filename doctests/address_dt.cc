#include "address.hh"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int
main()
{
    try {
        const Address google_webserver("www.google.com", "https");
        const Address a_dns_server("18.71.0.151", 53);
        const uint32_t a_dns_server_numeric = a_dns_server.ipv4_numeric();
        if ((google_webserver.port() != 443) || (a_dns_server_numeric != 0x12'47'00'97)) {
            throw std::runtime_error("unexpected value");
        }
    } catch (const std::exception& e) {
        std::cerr << "This test requires Internet access and working DNS.\n";
        std::cerr << "Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
