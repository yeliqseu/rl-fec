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
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "handlepacket.h"
#include "infoqueue.h"
#include <iomanip>
#include <string>
#include <deque>
#include <math.h>
#include "rlfec-stream-server.h"

extern "C" {
	#include "streamcodec.h"
}

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RlFecStreamServerApp");

NS_OBJECT_ENSURE_REGISTERED (RlFecStreamServerApp);

TypeId
RlFecStreamServerApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RlFecStreamServerApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<RlFecStreamServerApp> ()
    .AddAttribute ("TrainingMode", 
                   "Using e-greedy to train and learn",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RlFecStreamServerApp::m_training),
                   MakeBooleanChecker ())
    .AddAttribute ("CbrMode", 
                   "Constant Bit Rate Sending Mode",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RlFecStreamServerApp::m_cbrMode),
                   MakeBooleanChecker ())
    .AddAttribute ("CbrRate", 
                   "Constant Bit Rate",
                   DataRateValue (DataRate ("10Mbps")),
                   MakeDataRateAccessor (&RlFecStreamServerApp::m_cbrRate),
                   MakeDataRateChecker ())
    .AddAttribute ("EnablePacing", 
                   "Enable Pacing",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RlFecStreamServerApp::m_pacing),
                   MakeBooleanChecker ())
    .AddAttribute ("PacingGain", 
                   "Pacing gain to probe extra bandwidth",
                   DoubleValue (1),
                   MakeDoubleAccessor (&RlFecStreamServerApp::m_pacingGain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxPackets", 
                  "The maximum number of packets the application will send",
                   UintegerValue (100000),
                   MakeUintegerAccessor (&RlFecStreamServerApp::m_numPackets),
                   MakeUintegerChecker<int32_t> ())
      .AddAttribute ("ExtraRepair", 
                   "Extra fraction of repair packets to reduce in-order decoding delay",
                   DoubleValue (0.02),
                   MakeDoubleAccessor (&RlFecStreamServerApp::m_extraRepair),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PeerAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&RlFecStreamServerApp::m_peerAddress),
                   MakeAddressChecker ())
	  .AddTraceSource ("Tx", "A new packet is created and is sent",
	                 MakeTraceSourceAccessor (&RlFecStreamServerApp::m_txTrace),
					        "ns3::Packet::TracedCallback")
	  .AddTraceSource ("Rx", "A packet has been received",
					        MakeTraceSourceAccessor (&RlFecStreamServerApp::m_rxTrace),
					        "ns3::Packet::TracedCallback")
	  .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
					        MakeTraceSourceAccessor (&RlFecStreamServerApp::m_txTraceWithAddresses),						
					        "ns3::Packet::TwoAddressTracedCallback")																	
	  .AddTraceSource ("RxWithAddresses", "A packet has been received",								
					        MakeTraceSourceAccessor (&RlFecStreamServerApp::m_rxTraceWithAddresses),									
					        "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

RlFecStreamServerApp::RlFecStreamServerApp() 
 : m_minRttFilter(10000, 0, 0)            // minRtt expires after 10s (i.e., 10000ms)
{
  srand(static_cast<uint32_t>(time(0)));
  // Construct network coding encoder and decoder
  m_cp.gfpower = 8;
  m_socket = 0;
  m_paramSendInterval = 0.01;
  m_paramAcked = false;

  m_initCWnd = 10;
  m_cWnd = m_initCWnd;
  m_pacingTimer = Timer(Timer::CANCEL_ON_DESTROY);
  m_pacingRate = DataRate ("10Mbps");
  m_pacingTimer.SetFunction (&RlFecStreamServerApp::NotifyPacingPerformed, this);
  m_prevState = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0};
  m_currState = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0, 0};
}

