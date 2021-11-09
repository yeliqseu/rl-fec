#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/timer.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "infoqueue.h"
#include "tiles.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ptr.h"
#include "ns3/double.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/data-rate.h"
#include "ns3/windowed-filter.h"
#include "qlearning.h"

extern  "C" {
	#include "streamcodec.h"
}

namespace ns3 {

class Socket;
class Packet;

struct QlearningParameter
{
  double learning_rate;
  double reward_decay;
  double e_greedy;
  int ntiling;
  int ntiles;
  double learning_decay;
  double explore_decay;
  double lambda;
};

struct State
{

  double stateTime;
  double estBw;
  double throughput;
  double goodput;
  double propDelay;
  double avgDliDelay;
  double lossRate;
  double fecRate;
  double utility;
  int    dwWidth;
  int    dof;

  // For finding the history state when sending the corresponding packet
  int    packetType;
  int    packetId;
  int    actionId;

  void ToVector(double vec[])
  {
    double expected_dww = 1 / (1 - fecRate - lossRate);
    if (expected_dww < 0 || expected_dww >= 100)
      {
        vec[0] = 100;
      }
    else
      {
        vec[0] = expected_dww;
      }
    vec[1] = dwWidth;
  }

  void PrintState (double vec[])
  {
    std::cout<< "State vector: [ " << vec[0] << " " 
                                   << vec[1] << " " 
                                   << " ]" << std::endl; 
  }

  void ComputeUtility (double delta)
  {
    if (goodput == 0) {
      utility = 0;
    } else {
      utility = log(goodput);
    }
  }
};

class RlFecStreamServerApp: public Application
{
public:
  RlFecStreamServerApp();

  void SetAttribute (Address clientAddr, int32_t numPackets, int32_t packetSize, QlearningParameter learningParameter);

  static TypeId GetTypeId (void);

  void UseGarridoVQBased (void);
  
private:
  virtual void StartApplication (void);

  virtual void StopApplication (void);
  
private:
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  int32_t m_packetSize;
  int32_t m_numPackets;
  unsigned char *m_buf;
  struct parameters m_cp;
  struct encoder *m_enc;
  double m_extraRepair;         // extra repair rate added to pe (for testing static FEC)
  double m_paramSendInterval;   // parameter handshake repeat interval
  bool m_paramAcked = false;    // whether client ACKed FEC parameter
  double m_dataStartTime;       // data transmission start time

  InfoQueue m_pktInfoQueue;     // bookkeeping packet sending time and other information

  int32_t m_lastAckedSourceId = -1;
  int32_t m_lastAckedRepairId = -1;
  int32_t m_lastAckedInorderId = -1;
  int32_t m_lastAckedSourceNum = 0;
  int32_t m_lastAckedRepairNum = 0;
  int32_t m_lastSentSourceId = -1;
  int32_t m_lastSentRepairId = -1;

  int32_t m_packetsInFlight = 0;
  double m_lastAckTime = 0.0;           // time of receiving last ACK (in sec.)

  int32_t m_initCWnd;
  int32_t m_cWnd;
  // rtt estimation
  typedef WindowedFilter<int64_t, MinFilter<int64_t>, int64_t, int64_t> RTTFilter;  // in milliseconds
  RTTFilter m_minRttFilter;
  double m_iRtt = 0.0;    // instant RTT
  double m_sRtt = 0.0;    // smoothed RTT

  // bandwidth estimation
  int32_t m_newAcked = 0;
  int32_t m_bwWindowLength;
  double m_estBw = 0.0;
  double m_lastBwEstTime = 0.0;
  double m_avgAckInterval = 0.0;    // TIBET

  // loss rate estimation
  double m_lossRate = 0.0;

  // pacing
  bool m_pacing;
  double m_pacingGain;
  DataRate m_pacingRate;
  Timer m_pacingTimer {Timer::CANCEL_ON_DESTROY}; //!< Pacing Event

  // constant-rate sending mode
  bool m_cbrMode;
  DataRate m_cbrRate;

  // Reinforcement Learning
  bool                m_training = false;
  QlearningParameter  m_QlParameter;
  QlearningTable      *m_qTable;
  int                 m_prevAction;         // previous action
  State               m_prevState;
  State               m_currState;
  uint32_t            m_nActionSrc = 0;
  uint32_t            m_nActionRep = 0;
  bool                m_seenAck = false;
  bool                m_hasNewAck = false;
  double              m_lastNewInorderTime = 0.0;
  double              m_episodeReward = 0.0;

  deque<State>        m_stateQueue;         // state queue for RL-FEC

  // Garrido TNET 2019
  // Note: RL-FEC considers a "heavy traffic" scenario where the node always has source packets
  //       to send, therefore S_k = 1 - C_k in (6) of Garrido-TNET2019
  bool    m_garrido = false;
  double  m_vqlength_est = 0.0;
  
  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;
  
  /// Callbacks for tracing the packet Rx events
  TracedCallback<Ptr<const Packet> > m_rxTrace;
  
  /// Callbacks for tracing the packet Tx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_txTraceWithAddresses;
  
  /// Callbacks for tracing the packet Rx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;
  
private:
  int32_t PacketsInFlight (void);

  uint32_t SarsaAgentAct (void);

  uint32_t QlearningAgentAct (void);

  bool TimeToSendRepairPacket (void);

  bool TimeToSendRepairPacketGarridoTNet2019 (double gamma);

  void RttEstimation (double recvTime, double sendTime);

  void BwEstimationTibet(int32_t inorder, int32_t nsource, int32_t nrepair);

  void BwEstimationJersy(int32_t inorder, int32_t nsource, int32_t nrepair);

  int32_t PeEstimation (int32_t nSource, int32_t nRepair);

  void UpdateCWnd (void);

  void NotifyPacingPerformed (void);

  void SendDataPackets (void);

  uint32_t DoSendDataPackets (void);

  double GetReward(State old_state, State new_state)
    {
      double reward = 0;
      
      if (new_state.dwWidth == 0)
        {
          if (new_state.utility == -1)
            {
              reward = 50 * new_state.utility;
            }
          else
            {
              reward = 0.05 * new_state.utility;
            }
        }
      else
        {
          if (new_state.dwWidth >= 128)
            {
              reward = -0.0001 * new_state.dwWidth * new_state.dwWidth;
            }
          else
            {
              reward = -0.005 * new_state.dwWidth;
            }
        }
      return reward;
    }

  void SendParameter (void);

  void CompleteSend (void);

  void HandleRead (Ptr<Socket> socket);

  void PacketAckRecv (Ptr<Packet> packet);

  int32_t ParamAckRecv (Ptr<Packet> prm);
  
};

} //ns3 namespace 

#endif
