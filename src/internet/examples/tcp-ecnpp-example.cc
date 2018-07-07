/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 INRIA
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
 */

// Network topology
//
//     10Mbps 2ms
// n0 -------------|
//                 |  1.5Mbps 20ms      10Mbps 2ms
//                n2 -------------- n3 -------------- n4
//     10Mbps 2ms  |
// n0 -------------|


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("EcnPpExample");

static void PrintCWndCb (uint32_t oldVal, uint32_t newVal)
{
  std::cout<< Simulator::Now().GetSeconds() << "s " << newVal << std::endl;
}

static void ConfigTracing ()
{
  Config::ConnectWithoutContext ("NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&PrintCWndCb));
}

int main (int argc, char *argv[])
{
  LogComponentEnable ("EcnPpExample", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.Parse (argc, argv);

  std::string redLinkDataRate = "1.5Mbps";
  std::string redLinkDelay = "20ms";

  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  c.Create(5);
  NodeContainer n0n2 = NodeContainer (c.Get (0), c.Get (2));
  NodeContainer n1n2 = NodeContainer (c.Get (1), c.Get (2));
  NodeContainer n2n3 = NodeContainer (c.Get (2), c.Get (3));
  NodeContainer n3n4 = NodeContainer (c.Get (3), c.Get (4));

  NS_LOG_INFO ("Set default configurations.");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue(true));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue(false));
  Config::SetDefault ("ns3::TcpSocketBase::EcnMode", StringValue ("EcnPp"));

  NS_LOG_INFO ("Install internet stack.");
  InternetStackHelper internet;
  internet.Install (c);

  TrafficControlHelper tchPfifo;
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue ("1000p"));

  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (redLinkDataRate),
      "LinkDelay", StringValue (redLinkDelay));

  NS_LOG_INFO ("Create channels.");
  PointToPointHelper pointToPoint;

  pointToPoint.SetQueue ("ns3::DropTailQueue");
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devn0n2 = pointToPoint.Install (n0n2);
  tchPfifo.Install (devn0n2);
  NetDeviceContainer devn1n2 = pointToPoint.Install (n1n2);
  tchPfifo.Install (devn1n2);
  NetDeviceContainer devn3n4 = pointToPoint.Install (n3n4);
  tchPfifo.Install (devn3n4);

  pointToPoint.SetQueue ("ns3::DropTailQueue");
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (redLinkDataRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (redLinkDelay));
  NetDeviceContainer devn2n3 = pointToPoint.Install (n2n3);
  // only backbone link has RED queue disc
  QueueDiscContainer queueDiscs = tchRed.Install (devn2n3);

  NS_LOG_INFO("Assign IP Address.");
  Ipv4AddressHelper ipv4;

  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i2 = ipv4.Assign (devn0n2);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (devn1n2);

  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i3 = ipv4.Assign (devn2n3);

  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i4 = ipv4.Assign (devn3n4);

  NS_LOG_INFO("Set up routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t port = 50000;

  NS_LOG_INFO("Install Applications.");
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer sinkApp = sinkHelper.Install (c.Get (4));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper clientHelper ("ns3::TcpSocketFactory", Address());
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("10Mbps")));
  AddressValue remoteAddress (InetSocketAddress (i3i4.GetAddress (1), port));
  clientHelper.SetAttribute ("Remote", remoteAddress);
  ApplicationContainer clientApp;
  clientApp.Add (clientHelper.Install (c.Get(0)));
  clientApp.Add (clientHelper.Install (c.Get(1)));
  clientApp.Start (Seconds (0));
  clientApp.Stop (Seconds (10.0));

  Simulator::Schedule (Seconds (0.00001), &ConfigTracing);

  NS_LOG_INFO ("Run Simulation");
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

  // Ptr<PacketSink> sink = DynamicCast<PacketSink> (sinkApp.Get(0));
  // std::cout << "Total Bytes Received: " << sink->GetTotalRx () << std::endl;
  // return 0;
}