void
RlFecStreamServerApp::SetAttribute (Address clientAddr, int32_t numPackets, int32_t packetSize, QlearningParameter learningParameter)
{
  m_peerAddress = clientAddr;
  m_numPackets = numPackets;
  m_packetSize = packetSize;
  m_cp.pktsize = packetSize;
  // A buffer containing random bytes
  int32_t datasize = m_packetSize * m_numPackets;
  m_buf = (unsigned char *) malloc(datasize);
  generate_buf(m_buf, datasize);
  if(m_buf == NULL) {
  	std::cout << "ERROR : client generate m_buf failure" << std::endl;
  }
  int32_t pktlen = sizeof(short) + 4 * sizeof(int) + m_cp.pktsize;     // 包括streamc报头的分组长度
  std::cout << "Datagram packet size: " << pktlen << " bytes " << std::endl;

  uint8_t s[4];
  InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ().Serialize(s);
  char cs_id = int(s[2]) + '0';
  m_QlParameter = learningParameter;
  m_qTable =  new QlearningTable(2,
                                learningParameter.learning_rate,
                                learningParameter.reward_decay,
                                learningParameter.e_greedy,
                                cs_id,
                                learningParameter.ntiling,
                                learningParameter.ntiles,
                                learningParameter.learning_decay,
                                learningParameter.explore_decay, 
                                learningParameter.lambda);
}

void
RlFecStreamServerApp::SendParameter ()
{
  unsigned char *prmstr = serialize_parameter(&m_cp, m_numPackets);
  int32_t prmlen = sizeof(short)+ sizeof(int)*4 + sizeof(double);
  Ptr<Packet> prm = Create<Packet>(prmstr, prmlen);
  m_socket->Send(prm); 
  /*NS_LOG_UNCOND (
    "At time "
	  << Simulator::Now ().GetSeconds ()
	  << " (s) "
    << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	  << " server send parameters success "
  );*/

  if(prmstr != NULL) {
    free(prmstr);
	  prmstr = NULL;
  }
	
  if(!m_paramAcked) {
    Simulator::Schedule(Seconds(m_paramSendInterval), &RlFecStreamServerApp::SendParameter, this);
  }
}

int32_t 
RlFecStreamServerApp::ParamAckRecv (Ptr<Packet> prm)
{
  int32_t prmlen = sizeof(short) + 4 * sizeof(int) + sizeof(double);
  unsigned char *prmstr = (unsigned char *) calloc(prmlen, sizeof(unsigned char));
  prm->CopyData (prmstr, prmlen);
  unsigned char *prm_cp = serialize_ack_prm(&m_cp, m_numPackets);

  /*NS_LOG_UNCOND (
    "At time "
	<< Simulator::Now ().GetSeconds ()
	<< " (s) "
    << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	<< " server receive parameter ack success "
  );*/
  int judge = strcmp((const char *)prm_cp, (const char *)prmstr);

  if(prm_cp != NULL) {
    free(prm_cp);
	  prm_cp = NULL;
  }
  if(prmstr != NULL) {
    free(prmstr);
	  prmstr = NULL;
  }

  if(judge == 0) {
  	m_paramAcked = true;
    return 1;     //indicate receive parameters success
  } else {
    return 0;   //failure
  }

}

void
RlFecStreamServerApp::RttEstimation(double receive_time, double send_time)
{
  if(send_time == -1) {
    return ;
  }
  double alpha = 0.9;
  double historyRtt = m_sRtt;
  double new_rtt = receive_time - send_time;
  // std::cout << "Update new RTT: " << Seconds (new_rtt).GetMilliSeconds () << " to minRttFilter at " << Simulator::Now ().GetMicroSeconds () << std::endl;
  m_minRttFilter.Update (Seconds (new_rtt).GetMilliSeconds (), Simulator::Now ().GetMilliSeconds ());
  if(historyRtt == 0) {
    m_sRtt = new_rtt;
  } else {
    m_sRtt = alpha * historyRtt + (1 - alpha) * new_rtt;     // smoothed RTT estimation (follow standard TCP, Karn's algorithm is not needed since no retransmission is incurred)
  }
  m_iRtt = new_rtt;
  return;
}

