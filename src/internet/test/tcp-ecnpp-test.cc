/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Tsinghua University
 * Copyright (c) 2018 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Wenying Dai <dwy927@gmail.com>
 *          Mohit P. Tahiliani <tahiliani.nitk@gmail.com>
 */

 #include "ns3/ipv4.h"
 #include "ns3/ipv6.h"
 #include "ns3/ipv4-interface-address.h"
 #include "ns3/ipv4-route.h"
 #include "ns3/ipv6-route.h"
 #include "ns3/ipv4-routing-protocol.h"
 #include "ns3/ipv6-routing-protocol.h"
 #include "../model/ipv4-end-point.h"
 #include "../model/ipv6-end-point.h"
 #include "tcp-general-test.h"
 #include "ns3/node.h"
 #include "ns3/log.h"
 #include "tcp-error-model.h"
 #include "ns3/tcp-l4-protocol.h"
 #include "ns3/tcp-tx-buffer.h"
 #include "ns3/tcp-rx-buffer.h"
 #include "ns3/rtt-estimator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpEcnPpTestSuite");

 /**
  * \ingroup internet-test
  * \ingroup tests
  *
  * \brief A TCP socket which sends certain data packets with CE flags.
  *
  * The SendEmptyPacket function of this class are trying to construct CE SYN/ACK for case 6 and 7.
  * The SendEmptyPacket function alse helps to construct W probe packet for case 1, 2, 3, 4 and 5.
  * PersistTimeout function helps to construct CE W probe packet for 1, 2, 3, 4 and 5.
  *
  */
class TcpSocketCongestionRouter : public TcpSocketMsgBase
{
public:
  static TypeId GetTypeId (void);

  TcpSocketCongestionRouter () : TcpSocketMsgBase ()
  {
    m_controlPacketSent = 0;
  }

  TcpSocketCongestionRouter (const TcpSocketCongestionRouter &other)
    : TcpSocketMsgBase (other),
    m_testcase (other.m_testcase),
    m_who (other.m_who)
  {
  }

  enum SocketWho
  {
    SENDER,  //!< Sender node
    RECEIVER //!< Receiver node
  };

  void SetTestCase (uint32_t testCase, SocketWho who);

protected:
  virtual void SendEmptyPacket (uint16_t flags);
  virtual void PersistTimeout ();
  virtual Ptr<TcpSocketBase> Fork (void);
  void SetCE(Ptr<Packet> p);

private:
  uint32_t m_controlPacketSent;
  uint32_t m_testcase;
  SocketWho m_who;
};

NS_OBJECT_ENSURE_REGISTERED (TcpSocketCongestionRouter);

TypeId
TcpSocketCongestionRouter::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpSocketCongestionRouter")
    .SetParent<TcpSocketMsgBase> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpSocketCongestionRouter> ()
  ;
  return tid;
}

void
TcpSocketCongestionRouter::SetTestCase (uint32_t testCase, SocketWho who)
{
  m_testcase = testCase;
  m_who = who;
}

Ptr<TcpSocketBase>
TcpSocketCongestionRouter::Fork (void)
{
  return CopyObject<TcpSocketCongestionRouter> (this);
}

