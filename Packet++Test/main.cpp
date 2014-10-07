#include <Logger.h>
#include <Packet.h>
#include <EthLayer.h>
#include <VlanLayer.h>
#include <PayloadLayer.h>
#include <IPv4Layer.h>
#include <IPv6Layer.h>
#include <ArpLayer.h>
#include <UdpLayer.h>
#include <TcpLayer.h>
#include <IpAddress.h>
#include <fstream>
#include <stdlib.h>
#include <debug_new.h>
#include <iostream>
#include <sstream>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <in.h>
#endif

#define PACKETPP_TEST(TestName) bool TestName()

#define PACKETPP_ASSERT(exp, assertFailedFormat, ...) \
	if (!(exp)) \
	{ \
		printf("%-30s: FAILED. assertion failed: " assertFailedFormat "\n", __FUNCTION__, ## __VA_ARGS__); \
		return false; \
	}

#define PACKETPP_ASSERT_AND_RUN_COMMAND(exp, command, assertFailedFormat, ...) \
	if (!(exp)) \
	{ \
		printf("%-30s: FAILED. assertion failed: " assertFailedFormat "\n", __FUNCTION__, ## __VA_ARGS__); \
		command; \
		return false; \
	}

#define PACKETPP_TEST_PASSED printf("%-30s: PASSED\n", __FUNCTION__); return true

#define PACKETPP_START_RUNNING_TESTS bool allTestsPassed = true
#define PACKETPP_RUN_TEST(TestName) allTestsPassed &= TestName()
#define PACKETPP_END_RUNNING_TESTS \
		if (allTestsPassed) \
			printf("ALL TESTS PASSED!!\n"); \
		else \
			printf("NOT ALL TESTS PASSED!!\n");

int getFileLength(const char* filename)
{
	ifstream infile(filename, ifstream::binary);
	if (!infile)
		return -1;
	infile.seekg(0, infile.end);
    int length = infile.tellg();
    infile.close();
    return length;
}

uint8_t* readFileIntoBuffer(const char* filename, int& bufferLength)
{
	int fileLength = getFileLength(filename);
	if (fileLength == -1)
		return NULL;

	ifstream infile(filename);
	if (!infile)
		return NULL;

	bufferLength = fileLength/2 + 2;
	uint8_t* result = new uint8_t[bufferLength];
	int i = 0;
	while (!infile.eof())
	{
		char byte[3];
		memset(byte, 0, 3);
		infile.read(byte, 2);
		result[i] = (uint8_t)strtol(byte, NULL, 16);
		i++;
	}
	infile.close();
	bufferLength -= 2;
	return result;
}


PACKETPP_TEST(EthPacketCreation) {
	MacAddress srcMac("aa:aa:aa:aa:aa:aa");
	MacAddress dstMac("bb:bb:bb:bb:bb:bb");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_IP);

	uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04 };
	PayloadLayer payloadLayer(payload, 4, true);

	Packet ethPacket(1);
	PACKETPP_ASSERT(ethPacket.addLayer(&ethLayer), "Adding ethernet layer failed");
	PACKETPP_ASSERT(ethPacket.addLayer(&payloadLayer), "Adding payload layer failed");

	PACKETPP_ASSERT(ethPacket.isPacketOfType(Ethernet), "Packet is not of type Ethernet");
	PACKETPP_ASSERT(ethPacket.getLayerOfType(Ethernet) != NULL, "Ethernet layer doesn't exist");
	PACKETPP_ASSERT(ethPacket.getLayerOfType(Ethernet) == &ethLayer, "Ethernet layer doesn't equal to inserted layer");
	PACKETPP_ASSERT(((EthLayer*)ethPacket.getLayerOfType(Ethernet))->getDestMac() == dstMac, "Packet dest mac isn't equal to intserted dest mac");
	PACKETPP_ASSERT(((EthLayer*)ethPacket.getLayerOfType(Ethernet))->getSourceMac() == srcMac, "Packet src mac isn't equal to intserted src mac");
	PACKETPP_ASSERT(((EthLayer*)ethPacket.getLayerOfType(Ethernet))->getEthHeader()->etherType == ntohs(ETHERTYPE_IP), "Packet ether type isn't equal to ETHERTYPE_IP");

	RawPacket* rawPacket = ethPacket.getRawPacket();
	PACKETPP_ASSERT(rawPacket != NULL, "Raw packet is NULL");
	PACKETPP_ASSERT(rawPacket->getRawDataLen() == 18, "Raw packet length expected to be 18 but it's %d", rawPacket->getRawDataLen());

	uint8_t expectedBuffer[18] = { 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x08, 0x00, 0x01, 0x02, 0x03, 0x04 };
	PACKETPP_ASSERT(memcmp(rawPacket->getRawData(), expectedBuffer, 18) == 0, "Raw packet data is different than expected");
	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(EthAndArpPacketParsing) {
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/ArpResponsePacket.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

//	int* abba = new int[1000];
//	std::cout << abba[500];

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet ethPacket(&rawPacket);
	PACKETPP_ASSERT(ethPacket.isPacketOfType(Ethernet), "Packet is not of type Ethernet");
	PACKETPP_ASSERT(ethPacket.getLayerOfType(Ethernet) != NULL, "Ethernet layer doesn't exist");

	MacAddress expectedSrcMac(0x30, 0x46, 0x9a, 0x23, 0xfb, 0xfa);
	MacAddress expectedDstMac(0x6c, 0xf0, 0x49, 0xb2, 0xde, 0x6e);
	EthLayer* ethLayer = (EthLayer*)ethPacket.getLayerOfType(Ethernet);
	PACKETPP_ASSERT(ethLayer->getDestMac() == expectedDstMac, "Packet dest mac isn't equal to intserted dest mac");
	PACKETPP_ASSERT(ethLayer->getSourceMac() == expectedSrcMac, "Packet src mac isn't equal to intserted src mac");
	PACKETPP_ASSERT(ethLayer->getEthHeader()->etherType == ntohs(ETHERTYPE_ARP), "Packet ether type isn't equal to ETHERTYPE_ARP, it's 0x%x", ethLayer->getEthHeader()->etherType);

	PACKETPP_ASSERT(ethLayer->getNextLayer()->getProtocol() == ARP, "Next layer isn't of type 'ARP'");
	ArpLayer* arpLayer = (ArpLayer*)ethLayer->getNextLayer();
	PACKETPP_ASSERT(arpLayer->getArpHeader()->hardwareType == htons(1), "ARP hardwareType != 1");
	PACKETPP_ASSERT(arpLayer->getArpHeader()->protocolType == htons(ETHERTYPE_IP), "ARP protocolType != ETHERTYPE_IP, it's 0x%4X", ntohs(arpLayer->getArpHeader()->protocolType));
	PACKETPP_ASSERT(arpLayer->getArpHeader()->hardwareSize == 6, "ARP hardwareSize != 6");
	PACKETPP_ASSERT(arpLayer->getArpHeader()->protocolSize == 4, "ARP protocolSize != 4");
	PACKETPP_ASSERT(arpLayer->getArpHeader()->opcode == htons(ARP_REPLY), "ARP opcode != ARP_REPLY");
	PACKETPP_ASSERT(arpLayer->getSenderIpAddr() == IPv4Address(string("10.0.0.138")), "ARP sender IP addr != 10.0.0.138");
	PACKETPP_ASSERT(arpLayer->getTargetMacAddress() == MacAddress("6c:f0:49:b2:de:6e"), "ARP target mac addr != 6c:f0:49:b2:de:6e");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(ArpPacketCreation)
{
	MacAddress srcMac("6c:f0:49:b2:de:6e");
	MacAddress dstMac("ff:ff:ff:ff:ff:ff:");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_ARP);

	ArpLayer arpLayer(ARP_REQUEST, srcMac, srcMac, IPv4Address(string("10.0.0.1")), IPv4Address(string("10.0.0.138")));

	Packet arpRequestPacket(1);
	PACKETPP_ASSERT(arpRequestPacket.addLayer(&ethLayer), "Couldn't add eth layer");
	PACKETPP_ASSERT(arpRequestPacket.addLayer(&arpLayer), "Couldn't add arp layer");
	arpRequestPacket.computeCalculateFields();
	PACKETPP_ASSERT(arpRequestPacket.getRawPacket()->getRawDataLen() == 42, "arp packet size != 42 bytes, Actual: %d", arpRequestPacket.getRawPacket()->getRawDataLen());

	ArpLayer* pArpLayer = (ArpLayer*)arpRequestPacket.getLayerOfType(ARP);
	PACKETPP_ASSERT(pArpLayer != NULL, "Packet doesn't contain arp layer");

	arphdr* arpHeader = pArpLayer->getArpHeader();
	PACKETPP_ASSERT(arpHeader->hardwareSize == 6, "Arp header: hardwareSize != 6, Actual: %d", arpHeader->hardwareSize);
	PACKETPP_ASSERT(arpHeader->protocolType == htons(ETHERTYPE_IP), "Arp header: protocolType != ETHERTYPE_IP, Actual: %d", arpHeader->protocolType);

	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/ArpRequestPacket.dat", bufferLength);
	PACKETPP_ASSERT(buffer != NULL, "cannot read file");
	PACKETPP_ASSERT(bufferLength == arpRequestPacket.getRawPacket()->getRawDataLen(), "Generated packet len (%d) is different than read packet len (%d)", arpRequestPacket.getRawPacket()->getRawDataLen(), bufferLength);
	PACKETPP_ASSERT(memcmp(arpRequestPacket.getRawPacket()->getRawData(), buffer, bufferLength) == 0, "Raw packet data is different than expected");

	delete [] buffer;
	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(VlanParseAndCreation)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/ArpRequestWithVlan.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);
	Packet arpWithVlan(&rawPacket);
	VlanLayer* pFirstVlanLayer = NULL;
	VlanLayer* pSecondVlanLayer = NULL;
	PACKETPP_ASSERT((pFirstVlanLayer = (VlanLayer*)arpWithVlan.getLayerOfType(VLAN)) != NULL, "Couldn't get first vlan layer from packet");
	vlan_header* vlanHeader = pFirstVlanLayer->getVlanHeader();
	PACKETPP_ASSERT(pFirstVlanLayer->getVlanID() == 100, "first vlan ID != 100, it's 0x%2X", pFirstVlanLayer->getVlanID());
	PACKETPP_ASSERT(vlanHeader->cfi == htons(0), "first vlan CFI != 0");
	PACKETPP_ASSERT(vlanHeader->priority == htons(0), "first vlan priority != 0");
	PACKETPP_ASSERT((pSecondVlanLayer = (VlanLayer*)arpWithVlan.getNextLayerOfType(pFirstVlanLayer, VLAN)) != NULL, "Couldn't get second vlan layer from packet");
	vlanHeader = pSecondVlanLayer->getVlanHeader();
	PACKETPP_ASSERT(pSecondVlanLayer->getVlanID() == 200, "second vlan ID != 200");
	PACKETPP_ASSERT(vlanHeader->cfi == htons(0), "second vlan CFI != 0");
	PACKETPP_ASSERT(vlanHeader->priority == htons(0), "second vlan priority != 0");

	Packet arpWithVlanNew(1);
	MacAddress macSrc("ca:03:0d:b4:00:1c");
	MacAddress macDest("ff:ff:ff:ff:ff:ff");
	EthLayer ethLayer(macSrc, macDest, ETHERTYPE_VLAN);
	VlanLayer firstVlanLayer(100, 0, 0, ETHERTYPE_VLAN);
	VlanLayer secondVlanLayer(200, 0, 0, ETHERTYPE_ARP);
	ArpLayer arpLayer(ARP_REQUEST, macSrc, MacAddress("00:00:00:00:00:00"), IPv4Address(string("192.168.2.200")), IPv4Address(string("192.168.2.254")));
	PACKETPP_ASSERT(arpWithVlanNew.addLayer(&ethLayer), "Couldn't add eth layer");
	PACKETPP_ASSERT(arpWithVlanNew.addLayer(&firstVlanLayer), "Couldn't add first vlan layer");
	PACKETPP_ASSERT(arpWithVlanNew.addLayer(&secondVlanLayer), "Couldn't add second vlan layer");
	PACKETPP_ASSERT(arpWithVlanNew.addLayer(&arpLayer), "Couldn't add second arp layer");

	arpWithVlanNew.computeCalculateFields();

	PACKETPP_ASSERT(bufferLength == arpWithVlanNew.getRawPacket()->getRawDataLen(), "Generated packet len (%d) is different than read packet len (%d)", arpWithVlanNew.getRawPacket()->getRawDataLen(), bufferLength);
	PACKETPP_ASSERT(memcmp(arpWithVlanNew.getRawPacket()->getRawData(), buffer, bufferLength) == 0, "Raw packet data is different than expected");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(Ipv4PacketCreation)
{
	Packet ip4Packet(1);

	MacAddress srcMac("aa:aa:aa:aa:aa:aa");
	MacAddress dstMac("bb:bb:bb:bb:bb:bb");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_IP);
	PACKETPP_ASSERT(ip4Packet.addLayer(&ethLayer), "Adding ethernet layer failed");

	Packet tmpPacket(50);
	LoggerPP::getInstance().supressErrors();
	PACKETPP_ASSERT(!tmpPacket.addLayer(&ethLayer), "Wrongly succeeded to add the same Ethernet layer into 2 different packets");
	LoggerPP::getInstance().enableErrors();

	RawPacket* rawPacket = ip4Packet.getRawPacket();
	PACKETPP_ASSERT(rawPacket != NULL, "Raw packet is NULL");
	PACKETPP_ASSERT(rawPacket->getRawDataLen() == 14, "Raw packet length expected to be 14 but it's %d", rawPacket->getRawDataLen());


	IPv4Address ipSrc(string("1.1.1.1"));
	IPv4Address ipDst(string("20.20.20.20"));
	IPv4Layer ip4Layer(ipSrc, ipDst);
	ip4Layer.getIPv4Header()->protocol = PACKETPP_IPPROTO_TCP;
	PACKETPP_ASSERT(ip4Packet.addLayer(&ip4Layer), "Adding IPv4 layer failed");

	uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xa };
	PayloadLayer payloadLayer(payload, 10, true);
	PACKETPP_ASSERT(ip4Packet.addLayer(&payloadLayer), "Adding payload layer failed");

	ip4Packet.computeCalculateFields();

	PACKETPP_ASSERT(ip4Packet.getLayerOfType(Ethernet)->getDataLen() == 44, "Eth Layer data len != 44, it's %d", ip4Packet.getLayerOfType(Ethernet)->getDataLen());
	PACKETPP_ASSERT(ip4Packet.getLayerOfType(IPv4) != NULL, "Packet doesn't contain IPv4 layer");
	iphdr* ipHeader = ip4Layer.getIPv4Header();
	PACKETPP_ASSERT(ip4Layer.getSrcIpAddress() == ipSrc, "IPv4 Layer src IP isn't equal to inserted src IP");
	PACKETPP_ASSERT(ip4Layer.getDstIpAddress() == ipDst, "IPv4 Layer dst IP isn't equal to inserted dst IP");
	PACKETPP_ASSERT(ipHeader->ipVersion == 4, "IPv4 Layer version != 4, Actual: %d", ipHeader->ipVersion);
	PACKETPP_ASSERT(ipHeader->internetHeaderLength == 5, "IPv4 Layer header length != 5, Actual: %d", ipHeader->internetHeaderLength);
	PACKETPP_ASSERT(ipHeader->totalLength == htons(30), "IPv4 Layer total length != 30");
	PACKETPP_ASSERT(ipHeader->protocol == PACKETPP_IPPROTO_TCP, "IPv4 Layer protocol isn't PACKETPP_IPPROTO_TCP");
	PACKETPP_ASSERT(ipHeader->headerChecksum == htons(0x90b1), "IPv4 Layer header checksum is wrong. Expected: 0x%4X, Actual: 0x%4X", 0x90b1, ipHeader->headerChecksum);

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(Ipv4PacketParsing)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/IcmpPacket.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet ip4Packet(&rawPacket);
	PACKETPP_ASSERT(ip4Packet.isPacketOfType(Ethernet), "Packet is not of type Ethernet");
	PACKETPP_ASSERT(ip4Packet.getLayerOfType(Ethernet) != NULL, "Ethernet layer doesn't exist");
	PACKETPP_ASSERT(ip4Packet.isPacketOfType(IPv4), "Packet is not of type IPv4");
	PACKETPP_ASSERT(ip4Packet.getLayerOfType(IPv4) != NULL, "IPv4 layer doesn't exist");

	EthLayer* ethLayer = (EthLayer*)ip4Packet.getLayerOfType(Ethernet);
	PACKETPP_ASSERT(ntohs(ethLayer->getEthHeader()->etherType) == ETHERTYPE_IP, "Packet ether type isn't equal to ETHERTYPE_IP");

	IPv4Layer* ipv4Layer = (IPv4Layer*)ip4Packet.getLayerOfType(IPv4);
	IPv4Address ip4addr1(string("10.0.0.4"));
	IPv4Address ip4addr2(string("1.1.1.1"));
	PACKETPP_ASSERT(ipv4Layer->getIPv4Header()->protocol == 1, "Protocol read from packet isnt ICMP (=1). Protocol is: %d", ipv4Layer->getIPv4Header()->protocol);
	PACKETPP_ASSERT(ipv4Layer->getIPv4Header()->ipVersion == 4, "IP version isn't 4. Version is: %d", ipv4Layer->getIPv4Header()->ipVersion);
	PACKETPP_ASSERT(ipv4Layer->getIPv4Header()->ipSrc == ip4addr1.toInt(), "incorrect source address");
	PACKETPP_ASSERT(ipv4Layer->getIPv4Header()->ipDst == ip4addr2.toInt(), "incorrect dest address");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(Ipv4UdpChecksum)
{
	for (int i = 1; i<6; i++)
	{
		stringstream strStream;
		strStream << "PacketExamples/UdpPacket4Checksum" << i << ".dat";
		string fileName = strStream.str();
		int bufferLength = 0;
		uint8_t* buffer = readFileIntoBuffer(fileName.c_str(), bufferLength);
		PACKETPP_ASSERT(!(buffer == NULL), "cannot read file '%s'", fileName.c_str());

		timeval time;
		gettimeofday(&time, NULL);
		RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

		Packet udpPacket(&rawPacket);
		UdpLayer* udpLayer = NULL;
		PACKETPP_ASSERT((udpLayer = (UdpLayer*)udpPacket.getLayerOfType(UDP)) != NULL, "UDP layer doesn't exist");
		uint16_t packetChecksum = udpLayer->getUdpHeader()->headerChecksum;
		udpLayer->computeCalculateFields();
		PACKETPP_ASSERT(udpLayer->getUdpHeader()->headerChecksum == packetChecksum, "Calculated checksum (0x%4X) != original checksum (0x%4X)", udpLayer->getUdpHeader()->headerChecksum, packetChecksum);
	}

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(Ipv6UdpPacketParseAndCreate)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/IPv6UdpPacket.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet ip6UdpPacket(&rawPacket);
	PACKETPP_ASSERT(!ip6UdpPacket.isPacketOfType(IPv4), "Packet is of type IPv4 instead IPv6");
	PACKETPP_ASSERT(!ip6UdpPacket.isPacketOfType(TCP), "Packet is of type TCP where it shouldn't");
	IPv6Layer* ipv6Layer = NULL;
	PACKETPP_ASSERT((ipv6Layer = (IPv6Layer*)ip6UdpPacket.getLayerOfType(IPv6)) != NULL, "IPv6 layer doesn't exist");
	PACKETPP_ASSERT(ipv6Layer->getIPv6Header()->nextHeader == 17, "Protocol read from packet isnt UDP (17). Protocol is: %d", ipv6Layer->getIPv6Header()->nextHeader);
	PACKETPP_ASSERT(ipv6Layer->getIPv6Header()->ipVersion == 6, "IP version isn't 6. Version is: %d", ipv6Layer->getIPv6Header()->ipVersion);
	IPv6Address srcIP(string("fe80::4dc7:f593:1f7b:dc11"));
	IPv6Address dstIP(string("ff02::c"));
	PACKETPP_ASSERT(ipv6Layer->getSrcIpAddress() == srcIP, "incorrect source address");
	PACKETPP_ASSERT(ipv6Layer->getDstIpAddress() == dstIP, "incorrect dest address");
	UdpLayer* pUdpLayer = NULL;
	PACKETPP_ASSERT((pUdpLayer = (UdpLayer*)ip6UdpPacket.getLayerOfType(UDP)) != NULL, "UDP layer doesn't exist");
	PACKETPP_ASSERT(pUdpLayer->getUdpHeader()->portDst == htons(1900), "UDP dest port != 1900");
	PACKETPP_ASSERT(pUdpLayer->getUdpHeader()->portSrc == htons(63628), "UDP dest port != 63628");
	PACKETPP_ASSERT(pUdpLayer->getUdpHeader()->length == htons(154), "UDP dest port != 154");
	PACKETPP_ASSERT(pUdpLayer->getUdpHeader()->headerChecksum == htons(0x5fea), "UDP dest port != 0x5fea");

	Packet ip6UdpPacketNew(1);
	MacAddress macSrc("6c:f0:49:b2:de:6e");
	MacAddress macDest("33:33:00:00:00:0c");
	EthLayer ethLayer(macSrc, macDest, ETHERTYPE_IPV6);

	IPv6Layer ip6Layer(srcIP, dstIP);
	ip6_hdr* ip6Header = ip6Layer.getIPv6Header();
	ip6Header->hopLimit = 1;
	ip6Header->nextHeader = 17;

	UdpLayer udpLayer(63628, 1900);

	Layer* afterIpv6Layer = pUdpLayer->getNextLayer();
	uint8_t payloadData[afterIpv6Layer->getDataLen()];
	afterIpv6Layer->copyData(payloadData);
	PayloadLayer payloadLayer(payloadData, afterIpv6Layer->getDataLen(), true);

	PACKETPP_ASSERT(ip6UdpPacketNew.addLayer(&ethLayer), "Couldn't add eth layer");
	PACKETPP_ASSERT(ip6UdpPacketNew.addLayer(&ip6Layer), "Couldn't add IPv6 layer");
	PACKETPP_ASSERT(ip6UdpPacketNew.addLayer(&udpLayer), "Couldn't add udp layer");
	PACKETPP_ASSERT(ip6UdpPacketNew.addLayer(&payloadLayer), "Couldn't add payload layer");
	ip6UdpPacketNew.computeCalculateFields();

	PACKETPP_ASSERT(bufferLength == ip6UdpPacketNew.getRawPacket()->getRawDataLen(), "Generated packet len (%d) is different than read packet len (%d)", ip6UdpPacketNew.getRawPacket()->getRawDataLen(), bufferLength);
	PACKETPP_ASSERT(memcmp(ip6UdpPacketNew.getRawPacket()->getRawData(), buffer, bufferLength) == 0, "Raw packet data is different than expected");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(TcpPacketNoOptionsParsing)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketNoOptions.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet tcpPaketNoOptions(&rawPacket);
	PACKETPP_ASSERT(tcpPaketNoOptions.isPacketOfType(IPv4), "Packet isn't of type IPv4");
	PACKETPP_ASSERT(tcpPaketNoOptions.isPacketOfType(TCP), "Packet isn't of type TCP");
	TcpLayer* tcpLayer = NULL;
	PACKETPP_ASSERT((tcpLayer = (TcpLayer*)tcpPaketNoOptions.getLayerOfType(TCP)) != NULL, "TCP layer is NULL");

	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->portDst == htons(60388), "Dest port != 60388, it's %d", ntohs(tcpLayer->getTcpHeader()->portDst));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->portSrc == htons(80), "Src port != 80, it's %d", ntohs(tcpLayer->getTcpHeader()->portSrc));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->sequenceNumber == htonl(0xbeab364a), "Sequence number != 0xbeab364a, it's 0x%lX", ntohl(tcpLayer->getTcpHeader()->sequenceNumber));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->ackNumber == htonl(0xf9ffb58e), "Ack number != 0xf9ffb58e, it's 0x%lX", ntohl(tcpLayer->getTcpHeader()->ackNumber));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->dataOffset == 5, "Header length != 5 (20 bytes), it's %d", tcpLayer->getTcpHeader()->dataOffset);
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->urgentPointer == 0, "Urgent pointer != 0, it's %d", tcpLayer->getTcpHeader()->urgentPointer);
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->headerChecksum == htons(0x4c03), "Header checksum != 0x4c03, it's 0x%4X", ntohs(tcpLayer->getTcpHeader()->headerChecksum));

	// Flags
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->ackFlag == 1, "ACK Flag != 1");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->pshFlag == 1, "PSH Flag != 1");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->urgFlag == 0, "URG Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->cwrFlag == 0, "CWE Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->synFlag == 0, "SYN Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->finFlag == 0, "FIN Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->rstFlag == 0, "RST Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->eceFlag == 0, "ECE Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->reserved == 0, "Reserved != 0");

	// TCP options
	PACKETPP_ASSERT(tcpLayer->getTcpOptionsCount() == 0, "TCP options count isn't 0");
	PACKETPP_ASSERT(tcpLayer->getTcpOptionData(TCPOPT_NOP) == NULL, "TCP option NOP isn't NULL");
	PACKETPP_ASSERT(tcpLayer->getTcpOptionData(TCPOPT_TIMESTAMP) == NULL, "TCP option Timestamp isn't NULL");

	Layer* afterTcpLayer = tcpLayer->getNextLayer();
	PACKETPP_ASSERT(afterTcpLayer != NULL, "Layer after TCP is NULL");
	PACKETPP_ASSERT(afterTcpLayer->getProtocol() == Unknown, "Protocol layer after TCP isn't Unknown");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(TcpPacketWithOptionsParsing)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketWithOptions.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet tcpPaketWithOptions(&rawPacket);
	PACKETPP_ASSERT(tcpPaketWithOptions.isPacketOfType(IPv4), "Packet isn't of type IPv4");
	PACKETPP_ASSERT(tcpPaketWithOptions.isPacketOfType(TCP), "Packet isn't of type TCP");
	TcpLayer* tcpLayer = NULL;
	PACKETPP_ASSERT((tcpLayer = (TcpLayer*)tcpPaketWithOptions.getLayerOfType(TCP)) != NULL, "TCP layer is NULL");

	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->portSrc == htons(44147), "Src port != 44147, it's %d", ntohs(tcpLayer->getTcpHeader()->portSrc));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->portDst == htons(80), "Dest port != 80, it's %d", ntohs(tcpLayer->getTcpHeader()->portDst));
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->ackFlag == 1, "ACK Flag != 1");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->pshFlag == 1, "PSH Flag != 1");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->synFlag == 0, "SYN Flag != 0");
	PACKETPP_ASSERT(tcpLayer->getTcpHeader()->urgentPointer == 0, "Urgent pointer != 0, it's %d", tcpLayer->getTcpHeader()->urgentPointer);

	// TCP options
	PACKETPP_ASSERT(tcpLayer->getTcpOptionsCount() == 3, "TCP options count != 3, it's %d", tcpLayer->getTcpOptionsCount());
	TcpOptionData* nopOptionData = NULL;
	TcpOptionData* timestampOptionData = NULL;
	PACKETPP_ASSERT((timestampOptionData = tcpLayer->getTcpOptionData(TCPOPT_TIMESTAMP)) != NULL, "TCP option Timestamp is NULL");
	PACKETPP_ASSERT((nopOptionData = tcpLayer->getTcpOptionData(TCPOPT_NOP)) != NULL, "TCP option NOP is NULL");
	PACKETPP_ASSERT(timestampOptionData->len == 10, "TCP option Timestamp length != 10, it's 0x%X", timestampOptionData->len);
	uint32_t tsValue = 0;
	uint32_t tsEchoReply = 0;
	memcpy(&tsValue, timestampOptionData->value, 4);
	memcpy(&tsEchoReply, timestampOptionData->value+4, 4);
	PACKETPP_ASSERT(tsValue == htonl(195102), "TCP option Timestamp option: timestamp value != 195102, it's %ld", ntohl(tsValue));
	PACKETPP_ASSERT(tsEchoReply == htonl(3555729271), "TCP option Timestamp option: echo reply value != 3555729271, it's %ld", ntohl(tsEchoReply));

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(TcpPacketWithOptionsParsing2)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketWithOptions3.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet tcpPaketWithOptions(&rawPacket);

	TcpLayer* tcpLayer = NULL;
	PACKETPP_ASSERT((tcpLayer = (TcpLayer*)tcpPaketWithOptions.getLayerOfType(TCP)) != NULL, "TCP layer is NULL");

	PACKETPP_ASSERT(tcpLayer->getTcpOptionsCount() == 5, "TCP options count != 5, it's %d", tcpLayer->getTcpOptionsCount());
	TcpOptionData* mssOptionData = NULL;
	TcpOptionData* sackParmOptionData = NULL;
	TcpOptionData* windowScaleOptionData = NULL;
	PACKETPP_ASSERT((mssOptionData = tcpLayer->getTcpOptionData(TCPOPT_MSS)) != NULL, "TCP option MSS is NULL");
	PACKETPP_ASSERT((sackParmOptionData = tcpLayer->getTcpOptionData(TCPOPT_SACK_PERM)) != NULL, "TCP option SACK perm is NULL");
	PACKETPP_ASSERT((windowScaleOptionData = tcpLayer->getTcpOptionData(TCPOPT_WINDOW)) != NULL, "TCP option window scale is NULL");

	PACKETPP_ASSERT(mssOptionData->len == 4, "TCP option Timestamp length != 4, it's 0x%X", mssOptionData->len);
	PACKETPP_ASSERT(sackParmOptionData->len == 2, "TCP option SACK perm length != 2, it's 0x%X", sackParmOptionData->len);
	PACKETPP_ASSERT(windowScaleOptionData->len == 3, "TCP option window scale length != 3, it's 0x%X", mssOptionData->len);

	uint16_t mssValue = 0;
	memcpy(&mssValue, mssOptionData->value, 2);
	PACKETPP_ASSERT(mssValue == htons(1460), "TCP option MSS option: value != 1460, it's %d", ntohs(mssValue));

	uint8_t windowScale = *windowScaleOptionData->value;
	PACKETPP_ASSERT(windowScale == 4, "TCP option window scale option: value != 4, it's %d", windowScale);

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(TcpPacketCreation)
{
	MacAddress srcMac("30:46:9a:23:fb:fa");
	MacAddress dstMac("08:00:27:19:1c:78");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_IP);
	IPv4Address dstIP(string("10.0.0.6"));
	IPv4Address srcIP(string("212.199.202.9"));
	IPv4Layer ipLayer(srcIP, dstIP);
	ipLayer.getIPv4Header()->ipId = htons(20300);
	ipLayer.getIPv4Header()->fragmentOffset = htons(0x4000);
	ipLayer.getIPv4Header()->timeToLive = 59;
	TcpLayer tcpLayer((uint16_t)80, (uint16_t)44160, 3, TCPOPT_NOP, TCPOPT_NOP, TCPOPT_TIMESTAMP);
	tcpLayer.getTcpHeader()->sequenceNumber = htonl(0xb829cb98);
	tcpLayer.getTcpHeader()->ackNumber = htonl(0xe9771586);
	tcpLayer.getTcpHeader()->ackFlag = 1;
	tcpLayer.getTcpHeader()->pshFlag = 1;
	tcpLayer.getTcpHeader()->windowSize = htons(20178);
	TcpOptionData* tsOptionData = tcpLayer.getTcpOptionData(TCPOPT_TIMESTAMP);
	uint32_t tsValue = htonl(3555735960);
	memcpy(tsOptionData->value, &tsValue, 4);
	uint8_t payloadData[9] = { 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };
	PayloadLayer PayloadLayer(payloadData, 9, true);

	Packet tcpPacket(1);
	tcpPacket.addLayer(&ethLayer);
	tcpPacket.addLayer(&ipLayer);
	tcpPacket.addLayer(&tcpLayer);
	tcpPacket.addLayer(&PayloadLayer);

	uint32_t tsEchoReply = htonl(196757);
	tsOptionData = tcpLayer.getTcpOptionData(TCPOPT_TIMESTAMP);
	memcpy(tsOptionData->value+4, &tsEchoReply, 4);

	tcpPacket.computeCalculateFields();

	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketWithOptions2.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

