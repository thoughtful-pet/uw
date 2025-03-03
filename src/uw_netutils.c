#include <uw_netutils.h>

#include <arpa/inet.h>

uint16_t UW_ERROR_BAD_ADDRESS_FAMILY = 0;
uint16_t UW_ERROR_BAD_IP_ADDRESS = 0;
uint16_t UW_ERROR_MISSING_NETMASK = 0;
uint16_t UW_ERROR_BAD_NETMASK = 0;

[[ gnu::constructor ]]
static void init_netutils()
{
    // init statuses
    UW_ERROR_BAD_ADDRESS_FAMILY = uw_define_status("BAD_ADDRESS_FAMILY");
    UW_ERROR_BAD_IP_ADDRESS     = uw_define_status("BAD_IP_ADDRESS");
    UW_ERROR_MISSING_NETMASK    = uw_define_status("MISSING_NETMASK");
    UW_ERROR_BAD_NETMASK        = uw_define_status("BAD_NETMASK");
}

UwResult uw_parse_ipv4_address(UwValuePtr addr)
{
    UW_CSTRING_LOCAL(c_addr, addr);
    struct in_addr ipaddr;

    int rc = inet_pton(AF_INET, c_addr, &ipaddr);
    if (rc == -1) {
        return UwError(UW_ERROR_BAD_ADDRESS_FAMILY);
    }
    if (rc != 1) {
        UwValue error = UwError(UW_ERROR_BAD_IP_ADDRESS);
        _uw_set_status_desc(&error, "Bad IPv4 address %s", c_addr);
        return uw_move(&error);
    }
    return UwUnsigned(ntohl(ipaddr.s_addr));
}

UwResult uw_parse_ipv4_subnet(UwValuePtr subnet, UwValuePtr netmask)
{
    IPv4subnet ipv4_subnet;

    if (!uw_is_string(subnet)) {
        return UwError(UW_ERROR_BAD_IP_ADDRESS);
    }

    // check CIDR notation
    UwValue parts = uw_string_split_chr(subnet, '/');
    if (uw_error(&parts)) {
        return uw_move(&parts);
    }
    unsigned num_parts = uw_list_length(&parts);
    if (num_parts > 1) {
        // try CIDR netmask
        UwValue cidr_netmask = uw_list_item(&parts, 1);
        if (uw_error(&cidr_netmask)) {
            return uw_move(&cidr_netmask);
        }
        UW_CSTRING_LOCAL(c_netmask, &cidr_netmask);

        long n = strtol(c_netmask, nullptr, 10);
        if (n == 0 || n >= 32 || num_parts > 2) {
            UwValue error = UwError(UW_ERROR_BAD_NETMASK);
            UW_CSTRING_LOCAL(c_subnet, subnet);
            _uw_set_status_desc(&error, "Bad netmask %s", c_subnet);
            return uw_move(&error);
        }
        ipv4_subnet.netmask = 0xffffffff << (32 - n);

    } else {
        // not CIDR notation, parse netmask parameter
        if (!uw_is_string(netmask)) {
            return UwError(UW_ERROR_MISSING_NETMASK);
        }
        UwResult parsed_netmask = uw_parse_ipv4_address(netmask);
        if (uw_error(&parsed_netmask)) {
            return uw_move(&parsed_netmask);
        }
        ipv4_subnet.netmask = parsed_netmask.unsigned_value;
    }

    // parse subnet address
    UwValue addr = uw_list_item(&parts, 0);
    if (uw_error(&addr)) {
        return uw_move(&addr);
    }
    UwResult parsed_subnet = uw_parse_ipv4_address(&addr);
    if (uw_error(&parsed_subnet)) {
        return uw_move(&parsed_subnet);
    }
    ipv4_subnet.subnet = parsed_subnet.unsigned_value;

    return UwUnsigned(ipv4_subnet.value);
}
