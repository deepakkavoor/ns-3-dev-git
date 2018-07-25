/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
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
 *
 */

#ifndef TCP_ACCECN_DATA_H
#define TCP_ACCECN_DATA_H

namespace ns3 {

class TcpAccEcnData
{
public:
  // Default copy-constructor, destructor
  TcpAccEcnData () : m_ecnCepS(0), m_ecnCebS(0), m_ecnE0bS(0), m_ecnE1bS(0), m_ecnCepR(0), m_ecnCebR(0), m_ecnE0bR(0), m_ecnE1bR(0), m_isIniS(false), m_isIniR(false) {};

  void IniSenderCounters () { if(!m_isIniS) { m_isIniS = true; m_ecnCepS = 5; m_ecnE0bS = 1; m_ecnCebS = 0; m_ecnE1bS = 0;} }
  void IniReceiverCounters () { if(!m_isIniR) { m_isIniR = true; m_ecnCepR = 5; m_ecnE0bR = 1; m_ecnCebR = 0; m_ecnE1bR = 0;} }

  uint32_t   m_ecnCepS    {0}; //!< For data sender, the number of packets marked respectively with the CE
  uint32_t   m_ecnCebS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the CE
  uint32_t   m_ecnE0bS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the ECT(0)
  uint32_t   m_ecnE1bS    {0}; //!< For data sender, the number of TCP payload bytes in packets marked respectively with the ECT(1)
  uint32_t   m_ecnCepR    {0}; //!< For data receiver, the number of packets marked respectively with the CE
  uint32_t   m_ecnCebR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the CE
  uint32_t   m_ecnE0bR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the ECT(0)
  uint32_t   m_ecnE1bR    {0}; //!< For data receiver, the number of TCP payload bytes in packets marked respectively with the ECT(1)

private:
  bool       m_isIniS     {false};
  bool       m_isIniR     {false};
};

} // namespace ns3

#endif //TCP_ACCECN_DATA_H
