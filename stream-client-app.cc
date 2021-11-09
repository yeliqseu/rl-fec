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
#include "stream-client.h"

extern "C" {
  #include "streamcodec.h"
}

using namespace ns3;

int StreamClientApp::pkt_count = 0;

StreamClientApp::StreamClientApp()
{
  m_dec = NULL;
  m_buf = NULL;
  m_nPackets = 0;
  m_socket = 0;
  m_established = false;
  m_nSource = 0;
  m_nRepair = 0;
  m_lastAckedN = 0;
}

void
StreamClientApp::Setup (Address addr, Time ackInterval)
{
	m_addr = addr;
  m_ackInterval = ackInterval;
}

TypeId
StreamClientApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StreamClientApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<StreamClientApp> ()
    .AddAttribute ("Port", "Port on which we listen for incoming packets.",
                   UintegerValue (9),
                   MakeUintegerAccessor (&StreamClientApp::m_port),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("Rx", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamClientApp::m_rxTrace),
                     "ns3::Packet::TracedCallback")
    .AddTraceSource ("RxWithAddresses", "A packet has been received",
                     MakeTraceSourceAccessor (&StreamClientApp::m_rxTraceWithAddresses),
                     "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}


void
StreamClientApp::StartApplication (void)
{

  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
	  
	  //std::cout << "server real address " << m_addr << std::endl;

      if (m_socket->Bind (m_addr) == -1)
        {
          //NS_FATAL_ERROR ("Failed to bind socket");
		  std::cout << "client Failed to bind socket " << std::endl;
        }
    }

  m_socket->SetRecvCallback (MakeCallback (&StreamClientApp::HandleRead, this));
}

void 
StreamClientApp::StopApplication (void)
{
  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

}

void
StreamClientApp::ParameterRecv (Ptr<Packet> prm,Address from)
{
  int prmlen = sizeof(short) + 4 * sizeof(int) + sizeof(double);
  unsigned char *prmstr = (unsigned char *) calloc(prmlen, sizeof(unsigned char));
  prm->CopyData(prmstr, prmlen);
  struct parameters *rprm = deserialize_parameters(prmstr, &m_nPackets);
  if(rprm->pktsize == 0){

    NS_LOG_UNCOND (
      "At time "
      << Simulator::Now ().GetSeconds ()
	    << " (s) client "
      << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 () 
	    << " received parameter packet failure "
    );

  } else {
    m_cp.gfpower = rprm->gfpower;
	  m_cp.pktsize = rprm->pktsize;
	  m_cp.repfreq = rprm->repfreq;
	  m_cp.seed = rprm->seed;
    m_dec = initialize_decoder(&m_cp);

    /*NS_LOG_UNCOND (
      "At time "
	    << Simulator::Now ().GetSeconds ()
	    <<" (s) client "
        << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 () 
	    << " received parameter packet success "
	  );*/

	  AckSend(from);
	  SetBuf();
  }

  if(rprm != NULL) {
    free(rprm);
	  rprm = NULL;
  }
}

void 
StreamClientApp::AckSend(Address from)
{
  unsigned char *prmstr = serialize_ack_prm(&m_cp, m_nPackets);
  int prmlen = sizeof(short)+sizeof(int)*4+sizeof(double);
  Ptr<Packet> prm = Create<Packet>(prmstr, prmlen);
  m_socket->SendTo(prm, 0, from); 

  /*NS_LOG_UNCOND (
    "At time "
	<< Simulator::Now ().GetSeconds ()
	<< " (s) client "
    << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 () 
	<< " send parameter ack success "
	);*/
  if(prmstr != NULL) {
    free(prmstr);
	  prmstr = NULL;
  }
}


void 
StreamClientApp::SetBuf()
{
  int datasize = m_cp.pktsize * m_nPackets;
  m_buf = (unsigned char *) malloc(datasize);
  generate_buf(m_buf,datasize);
  if(m_buf == NULL) {
  	std::cout << "ERROR : client generate m_buf failure" << std::endl;
  }
}

void
StreamClientApp::InorderAckSend(Address from)
{
  // Only ACK when the receiver's state changes (e.g. received new packet)
  if (m_nSource + m_nRepair != m_lastAckedN) {
    int inodlen = sizeof(short) + sizeof(int) * 7;  // hard-coding 7 is ugly, but okay for quick development
    int dwWidth = 0;
    int dof = 0;
    if (m_dec->active == 1) {
      dwWidth = m_dec->win_e - m_dec->win_s + 1;
      dof = m_dec->dof;
    }
    // serialize the feedback information into packet
    unsigned char *inodstr = serializeAckPacket(m_newpacketInfo.m_packet_type, m_newpacketInfo.m_id, m_dec->inorder, m_nSource, m_nRepair, dwWidth, dof);

    Ptr<Packet> inoder_pkt = Create<Packet>(inodstr, inodlen);
    if(m_socket != NULL) {
      m_socket->SendTo(inoder_pkt, 0, from);
    }
    NS_LOG_UNCOND (
      "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
      << "[SendACK] inorder: "
      << m_dec->inorder
      << " and newest packet id is " 
      << m_newpacketInfo.m_id
    );
    if(inodstr != NULL) {
      free(inodstr);
      inodstr = NULL;
    }
    m_lastAckedN = m_nSource + m_nRepair;
  }

  //if (m_dec->inorder < m_nPackets - 1) {
  //	Simulator::Schedule(m_ackInterval, &StreamClientApp::InorderAckSend, this, from);   // sparser ACk, not currently used
  //}
}