void
RlFecStreamServerApp::BwEstimationTibet(int32_t inorder, int32_t nsource, int32_t nrepair)
{
  // TCP TIBET 
  // 1) 平滑ACK间隔
  if (m_lastAckTime == m_dataStartTime) {
    // 首个data ack，无法计算间隔，不进行估计
    m_lastAckTime = Simulator::Now ().GetSeconds ();
    return;
  }
  double sampleInterval = Simulator::Now ().GetSeconds () - m_lastAckTime;
  double alpha = 0.9;
  if (m_avgAckInterval != 0) {
    m_avgAckInterval = alpha * m_avgAckInterval + (1 - alpha) * sampleInterval;
  } else {
    m_avgAckInterval = sampleInterval;
  }
  int numAcked = nsource + nrepair - m_lastAckedSourceNum - m_lastAckedRepairNum;
  double bwe = (double) numAcked / m_avgAckInterval;
  // 2） 自适应bwe滤波
  double T0 = 0.1;
  if (m_estBw == 0) {
    m_estBw = bwe;
  } else {
    m_estBw = (1 - exp (- sampleInterval / T0)) * bwe + exp (- sampleInterval / T0) * m_estBw;
  }
  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[TIBET-ABE] sampleInterval: " << sampleInterval << " averageAckInterval: " << m_avgAckInterval << " numAcked: " << numAcked << " bwe: " << bwe << " pkt./sec." 
    << " (Averaged) Estimated-BW: " << m_estBw << endl;
  m_lastAckTime = Simulator::Now ().GetSeconds ();
  m_lastBwEstTime = Simulator::Now ().GetSeconds ();
  return;
}

void
RlFecStreamServerApp::BwEstimationJersy(int32_t inorder, int32_t nsource, int32_t nrepair)
{
  double ackInterval = Simulator::Now ().GetSeconds () - m_lastAckTime;
  if (m_lastAckTime == m_dataStartTime) {
    // first ack, skip
    m_lastAckTime = Simulator::Now ().GetSeconds ();
    return;
  }

  if (m_estBw == 0) {
    m_estBw = m_newAcked / ackInterval;
  } else {
    m_estBw = (m_sRtt * m_estBw + m_newAcked) / (ackInterval + m_sRtt);
  }

  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[Jersy-ABE] ackInterval: " << ackInterval << " numAcked: " << m_newAcked << " Estimated-BW: " << m_estBw << endl;
  m_lastAckTime = Simulator::Now ().GetSeconds ();
  m_lastBwEstTime = Simulator::Now ().GetSeconds ();
  return;
}

int32_t 
RlFecStreamServerApp::PeEstimation (int32_t nSource, int32_t nRepair)
{
  int32_t nLoss = nSource + nRepair - m_lastAckedSourceNum - m_lastAckedRepairNum;
  double alpha = 0.9;
  double new_lossRate = 1 - ((double) m_lastAckedSourceNum + m_lastAckedRepairNum) / (nSource + nRepair);
  if (m_lossRate == 0) {
    m_lossRate = new_lossRate;
  } else {
    m_lossRate = m_lossRate * alpha + new_lossRate * (1 - alpha);
  }
  return nLoss;
}

void 
RlFecStreamServerApp::UpdateCWnd (void)
{
  double cWndGain = 1.0;
  auto rtt_min=m_minRttFilter.GetBest();
  m_cWnd = (int32_t) std::floor(std::max (double(m_initCWnd), m_estBw * MilliSeconds(rtt_min).GetSeconds () * cWndGain));
  // Update pacing rate
  // DataRate pacingRate ((std::max (m_cWnd, m_packetsInFlight) * m_packetSize * 8 * gain) / m_rtt);
  /*
  if (m_estBw != 0 && m_pacing && m_cWnd > m_packetsInFlight) {
    DataRate pacingRate (m_estBw * 8 * m_packetSize * m_pacingGain);
    m_pacingRate = pacingRate;
    // NS_LOG_UNCOND ("Pacing rate updated to: " << pacingRate);
  }
  */
  return;
}