void
TcpSocketCongestionRouter::SendEmptyPacket (uint16_t flags)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (m_testcase) << static_cast<uint32_t> (flags));
  m_controlPacketSent++;
  if (m_endPoint == nullptr && m_endPoint6 == nullptr)
    {
      NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
      return;
    }

  Ptr<Packet> p = Create<Packet> ();
  TcpHeader header;
  SequenceNumber32 s = m_tcb->m_nextTxSequence;

  if (flags & TcpHeader::FIN)
    {
      flags |= TcpHeader::ACK;
    }
  else if (m_state == FIN_WAIT_1 || m_state == LAST_ACK || m_state == CLOSING)
    {
      ++s;
    }

  bool withEct = false;
  if (m_ecnMode == EcnMode_t::EcnPp && ((flags == (TcpHeader::SYN|TcpHeader::ACK|TcpHeader::ECE)) ||
    (flags == (TcpHeader::FIN|TcpHeader::ACK)) || (flags == TcpHeader::RST)))
    {
        withEct = true;
    }

  AddSocketTags (p, withEct);

  header.SetFlags (flags);
  header.SetSequenceNumber (s);
  header.SetAckNumber (m_rxBuffer->NextRxSequence ());
  if (m_endPoint != nullptr)
    {
      header.SetSourcePort (m_endPoint->GetLocalPort ());
      header.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
  else
    {
      header.SetSourcePort (m_endPoint6->GetLocalPort ());
      header.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
  AddOptions (header);

  // RFC 6298, clause 2.4
  m_rto = Max (m_rtt->GetEstimate () + Max (m_clockGranularity, m_rtt->GetVariation () * 4), m_minRto);

  uint16_t windowSize = AdvertisedWindowSize ();
  bool hasSyn = flags & TcpHeader::SYN;
  bool hasFin = flags & TcpHeader::FIN;
  bool isAck = flags == TcpHeader::ACK;
  if (hasSyn)
    {
      if (m_winScalingEnabled)
        { // The window scaling option is set only on SYN packets
          AddOptionWScale (header);
        }

      if (m_sackEnabled)
        {
          AddOptionSackPermitted (header);
        }

      if (m_synCount == 0)
        { // No more connection retries, give up
          NS_LOG_LOGIC ("Connection failed.");
          m_rtt->Reset (); //According to recommendation -> RFC 6298
          CloseAndNotify ();
          return;
        }
      else
        { // Exponential backoff of connection time out
          int backoffCount = 0x1 << (m_synRetries - m_synCount);
          m_rto = m_cnTimeout * backoffCount;
          m_synCount--;
        }

      if (m_synRetries - 1 == m_synCount)
        {
          UpdateRttHistory (s, 0, false);
        }
      else
        { // This is SYN retransmission
          UpdateRttHistory (s, 0, true);
        }

      windowSize = AdvertisedWindowSize (false);
    }
  header.SetWindowSize (windowSize);

  if (flags & TcpHeader::ACK)
    { // If sending an ACK, cancel the delay ACK as well
      m_delAckEvent.Cancel ();
      m_delAckCount = 0;
      if (m_highTxAck < header.GetAckNumber ())
        {
          m_highTxAck = header.GetAckNumber ();
        }
      if (m_sackEnabled && m_rxBuffer->GetSackListSize () > 0)
        {
          AddOptionSack (header);
        }
      NS_LOG_INFO ("Sending a pure ACK, acking seq " << m_rxBuffer->NextRxSequence ());
    }

  m_txTrace (p, header, this);

  // CE mark in control packet for test logic
  if ((m_testcase == 6 || m_testcase == 7) && flags == (TcpHeader::SYN|TcpHeader::ACK|TcpHeader::ECE) && m_who == RECEIVER)
  {
    SetCE(p);
  }

  if(m_testcase < 6 && (m_controlPacketSent < 6 && m_controlPacketSent > 3)  && m_who == RECEIVER)
  {
    NS_LOG_DEBUG("set Window size = 0, trying to trigger W probe");
    header.SetWindowSize (0);
  }

  if (m_endPoint != nullptr)
    {
      m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
                         m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
  else
    {
      m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
                         m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }


  if (m_retxEvent.IsExpired () && (hasSyn || hasFin) && !isAck )
    { // Retransmit SYN / SYN+ACK / FIN / FIN+ACK to guard against lost
      NS_LOG_LOGIC ("Schedule retransmission timeout at time "
                    << Simulator::Now ().GetSeconds () << " to expire at time "
                    << (Simulator::Now () + m_rto.Get ()).GetSeconds ());
      m_retxEvent = Simulator::Schedule (m_rto, &TcpSocketCongestionRouter::SendEmptyPacket, this, flags);
    }
  return;
}

void
TcpSocketCongestionRouter::PersistTimeout ()
{
  // return TcpSocketBase::PersistTimeout();
  NS_LOG_LOGIC ("PersistTimeout expired at " << Simulator::Now ().GetSeconds ());
  m_persistTimeout = std::min (Seconds (60), Time (2 * m_persistTimeout)); // max persist timeout = 60s
  Ptr<Packet> p = m_txBuffer->CopyFromSequence (1, m_tcb->m_nextTxSequence);
  m_txBuffer->ResetLastSegmentSent ();
  TcpHeader tcpHeader;
  tcpHeader.SetSequenceNumber (m_tcb->m_nextTxSequence);
  tcpHeader.SetAckNumber (m_rxBuffer->NextRxSequence ());
  tcpHeader.SetWindowSize (AdvertisedWindowSize ());
  if (m_endPoint != nullptr)
    {
      tcpHeader.SetSourcePort (m_endPoint->GetLocalPort ());
      tcpHeader.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
  else
    {
      tcpHeader.SetSourcePort (m_endPoint6->GetLocalPort ());
      tcpHeader.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
  AddOptions (tcpHeader);
  //Send a packet tag for setting ECT bits in IP header
  //Set ECT in W Probe packet when ECN++ enabled
  if (m_tcb->m_ecnState != TcpSocketState::ECN_DISABLED)
    {
      NS_LOG_DEBUG("Setting ECT for W probe");
      SocketIpTosTag ipTosTag;
      ipTosTag.SetTos (MarkEcnEct0 (0));
      p->AddPacketTag (ipTosTag);

      SocketIpv6TclassTag ipTclassTag;
      ipTclassTag.SetTclass (MarkEcnEct0 (0));
      p->AddPacketTag (ipTclassTag);
    }

  // Based on ECN++ draft 3.2.4: The sender of the probe will then reduce its congestion window as normal.
  // Congestion response for W probe in ECN++ will be postpone until the data packet send out.

  m_txTrace (p, tcpHeader, this);

  if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5)
  {
    SetCE(p);
  }

  if (m_endPoint != nullptr)
    {
      m_tcp->SendPacket (p, tcpHeader, m_endPoint->GetLocalAddress (),
                         m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
  else
    {
      m_tcp->SendPacket (p, tcpHeader, m_endPoint6->GetLocalAddress (),
                         m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }

  NS_LOG_LOGIC ("Schedule persist timeout at time "
                << Simulator::Now ().GetSeconds () << " to expire at time "
                << (Simulator::Now () + m_persistTimeout).GetSeconds ());
  m_persistEvent = Simulator::Schedule (m_persistTimeout, &TcpSocketCongestionRouter::PersistTimeout, this);
}

void TcpSocketCongestionRouter::SetCE(Ptr<Packet> p)
{
  SocketIpTosTag ipTosTag;
  ipTosTag.SetTos (MarkEcnCe (0));
  p->ReplacePacketTag (ipTosTag);

  SocketIpv6TclassTag ipTclassTag;
  ipTclassTag.SetTclass (MarkEcnCe (0));
  p->ReplacePacketTag (ipTclassTag);
}

/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief checks if ECT (in IP header), CWR and ECE (in TCP header) bits are set correctly in different patcket types.
 *
 * This test suite will run five combinations of EcnPp to different Ecn Modes.
 * case 1: SENDER EcnPp       RECEIVER NoEcn
 * case 2: SENDER EcnPp       RECEIVER ClassicEcn
 * case 3: SENDER NoEcn       RECEIVER EcnPp
 * case 4: SENDER ClassicEcn  RECEIVER EcnPp
 * case 5: SENDER EcnPp       RECEIVER EcnPp
 * The first five cases are trying to test the following things:
 * 1. ECT, CWR and ECE setting correctness in SYN, SYN/ACK and ACK in negotiation phases
 * 2. ECT setting correctness for W probe， FIN packet and RST packet
 * 3. Congestion response when W probe suffers congestion, if we treat W probe as a special data packet
 *
 * To test the congestion response for SYN/ACK packet, case 6 and 7 are constructed.
 * case 6: SENDER ClassicEcn  RECEIVER EcnPp
 * case 7: SENDER EcnPp       RECEIVER EcnPp
 * case 6 should ignore the CE mark in SYN/ACK when sender received this packet
 * cass 7 will feedback this information from sender to receiver and corresponding process.
 *
 */
class TcpEcnPpTest : public TcpGeneralTest
{
public:
  /**
   * \brief Constructor
   *
   * \param testcase test case number
   * \param desc Description about the ECN capabilities of sender and reciever
   */
  TcpEcnPpTest (uint32_t testcase, const std::string &desc);

protected:
  virtual void Rx (const Ptr<const Packet> p, const TcpHeader&h, SocketWho who);
  virtual void Tx (const Ptr<const Packet> p, const TcpHeader&h, SocketWho who);
  virtual void CWndTrace (uint32_t oldValue, uint32_t newValue);
  virtual Ptr<TcpSocketMsgBase> CreateSenderSocket (Ptr<Node> node);
  virtual Ptr<TcpSocketMsgBase> CreateReceiverSocket (Ptr<Node> node);
  void ConfigureProperties ();

private:
  uint32_t m_testcase;
  uint32_t m_senderSent;
  uint32_t m_senderReceived;
  uint32_t m_receiverSent;
  uint32_t m_receiverReceived;
  uint32_t m_cwndChangeCount;
};

TcpEcnPpTest::TcpEcnPpTest (uint32_t testcase, const std::string &desc)
  : TcpGeneralTest (desc),
  m_testcase (testcase),
  m_senderSent (0),
  m_senderReceived (0),
  m_receiverSent (0),
  m_receiverReceived (0),
  m_cwndChangeCount (0)
{
}

void TcpEcnPpTest::ConfigureProperties ()
{
  TcpGeneralTest::ConfigureProperties ();
  if (m_testcase == 1 || m_testcase == 2 || m_testcase == 5 || m_testcase == 7)
  {
    SetEcn (SENDER, TcpSocketBase::EcnPp);
  }
  else if (m_testcase == 4 || m_testcase == 6)
  {
    SetEcn (SENDER, TcpSocketBase::ClassicEcn);
  }

  if (m_testcase == 3 || m_testcase == 4 || m_testcase == 5 || m_testcase == 6 || m_testcase == 7)
  {
    SetEcn (RECEIVER, TcpSocketBase::EcnPp);
  }
  else if (m_testcase == 2)
  {
    SetEcn (RECEIVER, TcpSocketBase::ClassicEcn);
  }
}

Ptr<TcpSocketMsgBase> TcpEcnPpTest::CreateSenderSocket (Ptr<Node> node)
{
  Ptr<TcpSocketCongestionRouter> socket = DynamicCast<TcpSocketCongestionRouter> (
      CreateSocket (node, TcpSocketCongestionRouter::GetTypeId (), m_congControlTypeId));
  socket->SetTestCase (m_testcase, TcpSocketCongestionRouter::SENDER);
  return socket;
}

Ptr<TcpSocketMsgBase> TcpEcnPpTest::CreateReceiverSocket (Ptr<Node> node)
{
  Ptr<TcpSocketCongestionRouter> socket = DynamicCast<TcpSocketCongestionRouter> (
      CreateSocket (node, TcpSocketCongestionRouter::GetTypeId (), m_congControlTypeId));
  socket->SetTestCase (m_testcase, TcpSocketCongestionRouter::RECEIVER);
  return socket;
}

void
TcpEcnPpTest::CWndTrace (uint32_t oldValue, uint32_t newValue)
{
  if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5)
    {
      if (newValue < oldValue)
        {
          m_cwndChangeCount++;
          NS_LOG_DEBUG("CWndTrace" << oldValue << " " << newValue);
          if (m_cwndChangeCount == 2) // bacause of trying to trigger W probe, there is one retransmission
          {
            NS_TEST_ASSERT_MSG_EQ (newValue, oldValue / 2, "Congestion window should be reduced by half");
          }
        }
    }
}

void
TcpEcnPpTest::Rx (const Ptr<const Packet> p, const TcpHeader &h, SocketWho who)
{
  NS_LOG_FUNCTION(this << m_testcase << who);

  if (who == RECEIVER)
  {
    m_receiverReceived++;
    NS_LOG_DEBUG("Packet size is: " << p->GetSize() << " at packet: " << m_receiverReceived);
    if (m_receiverReceived == 1) // SYN for negotiation test in TCP header
    {
      NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::SYN), 0, "SYN should be received as first message at the receiver");
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 4 ||m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::ECE) && ((h.GetFlags ()) & TcpHeader::CWR), 0, "The flags ECE + CWR should be set in the TCP header of SYN at receiver when sender is ECN Capable");
      }
      else if (m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE) || ((h.GetFlags ()) & TcpHeader::CWR), 0, "The flags ECE + CWR should not be set in the TCP header of SYN at receiver when sender is not ECN Capable");
      }

    }

    if (m_receiverReceived == 2) // ACK for negotiation test in TCP header
    {
      NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::ACK), 0, "ACK should be received as second message at receiver");
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3 || m_testcase == 4 || m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE), 0, "ECE should not be set if the SYN/ACK not CE in any cases");
      }

      // test if ACK with ECE in TCP header when received SYN/ACK with CE mark
      if (m_testcase == 6)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE), 0, "ECE should not be set if the sender is classicECN when received SYN/ACK with CE mark");
      }
      else if (m_testcase == 7)
      {
        NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::ECE), 0, "ECE should be set if the sender is EcnPp when received SYN/ACK with CE mark");
      }
    }

    if (h.GetFlags () & TcpHeader::CWR) // just for debug to findout which data packet carried CWR because of previous ECE
    {
      NS_LOG_DEBUG("CWR at: " << m_receiverReceived);
    }

    if (m_receiverReceived == 12) // The next data packet after CE W probe
    {
      if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::CWR), 0, "The flag ECE should not be set in the TCP header even if W probe get congestion");
      }

      if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_NE ((h.GetFlags () & TcpHeader::CWR), 0, "The flag ECE should be set in the TCP header if W probe get congestion");
      }
    }
  }

  if (who == SENDER)
  {
    m_senderReceived++;
    if (m_senderReceived == 1) // SYN/ACK for negotiation test in TCP header
    {
      NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::SYN) && ((h.GetFlags ()) & TcpHeader::ACK), 0, "SYN+ACK received as first message at sender");
      if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5|| m_testcase == 6 || m_testcase == 7)
      {
        NS_TEST_ASSERT_MSG_NE ((h.GetFlags () & TcpHeader::ECE), 0, "The flag ECE should be set in the TCP header of SYN/ACK at sender when both receiver and sender are ECN Capable");
      }
      else if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE), 0, "The flag ECE should not be set in the TCP header of SYN/ACK at sender when either receiver or sender are not ECN Capable");
      }
    }

    if ((h.GetFlags () & TcpHeader::ECE) == TcpHeader::ECE) // just for debug to findout which ACK carried ECE because of CE
    {
      NS_LOG_DEBUG("ECE at: " << m_senderReceived);
    }

    if (m_senderReceived == 6) // ACK for W probe
    {
      if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE), 0, "The flag ECE should not be set in the TCP header even if W probe get congestion");
      }

      if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_NE ((h.GetFlags () & TcpHeader::ECE), 0, "The flag ECE should be set in the TCP header if W probe get congestion");
      }
    }
  }
}

