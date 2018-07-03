/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
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
 * Authors: Shravya Ks <shravya.ks0@gmail.com>
 *
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

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpEcnPpTestSuite");

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
  void ConfigureProperties ();

private:
  uint32_t m_testcase;
  uint32_t m_senderSent;
  uint32_t m_senderReceived;
  uint32_t m_receiverSent;
  uint32_t m_receiverReceived;
};

TcpEcnPpTest::TcpEcnPpTest (uint32_t testcase, const std::string &desc)
  : TcpGeneralTest (desc),
  m_testcase (testcase),
  m_senderSent (0),
  m_senderReceived (0),
  m_receiverSent (0),
  m_receiverReceived (0)
{
}

// case 1: SENDER EcnPp       RECEIVER NoEcn
// case 2: SENDER EcnPp       RECEIVER ClassicEcn
// case 3: SENDER NoEcn       RECEIVER EcnPp
// case 4: SENDER ClassicEcn  RECEIVER EcnPp
// case 5: SENDER EcnPp       RECEIVER EcnPp
void TcpEcnPpTest::ConfigureProperties ()
{
  TcpGeneralTest::ConfigureProperties ();
  if (m_testcase == 1 || m_testcase == 2 || m_testcase == 5)
  {
    SetEcn (SENDER, TcpSocketBase::EcnPp);
  }
  else if (m_testcase == 4)
  {
    SetEcn (SENDER, TcpSocketBase::ClassicEcn);
  }

  if (m_testcase == 3 || m_testcase == 4 ||m_testcase == 5)
  {
    SetEcn (RECEIVER, TcpSocketBase::EcnPp);
  }
  else if (m_testcase == 2)
  {
    SetEcn (RECEIVER, TcpSocketBase::ClassicEcn);
  }
}

void
TcpEcnPpTest::Rx (const Ptr<const Packet> p, const TcpHeader &h, SocketWho who)
{
  if (who == RECEIVER)
  {
    m_receiverReceived++;
    if (m_receiverReceived == 1) // SYN
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

    if (m_receiverReceived == 2) // ACK
    {
      NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::ACK), 0, "ACK should be received as second message at receiver");
    }
  }

  if (who == SENDER)
  {
    m_senderReceived++;
    if (m_senderReceived == 1) // SYN/ACK
    {
      NS_TEST_ASSERT_MSG_NE (((h.GetFlags ()) & TcpHeader::SYN) && ((h.GetFlags ()) & TcpHeader::ACK), 0, "SYN+ACK received as first message at sender");
      if (m_testcase == 2 || m_testcase == 4 || m_testcase == 5)
      {
        NS_TEST_ASSERT_MSG_NE ((h.GetFlags () & TcpHeader::ECE), 0, "The flag ECE should be set in the TCP header of SYN/ACK at sender when both receiver and sender are ECN Capable");
      }
      else if (m_testcase == 1 || m_testcase == 3)
      {
        NS_TEST_ASSERT_MSG_EQ (((h.GetFlags ()) & TcpHeader::ECE), 0, "The flag ECE should not be set in the TCP header of SYN/ACK at sender when  either receiver or sender are not ECN Capable");
      }
    }

  }
}

void TcpEcnPpTest::Tx (const Ptr<const Packet> p, const TcpHeader &h, SocketWho who)
{
  SocketIpTosTag ipTosTag;
  p->PeekPacketTag (ipTosTag);
  uint16_t ipTos = static_cast<uint16_t> (ipTosTag.GetTos () & 0x3);
  if (who == SENDER)
  {
    m_senderSent++;
    if (m_senderSent == 1) // SYN
    {
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3 || m_testcase == 4 || m_testcase == 5 )
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in SYN");
      }
    }

    if (m_senderSent == 2) //ACK
    {
      if (m_testcase == 1 || m_testcase == 2 || m_testcase == 3 || m_testcase == 4 || m_testcase == 5 )
      {
        NS_TEST_ASSERT_MSG_EQ (ipTos, 0x0, "IP TOS should not have ECT set in pure ACK");
      }
    }
  }

  if (who == RECEIVER)
  {
    m_receiverSent++;
    if (m_receiverSent == 1) // SYN/ACK
    {
      if (m_testcase == 4 || m_testcase == 5 )
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
    // AddTestCase (new TcpEcnPpTest (1, "ECN Negotiation Test : ECN++ capable sender and ECN incapable receiver"),
    //              TestCase::QUICK);
    // AddTestCase (new TcpEcnPpTest (2, "ECN Negotiation Test : ECN++ capable sender and classicECN capable receiver"),
    //              TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (3, "ECN Negotiation Test : ECN incapable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (4, "ECN Negotiation Test : classicECN capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
    AddTestCase (new TcpEcnPpTest (5, "ECN Negotiation Test : ECN++ capable sender and ECN++ capable receiver"),
                 TestCase::QUICK);
  }
} g_tcpECNPpTestSuite;

}// namespace ns3