//	printf("\n\n\n");
//	for(int i = 0; i<bufferLength; i++)
//		printf(" 0x%2X  ", buffer[i]);
//	printf("\n\n\n");
//	for(int i = 0; i<bufferLength; i++)
//	{
//		if (tcpPacket.getRawPacket()->getRawData()[i] != buffer[i])
//			printf("*0x%2X* ", tcpPacket.getRawPacket()->getRawData()[i]);
//		else
//			printf(" 0x%2X  ", tcpPacket.getRawPacket()->getRawData()[i]);
//	}
//	printf("\n\n\n");

	PACKETPP_ASSERT(memcmp(tcpPacket.getRawPacket()->getRawData(), buffer, bufferLength) == 0, "Raw packet data is different than expected");

	delete [] buffer;

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(InsertDataToPacket)
{
	// Creating a packet
	// ~~~~~~~~~~~~~~~~~

	Packet ip4Packet(1);

	MacAddress srcMac("aa:aa:aa:aa:aa:aa");
	MacAddress dstMac("bb:bb:bb:bb:bb:bb");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_IP);
	PACKETPP_ASSERT(ip4Packet.addLayer(&ethLayer), "Adding ethernet layer failed");

	IPv4Address ipSrc(string("1.1.1.1"));
	IPv4Address ipDst(string("20.20.20.20"));
	IPv4Layer ip4Layer(ipSrc, ipDst);
	ip4Layer.getIPv4Header()->protocol = PACKETPP_IPPROTO_TCP;
	PACKETPP_ASSERT(ip4Packet.addLayer(&ip4Layer), "Adding IPv4 layer failed");

	uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xa };
	PayloadLayer payloadLayer(payload, 10, true);
	PACKETPP_ASSERT(ip4Packet.addLayer(&payloadLayer), "Adding payload layer failed");

	ip4Packet.computeCalculateFields();

