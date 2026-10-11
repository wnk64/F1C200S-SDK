/(^|: )sending ACK to / {
    address = $NF
    if (address !~ /^192[.]168[.]49[.][0-9]+$/) next
    split(address, octets, ".")
    if (octets[4] < 2 || octets[4] > 254) next
    print "REQUEST udhcpd ip=" address
    fflush()
}