void
RlFecStreamServerApp::PacketAckRecv(Ptr<Packet> packet)
{
  int32_t inodlen=sizeof(short)+sizeof(int)*7;   // 7 is the numbers of int feedback from the client. We know it is ugly, but okay for quick development.
  unsigned char *inodstr = (unsigned char *) calloc(inodlen, sizeof(unsigned char));
  packet->CopyData (inodstr, inodlen);
  int inorder, nsource, nrepair;
  int ackPktType, ackPktId;
  int dwWidth;
  int dof;
  deserializeAckPacket(inodstr, &ackPktType, &ackPktId, &inorder, &nsource, &nrepair, &dwWidth, &dof);
  if (m_lastAckedInorderId==inorder && m_lastAckedSourceNum==nsource && m_lastAckedRepairNum==nrepair) {
    // Nothing changed, skip
    free(inodstr);
    return;
  }


  // available bandwidth estimation
  m_newAcked = nsource + nrepair - m_lastAckedRepairNum - m_lastAckedSourceNum;
  BwEstimationJersy(inorder, nsource, nrepair);
  // BwEstimationTibet(inorder, nsource, nrepair);

  m_lastAckedSourceNum = nsource;
  m_lastAckedRepairNum = nrepair;

  double recv_ack_time = double(Simulator::Now ().GetSeconds ());
  // SearchedInfo info = m_pktInfoQueue.find(pkt_info);
  PacketInfo info = m_pktInfoQueue.find(ackPktId, ackPktType);
  double send_time = info.m_time;
  int other_type_packet_num = info.m_another_packet_num;

  RttEstimation(recv_ack_time, send_time);


  // Update other information
  // double dataDuration = double(Simulator::Now ().GetSeconds ()) - m_dataStartTime;        // data transmission time so far
  // Pe estimation
  int source_count = 0,repair_count = 0;    // Count the numbers of source and repair packets that had been sent up to the time sending the ACKed packet
  int32_t previousAckedSourceId = m_lastAckedSourceId;
  std::string s;
  if(ackPktType == SOURCE_PACKET) {
    s=std::string("SOURCE");
    source_count = ackPktId + 1;      // packets are indexed from 0
    repair_count = other_type_packet_num;
    m_lastAckedSourceId = ackPktId;
  } else {
    s=std::string("REPAIR");
    repair_count = ackPktId + 1;
    source_count = other_type_packet_num;
    m_lastAckedRepairId = ackPktId;
  }

  int32_t nTotalLoss = PeEstimation (source_count, repair_count);
  
  m_packetsInFlight = m_lastSentSourceId - m_lastAckedSourceId + m_lastSentRepairId - m_lastAckedRepairId;

  UpdateCWnd();

  // HERE IS FOR PERFORMANCE COMPARISON WITH GARRIDO-TNET2019'S THRESHOLD-BASED SCHEME
  // Obtain the numbers of the source and repair packets sent in the previous 1/2 RTT time (Garrido TNET2019)
  double st = recv_ack_time - (recv_ack_time - send_time) / 2;
  int Sn = 0, Cn = 0;
  Sn = m_pktInfoQueue.num_source_sent_since (st);
  Cn = m_pktInfoQueue.num_repair_sent_since (st);
  // The virtual queue's length 1/2 RTT ago is obtained from the feedback as decoding window length (i.e., # of source pakcets covered)
  // subtracted by the number of DoF's that have been collected (including repair and out-of-order source packets).
  m_vqlength_est = (dwWidth - dof) + Sn * m_lossRate - Cn * (1 - m_lossRate);
  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[GarridoTNet2019] dwWidth: " << dwWidth << " dof: " << dof
    << " # of source sent since " << st << " : " << Sn
    << " # of repair sent since " << st << " : " << Cn
    << " new-estimated-vq-length: " << m_vqlength_est
    << std::endl;

  // Update states
  double prop_time = MilliSeconds(m_minRttFilter.GetBest ()).GetSeconds () / 2;
  m_currState.estBw = m_estBw;
  m_currState.throughput = (nsource + nrepair) / (recv_ack_time - prop_time);
  if (inorder >= 0) {
    m_currState.goodput = (inorder + 1) / (recv_ack_time - prop_time);
  }
  m_currState.lossRate = m_lossRate;
  int inflight_source = m_lastSentSourceId - m_lastAckedSourceId;
  int inflight_repair = m_lastSentRepairId - m_lastAckedRepairId;
  m_currState.fecRate =  ((double) inflight_source) / (inflight_source + inflight_repair);   // fecRate in-flight
  m_currState.propDelay = m_iRtt;
  m_currState.dwWidth = dwWidth;
  m_currState.dof = dof;
  
  PacketInfo info2 = m_pktInfoQueue.find(inorder, SOURCE_PACKET);
  if (info2.m_id != -1)
    {
      m_currState.avgDliDelay = recv_ack_time - info2.m_time - prop_time;
      m_currState.stateTime = recv_ack_time;
    }
  // Update the average delivery delay for the delivered packets (not used for now)
  /*
  if (previousAckedSourceId >= 0 && m_lastAckedSourceId >= 0)
    {
      double sumDelay = m_currState.avgDliDelay * (previousAckedSourceId + 1);    // 之前的总有序递送时延
      for (int i=m_lastAckedInorderId+1; i<=previousAckedSourceId; i++)
        {
          // 曾经已ACK过的源分组每个的递送时延加上自上次ACK以来消逝的时间
          sumDelay += recv_ack_time - m_lastAckTime;
        }
      for (int i=previousAckedSourceId+1; i<=m_lastAckedSourceId; i++)
        {
          // 新ACK的源分组的delay则计算当前与发送的时间差
          PacketInfo info2 = m_pktInfoQueue.find(i, SOURCE_PACKET);
          double ddelay = recv_ack_time - info2.m_time - prop_time;   // use minRtt/2 to approximate the one-way propagation delay
          sumDelay += ddelay;
        }
      m_currState.avgDliDelay = sumDelay / (m_lastAckedSourceId + 1);
      m_currState.stateTime = recv_ack_time;    // 刷新当前状态的更新时间
    }
  */
  
  if (inorder != m_lastAckedInorderId)
    {
      m_currState.utility = (inorder - m_lastAckedInorderId) / (recv_ack_time - m_lastNewInorderTime);   // utility variable stores the goodput of the latest busy period of decoder
      std::cout << "Inorder updated from " << m_lastAckedInorderId << " at " << m_lastNewInorderTime 
                << " to " << inorder << " at " << recv_ack_time 
                << " instant-gpt: " << m_currState.utility
                << std::endl;  
      m_lastAckedInorderId = inorder;
      m_lastNewInorderTime = recv_ack_time;
    }
  else if (dwWidth == 0)
    {
      m_currState.utility = -1; //no progress of inorder while decoder is inactive, showing waste of sending a repair packet
    }
  m_lastAckTime = recv_ack_time;
  m_hasNewAck = true;

  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[ACK] inorder: "
	  << m_lastAckedInorderId 
    << " latest received "
    << s
    << " packet: "
    << ackPktId
    << " total-received [nSource nRepair] = [ "
    << m_lastAckedSourceNum
    << " "
    << m_lastAckedRepairNum
    << " ]. "
    << std::endl;
  
  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[StatusUpdate] lastSentSourceId: "
    << m_lastSentSourceId
    << " lastSentRepairId: "
    << m_lastSentRepairId
    << " lastAckedSourceId: "
    << m_lastAckedSourceId
    << " lastAckedRepairId: "
    << m_lastAckedRepairId
    << " ACKed [inorder nsource nrepair] = [ "
    << inorder << " " << nsource << " " << nrepair << " ]"
    << " DW-Width: " << dwWidth
    << " DoF: " << dof
    << std::endl;

  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[Estimation] BandWidth: " << m_estBw << " (pkts./sec.) smoothed-RTT : " << m_sRtt * 1000 
    << " (ms) LossRate: " << m_lossRate << " Current cWnd: " << m_cWnd << " (pkts.)" 
    << " packetsInFlight: " << m_packetsInFlight
    << " totalLoss: " << nTotalLoss
    << " filteredRttMin: " << m_minRttFilter.GetBest () << " (ms) "
    << " instant-RTT: " << m_iRtt * 1000 << " (ms) "
    << std::endl;


  if (inorder >= 0 && inorder < m_numPackets - 1) {
  	flush_acked_packets(m_enc, inorder);
  }
  if (inorder >= m_numPackets - 1) {
    Simulator::ScheduleNow(&RlFecStreamServerApp::CompleteSend, this);
	  StopApplication();
  }	

  // prevState is the state when sending the current ACKed packet
  // find it in m_stateQueue
  for (auto i = m_stateQueue.begin(); i < m_stateQueue.end(); i++) 
    {
      if ((*i).packetType == ackPktType && (*i).packetId == ackPktId)
        {
          m_prevState = *i;
          std::cout << "Found previous state when sending packet " << ackPktId << " of type " << ackPktType << std::endl;
          break;
        }
    }

  free(inodstr);
  // Continue to send data packets if there is data and cWnd allows
  // if pacing, sending is controlled by pacingTimer
  if (!m_cbrMode && m_cWnd > m_packetsInFlight) {
    Simulator::ScheduleNow(&RlFecStreamServerApp::SendDataPackets, this);
  }
}