void TcpEcnPpTest::Tx (const Ptr<const Packet> p, const TcpHeader &h, SocketWho who)
{
  NS_LOG_FUNCTION(this << m_testcase << who);
  SocketIpTosTag ipTosTag;
  p->PeekPacketTag (ipTosTag);
  uint16_t ipTos = static_cast<uint16_t> (ipTosTag.GetTos () & 0x3);
  if (who == SENDER)
  {
    m_senderSent++;
    if (m_senderSent == 1) // SYN for negotiation test in IP header
    {
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3 || m_testcase == 4 || m_testcase == 5 || m_testcase == 6 || m_testcase == 7)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in SYN");
      }
    }

    if (m_senderSent == 2) //ACK for negotiation test in IP header
    {
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3 || m_testcase == 4 || m_testcase == 5 || m_testcase == 6 || m_testcase == 7)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in pure ACK");
      }
    }

    if (p->GetSize() == 1) // W probe
    {
      NS_LOG_DEBUG("W probe being triggered " << m_senderSent);
      if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in W probe if the sender is NoEcn");
      }
      if (m_testcase == 2 || m_testcase == 4|| m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x2, "IP TOS should have ECT set in W probe if the sender is ClassicEcn/EcnPp");
      }
    }

    if (h.GetFlags() & TcpHeader::FIN) // FIN
    {
      NS_LOG_DEBUG("Send out FIN packet");
      if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in FIN if the sender is not EcnPp");
      }
      if (m_testcase == 2 || m_testcase == 5) // classicEcn will set ECT in FIN
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x2, "IP TOS should have ECT set in FIN if the sender is EcnPp");
      }
    }

    if (h.GetFlags() & TcpHeader::RST) // RST in IP header
    {
      NS_LOG_DEBUG("Send out RST packet");
    }
  }

  if (who == RECEIVER)
  {
    m_receiverSent++;
    if (m_receiverSent == 1) // SYN/ACK for negotiation test
    {
      if (m_testcase == 4 || m_testcase == 5 || m_testcase == 6 || m_testcase == 7)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x2, "IP TOS should have ECT set in SYN/ACK");
      }
      else if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in SYN/ACK");
      }
    }
  }
}
/**
 * \ingroup internet-test
 * \ingroup tests
 *
 * \brief TCP ECN++ TestSuite
 */
static class TcpEcnPpTestSuite : public TestSuite
{
public:
  TcpEcnPpTestSuite () : TestSuite ("tcp-ecnpp-test", UNIT)
  {
    AddTestCase (new TcpEcnPpTest (1, "ECN++ Negotiation Test : ECN++ capable sender and ECN incapable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (2, "ECN++ Negotiation Test : ECN++ capable sender and classicECN capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (3, "ECN++ Negotiation Test : ECN incapable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (4, "ECN++ Negotiation Test : classicECN capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (5, "ECN++ Negotiation Test : ECN++ capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (6, "ECN++ SYN+ACK CE Test : classicECN capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (7, "ECN++ SYN+ACK CE Test : ECN++ capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);

  }
} g_tcpECNPpTestSuite;

}// namespace ns3
