#include "infoqueue.h"
#include <deque>
#include <queue>

using namespace std;

void InfoQueue::add(int packet_type, int packet_id, double send_time, int another_packet_num, int donothing_num) 
{
  PacketInfo e = PacketInfo(packet_type, packet_id, send_time, another_packet_num, donothing_num);
  if(packet_type == SOURCE_PACKET) {
    source_queue.push_back(e);
  }
  else {
    repair_queue.push_back(e);
  }
}

void InfoQueue::add(int packet_type, int packet_id, double send_time, int another_packet_num, int donothing_num, int delivered_num, double delivered_time) 
{
  PacketInfo e = PacketInfo(packet_type, packet_id, send_time, another_packet_num, donothing_num, delivered_num, delivered_time);
  if(packet_type == SOURCE_PACKET) {
    source_queue.push_back(e);
  }
  else {
    repair_queue.push_back(e);
  }
}

PacketInfo InfoQueue::find(uint32_t pktid, int pkt_type)
{
  if (pkt_type == SOURCE_PACKET)
    {
      for (auto i = source_queue.begin(); i < source_queue.end(); i++) 
        {
          if ((*i).m_id == pktid)
            {
              // return {(*i).m_time, (*i).m_another_packet_num, (*i).m_donoting_num};
              return *i;
            }
        }

    }
  else if (pkt_type == REPAIR_PACKET)
    {
      for (auto i = repair_queue.begin(); i < repair_queue.end(); i++) 
        {
          if ((*i).m_id == pktid)
            {
              return *i;
            }
        }

    }
  else
    {
      return PacketInfo();
    }
}

SearchedInfo InfoQueue::find(PacketInfo* pkt_info) 
{
  //cout << "+++++++++++++++++++++packet_id is " << packet_id << endl;
  deque<PacketInfo>::iterator t;
  if(pkt_info->m_packet_type==SOURCE_PACKET) {
    t=lower_bound(source_queue.begin(),source_queue.end(),*pkt_info);
    if((*t).m_id!=pkt_info->m_id) return {-1,0};
    source_queue.erase(source_queue.begin(),t+1);
  }
  else { 
    t=lower_bound(repair_queue.begin(),repair_queue.end(),*pkt_info);
    if((*t).m_id!=pkt_info->m_id) return {-1,0};
    repair_queue.erase(repair_queue.begin(),t+1);
  }
  return {(*t).m_time, (*t).m_another_packet_num, (*t).m_donoting_num};
 
  //show();
  //return packet_type == SOURCE_PACKET? source_queue.find(packet_id): repair_queue.find(packet_id);
}

PacketInfo InfoQueue::find2(PacketInfo* pkt_info) 
{
  //cout << "+++++++++++++++++++++packet_id is " << packet_id << endl;
  deque<PacketInfo>::iterator t;
  if(pkt_info->m_packet_type==SOURCE_PACKET) {
    t=lower_bound(source_queue.begin(),source_queue.end(),*pkt_info);
    if ((*t).m_id!=pkt_info->m_id) {
      return {-1, -1, -1, -1, -1, -1, -1};
    }
    source_queue.erase(source_queue.begin(),t+1);
  }
  else { 
    t=lower_bound(repair_queue.begin(),repair_queue.end(),*pkt_info);
    if ((*t).m_id!=pkt_info->m_id) {
      return {-1, -1, -1, -1, -1, -1, -1};
    }
    repair_queue.erase(repair_queue.begin(),t+1);
  }
  return (*t);
 
  //show();
  //return packet_type == SOURCE_PACKET? source_queue.find(packet_id): repair_queue.find(packet_id);
}

int InfoQueue::num_source_sent_since (double send_time)
{
  int nsent = 0;
  for (auto i = source_queue.begin(); i < source_queue.end(); i++) 
    {
      if ((*i).m_time >= send_time)
        {
          nsent += 1;
        }
    }
  return nsent;
}

int InfoQueue::num_repair_sent_since (double send_time)
{
  int nsent = 0;
  for (auto i = repair_queue.begin(); i < repair_queue.end(); i++) 
    {
      if ((*i).m_time >= send_time)
        {
          nsent += 1;
        }
    }
  return nsent;
}



int InfoQueue::get_source_queue_size()
{
  return source_queue.size();
}

int InfoQueue::get_repair_queue_size()
{
  return repair_queue.size();
}

/*void InfoQueue::show()
{
  cout << "SOURCE_QUEUE:" << endl;
  source_queue.show();
  cout << "REPAIR_QUEUE:" << endl;
  repair_queue.show();
}*/