void
RlFecStreamServerApp::NotifyPacingPerformed (void)
{
  SendDataPackets ();
}

void 
RlFecStreamServerApp::SendDataPackets (void)
{
  double curr_time = double(Simulator::Now ().GetSeconds ());
  if (!m_cbrMode)
    {
      // sending according to the CWND and the pacing timer (if enabled)
      while (m_cWnd > m_packetsInFlight) 
        {
          // If pacing turned on, check pacing timer
          if (m_pacing && m_cWnd != m_initCWnd && m_pacingTimer.IsRunning ()) break;

          uint32_t pktsize = DoSendDataPackets ();
          m_packetsInFlight++;
          // Set next sending time based on the pacing rate
          if (m_pacing) 
            {
              if (m_pacingTimer.IsExpired ()) 
                {
                  // NS_LOG_UNCOND ("Current Pacing Rate " << m_pacingRate);
                  // NS_LOG_UNCOND ("Timer is in expired state, activate it " << m_pacingRate.CalculateBytesTxTime (pkt->GetSize ()));
                  m_pacingTimer.Schedule (m_pacingRate.CalculateBytesTxTime (pktsize));
                  break;
                }
            }
        }
    }
  else
    {
      // CBR mode, schedule packet sending at fixed interval
      uint32_t pktsize = DoSendDataPackets ();
      m_packetsInFlight++;
      Simulator::Schedule (m_cbrRate.CalculateBytesTxTime (pktsize), &RlFecStreamServerApp::SendDataPackets, this);
    }
  return;
}