//	printf("\n\n\n");
//	for(int i = 0; i<ip4Packet.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", ip4Packet.getRawPacket()->getRawData()[i]);


	// Adding a VLAN layer between Eth and IP
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	VlanLayer vlanLayer(100, 0, 0, ETHERTYPE_IP);

	PACKETPP_ASSERT(ip4Packet.insertLayer(&ethLayer, &vlanLayer) == true, "Couldn't insert VLAN layer after Eth later");

//	printf("\n\n\n");
//	for(int i = 0; i<ip4Packet.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", ip4Packet.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	PACKETPP_ASSERT(ethLayer.getDestMac() == dstMac, "MAC dest after insert is different than MAC dest before insert");
	PACKETPP_ASSERT(ip4Layer.getIPv4Header()->internetHeaderLength == 5, "IP header len != 5");
	PACKETPP_ASSERT(ip4Layer.getDstIpAddress() == ipDst, "IP dst after insert is different than IP dst before insert");
	PACKETPP_ASSERT(ip4Layer.getSrcIpAddress() == ipSrc, "IP src after insert is different than IP src before insert");
	PACKETPP_ASSERT(payloadLayer.getPayload()[3] == 0x04, "Payload after insert is different than payload before insert");


	// Adding another Eth layer at the beginning of the packet
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	MacAddress srcMac2("cc:cc:cc:cc:cc:cc");
	MacAddress dstMac2("dd:dd:dd:dd:dd:dd");
	EthLayer ethLayer2(srcMac2, dstMac2, ETHERTYPE_IP);
	PACKETPP_ASSERT(ip4Packet.insertLayer(NULL, &ethLayer2), "Adding 2nd ethernet layer failed");

	PACKETPP_ASSERT(ip4Packet.getFirstLayer() == &ethLayer2, "1st layer in packet isn't ethLayer2");
	PACKETPP_ASSERT(ip4Packet.getFirstLayer()->getNextLayer() == &ethLayer, "2nd layer in packet isn't ethLayer");
	PACKETPP_ASSERT(ip4Packet.getFirstLayer()->getNextLayer()->getNextLayer() == &vlanLayer, "3rd layer in packet isn't vlanLayer");
	PACKETPP_ASSERT(ethLayer.getDestMac() == dstMac, "MAC dest after insert is different than MAC dest before insert");
	PACKETPP_ASSERT(ip4Layer.getIPv4Header()->internetHeaderLength == 5, "IP header len != 5");
	PACKETPP_ASSERT(ip4Layer.getDstIpAddress() == ipDst, "IP dst after insert is different than IP dst before insert");
	PACKETPP_ASSERT(ip4Layer.getSrcIpAddress() == ipSrc, "IP src after insert is different than IP src before insert");
	PACKETPP_ASSERT(payloadLayer.getPayload()[3] == 0x04, "Payload after insert is different than payload before insert");