void 
StreamClientApp::PacketRecv (Ptr<Packet> pkt,Address from)
{
  if (m_dec->inorder == m_nPackets - 1) {
    return;
  }

  int pktsize = m_cp.pktsize;
  int pktlen = sizeof(short) + 4 * sizeof(int) + pktsize;
  unsigned char *pktstr = (unsigned char *) calloc(pktlen, sizeof(unsigned char));
  pkt->CopyData(pktstr, pktlen);
  unsigned char *str = take_off_type(pktstr);
  struct packet *rpkt = deserialize_packet(m_dec, str);  // rpkt is a pointer to memory buffer in m_dec, do NOT free it
  double receive_time = double(Simulator::Now ().GetSeconds ());

  // print decoder status
  std::ostringstream oss;
  if (m_dec->active) {
    oss << " (active decoding window: [ " << m_dec->win_s << " , " << m_dec->win_e << " ] inorder: " << m_dec->inorder << " )";
  } else {
    oss << " (inactive decoder with inorder: " << m_dec->inorder << " )";
  } 
  if (rpkt->sourceid != -1) {
    NS_LOG_UNCOND (
      "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
      << "[PacketReceive] received source packet no. "
      << rpkt->sourceid
      << oss.str()
    );
    m_nSource += 1;
    m_newpacketInfo.update(SOURCE_PACKET, rpkt->sourceid, receive_time);
  } else {
    NS_LOG_UNCOND (
      "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
      << "[PacketReceive] received repair packet no. "
      << rpkt->repairid
      << " with window [ "
      << rpkt->win_s
      << " , "
      << rpkt->win_e
      << " ]"
      << oss.str()
    );
    m_nRepair += 1;
    m_newpacketInfo.update(REPAIR_PACKET, rpkt->repairid, receive_time);
  }

  int old_state = m_dec->active;
  int old_inorder = m_dec->inorder;
  receive_packet(m_dec, rpkt);
  int new_state = m_dec->active;
  int new_inorder = m_dec->inorder;
  if (new_inorder > old_inorder) {
    NS_LOG_UNCOND (
      "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	    << "[DecoderSuccess] Decoder delivered in-order source packets between: [ "
      << old_inorder + 1
      << " , "
      << new_inorder
      << " ] "
    );
  }
  InorderAckSend(from);   // Send ACK after processin the received packet
  
  if (m_dec->inorder == m_nPackets - 1) {
    // verify decoded data
    NS_LOG_UNCOND (
      "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
      << "[Completion] receiving all the "
      << m_dec->inorder + 1
      << " packets."
    );

    int correct = 1;
    for (int i=0; i<m_nPackets; i++) {
        if (memcmp(m_buf+i*pktsize, m_dec->recovered[i], pktsize) !=0) {
            correct = 0;
            std::cout << "ERROR: recovered " << i << "is NOT identical to original" << std::endl;
        }
    }
    if (!correct) {
        std::cout << "ERROR: All source packets are NOT recovered correctly" << std::endl;
        //printf("[Summary] snum: %d repfreq: %.3f erasure: %.3f nuses: %d \n", snum, cp.repfreq, pe, nuse);
    } else {
      NS_LOG_UNCOND (
        "[Client: " << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
        << "[DataCheck] all packets are recovered correctly."
      );
    }
  }

  //free space
  if(pktstr != NULL) {
	  free(pktstr);
	  pktstr = NULL;
  }
}

void 
StreamClientApp::StopRecv (Ptr<Packet> pkt_end)
{
  /*NS_LOG_UNCOND (
    "At time " 
    << Simulator::Now ().GetSeconds () 
	<<" (s) "
    << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 () 
    << " client completed recovery of "
    << m_dec->inorder + 1
    << " packets"
  );*/

  int pktsize = m_cp.pktsize;

  int correct = 1;
  for (int i=0; i<m_nPackets; i++) {
      if (memcmp(m_buf+i*pktsize, m_dec->recovered[i], pktsize) !=0) {
          correct = 0;
          std::cout << "ERROR: recovered " << i << "is NOT identical to original" << std::endl;
      }
  }
  if (correct) {
    std::cout << "All source packets are recovered correctly" << std::endl;
  }
}

void
StreamClientApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
    {
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      /*if (InetSocketAddress::IsMatchingType (from))
        {
          NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds () << "s client received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());
        }*/
      
      // check packet type (parameter, data_packet)
      int type_len=sizeof(short);
      unsigned char *type_str = (unsigned char *) calloc(type_len, sizeof(unsigned char));
      packet->CopyData(type_str, type_len);
      short type;
      memcpy(&type, type_str, sizeof(short));
      switch (type) {
        case PARAMETER : 
          if (!m_established) {
            ParameterRecv(packet,from);
          }
          break;
        case DATA_PACKET :
          if (!m_established) {
            m_established = true;
			      // Simulator::Schedule(MilliSeconds(10), &StreamClientApp::InorderAckSend, this, from);   // asynchrnous (sparser) ACK, not currently used
          }
          PacketRecv(packet,from);
          break;
        case COMPLETE :
          StopRecv(packet); 
          break; 
        default :
          NS_LOG_UNCOND (
            "At time "
	        << Simulator::Now ().GetSeconds ()
	        << " (s) "
    		  << InetSocketAddress::ConvertFrom (m_addr).GetIpv4 () 
	        << " client receive packet type error "
	      );
      }
	  if(type_str != NULL) {
	    free(type_str);
		  type_str = NULL;
	  }
    } 
}


