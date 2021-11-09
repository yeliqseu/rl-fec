#ifndef HANDLEPACKET_H
#define HANDLEPACKET_H

#include <stdio.h>
#include <string.h>

extern "C" {
	#include "streamcodec.h"   // libstreamc is written in C
}


class PacketInfo
{
public:
  int m_packet_type;
  int m_id;
  double m_time;
  int m_another_packet_num;
  int m_donoting_num;
  int m_delivered;
  double m_deliveredTime;
public:
  PacketInfo():m_id(-1) {}
  PacketInfo(int packet_type, int id, double time, int another_packet_num,int donoting_num):m_packet_type(packet_type), 
                                                                                            m_id(id), 
                                                                                            m_time(time), 
                                                                                            m_another_packet_num(another_packet_num),
                                                                                            m_donoting_num(donoting_num) {}
  PacketInfo(int packet_type, int id, double time, int another_packet_num,int donoting_num, int delivered_num, double delivered_time):m_packet_type(packet_type), 
                                                                                            m_id(id), 
                                                                                            m_time(time), 
                                                                                            m_another_packet_num(another_packet_num),
                                                                                            m_donoting_num(donoting_num),
                                                                                            m_delivered(delivered_num),
                                                                                            m_deliveredTime(delivered_time) {}
  void update(int packet_type, int id, double time) 
  {
    m_packet_type = packet_type;
	  m_id = id;
	  m_time = time;
  }
  bool operator<(const PacketInfo m)const
  { 
	  return m_id < m.m_id;
  }
};

enum INFO_PACKET_TYPE {
	SOURCE_PACKET = 20000,
	REPAIR_PACKET
};

enum PACKET_TYPE {
	ACK_PRM = 10000,
  PARAMETER,
  BUF,
  DATA_PACKET,
	INORDER,
	COMPLETE
};  
		

unsigned char *serialize_parameter(struct parameters *prm, int nPackets);
unsigned char *serialize_ack_prm(struct parameters *prm, int nPackets);
unsigned char *serialize_inorder_ack(PacketInfo pkt_info, int inorder, int nsource, int nrepair);
unsigned char *serializeAckPacket(int packetType, int packetId, int inorder, int nSource, int nRepair, int dwWidth, int dof);
void deserializeAckPacket(unsigned char *msg_str, int *packetType, int *packetId, int *inorder, int *nSource, int *nRepair, int *dwWidth, int *dof);

unsigned char *add_packet_type(struct encoder *ec,unsigned char *str);
struct parameters *deserialize_parameters(unsigned char *prmstr, int *m_nPackets);
PacketInfo *deserialize_inorder_ack(unsigned char *pkt_info, int *inorder, int *nsource, int *nrepair);

unsigned char *take_off_type(unsigned char *pktstr);
void generate_buf(unsigned char *m_buf, int datasize);


#endif
