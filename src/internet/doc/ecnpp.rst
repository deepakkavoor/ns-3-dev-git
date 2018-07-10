ECN++: Adding Explicit Congestion Notification (ECN) to TCP Control Packets
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

RFC 3168 classic ECN specifies support ECN solely on data packet. 
ECN++ adding ECN support on SYN, SYN/ACK, pure ACK, W probe, FIN, RST and Re-XMT. 
ECN++ can be used either classic ECN feedback or AccECN feedback.
More information is available in ECN++ draft https://tools.ietf.org/html/draft-ietf-tcpm-generalized-ecn-02

In ns-3, ECN++ means adding ECN support in aforementioned types of TCP packet except for SYN  with classic ECN feedback.

SYN/ACK
^^^^^^^

1. the sender of SYN/ACK will set ECT in IP header in ECN++.

2. if the sender of SYN/ACK get congestion feedback, it will adopt ECN+ for congestion response.
Reduce IW from 10 segments to 1 segment.

3. if the sender of SYN/ACK timeout, it will still persist send out SYN/ACK with ECT in IP header.
ECN++ draft suggests the fall-back strategy to retransmit only one more ECT SYN/ACK and cache failed connection attempts in chap 3.2.2.3.
But it do not implement so far.

pure ACKs
^^^^^^^^^

Pure ACKs means acknowledgement packet without data.
ECN++ draft suggest to set ECT in pure ACKs. 
But the logic about congestion response in pure ACKs, ECN supporting for pure ACKs in ECN++ may be removed in next draft version. 
So not support ECN for pure ACKs in ECN++ so far.

W probe
^^^^^^^

1. The sender of W probe will set ECT in IP header.

2. If the sender of W probe get congestion feedback, there are two situations.
situation 1: the r_wnd is still zero, so the next data packet is still W probe;
situation 2: the r_wnd is open, the sender can send out data packet.
Actually, the situation 1 is rare, because we get the congestion feedback, it means the W probe is in the receiving window.
So the congestion response will be postponed until the moment new data packet send out.

3. The packet loss for W probe will be ignored, because W probe is always lost and the sender will persist to probe with the timer expires.

FIN and RST
^^^^^^^^^^^

1. The sender of FIN and RST will set ECT in IP header.

2. The congestion response for FIN and RST is not required.
Bescause, after the sender send out FIN or RST packet, no data will be send out anymore, so no need to do congestion response.

Re-XMT
^^^^^^

1. The sender of Re-XMT will set ECT in IP header.

2. The congestion response for Re-XMT packet is same as origin data packet.
The sender will postpone the congestion response till new data packet sendout.

