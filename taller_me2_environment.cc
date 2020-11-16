/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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

//
// This program configures a grid (default 5x5) of nodes on an
// 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to node 1.
//
// The default layout is like this, on a 2-D grid.
//
// n20  n21  n22  n23  n24
// n15  n16  n17  n18  n19
// n10  n11  n12  n13  n14
// n5   n6   n7   n8   n9
// n0   n1   n2   n3   n4
//
// the layout is affected by the parameters given to GridPositionAllocator;
// by default, GridWidth is 5 and numNodes is 25..
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc-grid --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the ns-3 documentation.
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when distance increases beyond
// the default of 500m.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --distance=500"
// ./waf --run "wifi-simple-adhoc --distance=1000"
// ./waf --run "wifi-simple-adhoc --distance=1500"
//
// The source node and sink node can be changed like this:
//
// ./waf --run "wifi-simple-adhoc --sourceNode=20 --sinkNode=10"
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc-grid --verbose=1"
//
// By default, trace file writing is off-- to enable it, try:
// ./waf --run "wifi-simple-adhoc-grid --tracing=1"
//
// When you are done tracing, you will notice many pcap trace files
// in your directory.  If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-grid-0-0.pcap -nn -tt
//

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Taller_ME");

uint32_t bridgeAB_id;
uint32_t bridgeAC_id;

uint32_t sourceNode = 0; // numero del nodo de salida
uint32_t sinkNode = 4; // numero del nodo de llegada

int numberOfPackets;
uint32_t packetSize;
double interval;

Ipv4InterfaceContainer ipv4Interface_Cluster_A;
Ipv4InterfaceContainer ipv4Interface_Cluster_B;
Ipv4InterfaceContainer ipv4Interface_Cluster_C;

// Función generadora de tráfico
static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval) {
  if (pktCount > 0) {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
                           socket, pktSize,pktCount - 1, pktInterval);
  } else {
      socket->Close ();
  }
}

// Función callback llamada tras la recepción de paquetes
void ReceivePacket (Ptr<Socket> socket) {
    bool retransmit = false;
    while (socket->Recv ()) {
        retransmit = false;
        uint32_t id_of_reciving_node = socket->GetNode()->GetId();
        NS_LOG_UNCOND (id_of_reciving_node << " Received one packet!");
        TypeId tidP = TypeId::LookupByName ("ns3::UdpSocketFactory");
        Ptr<Socket> source = Socket::CreateSocket(socket->GetNode(),tidP);
        InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"),80);

        if (id_of_reciving_node == bridgeAB_id) {
            NS_LOG_UNCOND ("Cluster head for net B");
            retransmit = true;
            remote = InetSocketAddress(ipv4Interface_Cluster_A.GetAddress (bridgeAC_id, 0),80);
        } else if (id_of_reciving_node == bridgeAC_id){
            NS_LOG_UNCOND ("Cluster head for net C");
            retransmit = true;
            remote = InetSocketAddress(ipv4Interface_Cluster_C.GetAddress (sinkNode, 0),80);
        } else {
            NS_LOG_UNCOND ("Destiny Node!!");
            numberOfPackets++;
            std::cout <<  "SE HAN RECIBIDO " << numberOfPackets << '\n';
        }

        if(retransmit) {
            source->SetAllowBroadcast(true);
            source->Connect(remote);
            Simulator::ScheduleWithContext(id_of_reciving_node,Seconds(0.1),
                                            &GenerateTraffic,source,packetSize,1,Seconds(interval));
        }
    }
}

