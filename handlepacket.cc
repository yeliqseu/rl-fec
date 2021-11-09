#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "handlepacket.h"

extern "C" {
	#include "streamcodec.h"   // libstreamc is written in C
}
		
unsigned char *serialize_parameter(struct parameters *prm, int nPackets)
{
    short type = PARAMETER;
    int strlen = sizeof(short) + sizeof(int) * 4 + sizeof(double);
	unsigned char *prmstr = (unsigned char *)calloc(strlen, sizeof(unsigned char));
    memcpy(prmstr,&type,sizeof(short));
	memcpy(prmstr+sizeof(short), &prm->gfpower, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int), &prm->pktsize, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int)*2, &prm->repfreq, sizeof(double));
	memcpy(prmstr+sizeof(short)+sizeof(int)*2+sizeof(double), &prm->seed, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int)*3+sizeof(double), &nPackets, sizeof(int));
	return prmstr;
}


unsigned char *serialize_ack_prm(struct parameters *prm, int nPackets)
{
    short type = ACK_PRM;
	int strlen = sizeof(short) + sizeof(int) * 4 + sizeof(double);
	unsigned char *prmstr = (unsigned char *)calloc(strlen, sizeof(unsigned char));
    memcpy(prmstr,&type,sizeof(short));
	memcpy(prmstr+sizeof(short), &prm->gfpower, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int), &prm->pktsize, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int)*2, &prm->repfreq, sizeof(double));
	memcpy(prmstr+sizeof(short)+sizeof(int)*2+sizeof(double), &prm->seed, sizeof(int));
	memcpy(prmstr+sizeof(short)+sizeof(int)*3+sizeof(double), &nPackets, sizeof(int));
	return prmstr;
}

unsigned char *serialize_inorder_ack(PacketInfo pkt_info, int inorder, int nsource, int nrepair)
{
	short type = INORDER;
    int inodlen = sizeof(short) + sizeof(int) * 3 + sizeof(PacketInfo);
    unsigned char *inodstr = (unsigned char *)calloc(inodlen, sizeof(unsigned char));
    memcpy(inodstr, &type, sizeof(short));
    memcpy(inodstr+sizeof(short), &inorder, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int), &nsource, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*2, &nrepair, sizeof(int));
	memcpy(inodstr+sizeof(short)+sizeof(int)*3, &pkt_info.m_packet_type, sizeof(int));
	memcpy(inodstr+sizeof(short)+sizeof(int)*4, &pkt_info.m_id, sizeof(int));
	memcpy(inodstr+sizeof(short)+sizeof(int)*5, &pkt_info.m_time, sizeof(double));
	memcpy(inodstr+sizeof(short)+sizeof(int)*5+sizeof(double), &pkt_info.m_another_packet_num, sizeof(int));
	

	return inodstr;
}

unsigned char *serializeAckPacket(int packetType, int packetId, int inorder, int nSource, int nRepair, int dwWidth, int dof)
{
    short type = INORDER;
    int inodlen = sizeof(short) + sizeof(int) * 7;
    unsigned char *inodstr = (unsigned char *) calloc(inodlen, sizeof(unsigned char));
    memcpy(inodstr, &type, sizeof(short));
    memcpy(inodstr+sizeof(short), &packetType, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int), &packetId, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*2, &inorder, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*3, &nSource, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*4, &nRepair, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*5, &dwWidth, sizeof(int));
    memcpy(inodstr+sizeof(short)+sizeof(int)*6, &dof, sizeof(int));
    return inodstr;
}

void deserializeAckPacket(unsigned char *msg_str, int *packetType, int *packetId, int *inorder, int *nSource, int *nRepair, int *dwWidth, int *dof)
{
    memcpy(packetType, msg_str+sizeof(short), sizeof(int));
    memcpy(packetId, msg_str+sizeof(short)+sizeof(int), sizeof(int));
    memcpy(inorder, msg_str+sizeof(short)+sizeof(int)*2, sizeof(int));
    memcpy(nSource, msg_str+sizeof(short)+sizeof(int)*3, sizeof(int));
    memcpy(nRepair, msg_str+sizeof(short)+sizeof(int)*4, sizeof(int));
    memcpy(dwWidth, msg_str+sizeof(short)+sizeof(int)*5, sizeof(int));
    memcpy(dof, msg_str+sizeof(short)+sizeof(int)*6, sizeof(int));
    return;
}

void generate_buf(unsigned char *m_buf, int datasize)
{
	if(m_buf != NULL)
	{
		for (int i=0; i<datasize; i++) {
			m_buf[i] = i+1;
		}
	}

}


unsigned char *add_packet_type(struct encoder *ec,unsigned char *str)
{		
    short type = DATA_PACKET;
    int sym_len  = ec->cp->pktsize;
    int strlen = sizeof(short) + sizeof(int) * 4 + sym_len;
    unsigned char *pktstr = (unsigned char *)calloc(strlen, sizeof(unsigned char));
    memcpy(pktstr, &type, sizeof(short));
    memcpy(pktstr+sizeof(short), str, sizeof(unsigned char)*strlen-sizeof(short));
    free(str);
    return pktstr;
}

struct parameters *deserialize_parameters(unsigned char *prmstr, int *m_nPackets)
{
	struct parameters *prm = (struct parameters *)calloc(1,sizeof(struct parameters));
	memcpy(&prm->gfpower, prmstr+sizeof(short), sizeof(int));
	memcpy(&prm->pktsize, prmstr+sizeof(short)+sizeof(int), sizeof(int));
	memcpy(&prm->repfreq, prmstr+sizeof(short)+sizeof(int)*2, sizeof(double));
	memcpy(&prm->seed, prmstr+sizeof(short)+sizeof(int)*2+sizeof(double), sizeof(int));
	memcpy(m_nPackets, prmstr+sizeof(short)+sizeof(int)*3+sizeof(double), sizeof(int));

	if(prmstr != NULL) {
		free(prmstr);
		prmstr = NULL;
	}

    return prm;
}

PacketInfo *deserialize_inorder_ack(unsigned char *pkt_info_str, int *inorder, int *nsource, int *nrepair)
{
	PacketInfo *pkt_info = (PacketInfo *)calloc(1,sizeof(PacketInfo));
	memcpy(inorder, pkt_info_str+sizeof(short), sizeof(int));
	memcpy(nsource, pkt_info_str+sizeof(short)+sizeof(int), sizeof(int));
	memcpy(nrepair, pkt_info_str+sizeof(short)+sizeof(int)*2, sizeof(int));
	memcpy(&pkt_info->m_packet_type, pkt_info_str+sizeof(short)+sizeof(int)*3, sizeof(int));
	memcpy(&pkt_info->m_id, pkt_info_str+sizeof(short)+sizeof(int)*4, sizeof(int));
	memcpy(&pkt_info->m_time, pkt_info_str+sizeof(short)+sizeof(int)*5, sizeof(double));
	memcpy(&pkt_info->m_another_packet_num, pkt_info_str+sizeof(short)+sizeof(int)*5+sizeof(double), sizeof(int));
	


	if(pkt_info_str != NULL) {
		free(pkt_info_str);
		pkt_info_str = NULL;
	}

    return pkt_info;
}

unsigned char *take_off_type(unsigned char *pktstr)
{
	unsigned char *teststr = pktstr+sizeof(short);
	return teststr;
}



