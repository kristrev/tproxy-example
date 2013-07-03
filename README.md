tproxy-example
==============

The Linux iptables-firewall is one of the most powerful networking tools out
there. One of the targets I have been working with lately is TPROXY, which, as
the name implies, enables you to proxy different types of connections. Combined
with the other iptables targets and matches, the imagination is the only limit 
for what TPROXY can be used for.

When looking for examples of how to use TPROXY, I came up short. The only
examples I could find was the sources of large projects like Squid. I therefore
decided to create a small example showing how TPROXY can be used to proxy TCP
connections. 

The example transparent proxy application accepts TCP connections on the
specified fort (set to 9876 in tproxy\_test.h) and attempts a TCP connection to
the original host. If it is successful, the application starts forwarding data
between the two connections (using splice()). The application supports multiple
simultaneous connections and handles connections which fail (at least the
scenarios I have tested).

For the transparent proxy example to work you also need to configure routing and
the firewall.  The steps are found in the
[TPROXY-documentation](http://lxr.linux.no/linux+v3.10/Documentation/networking/tproxy.txt).
The only required steps is the routing and the TPROXY iptables-rule, the
DIVERT-rule is an optimisation to prevent packets from unnecessary processing of
the TPROXY target (socket checkes for a socket matching network packet header).
Note that that the --tproxy-mark and fwmark must match, and that --on-port is
the same port as used in the transparent proxy.

One thing worth being aware of is that the proxy example, to avoid
over-complicating it, uses blocking sockets. So the performance might suffer
with a large number of connections.

Ideas, suggestion and fixes are more than welcome. I hope you find this
application useful!
