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
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/node-list.h"
#include "ns3/opengym-module.h"
//// NS3-GYM
#include "ns3/rng-seed-manager.h"
#include "ns3/opengym-module.h"
//// NS3-GYM

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Taller_ME");

//// NS3-GYM
/*
Define observation space
*/
Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
  uint32_t nodeNum = 5;
  float low = 0.0;
  float high = 10.0;
  std::vector<uint32_t> shape = {nodeNum,};
  std::string dtype = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND ("MyGetObservationSpace: " << space);
  return space;
}

/*
Define action space
*/
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
  uint32_t nodeNum = 5;

  Ptr<OpenGymDiscreteSpace> space = CreateObject<OpenGymDiscreteSpace> (nodeNum);
  NS_LOG_UNCOND ("MyGetActionSpace: " << space);
  return space;
}

/*
Define game over condition
*/
bool MyGetGameOver(void)
{

  bool isGameOver = false;
  bool test = false;
  static float stepCounter = 0.0;
  stepCounter += 1;
  if (stepCounter == 10 && test) {
      isGameOver = true;
  }
  NS_LOG_UNCOND ("MyGetGameOver: " << isGameOver);
  return isGameOver;
}

/*
Collect observations
*/
Ptr<OpenGymDataContainer> MyGetObservation(void)
{
  uint32_t nodeNum = 5;
  uint32_t low = 0.0;
  uint32_t high = 10.0;
  Ptr<UniformRandomVariable> rngInt = CreateObject<UniformRandomVariable> ();

  std::vector<uint32_t> shape = {nodeNum,};
  Ptr<OpenGymBoxContainer<uint32_t> > box = CreateObject<OpenGymBoxContainer<uint32_t> >(shape);

  // generate random data
  for (uint32_t i = 0; i<nodeNum; i++){
    uint32_t value = rngInt->GetInteger(low, high);
    box->AddValue(value);
  }

  NS_LOG_UNCOND ("MyGetObservation: " << box);
  return box;
}

/*
Define reward function
*/
float MyGetReward(void)
{
  static float reward = 0.0;
  reward += 1;
  return reward;
}

/*
Define extra info. Optional
*/
std::string MyGetExtraInfo(void)
{
  std::string myInfo = "testInfo";
  myInfo += "|123";
  NS_LOG_UNCOND("MyGetExtraInfo: " << myInfo);
  return myInfo;
}