int main (int argc, char *argv[]) {
  //--------------------------------------------------------------
  //---                 Definicion de variables                ---
  //--------------------------------------------------------------

  std::string phyMode ("DsssRate1Mbps");
  double distanceA = 100;  // distancia inicial entre nodos en el cuadrante A
  double distanceB = 100;  // distancia inicial entre nodos en el cuadrante B
  double distanceC = 100;  // distancia inicial entre nodos en el cuadranteC
  packetSize = 1024; // tamaño de los paquetes en bytes
  uint32_t numPackets = 100; // numero de paquetes a enviar 
  uint32_t numNodesA = 25;  // numero de nodos cuandrante A
  uint32_t numNodesB = 25;  // numero de nodos cuandrante B
  uint32_t numNodesC = 25;  // numero de nodos cuandrante C
  double originA_x = numNodesB/5 * 100 + 10;  // coordenada x origen del cuandrante A
  double originA_y = numNodesB/5 * 100 + 10;  // coordenada y origen del cuandrante A
  double originB_x = 100.0;  // coordenada x origen del cuandrante B
  double originB_y = 100.0;  // coordenada y origen del cuandrante B
  double originC_x = (numNodesB+numNodesA)/5 * 100 + 10;  // coordenada x origen del cuandrante C
  double originC_y = 100.0;  // coordenada y origen del cuandrante C
  char sourceCluster = 'B'; // Sector de origen de los paquetes
  char sinkCluster = 'C'; // Sector de llegada de los paquetes
  interval = 1.0; // Intevalo entre paquetes
  bool verbose = false;
  bool tracing = false;

 // Se agregan las varibles a la linea de comandos como parametros que pueden ser recibidos al ejecutar la simulación
  CommandLine cmd;
  cmd.AddValue ("distanceA", "distance (m)A", distanceA);
  cmd.AddValue ("distanceB", "distance (m)B", distanceB);
  cmd.AddValue ("distanceC", "distance (m)C", distanceC);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodesA", "number of nodes in A", numNodesA);
  cmd.AddValue ("numNodesB", "number of nodes in B", numNodesB);
  cmd.AddValue ("numNodesC", "number of nodes in C", numNodesC);
  cmd.AddValue ("originA_x", "Origin x sector A", originA_x);
  cmd.AddValue ("originA_y", "Origin y sector A", originA_y);
  cmd.AddValue ("originB_x", "Origin x sector B", originB_x);
  cmd.AddValue ("originB_y", "Origin y sector B", originB_y);
  cmd.AddValue ("originC_x", "Origin x sector C", originC_x);
  cmd.AddValue ("originC_y", "Origin y sector C", originC_y);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);
  cmd.Parse (argc, argv);

  uint32_t simTime = numPackets*interval+1;

  // Se parsea el intervalo como un objeto de tiempo
  Time interPacketInterval = Seconds (interval);

  // Se establece un numero minimo de nodos por cuadrante
  uint32_t minNodes = 3;
  if (numNodesA<minNodes) {
      numNodesA = minNodes;
  }
  if (numNodesB<minNodes) {
      numNodesB = minNodes;
  }
  if (numNodesC<minNodes) {
      numNodesC = minNodes;
  }

  // Si el sector de salida y llegada no es A, B o C, se establece un predeterminado
  if (sourceCluster!='A' && sourceCluster!='B' && sourceCluster!='C'){
      sourceCluster = 'A';
  }
  if (sinkCluster!='A' && sinkCluster!='B' && sinkCluster!='C'){
      sinkCluster = 'C';
  }

  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  //Se definen los nodos de la red A que servirán de puente
  uint32_t bridgeAB = 0;
  uint32_t bridgeAC = 4;

  // Habilitación del protocolo OSLR
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);

  //--------------------------------------------------------------
  //---                 Definicion Red A                       ---
  //--------------------------------------------------------------

  // Se crea un contenedor con el numero de nodos definido
  NodeContainer cluster_A;
  cluster_A.Create (numNodesA);

  // Se definen los nodos puente
  bridgeAB_id = cluster_A.Get(bridgeAB)->GetId();
  bridgeAC_id = cluster_A.Get(bridgeAC)->GetId();


  // Se inicializan y configuran los helpers para la construccion de las interfacez de red necesarias
  WifiHelper wifiA;

  YansWifiPhyHelper wifiPhyA =  YansWifiPhyHelper::Default ();
  // Se define la ganacia tras la recepcion de paquetes
  wifiPhyA.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhyA.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannelA;
  wifiChannelA.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannelA.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhyA.SetChannel (wifiChannelA.Create ());

  // Se añade una mac y se deshabilita el rate control
  WifiMacHelper wifiMacA;
  wifiA.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifiA.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Se indica que utilizara como modo AdHoc
  wifiMacA.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices_A = wifiA.Install (wifiPhyA, wifiMacA, cluster_A);

  // Se especifica el posicionamiento y el patrón de mobilidad que seguirá la red en la simulación
  MobilityHelper mobilityA;
  mobilityA.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (originA_x),
                                 "MinY", DoubleValue (originA_y),
                                 "DeltaX", DoubleValue (distanceA),
                                 "DeltaY", DoubleValue (distanceA),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobilityA.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (originA_x-10,originA_x+distanceA*5+10,originA_y-10,originA_y+distanceA*numNodesA/5+10)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                                "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.7]"));

  mobilityA.Install (cluster_A);

  // Se establece el protocolo OLSR como el protocolo de enrutamiento de la red
  InternetStackHelper internetA;
  internetA.SetRoutingHelper (list);
  internetA.Install (cluster_A);

  // Se asigna una dirección IP de red y se asigna una direccion a cada uno de los dispositivos
  Ipv4AddressHelper ipv4Addrs_A;
  NS_LOG_INFO ("Assign IP Addresses for A.");
  ipv4Addrs_A.SetBase ("192.168.0.0", "255.255.255.0");
  ipv4Interface_Cluster_A = ipv4Addrs_A.Assign (devices_A);

  //--------------------------------------------------------------
  //---                 Definicion Red B                       ---
  //--------------------------------------------------------------

  //Se crea un contenedor temporal con el número de nodos definido
  NodeContainer cluster_B;
  //Esta vez se crean n-1 nodos porque uno de los nodos será el de unión con la red principal
  cluster_B.Create (numNodesB-1);

  // Se establece el protocolo OLSR como el protocolo de enrutamiento de la red
  InternetStackHelper internetB;
  internetB.SetRoutingHelper (list);
  internetB.Install (cluster_B);

  // Se especifica el posicionamiento y el patrón de mobilidad que seguirá la red en la simulación
  MobilityHelper mobilityB;
  mobilityB.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (originB_x),
                                 "MinY", DoubleValue (originB_y),
                                 "DeltaX", DoubleValue (distanceB),
                                 "DeltaY", DoubleValue (distanceB),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobilityB.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (originB_x-10,originB_x+distanceB*5+10,originB_y-10,originB_y+distanceB*numNodesB/5+10)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                                "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.7]"));

  mobilityB.Install (cluster_B);

  //Se agrega el nodo puente
  cluster_B.Add(cluster_A.Get(bridgeAB));

  // Se inicializan y configuran los helpers para la construccion de las interfacez de red necesarias
  WifiHelper wifiB;

  YansWifiPhyHelper wifiPhyB =  YansWifiPhyHelper::Default ();
  // Se define la ganacia tras la recepcion de paquetes
  wifiPhyB.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhyB.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannelB;
  wifiChannelB.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannelB.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhyB.SetChannel (wifiChannelB.Create ());

  // Se añade una mac y se deshabilita el rate control
  WifiMacHelper wifiMacB;
  wifiB.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifiB.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Se indica que utilizara como modo AdHoc
  wifiMacB.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices_B = wifiB.Install (wifiPhyB, wifiMacB, cluster_B);

  Ipv4AddressHelper ipv4Addrs_B;
  NS_LOG_INFO ("Assign IP Addresses for B.");
  ipv4Addrs_B.SetBase ("192.168.1.0", "255.255.255.0");
  ipv4Interface_Cluster_B = ipv4Addrs_B.Assign (devices_B);

  //--------------------------------------------------------------
  //---                 Definicion Red C                       ---
  //--------------------------------------------------------------

  //Se crea un contenedor temporal con el número de nodos definido
  NodeContainer cluster_C;
  //Esta vez se crean n-1 nodos porque uno de los nodos será el de unión con la red principal
  cluster_C.Create (numNodesC-1);

  // Se establece el protocolo OLSR como el protocolo de enrutamiento de la red
  InternetStackHelper internetC;
  internetC.SetRoutingHelper (list);
  internetC.Install (cluster_C);

  // Se especifica el posicionamiento y el patrón de mobilidad que seguirá la red en la simulación
  MobilityHelper mobilityC;
  mobilityC.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (originC_x),
                                 "MinY", DoubleValue (originC_y),
                                 "DeltaX", DoubleValue (distanceC),
                                 "DeltaY", DoubleValue (distanceC),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobilityC.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                                "Bounds", RectangleValue (Rectangle (originC_x-10,originC_x+distanceC*5+10,originC_y-10,originC_y+distanceC*numNodesC/5+10)),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=0.5]"),
                                "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.7]"));

  mobilityC.Install (cluster_C);

  //Se agrega el nodo puente
  cluster_C.Add(cluster_A.Get(bridgeAC));

  // Se inicializan y configuran los helpers para la construccion de las interfacez de red necesarias
  WifiHelper wifiC;

  YansWifiPhyHelper wifiPhyC =  YansWifiPhyHelper::Default ();
  // Se define la ganacia tras la recepcion de paquetes
  wifiPhyC.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhyC.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannelC;
  wifiChannelC.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannelC.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhyC.SetChannel (wifiChannelC.Create ());

  // Se añade una mac y se deshabilita el rate control
  WifiMacHelper wifiMacC;
  wifiC.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifiC.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  // Se indica que utilizara como modo AdHoc
  wifiMacC.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices_C = wifiC.Install (wifiPhyC, wifiMacC, cluster_C);

  Ipv4AddressHelper ipv4Addrs_Cluster_C;
  NS_LOG_INFO ("Assign IP Addresses for C.");
  ipv4Addrs_Cluster_C.SetBase ("192.168.2.0", "255.255.255.0");
  ipv4Interface_Cluster_C = ipv4Addrs_Cluster_C.Assign (devices_C);

  // Activacion de los logs para cada una de las redes Wifi
  if (verbose) {
      wifiA.EnableLogComponents ();
      wifiB.EnableLogComponents ();
      wifiC.EnableLogComponents ();
  }

  //--------------------------------------------------------------
  //---                 Comunicación entre clusters             ---
  //--------------------------------------------------------------

  //ENVIO DE MENSAJE ENTRE NODO CLUSTER B y NODO CLUSTER C

  // Se declara el ID del socket UDP y la dirección desde la que se permitirá recibir paquetes
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  InetSocketAddress anyAddress = InetSocketAddress (Ipv4Address::GetAny (), 80);

  // Se establece el nodo puente entre A y B como un nodo que escuchara a los paquetes en ambas redes
  Ptr<Socket> recvSink_bridgeAB_B = Socket::CreateSocket (cluster_B.Get (numNodesB-1), tid);
  recvSink_bridgeAB_B->Bind (anyAddress);
  recvSink_bridgeAB_B->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> recvSink_bridgeAB_A = Socket::CreateSocket (cluster_A.Get (bridgeAB), tid);
  recvSink_bridgeAB_A->Bind (anyAddress);
  recvSink_bridgeAB_A->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Se establece el nodo puente entre A y C como un nodo que escuchara a los paquetes en ambas redes
  Ptr<Socket> recvSink_bridgeAC_C = Socket::CreateSocket (cluster_C.Get (numNodesC-1), tid);
  recvSink_bridgeAC_C->Bind (anyAddress);
  recvSink_bridgeAC_C->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> recvSink_bridgeAC_A = Socket::CreateSocket (cluster_A.Get (bridgeAC), tid);
  recvSink_bridgeAC_A->Bind (anyAddress);
  recvSink_bridgeAC_A->SetRecvCallback (MakeCallback (&ReceivePacket));

  // Se declara la fuente de emision de los paquetes como un nodo de la red B
  Ptr<Socket> source = Socket::CreateSocket (cluster_B.Get (sourceNode), tid);
  InetSocketAddress remote = InetSocketAddress (ipv4Interface_Cluster_B.GetAddress (numNodesB-1, 0), 80);
  source->Connect (remote);

  // Se declara el destino de los paquetes como un nodo de la red C
  Ptr<Socket> sink = Socket::CreateSocket (cluster_C.Get(sinkNode), tid);
  sink->Bind (anyAddress);
  sink->SetRecvCallback (MakeCallback (&ReceivePacket));

  //--------------------------------------------------------------
  //---                 Configuraciones finales                ---
  //--------------------------------------------------------------

  // Se agenda la generación de tráfico para el segundo 20 de la simulación, dando asi tiempo al protocolo OLSR para actualizar las tablas de enrutamiento
  Simulator::Schedule (Seconds (20.0), &GenerateTraffic,
                       source, packetSize, numPackets, interPacketInterval);

  // Mensaje indicando la fuente y el destino de los mensajes
  NS_LOG_UNCOND ("Testing from node " << sourceNode << " of " << sourceCluster << " to " << sinkNode << " of " << sinkCluster << " with grid distance " << distanceA);


  Simulator::Stop (Seconds (simTime));

  // Generación del archivo .xml para la animación con NetAnim
  AnimationInterface anim ("wifi_Anim.xml"); // Mandatory
  anim.EnablePacketMetadata (); // Optional
  anim.EnableIpv4RouteTracking ("routingtable-wireless.xml", Seconds (0), Seconds (5), Seconds (0.25)); //Optional
  anim.EnableWifiMacCounters (Seconds (0), Seconds (10)); //Optional
  anim.EnableWifiPhyCounters (Seconds (0), Seconds (10)); //Optional

  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
}

