#ifndef STREAM_CLIENT_H
#define STREAM_CLIENT_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "handlepacket.h"

extern "C" {
	#include "streamcodec.h"
}

namespace ns3 {

class Socket;
class Packet;


class StreamClientApp : public Application
{
public:
	StreamClientApp();
	void Setup(Address addr, Time ackInterval);							   
	static TypeId GetTypeId (void);

private:
	virtual void StartApplication (void);
	virtual void StopApplication (void);
		  
private: 
	struct parameters m_cp;
	struct decoder *m_dec;
	Ptr<Socket> m_socket;
	uint32_t m_port;
	int m_nPackets;
	unsigned char *m_buf;
	Time m_ackInterval;
	Address m_addr;
	bool m_established;
	PacketInfo m_newpacketInfo;
	int m_nSource;			// number of source packets received so far
	int m_nRepair;			// number of repair packets received so far
	int m_lastAckedN;		// number of received packets when sending the last ACK

	/// Callbacks for tracing the packet Rx events
	TracedCallback<Ptr<const Packet> > m_rxTrace;
					 
	/// Callbacks for tracing the packet Rx events, includes source and destination addresses
	TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;
public:
	static int pkt_count;
private:
	void PacketRecv(Ptr<Packet> pkt,Address from);
	void ParameterRecv(Ptr<Packet> prm,Address from);
	void SetPacketNum(Ptr<Packet> pkt_num);
	void SetBuf();
	void StopRecv(Ptr<Packet> pkt_end);
	void HandleRead(Ptr<Socket> socket);
	void AckSend(Address from);
	void InorderAckSend(Address from);
	
};

}  //ns3 namespace

#endif