/*
Execute received actions
*/
bool MyExecuteActions(Ptr<OpenGymDataContainer> action)
{
  Ptr<OpenGymDiscreteContainer> discrete = DynamicCast<OpenGymDiscreteContainer>(action);
  NS_LOG_UNCOND ("MyExecuteActions: " << action);
  return true;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGym){
  Simulator::Schedule (Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGym);
  openGym->NotifyCurrentState();
}
//// NS3-GYM


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
  double distanceA = 50;  // distancia inicial entre nodos en el cuadrante A
  double distanceB = 50;  // distancia inicial entre nodos en el cuadrante B
  double distanceC = 50;  // distancia inicial entre nodos en el cuadranteC
  packetSize = 1024; // tamaño de los paquetes en bytes
  uint32_t numPackets = 20; // numero de paquetes a enviar 
  uint32_t numNodesA = 25;  // numero de nodos cuandrante A
  uint32_t numNodesB = 25;  // numero de nodos cuandrante B
  uint32_t numNodesC = 25;  // numero de nodos cuandrante C
  double originA_x = numNodesB/5 * 50 + 10;  // coordenada x origen del cuandrante A
  double originA_y = numNodesB/5 * 50 + 10;  // coordenada y origen del cuandrante A
  double originB_x = 50.0;  // coordenada x origen del cuandrante B
  double originB_y = 50.0;  // coordenada y origen del cuandrante B
  double originC_x = (numNodesB+numNodesA)/5 * 50 + 10;  // coordenada x origen del cuandrante C
  double originC_y = 50.0;  // coordenada y origen del cuandrante C
  char sourceCluster = 'B'; // Sector de origen de los paquetes
  char sinkCluster = 'C'; // Sector de llegada de los paquetes
  interval = 1; // Intevalo entre paquetes
  bool verbose = false;
  bool tracing = false;

  //// NS3-GYM
  uint32_t simSeed = 1;
  double simulationTime = 10; //seconds
  double envStepTime = 0.1; //seconds, ns3gym env step time interval
  uint32_t openGymPort = 5555;
  uint32_t testArg = 0;
  //// NS3-GYM

  //Aplicar la semilla de la simulacion
  RngSeedManager::SetSeed (7);
  RngSeedManager::SetRun (simSeed);

  //Definicion del nodo de salida y llegada
  Ptr<UniformRandomVariable> startNodeValue = CreateObject<UniformRandomVariable> ();
  startNodeValue->SetAttribute ("Min", DoubleValue (0.0));
  startNodeValue->SetAttribute ("Max", DoubleValue (numNodesB-2));

  sourceNode = startNodeValue->GetInteger(); // numero del nodo de salida

  Ptr<UniformRandomVariable> endNodeValue = CreateObject<UniformRandomVariable> ();
  endNodeValue->SetAttribute ("Min", DoubleValue (0.0));
  endNodeValue->SetAttribute ("Max", DoubleValue (numNodesC-2));

  sinkNode = endNodeValue->GetInteger(); // numero del nodo de llegada

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
  cmd.AddValue  ("sourceNode", "Sender node number", sourceNode);
  //// NS3-GYM
  cmd.AddValue ("openGymPort", "Port number for OpenGym env. Default: 5555", openGymPort);
  cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", simSeed);
  // optional parameters
  cmd.AddValue ("simTime", "Simulation time in seconds. Default: 10s", simulationTime);
  cmd.AddValue ("testArg", "Extra simulation argument. Default: 0", testArg);
  //// NS3-GYM
  cmd.Parse (argc, argv);

  uint32_t simTime = numPackets*interval*2+1;

  //parametros por consola
  NS_LOG_UNCOND("Ns3Env parameters:");
  NS_LOG_UNCOND("--simTime: " << simTime);
  NS_LOG_UNCOND("--distanceA: " << distanceA);
  NS_LOG_UNCOND("--distanceB: " << distanceB);
  NS_LOG_UNCOND("--distanceC: " << distanceC);
  NS_LOG_UNCOND("--packetSize: " << packetSize);
  NS_LOG_UNCOND("--numPackets: " << numPackets);
  NS_LOG_UNCOND("--interval: " << interval);
  NS_LOG_UNCOND("--verbose: " << verbose);
  NS_LOG_UNCOND("--tracing: " << tracing);
  NS_LOG_UNCOND("--numNodesA: " << numNodesA);
  NS_LOG_UNCOND("--numNodesB: " << numNodesB);
  NS_LOG_UNCOND("--numNodesC: " << numNodesC);
  NS_LOG_UNCOND("--originA_x: " << originA_x);
  NS_LOG_UNCOND("--originA_y: " << originA_y);
  NS_LOG_UNCOND("--originB_x: " << originB_x);
  NS_LOG_UNCOND("--originB_y: " << originB_y);
  NS_LOG_UNCOND("--originC_x: " << originC_x);
  NS_LOG_UNCOND("--originC_y: " << originC_y);
  NS_LOG_UNCOND("--sinkNode: " << sinkNode);
  NS_LOG_UNCOND("--sourceNode: " << sourceNode);
  
  //// NS3-GYM
  NS_LOG_UNCOND("Ns3Env parameters:");
  NS_LOG_UNCOND("--simulationTime: " << simulationTime);
  NS_LOG_UNCOND("--openGymPort: " << openGymPort);
  NS_LOG_UNCOND("--envStepTime: " << envStepTime);
  NS_LOG_UNCOND("--seed: " << simSeed);
  NS_LOG_UNCOND("--testArg: " << testArg);

  RngSeedManager::SetSeed (1);
  RngSeedManager::SetRun (simSeed);

  Ptr<OpenGymInterface> openGym = CreateObject<OpenGymInterface> (openGymPort);
  openGym->SetGetActionSpaceCb( MakeCallback (&MyGetActionSpace) );
  openGym->SetGetObservationSpaceCb( MakeCallback (&MyGetObservationSpace) );
  openGym->SetGetGameOverCb( MakeCallback (&MyGetGameOver) );
  openGym->SetGetObservationCb( MakeCallback (&MyGetObservation) );
  openGym->SetGetRewardCb( MakeCallback (&MyGetReward) );
  openGym->SetGetExtraInfoCb( MakeCallback (&MyGetExtraInfo) );
  openGym->SetExecuteActionsCb( MakeCallback (&MyExecuteActions) );
  Simulator::Schedule (Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGym);
  //// NS3-GYM

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
      sourceCluster = 'B';
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

  // Se muestran las IDs globales de los nodos elegidos aleatoriamente
  NS_LOG_UNCOND("SourceNode "<<sourceNode<<" ID: "<<cluster_B.Get (sourceNode)->GetId());
  NS_LOG_UNCOND("SinkNode "<<sinkNode<<" ID: "<<cluster_C.Get (sinkNode)->GetId());

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

  openGym->NotifySimulationEnd();

  Simulator::Destroy ();


  return 0;
}