//	printf("\n\n\n");
//	for(int i = 0; i<ip4Packet.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", ip4Packet.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");


	// Adding a TCP layer at the end of the packet
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	TcpLayer tcpLayer((uint16_t)12345, (uint16_t)80, 0);
	PACKETPP_ASSERT(ip4Packet.insertLayer(&payloadLayer, &tcpLayer), "Adding tcp layer at the end of packet failed");


	// Create a new packet and use insertLayer for the first layer in packet
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	Packet testPacket(1);
	EthLayer ethLayer3(srcMac2, dstMac2, ETHERTYPE_IP);
	testPacket.insertLayer(NULL, &ethLayer3);
	PACKETPP_ASSERT(testPacket.getFirstLayer() == &ethLayer3, "ethLayer3 isn't the first layer in testPacket");
	PACKETPP_ASSERT(testPacket.getFirstLayer()->getNextLayer() == NULL, "ethLayer3 wrongly has a next layer")
	PACKETPP_ASSERT(ethLayer3.getDestMac() == dstMac2, "ethLayer3 MAC dest is different than before inserting to packet");

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(InsertVlanToPacket)
{
	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketWithOptions3.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet tcpPacket(&rawPacket);

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	VlanLayer vlanLayer(4001, 0, 0, ETHERTYPE_IP);
	tcpPacket.insertLayer(tcpPacket.getFirstLayer(), &vlanLayer);

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	PACKETPP_ASSERT(tcpPacket.getRawPacket()->getRawDataLen() == 78, "Size of packet after vlan insert isn't 78, it's %d", tcpPacket.getRawPacket()->getRawDataLen());
	PACKETPP_ASSERT(tcpPacket.getFirstLayer()->getNextLayer() == &vlanLayer, "VLAN layer isn't the second layer as expected");
	PACKETPP_ASSERT(vlanLayer.getNextLayer() != NULL, "VLAN layer next layer is null");
	PACKETPP_ASSERT(vlanLayer.getNextLayer()->getProtocol() == IPv4, "VLAN layer next layer isn't IPv4");

	PACKETPP_TEST_PASSED;
}

