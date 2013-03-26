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
  uint32_t nWifi = 10;
  uint32_t  pktSize = 512;
  uint32_t	pktnumber = 50;
  uint32_t	numNodes = 20;
  double   	txPower = 1;
  std::string   routing = "AODV";

  CommandLine cmd;
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("pktSize", "Set OnOff App Packet Size", pktSize);
  cmd.AddValue ("pktnumber","Number of Packets", pktnumber);
  cmd.AddValue ("numNodes", "Number of nodes", numNodes);
  cmd.AddValue ("txPower", "Transmitted Power", txPower);
  cmd.AddValue ("routing", "Routing Algorithm", routing);
  cmd.Parse (argc,argv);

  if (nWifi > 18)
    {
      std::cout << "Number of wifi nodes " << nWifi << 
                   " specified exceeds the mobility bounding box" << std::endl;
      exit (1);
    }

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  if ((routing != "AODV") && (routing != "OLSR"))
    {
      NS_ABORT_MSG ("Invalid routing algorithm: Use --routing=AODV or --routing=OLSR");
    }

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = wifiStaNodes.Get (0);
  
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.Set("TxPowerStart", DoubleValue(txPower));
  phy.Set("TxPowerEnd", DoubleValue(txPower));  
  phy.SetChannel (channel.Create ());

  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

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

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

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
  address.Assign (apDevices);

  UdpEchoServerHelper echoServer (9);

  // UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
  // echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  // echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  // echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  // ApplicationContainer clientApps = 
  //   echoClient.Install (wifiStaNodes.Get (nWifi - 1));
  // clientApps.Start (Seconds (2.0));
  // clientApps.Stop (Seconds (10.0));

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
