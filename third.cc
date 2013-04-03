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
  uint32_t      nWifi = 10;
  uint32_t	numNodes = 20;
  //uint32_t i;
  //uint32_t j;
  std::string appDataRate = "1024kb/s";
  double   	txPower = 1; //In terms of mW
  std::string   routing = "AODV";

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
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
  NodeContainer wifiApNode = wifiStaNodes.Get (0);
  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set("TxPowerStart", DoubleValue(10.0*log10(txPower)));
  phy.Set("TxPowerEnd", DoubleValue(10.0*log10(txPower)));  
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                                "DataMode", StringValue ("OfdmRate6Mbps")); 

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);

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
  address.Assign (staDevices);

  UdpEchoServerHelper echoServer (9);

  // Install On/Off apps
  OnOffHelper UDPclientHelper ("ns3::UdpSocketFactory", Address ());
  UDPclientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.,Max=1.]"));
  UDPclientHelper.SetAttribute ("OffTime", StringValue ("0"));
  ApplicationContainer UDPclientApps;
  char bit[1000] = {1};
  // uint16_t port = 4000;
  for (uint32_t i = 0; i < nWifi ; ++i)
    {
      uint32_t send;
      // Create an on/off app sending packets to the left side
      while( true ){
        uint32_t temp = rand() % nWifi + 1;
        if( bit[temp] ){
          send = temp;
          bit[temp] = 0;
          break;
        }
      }     
      AddressValue remoteAddress (staDevices.Get(send)->GetAddress());
      UDPclientHelper.SetAttribute ("Remote", remoteAddress);
      UDPclientApps.Add (UDPclientHelper.Install (*(staDevices.Get (i))));
    }
  UDPclientApps.Start (Seconds (2.0));
  UDPclientApps.Stop (Seconds (10.0));

  uint16_t port = 9;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkUDPHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkUDPApps; 
  for (uint32_t i = 0; i < nWifi ; ++i)
    {
      sinkUDPApps.Add (packetSinkUDPHelper.Install (staDevices.Get (i)));
    }
  sinkUDPApps.Start (Seconds (0.0));
  sinkUDPApps.Stop (Seconds (20.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (10.0));

  // Create the animation object and configure for specified output
  AnimationInterface anim (animFile);
  
 std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
    NS_LOG_INFO ("Done.");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
