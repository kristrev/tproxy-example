tproxy-example
==============

The Linux [iptables-firewall](http://en.wikipedia.org/wiki/Iptables) is one of
the most powerful networking tools out there. One of my favourite features is
the TPROXY-target, which, as the name implies, enables you to proxy different
types of connections.

When looking for examples of how to use TPROXY, I came up short. The only
examples I could find was the sources of large projects like Squid. I therefore
decided to create a small example showing how TPROXY can be used to proxy TCP
connections. 

The example transparent proxy application accepts TCP connections on the
specified port (set to 9876 in tproxy\_test.h) and attempts a TCP connection to
the original host. If it is successful, the application starts forwarding data
between the two connections (using splice()). The application supports multiple
simultaneous connections and handles connections which fail (at least the
scenarios I have tested).

Please note that TPROXY only works in iptables PREROUTING-chain, which is only
hit by forwarded packets. I.e., it can't be used on the same machine as the
traffic originates from.

For the transparent proxy example to work you need to configure routing and the
firewall.  The steps are found in the
[TPROXY-documentation](http://lxr.linux.no/linux+v3.10/Documentation/networking/tproxy.txt).
The only required steps are the routing and the TPROXY iptables-rule, the
DIVERT-rule is an optimisation to prevent unnecessary processing of packets in
the TPROXY target (-m socket checks for a socket matching the network packet
header).  Note that that the --tproxy-mark and fwmark must be the same, and that
--on-port is the same port as used in the transparent proxy. If you experience
any problems, the [Squid website](http://wiki.squid-cache.org/Features/Tproxy4)
has some general tips on how to get TPROXY to work.

One thing worth being aware of is that the proxy example, to avoid
over-complicating it, uses blocking sockets. So the performance might suffer
with a large number of connections.

Ideas, suggestion and fixes are more than welcome. I hope you find this
example useful!
