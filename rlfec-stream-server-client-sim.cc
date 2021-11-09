#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/config-store-module.h"

extern "C" {
	#include "stream-client.h"
  	#include "rlfec-stream-server.h"   // libstreamc is written in C
}
  

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RlFecStreamClientServerExample");

void
BytesInQueueTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newVal << std::endl;
}

//QueueDiscDropTracer (Ptr<OutputStreamWrapper>stream, Ptr<const QueueDiscItem> item)
static void
QueueDiscDropTracer (Ptr<OutputStreamWrapper>stream, std::string context, Ptr<const QueueDiscItem> item)
{
  *stream->GetStream () 
    << Simulator::Now ().GetSeconds () 
    << " drop from queue "
    << item->GetTxQueueIndex() 
    << " protocol: "
    << item->GetProtocol()
    << " address: "
    << item->GetAddress()
    << " with "
    << item->GetPacket()->GetSize()
    << " bytes"
    << context
    << std::endl;
}

static void
PhyTxDrop (std::string context, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("PhyTxDrop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
PhyRxDrop (std::string context, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("PhyRxDrop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

double pe_change_interval = 1.0;

static void
change_lossrate (void)
{
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (0.0));
  x->SetAttribute ("Max", DoubleValue (10.0));
  double newPe = ((double) x->GetInteger(0, 10)) / 100.0;

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (newPe));
  em->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/ReceiveErrorModel",PointerValue (em));
  Simulator::Schedule(Seconds(pe_change_interval), &change_lossrate);
  std::cout << "change link loss rate to " << newPe << std::endl;
}

int
main (int argc, char *argv[])
{
  srand(0); 

  int       nPackets = 1000;
  uint32_t  nLeaf = 1;
  double    repFreq  = 0;           // not used
  double    lossRate = 0.0;
  double    ackLossRate = 0.0;
  double    learning_rate = 0.1; 
  double    reward_decay = 0.95; 
  double    e_greedy = 0.05;
  int       ntiling = 8;
  int       ntiles = 10000;
  double    learning_decay = 1;     // not used
  double    explore_decay = 1;      // not used
  double    lambda = 0.0;           // keep 0.0
  int       appPktSize = 1440;      // in bytes
  bool      cbrMode = false;
  DataRate  cbrRate("10Mbps");      // if use constant bit rate, set this rate
  bool      training = false;
  bool      dynamicPe = false;
  
  bool      garrido = false;  // Simulate Garrido et al's TNET 2019 scheme

  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of source packets", nPackets);
  cmd.AddValue ("nLeaf", "Number of leaves", nLeaf);
  cmd.AddValue ("repFreq", "Frequency of sending repair packets", repFreq);
  cmd.AddValue ("lossRate", "Link packet loss rate", lossRate);
  cmd.AddValue ("ackLossRate", "Link packet loss rate", ackLossRate);
  cmd.AddValue ("egreedy", "e-greedy's probabiltiy of randomly choosing an action", e_greedy);
  cmd.AddValue ("lambda", "lambda for eligibility trace, double in [0, 1]", lambda);
  cmd.AddValue ("alpha", "alpha for learning rate, default is 0.1", learning_rate);
  cmd.AddValue ("gamma", "gamma for reward decay, default is 0.95", reward_decay);
  cmd.AddValue ("cbrMode", "set constant bit rate sending", cbrMode);
  cmd.AddValue ("cbrRate", "constant bit rate sending", cbrRate);
  cmd.AddValue ("training", "Whether in training mode to use e-greedy, default is false", training);
  cmd.AddValue ("dynamicPe", "Dynamically changing loss rate, taking a random value in [0,0.01,...,0.09,0.1] every 1 second, default is false", dynamicPe);
  cmd.AddValue ("garrido", "determine repair packet sending based on Garrido et al's virtual queue est. (TNET2019), default is false", garrido);

  cmd.Parse (argc, argv);

  if (cbrMode)
    {
      Config::SetDefault ("ns3::RlFecStreamServerApp::CbrMode", BooleanValue (cbrMode));
      Config::SetDefault ("ns3::RlFecStreamServerApp::CbrRate", DataRateValue (cbrRate));
    }
  Config::SetDefault ("ns3::RlFecStreamServerApp::TrainingMode", BooleanValue (training));

  // Jitter start
  double min=0.0;
  double max=10.0;
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (min));
  x->SetAttribute ("Max", DoubleValue (max));
  if (training) {
    lossRate = ((double) x->GetInteger(0, 10)) / 100.0;     // random pick pe when training, otherwise take value from default or --lossRate
  }

  QlearningParameter learningParameter = {learning_rate,
                                         reward_decay,
                                         e_greedy,
                                         ntiling,
                                         ntiles,
                                         learning_decay,
                                         explore_decay,
                                         lambda 
                                        };


//
// Explicitly create the nodes required by the topology (shown above).
//

  NS_LOG_UNCOND ("Create channels.");
  NS_LOG_UNCOND ("nPackets : "
        << nPackets
        << "  lossRate : "
  		  << lossRate
		);

//
// Explicitly create the channels required by the topology (shown above).
//


  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("250ms"));


  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("40Mbps"));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue ("25ms"));

  uint32_t nLeftLeaf = nLeaf;
  uint32_t nRightLeaf = nLeaf; 

  PointToPointDumbbellHelper d (nLeftLeaf,pointToPointLeaf,
  								              nRightLeaf,pointToPointLeaf,
								                pointToPointRouter);

  // Packet loss on the forward link of the bottleneck
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (lossRate));
  em->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  d.GetRight()->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Packet loss on the reverse link (ACK) of the bottleneck
  Ptr<RateErrorModel> ackEm = CreateObject<RateErrorModel> ();
  ackEm->SetAttribute ("ErrorRate", DoubleValue (ackLossRate));
  ackEm->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  d.GetLeft()->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (ackEm));

  InternetStackHelper stack;
  d.InstallStack (stack);


  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback (&PhyRxDrop));

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  NS_LOG_UNCOND ("Assign IP Addresses.");


  uint16_t clientPort = 8080;

  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
  					     Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
					     Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // connect to qdisc trace after assigning address [Model Library：Traffic Control Layer]
  std::ostringstream oss;
  oss << "/NodeList/"
      << d.GetLeft()->GetId ()
      << "/$ns3::TrafficControlLayer/RootQueueDiscList/0/Drop";
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> streamDropQueueDisc = ascii.CreateFileStream ("FqCoDel-Drop.txt");
  Config::Connect(oss.str(), MakeBoundCallback (&QueueDiscDropTracer, streamDropQueueDisc));


  NS_LOG_UNCOND ("Create Applications.");