uint32_t
RlFecStreamServerApp::DoSendDataPackets (void)
{
  struct packet *cpkt = NULL;
  uint32_t pktsize = 0;
  if (m_lastSentSourceId == -1) 
    {
      cpkt = output_source_packet (m_enc);
    } 
  else if (m_enc->nextsid == m_enc->snum) 
    {
      // all source packets have been sent once, fallback the action to sending repair packets
      cpkt = output_repair_packet (m_enc);
    } 
  else 
    {
      bool send_repair;
      if (m_garrido)
        {
          send_repair = TimeToSendRepairPacketGarridoTNet2019 (1.0);
        }
      else
        {
          send_repair = TimeToSendRepairPacket();
        }
      if (send_repair) 
        {
          cpkt = output_repair_packet (m_enc);
        } 
      else 
        {
          cpkt = output_source_packet (m_enc);
        }
    }
  // serialize streaming coded packet
  int pktlen = sizeof(short) + 4 * sizeof(int) + m_cp.pktsize;
  if(cpkt != NULL) 
    {
      unsigned char *str = serialize_packet(m_enc, cpkt);
      unsigned char *pktstr = add_packet_type(m_enc, str);
      
      Ptr<Packet> pkt = Create<Packet>(pktstr, pktlen);
      pktsize = pkt->GetSize ();
      m_socket->Send(pkt);

      double send_time = double(Simulator::Now ().GetSeconds ());
      // bookkeeping send time and other state variables, push to stateQueue
      if (cpkt->sourceid != -1) 
        {
          m_pktInfoQueue.add(SOURCE_PACKET, cpkt->sourceid, send_time, m_enc->rcount, 0);
          m_lastSentSourceId = cpkt->sourceid;
          std::cout 
            << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " << send_time << " (s) ]"
            << "[SendPacket] server sends SOURCE packet " << cpkt->sourceid << std::endl;
          int inflight_source = m_lastSentSourceId - m_lastAckedSourceId;
          int inflight_repair = m_lastSentRepairId - m_lastAckedRepairId;
          m_currState.fecRate =  ((double) inflight_source) / (inflight_source + inflight_repair);   // fecRate in-flight
          m_currState.packetType = SOURCE_PACKET;
          m_currState.packetId = cpkt->sourceid;
          m_currState.actionId = 0;
          m_stateQueue.push_back(m_currState);
        } 
      else 
        {
          m_pktInfoQueue.add(REPAIR_PACKET, cpkt->repairid, send_time, m_enc->nextsid, 0);
          m_lastSentRepairId = cpkt->repairid;
          std::cout 
            << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " << send_time << " (s) ]"
            << "[SendPacket] server sends REPAIR packet " << cpkt->repairid << std::endl;
          int inflight_source = m_lastSentSourceId - m_lastAckedSourceId;
          int inflight_repair = m_lastSentRepairId - m_lastAckedRepairId;
          m_currState.fecRate =  ((double) inflight_source) / (inflight_source + inflight_repair);   // fecRate in-flight
          m_currState.packetType = REPAIR_PACKET;
          m_currState.packetId = cpkt->repairid;
          m_currState.actionId = 1;
          m_stateQueue.push_back(m_currState);
        }

      //free space
      free_packet(cpkt);
      if(pktstr != NULL) 
        {
          free(pktstr);
          pktstr = NULL;
        }
    }
  return pktsize;
}

