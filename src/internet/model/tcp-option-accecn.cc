/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 *
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
 *
 */

#include "tcp-option-accecn.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpOptionAccEcn");

NS_OBJECT_ENSURE_REGISTERED (TcpOptionAccEcn);

TcpOptionAccEcn::TcpOptionAccEcn ()
    : TcpOption (),
      m_e0b (0),
      m_ceb (0),
      m_e1b (0)
{
}

TcpOptionAccEcn::~TcpOptionAccEcn ()
{
}

TypeId
TcpOptionAccEcn::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::TcpOptionAccEcn")
        .SetParent<TcpOption> ()
        .SetGroupName ("Internet")
        .AddConstructor<TcpOptionAccEcn> ()
    ;
    return tid;
}

TypeId
TcpOptionAccEcn::GetInstanceTypeId (void) const
{
    return GetTypeId ();
}

void
TcpOptionAccEcn::Print(std::ostream &os) const
{
    os << "e0b: " << m_e0b << std::endl;
    os << "ceb: " << m_ceb << std::endl;
    os << "e1b: " << m_e1b << std::endl;
}

uint8_t
TcpOptionAccEcn::GetKind (void) const
{
    return TcpOption::EXPERIMENTAL;
}

uint32_t
TcpOptionAccEcn::GetSerializedSize (void) const
{
    NS_LOG_FUNCTION (this);
    // 2: Kind(1 byte) + Length(1 byte)
    // 2: ExID for magic number(2 bytes)
    // 3*3: option content field with 3 counters * 3 bytes for each counter
    NS_LOG_LOGIC ("Serialized size: " << 2 + 2 + 3 * 3);
    return 13;
}

void
TcpOptionAccEcn::Serialize (Buffer::Iterator start) const
{
    NS_LOG_FUNCTION (this);
    Buffer::Iterator i = start;
    i.WriteU8 (GetKind ()); // Kind
    uint8_t length = static_cast<uint8_t> (GetSerializedSize ());
    i.WriteU8 (length); // Length
    i.WriteHtonU16 (GetExID()); // ExID for magic number(0xACCE)

    //write e0b
    uint16_t high_16 = static_cast<uint16_t> ((m_e0b & 0xFFFF00) >> 8);
    uint8_t low_8 = static_cast<uint8_t> (m_e0b & 0xFF);
    i.WriteHtonU16(high_16);
    i.WriteU8(low_8);

    //write ceb
    high_16 = static_cast<uint16_t> ((m_ceb & 0xFFFF00) >> 8);
    low_8 = static_cast<uint8_t> (m_ceb & 0xFF);
    i.WriteHtonU16(high_16);
    i.WriteU8(low_8);

    //write e1b
    high_16 = static_cast<uint16_t> ((m_e1b & 0xFFFF00) >> 8);
    low_8 = static_cast<uint8_t> (m_e1b & 0xFF);
    i.WriteHtonU16(high_16);
    i.WriteU8(low_8);

}

uint32_t
TcpOptionAccEcn::Deserialize (Buffer::Iterator start)
{
    NS_LOG_FUNCTION (this);
    Buffer::Iterator i = start;
    uint8_t readKind = i.ReadU8 ();
    uint8_t size = i.ReadU8 ();
    uint16_t exID = i.ReadNtohU16 ();
    if (readKind != GetKind () || exID != GetExID())
    {
        NS_LOG_WARN ("Malformed AccEcn option, wrong type");
        return 0;
    }
    NS_LOG_LOGIC ("Size: " << static_cast<uint32_t> (size));

    //read e0b
    uint32_t high_16 = static_cast<uint32_t> (i.ReadNtohU16 ());
    uint32_t low_8 = static_cast<uint32_t> (i.ReadU8 ());
    m_e0b = (high_16 << 8) + low_8;

    //read ceb
    high_16 = static_cast<uint32_t> (i.ReadNtohU16 ());
    low_8 = static_cast<uint32_t> (i.ReadU8 ());
    m_ceb = (high_16 << 8) + low_8;

    //read e1b
    high_16 = static_cast<uint32_t> (i.ReadNtohU16 ());
    low_8 = static_cast<uint32_t> (i.ReadU8 ());
    m_e1b = (high_16 << 8) + low_8;

    return GetSerializedSize ();
}

uint16_t
TcpOptionAccEcn::GetExID (void) const
{
    return 0xACCE;
}

uint32_t
TcpOptionAccEcn::GetE0B (void) const
{
    return m_e0b;
}

uint32_t
TcpOptionAccEcn::GetCEB (void) const
{
    return m_ceb;
}

uint32_t
TcpOptionAccEcn::GetE1B (void) const
{
    return m_e1b;
}

void
TcpOptionAccEcn::SetE0B (uint32_t e0b)
{
    m_e0b = e0b;
}

void
TcpOptionAccEcn::SetCEB (uint32_t ceb)
{
    m_ceb = ceb;
}

void
TcpOptionAccEcn::SetE1B (uint32_t e1b)
{
    m_e1b = e1b;
}

}// namespace ns3