//
// Create one udpServer applications on node one.
//


  ApplicationContainer serverApps;
  ApplicationContainer clientApps;
 

  NodeContainer nodes;

  for(uint32_t i = 0; i < d.LeftCount (); ++i)
  	{
  	  Ptr<Application> app;
  	  Ptr<StreamClientApp> client = CreateObject<StreamClientApp> ();
	    Address clientAddress = InetSocketAddress (d.GetRightIpv4Address (i), clientPort);
	    client->Setup(clientAddress, Seconds(0));
	    d.GetRight (i)->AddApplication (client);
      //client->SetStartTime (Seconds (delay_time));
	    app = client;
	    clientApps.Add (app);

  	  Ptr<RlFecStreamServerApp> server = CreateObject<RlFecStreamServerApp> ();
  	  server->SetAttribute(clientAddress, nPackets, appPktSize, learningParameter);
      if (garrido) 
        {
          server->UseGarridoVQBased ();
        }
	    d.GetLeft (i)->AddApplication (server);
      //server->SetStartTime (Seconds (delay_time));

      nodes.Add(d.GetRight (i));
      nodes.Add(d.GetLeft (i));
	    app = server;
	    serverApps.Add (app);
    } 
	
  serverApps.Start (Seconds (0.0));
  clientApps.Start (Seconds (0.0));
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.Install(nodes);

  if (dynamicPe)
    {
      Simulator::Schedule(Seconds(pe_change_interval), &change_lossrate);       // schedule pe change if simulate dynamic environment
    }
	
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Now, do the actual simulation.
  NS_LOG_UNCOND ("Run Simulation.");
  Simulator::Stop (Seconds(4000.0));

  // Output default attributes for information
  /*
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("rl-fec-output-attributes.xml"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();
  */
  Simulator::Run ();
  //Simulator::Destroy ();
  NS_LOG_UNCOND ("Done.");

}
