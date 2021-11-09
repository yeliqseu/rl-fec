#ifndef INFOQUEUE_H
#define INFOQUEUE_H

#include <iostream>
#include <deque>
#include <queue>
#include "handlepacket.h"

using namespace std;

struct SearchedInfo
{
  double time;
  int other_kind_packet_num;
  int donothing_num;
};

class InfoQueue
{
private:
  deque<PacketInfo> source_queue;
  deque<PacketInfo> repair_queue;

public:
  void add(int packet_type, int packet_id, double send_time, int another_packet_num, int donothing_num);
  void add(int packet_type, int packet_id, double send_time, int another_packet_num, int donothing_num, int delivered_num, double delivered_time);
  PacketInfo find(uint32_t pktid, int pkt_type);
  SearchedInfo find(PacketInfo* pkt_info);
  PacketInfo find2(PacketInfo* pkt_info);
  int get_source_queue_size();
  int get_repair_queue_size();
  int num_source_sent_since (double send_time);
  int num_repair_sent_since (double send_time);
  //void show();
};

#endif