PACKETPP_TEST(RemoveLayerTest)
{
	// parse packet and remove layers
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	int bufferLength = 0;
	uint8_t* buffer = readFileIntoBuffer("PacketExamples/TcpPacketNoOptions.dat", bufferLength);
	PACKETPP_ASSERT(!(buffer == NULL), "cannot read file");

	timeval time;
	gettimeofday(&time, NULL);
	RawPacket rawPacket((const uint8_t*)buffer, bufferLength, time, true);

	Packet tcpPacket(&rawPacket);

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	// a. Remove layer from the middle
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	IPv4Layer* ipLayer = (IPv4Layer*)tcpPacket.getLayerOfType(IPv4);
	PACKETPP_ASSERT(tcpPacket.removeLayer(ipLayer), "Remove IPv4 layer failed");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(IPv4) == false, "Packet is still of type IPv4");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(Ethernet) == true, "Packet isn't of type Ethernet");
	PACKETPP_ASSERT(tcpPacket.getLayerOfType(IPv4) == NULL, "Can still retrieve IPv4 layer");
	PACKETPP_ASSERT(tcpPacket.getFirstLayer()->getNextLayer()->getProtocol() == TCP, "Layer next to Ethernet isn't TCP");
	PACKETPP_ASSERT(tcpPacket.getRawPacket()->getRawDataLen() == 271, "Data length != 271, it's %d", tcpPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	// b. Remove first layer
	// ~~~~~~~~~~~~~~~~~~~~~

	PACKETPP_ASSERT(tcpPacket.removeLayer(tcpPacket.getFirstLayer()), "Remove first layer failed");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(IPv4) == false, "Packet is still of type IPv4");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(Ethernet) == false, "Packet is still of type Ethernet");
	PACKETPP_ASSERT(tcpPacket.getFirstLayer()->getProtocol() == TCP, "First layer isn't of type TCP");
	PACKETPP_ASSERT(tcpPacket.getFirstLayer()->getNextLayer()->getNextLayer() == NULL, "More than 2 layers in packet");
	PACKETPP_ASSERT(tcpPacket.getRawPacket()->getRawDataLen() == 257, "Data length != 257, it's %d", tcpPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	// c. Remove last layer
	// ~~~~~~~~~~~~~~~~~~~~
	PACKETPP_ASSERT(tcpPacket.removeLayer(tcpPacket.getLastLayer()), "Remove last layer failed");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(IPv4) == false, "Packet is still of type IPv4");
	PACKETPP_ASSERT(tcpPacket.isPacketOfType(Ethernet) == false, "Packet is still of type Ethernet");
	PACKETPP_ASSERT(tcpPacket.getFirstLayer() == tcpPacket.getLastLayer(), "More than 1 layer still in packet");
	PACKETPP_ASSERT(tcpPacket.getFirstLayer()->getProtocol() == TCP, "TCP layer was accidently removed from packet");
	PACKETPP_ASSERT(tcpPacket.getRawPacket()->getRawDataLen() == 20, "Data length != 20, it's %d", tcpPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<tcpPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", tcpPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	// create packet and remove layers
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	Packet testPacket(10);

	MacAddress srcMac("aa:aa:aa:aa:aa:aa");
	MacAddress dstMac("bb:bb:bb:bb:bb:bb");
	EthLayer ethLayer(srcMac, dstMac, ETHERTYPE_IP);
	PACKETPP_ASSERT(testPacket.addLayer(&ethLayer), "Adding ethernet layer failed");

	IPv4Address ipSrc(string("1.1.1.1"));
	IPv4Address ipDst(string("20.20.20.20"));
	IPv4Layer ip4Layer(ipSrc, ipDst);
	ip4Layer.getIPv4Header()->protocol = PACKETPP_IPPROTO_TCP;
	PACKETPP_ASSERT(testPacket.addLayer(&ip4Layer), "Adding IPv4 layer failed");

	uint8_t payload[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xa };
	PayloadLayer payloadLayer(payload, 10, true);
	PACKETPP_ASSERT(testPacket.addLayer(&payloadLayer), "Adding payload layer failed");

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");

	// a. remove first layer
	// ~~~~~~~~~~~~~~~~~~~~~

	PACKETPP_ASSERT(testPacket.removeLayer(&ethLayer), "Couldn't remove Eth layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer() == &ip4Layer, "IPv4 layer isn't the first layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer()->getNextLayer()->getNextLayer() == NULL, "More than 2 layers remain in packet");
	PACKETPP_ASSERT(testPacket.isPacketOfType(Ethernet) == false, "Packet is wrongly of type Ethernet");
	PACKETPP_ASSERT(testPacket.isPacketOfType(IPv4) == true, "Packet isn't of type IPv4");
	PACKETPP_ASSERT(testPacket.getRawPacket()->getRawDataLen() == 30, "Raw packet length != 30, it's %d", testPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");


	// b. remove last layer
	// ~~~~~~~~~~~~~~~~~~~~

	PACKETPP_ASSERT(testPacket.removeLayer(&payloadLayer), "Couldn't remove Payload layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer() == &ip4Layer, "IPv4 layer isn't the first layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer()->getNextLayer() == NULL, "More than 1 layer remain in packet");
	PACKETPP_ASSERT(testPacket.isPacketOfType(IPv4) == true, "Packet isn't of type IPv4");
	PACKETPP_ASSERT(testPacket.isPacketOfType(Ethernet) == false, "Packet is wrongly of type Ethernet");
	PACKETPP_ASSERT(testPacket.getRawPacket()->getRawDataLen() == 20, "Raw packet length != 20, it's %d", testPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");


	// c. insert a layer
	// ~~~~~~~~~~~~~~~~~

	VlanLayer vlanLayer(4001, 0, 0, ETHERTYPE_IP);
	PACKETPP_ASSERT(testPacket.insertLayer(NULL, &vlanLayer), "Couldn't add VLAN layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer() == &vlanLayer, "VLAN isn't the first layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer()->getNextLayer() == &ip4Layer, "IPv4 isn't the second layer");
	PACKETPP_ASSERT(testPacket.isPacketOfType(VLAN) == true, "Packet isn't of type VLAN");
	PACKETPP_ASSERT(testPacket.getRawPacket()->getRawDataLen() == 24, "Raw packet length != 24, it's %d", testPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");


	// d. remove the remaining layers (packet remains empty!)
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	PACKETPP_ASSERT(testPacket.removeLayer(&ip4Layer), "Couldn't remove IPv4 layer");
	PACKETPP_ASSERT(testPacket.getFirstLayer() == &vlanLayer, "VLAN isn't the first layer");
	PACKETPP_ASSERT(testPacket.isPacketOfType(IPv4) == false, "Packet is wrongly of type IPv4");
	PACKETPP_ASSERT(testPacket.isPacketOfType(VLAN) == true, "Packet isn't of type VLAN");
	PACKETPP_ASSERT(testPacket.getRawPacket()->getRawDataLen() == 4, "Raw packet length != 4, it's %d", testPacket.getRawPacket()->getRawDataLen());
	PACKETPP_ASSERT(testPacket.removeLayer(&vlanLayer), "Couldn't remove VLAN layer");
	PACKETPP_ASSERT(testPacket.isPacketOfType(VLAN) == false, "Packet is wrongly of type VLAN");
	PACKETPP_ASSERT(testPacket.getRawPacket()->getRawDataLen() == 0, "Raw packet length != 0, it's %d", testPacket.getRawPacket()->getRawDataLen());

//	printf("\n\n\n");
//	for(int i = 0; i<testPacket.getRawPacket()->getRawDataLen(); i++)
//		printf("0x%2X ", testPacket.getRawPacket()->getRawData()[i]);
//	printf("\n\n\n");


	PACKETPP_TEST_PASSED;
}

int main(int argc, char* argv[]) {
	PACKETPP_START_RUNNING_TESTS;

	PACKETPP_RUN_TEST(EthPacketCreation);
	PACKETPP_RUN_TEST(EthAndArpPacketParsing);
	PACKETPP_RUN_TEST(ArpPacketCreation);
	PACKETPP_RUN_TEST(VlanParseAndCreation);
	PACKETPP_RUN_TEST(Ipv4PacketCreation);
	PACKETPP_RUN_TEST(Ipv4PacketParsing);
	PACKETPP_RUN_TEST(Ipv4UdpChecksum);
	PACKETPP_RUN_TEST(Ipv6UdpPacketParseAndCreate);
	PACKETPP_RUN_TEST(TcpPacketNoOptionsParsing);
	PACKETPP_RUN_TEST(TcpPacketWithOptionsParsing);
	PACKETPP_RUN_TEST(TcpPacketWithOptionsParsing2);
	PACKETPP_RUN_TEST(TcpPacketCreation);
	PACKETPP_RUN_TEST(InsertDataToPacket);
	PACKETPP_RUN_TEST(InsertVlanToPacket);
	PACKETPP_RUN_TEST(RemoveLayerTest);

	check_leaks();
	PACKETPP_END_RUNNING_TESTS;
}
