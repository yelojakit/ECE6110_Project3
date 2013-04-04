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
  uint32_t      nWifi = 25;
  float trafficIntensityPct = .5;
  uint32_t   	txPower = 500; //In terms of mW
  std::string   routing = "AODV";

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("trafficIntensityPct", "Set trafficIntensity", trafficIntensityPct);
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
  
  float rate = 11*1024*trafficIntensityPct/nWifi;
  char appDataRate[16];
  sprintf( appDataRate , "%dkbps" , (int) (rate) );
  Config::SetDefault ("ns3::OnOffApplication::DataRate", 
                      StringValue(appDataRate));

  NodeContainer wifiAdhocNodes;
  wifiAdhocNodes.Create (nWifi);
  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set("TxPowerStart", DoubleValue(10.0*log10(txPower))); // Convert mW to dbm
  phy.Set("TxPowerEnd", DoubleValue(10.0*log10(txPower)));   // Convert mW to dbm
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                "DataMode", StringValue ("DsssRate11Mbps"),
                                "ControlMode",StringValue ("DsssRate11Mbps")); 

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  
  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, wifiMac, wifiAdhocNodes);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"),
                                 "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));


  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiAdhocNodes);

  InternetStackHelper stack;
  if( routing == "AODV" ){
    AodvHelper aodv;
    stack.SetRoutingHelper(aodv);
  } else {
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
  } 
  stack.Install (wifiAdhocNodes);

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
  vector<uint32_t> destinations;
  for(uint32_t i = 0; i < nWifi ; ++i ){
    bit2.push_back(i);
  }
  random_shuffle( bit2.begin() , bit2.end() );
  uint16_t port = 9;
  for (uint32_t i = 0; i < nWifi ; ++i)
    {
      while( bit2[0] == i ){
        bit2.push_back(i);
        bit2.erase(bit2.begin());
      }
      AddressValue remoteAddress (InetSocketAddress (adhocInterfaces.GetAddress (bit2[0]), port));
      UDPclientHelper.SetAttribute ("Remote", remoteAddress);
      UDPclientApps.Add (UDPclientHelper.Install (wifiAdhocNodes.Get (i)));
      destinations.push_back(bit2[0]);
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
      sinkUDPApps.Add (packetSinkUDPHelper.Install (wifiAdhocNodes.Get (i)));
    }
  sinkUDPApps.Start (Seconds (0.0));
  sinkUDPApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (15.0));

  // Create the animation object and configure for specified output
  AnimationInterface anim (animFile);

  Simulator::Run ();
  uint64_t totalRx = 0;
  uint64_t totalTx = 0;
  for (uint32_t i = 0; i < sinkUDPApps.GetN (); i++)
    {
      Ptr <Application> app = sinkUDPApps.Get (i);
      Ptr <PacketSink> pktSink = DynamicCast <PacketSink> (app);
      totalRx += pktSink->GetTotalRx ();
    }
  for (uint32_t i = 0; i < UDPclientApps.GetN (); i++)
    {
      Ptr <Application> app = UDPclientApps.Get (i);
      Ptr <OnOffApplication> onOffApp = DynamicCast <OnOffApplication> (app);
      totalTx += onOffApp->m_totBytes;
    }
  
  if( totalTx > 0 ){
    printf("MyOutput\t%d\t%d\t%f\t%s\t%f\n",nWifi,txPower,trafficIntensityPct,routing.c_str(),(float) totalRx/(float) totalTx);
  } else {
    cout << "ERROR: totalTx = 0" << endl;
  }
  
  NS_LOG_INFO ("Done.");
  Simulator::Destroy ();
  return 0;
}
