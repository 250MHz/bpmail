import copy

from dnslib import RCODE, RD, RR, DNSLabel
from dnslib.server import BaseResolver, DNSHandler, DNSLogger, DNSServer


class CustomResolver(BaseResolver):
    """Fixed resolver with the following records:
    *.com.                 0  IN  IPN  1
    *.edu.                 0  IN  IPN  2
    *.net.                 0  IN  IPN  1
    *.net.                 0  IN  IPN  2
    *.net.                 0  IN  IPN  3
    *.org.                 0  IN  IPN  2
    *.org.                 0  IN  IPN  3
    *.org.                 0  IN  IPN  5
    xn--gieen-nqa.de.      0  IN  IPN  1
    xn--hxa3aa3a0982a.gr.  0  IN  IPN  2
    """

    def __init__(self):
        self.rrs = [
            RR(
                rname=DNSLabel('*.com.'),
                rtype=264,
                rdata=RD(data=(1).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.edu.'),
                rtype=264,
                rdata=RD(data=(2).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.net.'),
                rtype=264,
                rdata=RD(data=(1).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.net.'),
                rtype=264,
                rdata=RD(data=(2).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.net.'),
                rtype=264,
                rdata=RD(data=(3).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.org.'),
                rtype=264,
                rdata=RD(data=(2).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.org.'),
                rtype=264,
                rdata=RD(data=(3).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('*.org.'),
                rtype=264,
                rdata=RD(data=(5).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('xn--gieen-nqa.de'),
                rtype=264,
                rdata=RD(data=(1).to_bytes(8)),
            ),
            RR(
                rname=DNSLabel('xn--hxa3aa3a0982a.gr'),
                rtype=264,
                rdata=RD(data=(2).to_bytes(8)),
            ),
        ]

    def resolve(self, request, handler):
        reply = request.reply()
        qname = request.q.qname
        qtype = request.q.qtype
        for rr in self.rrs:
            name = rr.rname
            rtype = rr.rtype
            if qname.matchWildcard(name):
                if qtype in (rtype, 'ANY', 'CNAME'):
                    a = copy.copy(rr)
                    a.rname = qname
                    reply.add_answer(a)
        if not reply.rr:
            reply.header.rcode = RCODE.NXDOMAIN
        return reply


def get_dns_server(port: int) -> DNSServer:
    """Returns a DNSServer using the custom resolver"""
    resolver = CustomResolver()
    logger = DNSLogger(prefix=False)
    return DNSServer(resolver, port=port, address='', logger=logger)


if __name__ == '__main__':
    import argparse
    import time

    p = argparse.ArgumentParser(description='Custom DNS Resolver')
    p.add_argument(
        '--port',
        '-p',
        type=int,
        default=53,
        metavar='<port>',
        help='Server port (default:53)',
    )
    p.add_argument(
        '--address',
        '-a',
        default='',
        metavar='<address>',
        help='Listen address (default:all)',
    )
    p.add_argument(
        '--udplen',
        '-u',
        type=int,
        default=0,
        metavar='<udplen>',
        help='Max UDP packet length (default:0)',
    )
    p.add_argument(
        '--tcp',
        action='store_true',
        default=False,
        help='TCP server (default: UDP only)',
    )
    p.add_argument(
        '--log',
        default='request,reply,truncated,error',
        help='Log hooks to enable (default: +request,+reply,+truncated,+error,-recv,-send,-data)',
    )
    p.add_argument(
        '--log-prefix',
        action='store_true',
        default=False,
        help='Log prefix (timestamp/handler/resolver) (default: False)',
    )
    args = p.parse_args()

    resolver = CustomResolver()
    logger = DNSLogger(args.log, prefix=args.log_prefix)

    print(
        'Starting Custom Resolver (%s:%d) [%s]'
        % (args.address or '*', args.port, 'UDP/TCP' if args.tcp else 'UDP')
    )

    for rr in resolver.rrs:
        print('    | ', rr.toZone().strip(), sep='')
    print()

    if args.udplen:
        DNSHandler.udplen = args.udplen

    udp_server = DNSServer(
        resolver, port=args.port, address=args.address, logger=logger
    )
    udp_server.start_thread()

    if args.tcp:
        tcp_server = DNSServer(
            resolver, port=args.port, address=args.address, tcp=True, logger=logger
        )
        tcp_server.start_thread()

    while udp_server.isAlive():
        time.sleep(1)