bool 
RlFecStreamServerApp::TimeToSendRepairPacketGarridoTNet2019 (double gamma)
{
  if (!m_seenAck)
    {
      // augmented: blindly use a 9/10 FEC rate when no ACK is seen at the beginning of transmission 
      int newAction = ((m_lastSentSourceId+m_lastSentRepairId+2 + 1) % 10 == 0) ? 1 : 0;
      return newAction==0 ? false : true;
    }
  if (m_vqlength_est > gamma) {
    return true;
  } else {
    return false;
  }
}

// Currently is a function placeholder
bool
RlFecStreamServerApp::TimeToSendRepairPacket (void)
{
  int newAction;
  if (!m_training && !m_seenAck)
    {
      // augmented: blindly use a 9/10 FEC rate when no ACK is seen at the beginning of transmission 
      newAction = ((m_lastSentSourceId+m_lastSentRepairId+2 + 1) % 10 == 0) ? 1 : 0;
      return newAction==0 ? false : true;
    }

  double currStateVec[NDIM];
  m_currState.ToVector (currStateVec);
  m_currState.PrintState(currStateVec);
  if (!m_hasNewAck)
    {
      if (m_training)
        {
          newAction = m_qTable->choose_action(currStateVec);  // choose e-greedy action when training
        }
      else
        {
          newAction = m_qTable->choose_best(currStateVec);    // choose best action in testing
        }
      return newAction==0 ? false : true; 
    }

  double reward = GetReward (m_prevState, m_currState);
  m_episodeReward += reward;

  double prevStateVec[NDIM];
  m_prevState.ToVector (prevStateVec);
  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[RL-state] prev-lossRate: " << m_prevState.lossRate << " prev-fecRate: " << m_prevState.fecRate << " prev-dwWidth: " << m_prevState.dwWidth << " prev-Action: " << m_prevState.actionId
    << " new-lossRate: " << m_currState.lossRate << " new-fecRate: " << m_currState.fecRate << " new-dwWidth: " << m_currState.dwWidth
    << " reward: " << reward
    << std::endl;
  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[RL-status] old-Goodput: " << m_prevState.goodput << " (pkts./sec.) old-avgDliDelay: " << m_prevState.avgDliDelay * 1000 << " (ms) old-utility: " << m_prevState.utility
    << " new-Goodput: " << m_currState.goodput << " (pkts./sec.) new-avgDliDelay: " << m_currState.avgDliDelay * 1000 << " (ms) new-utility: " << m_currState.utility 
    << " reward: " << reward
    << std::endl;

  if (m_training)
    {
      // Sarsa(lambda) (Sutton2018 pp.305)
      // TDerror = Reward + gamma * new_state_action_value - old_state_action_value
      // (Step 1) First calcuate the part: reward - old_state_action_value
      double TDerror = m_qTable->update_sarsa_lambda_before(prevStateVec, m_prevState.actionId, reward);   

      //new action from the current state
      newAction = m_qTable->choose_action(currStateVec);  //new action based on current state

      // Approach 2: Q-learning (Sutton2018 pp.131) [not used for now]
      /* 
      m_qTable->update(prevStateVec, m_prevState.actionId, reward, currStateVec);
      int newAction = m_qTable->choose_action(currStateVec);
      */

      if (m_currState.dwWidth == 0)
        {
          // 若译码窗口为0，则认为是terminal状态, 相应地更新权重并重置eligibility trace
          m_qTable->update_sarsa_lambda_terminal(TDerror);
          std::cout
            << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
            << "[RL-status2] episode-accumulated reward: " << m_episodeReward
            << std::endl;
          m_episodeReward = 0.0;
        }
      else
        {
          // (Step 2) Calculate the complete TDerror and update weights and trace (if lambda != 0)
          //          TDerror = TDerror + gamma * new_state_action_value
          m_qTable->update_sarsa_lambda_after(currStateVec, newAction, TDerror);
        }
      
      //m_qTable->check_qtable();
    }
  else
    {
      newAction = m_qTable->choose_best(currStateVec);
      if (m_currState.dwWidth == 0)
        {
          std::cout
            << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
            << "[RL-status2] episode-accumulated reward: " << m_episodeReward
            << std::endl;
          m_episodeReward = 0.0;
        }
    }
  
  m_hasNewAck = false;
  return newAction == 0 ? false : true;
}

