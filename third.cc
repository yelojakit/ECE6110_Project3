/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-variable.h"
#include "ns3/udp-client-server-helper.h"
#include "string.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/olsr-helper.h"
#include <stdlib.h>
#include <algorithm>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("P3");

int 
main (int argc, char *argv[])
{
  LogComponentEnable ("P3", LOG_LEVEL_ALL);
  NS_LOG_INFO ("Creating Topology");
  std::string animFile = "AnimTrace.xml" ;  // Name of file for animation output
  bool verbose = true;
  uint32_t      nWifi = 50;
  std::string appDataRate = "1024kb/s";
  double   	txPower = 500; //In terms of mW
  std::string   routing = "AODV";

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("txPower", "Transmitted Power", txPower);
  cmd.AddValue ("routing", "Routing Algorithm", routing);
  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  if ((routing != "AODV") && (routing != "OLSR"))
    {
      NS_ABORT_MSG ("Invalid routing algorithm: Use --routing=AODV or --routing=OLSR");
    }

  Config::SetDefault ("ns3::OnOffApplication::DataRate", 
                      StringValue (appDataRate));

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set("TxPowerStart", DoubleValue(10.0*log10(txPower)));
  phy.Set("TxPowerEnd", DoubleValue(10.0*log10(txPower)));  
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                "DataMode", StringValue ("OfdmRate6Mbps")); 

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, wifiMac, wifiStaNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));


  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiStaNodes);

  InternetStackHelper stack;
  if( routing == "AODV" ){
    AodvHelper aodv;
    stack.SetRoutingHelper(aodv);
  } else {
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
  } 
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces =   address.Assign (staDevices);

  NS_LOG_INFO ("Creating On/Off apps");
  // Install On/Off apps
  OnOffHelper UDPclientHelper ("ns3::UdpSocketFactory", Address ());
  UDPclientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.,Max=1.]"));
  UDPclientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.,Max=0.]"));
  ApplicationContainer UDPclientApps;
  vector<uint32_t> bit2;
  for(uint32_t i = 0; i < nWifi ; ++i ){
    bit2.push_back(i);
  }
  random_shuffle( bit2.begin() , bit2.end() );
  // char bit[1000] = {1};
  uint16_t port = 9;
  for (uint32_t i = 0; i < nWifi ; ++i)
    {
      while( bit2[0] == i ){
        bit2.push_back(i);
        bit2.erase(bit2.begin());
      }
      char msg[50] = "";
      sprintf( msg , "%u sends to %u" , i , bit2[0] );
      NS_LOG_INFO (msg);
      AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (bit2[0]), port));
      UDPclientHelper.SetAttribute ("Remote", remoteAddress);
      UDPclientApps.Add (UDPclientHelper.Install (wifiStaNodes.Get (i)));
      bit2.erase(bit2.begin());
    }
  UDPclientApps.Start (Seconds (2.0));
  UDPclientApps.Stop (Seconds (10.0));

  NS_LOG_INFO ("Creating UDP sink apps");
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkUDPHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkUDPApps; 
  for (uint32_t i = 0; i < nWifi ; ++i)
    {
      sinkUDPApps.Add (packetSinkUDPHelper.Install (wifiStaNodes.Get (i)));
    }
  sinkUDPApps.Start (Seconds (0.0));
  sinkUDPApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));

  // Create the animation object and configure for specified output
  AnimationInterface anim (animFile);
  // Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  monitor->SerializeToXmlFile("xmlfile.xml",false,false);
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if ((t.destinationAddress == "10.2.1.1"))
        {
          double start = i->second.timeFirstTxPacket.GetSeconds();
          minStartTime = start < minStartTime ? start:minStartTime;
          double last = i->second.timeLastRxPacket.GetSeconds();
          maxEndTime = last > maxEndTime ? last:maxEndTime;
          std::cout << "Flow " << i->first << ": " << last << "-" << 
            start << " = " << last-start << "\t" << i->second.rxBytes << 
            std::endl;
          
        }
    }
  
 std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
    NS_LOG_INFO ("Done.");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