void 
RlFecStreamServerApp::CompleteSend (void)
{
  short type = COMPLETE;
  int pktlen = sizeof(short);
  unsigned char *pktstr = (unsigned char *)calloc(pktlen, sizeof(unsigned char));
  memcpy(pktstr, &type, sizeof(short));
  Ptr<Packet> stop_pkt = Create<Packet>(pktstr, pktlen);
  m_socket->Send(stop_pkt); 
  if (m_training) m_qTable->save_table();
  Simulator::Stop ();
}

void
RlFecStreamServerApp::StartApplication (void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              //NS_FATAL_ERROR ("Failed to bind socket");
			  std::cout << "Failed to bind socket" << std::endl; 
            }
		  if (m_socket->Connect (m_peerAddress) != 0)
		    {
			  std::cout << "server Failed to connect " << std::endl;
			}

        }
      else
        {
          //NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
		      std::cout << "Incompatible address type: " << m_peerAddress << std::endl;
        }

    }
  
	
  int datasize = m_cp.pktsize * m_numPackets;
  m_enc = initialize_encoder(&m_cp, m_buf, datasize);
  m_socket->SetRecvCallback (MakeCallback (&RlFecStreamServerApp::HandleRead, this));
  Simulator::ScheduleNow(&RlFecStreamServerApp::SendParameter, this);
}
  
void 
RlFecStreamServerApp::StopApplication (void)
{

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_pacing) {
    m_pacingTimer.Cancel ();
  }
}

void
RlFecStreamServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
      if (InetSocketAddress::IsMatchingType (from))
        {
          /*NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());*/
        }
     
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      // Check ACK type
      int type_len=sizeof(short);
      unsigned char *type_str = (unsigned char *) calloc(type_len, sizeof(unsigned char));
      packet->CopyData(type_str, type_len);
      short type;
      memcpy(&type, type_str, sizeof(short));
      // 检查消息类型
      switch (type) {
        case ACK_PRM : {
          // Ack of the FEC parameter handshake
          if(!m_paramAcked && ParamAckRecv(packet)) {
            NS_LOG_UNCOND (
              "At time "
	          << Simulator::Now ().GetSeconds ()
			      << " (s) "
      		  << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	          << " server receive parameters ack correctly "
            );
            m_dataStartTime = Simulator::Now ().GetSeconds ();      // mark actual data start time
            m_lastAckTime = m_dataStartTime;
			      /// Start data transmission
            Simulator::ScheduleNow(&RlFecStreamServerApp::SendDataPackets, this);
          } else if(!m_paramAcked) {
            Simulator::ScheduleNow(&RlFecStreamServerApp::SendParameter, this);  
          }
        }
          break;
        case INORDER :
          // Data packet ack
          m_seenAck = true;
		      PacketAckRecv(packet);
          break;
        default : 
          NS_LOG_UNCOND (
            "At time "
	        << Simulator::Now ().GetSeconds ()
			    << " (s) "
			    << m_peerAddress 
	        << " server receive packet type error "
	      );
      }
  }
}

void 
RlFecStreamServerApp::UseGarridoVQBased (void)
{
  m_garrido = true;
  return;
}

} // namespace ns3