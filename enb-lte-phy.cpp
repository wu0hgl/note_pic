/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 TELEMATICS LAB, Politecnico di Bari
 *
 * This file is part of 5G-simulator
 *
 * 5G-simulator is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation;
 *
 * 5G-simulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 5G-simulator; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Giuseppe Piro <g.piro@poliba.it>
 * Author2: Alessandro Grassi <alessandro.grassi@poliba.it>
 */


#include "enb-lte-phy.h"
#include "../device/NetworkNode.h"
#include "../channel/LteChannel.h"
#include "../core/spectrum/bandwidth-manager.h"
#include "../protocolStack/packet/packet-burst.h"
#include "../core/spectrum/transmitted-signal.h"
#include "../core/idealMessages/ideal-control-messages.h"
#include "../device/ENodeB.h"
#include "../device/UserEquipment.h"
#include "interference.h"
#include "error-model.h"
#include "../channel/propagation-model/propagation-loss-model.h"
#include "../protocolStack/mac/AMCModule.h"
#include "../utility/eesm-effective-sinr.h"
#include "../utility/miesm-effective-sinr.h"
#include "../componentManagers/FrameManager.h"


#define UL_INTERFERENCE 4

EnbLtePhy::EnbLtePhy()
{
  SetDevice(nullptr);
  SetDlChannel(nullptr);
  SetUlChannel(nullptr);
  SetBandwidthManager(nullptr);
  SetTxSignal (nullptr);
  SetErrorModel (nullptr);
  SetInterference (nullptr);
  SetTxPower(43); //dBm
  SetTxAntennas(1);
  SetRxAntennas(1);
  GetAntennaParameters ()->SetType(LtePhy::AntennaParameters::ANTENNA_TYPE_OMNIDIRECTIONAL);
  GetAntennaParameters ()->SetBearing(0);
  GetAntennaParameters ()->SetEtilt(15);
}

EnbLtePhy::~EnbLtePhy()
{
  Destroy ();
}

void
EnbLtePhy::DoSetBandwidthManager (void)
{
  BandwidthManager* s = GetBandwidthManager ();
  vector<double> channels = s->GetDlSubChannels ();

  TransmittedSignal* txSignal = new TransmittedSignal ();

  vector< vector<double> > values;
  values.resize( GetTxAntennas () );

  double powerTx = pow (10., (GetTxPower () - 30) / 10); // in natural unit

  double txPower = 10 * log10 (powerTx / channels.size () ); //in dB

  for (int i = 0; i < GetTxAntennas (); i++)
    {
      for (auto channel : channels)
        {
          values.at(i).push_back(txPower);
        }
    }
  txSignal->SetValues (values);
  //txSignal->SetBandwidthManager (s->Copy());

  SetTxSignal (txSignal);
}

void
EnbLtePhy::StartTx (shared_ptr<PacketBurst> p)
{
  //cout << "Node " << GetDevice()->GetIDNetworkNode () << " starts phy tx" << endl;

  if (FrameManager::Init()->MbsfnEnabled()==true && FrameManager::Init()->isMbsfnSubframe()==true)
    {
      GetDlMcChannel ()->StartTx (p, GetTxSignal (), GetDevice ());
    }
  else
    {
      GetDlChannel ()->StartTx (p, GetTxSignal (), GetDevice ());
    }
}

void
EnbLtePhy::StartRx (shared_ptr<PacketBurst> p, TransmittedSignal* txSignal, NetworkNode* src)
{
DEBUG_LOG_START_1(LTE_SIM_TEST_DEVICE_ON_CHANNEL)
  cout << "Node " << GetDevice()->GetIDNetworkNode () << " starts phy rx" << endl;
DEBUG_LOG_END

  //COMPUTE THE SINR
  vector<double> measuredSinr;
  vector<int> channelsForRx;
  vector<double> rxSignalValues = txSignal->GetValues().at(0);

  double interference = 0;
  double noise_interference = 10. * log10 (pow(10., GetThermalNoise()/10) + interference); // dB

  int chId = 0;
  for ( auto power : rxSignalValues ) // transmission power for the current sub channel [dB]
    {
      if (power != 0.)
        {
          channelsForRx.push_back (chId);
        }
      chId++;
      measuredSinr.push_back (power - noise_interference - UL_INTERFERENCE);
    }

  //CHECK FOR PHY ERROR
  bool phyError = false;

  vector<int> cqi; //compute the CQI

  UserEquipment*eq = (UserEquipment*)src;
  cout << "\t UE " << eq->GetIDNetworkNode() << " mcs " << eq->GetTargetNodeRecord()->GetUlMcs() <<
		  " data to translate: " << eq->GetTargetNodeRecord()->m_schedulingRequest << " sinr ";			// by zyb
  cqi.push_back(eq->GetTargetNodeRecord()->GetUlMcs());
//  ENodeB* enb = GetDevice();
//  for (auto record : *enb->GetUserEquipmentRecords ()) {
//	  if (record->GetUlMcs() != -1) {
//		  cout << "StartRx mcs: " << record->m_ulMcs << " Data to Translate : " << record->m_schedulingRequest << endl;
//		  cqi.push_back(record->m_ulMcs);
//	  }
//  }

  if (GetErrorModel() != nullptr)
    {

    phyError = GetErrorModel ()->CheckForPhysicalError (channelsForRx, cqi, measuredSinr);
    if (_PHY_TRACING_)
      {
        if (phyError)
          {
            cout << "**** YES PHY ERROR (node " << GetDevice ()->GetIDNetworkNode () << ") ****" << endl;
          }
        else
          {
            cout << "**** NO PHY ERROR (node " << GetDevice ()->GetIDNetworkNode () << ") ****" << endl;
          }
      }
    }


  if (!phyError && p->GetNPackets() > 0)
    {
      //FORWARD RECEIVED PACKETS TO THE DEVICE
      GetDevice()->ReceivePacketBurst(p);
      eq->GetTargetNodeRecord()->SetUlMcs(-1);
    }

  delete txSignal;
}

void
EnbLtePhy::StartRx (shared_ptr<PacketBurst> p, TransmittedSignal* txSignal)
{
DEBUG_LOG_START_1(LTE_SIM_TEST_DEVICE_ON_CHANNEL)
  cout << "Node " << GetDevice()->GetIDNetworkNode () << " starts phy rx" << endl;
DEBUG_LOG_END

  //COMPUTE THE SINR
  vector<double> measuredSinr;
  vector<int> channelsForRx;
  vector<double> rxSignalValues = txSignal->GetValues().at(0);

  double interference = 0;
  double noise_interference = 10. * log10 (pow(10., GetThermalNoise()/10) + interference); // dB

  int chId = 0;
  for ( auto power : rxSignalValues ) // transmission power for the current sub channel [dB]
    {
      if (power != 0.)
        {
          channelsForRx.push_back (chId);
        }
      chId++;
      measuredSinr.push_back (power - noise_interference - UL_INTERFERENCE);
    }

  //CHECK FOR PHY ERROR
  bool phyError = false;
  /*
  if (GetErrorModel() != nullptr)
    {
    vector<int> cqi; //compute the CQI
    phyError = GetErrorModel ()->CheckForPhysicalError (channelsForRx, cqi, measuredSinr);
    if (_PHY_TRACING_)
      {
        if (phyError)
          {
            cout << "**** YES PHY ERROR (node " << GetDevice ()->GetIDNetworkNode () << ") ****" << endl;
          }
        else
          {
            cout << "**** NO PHY ERROR (node " << GetDevice ()->GetIDNetworkNode () << ") ****" << endl;
          }
      }
    }
    */

  if (!phyError && p->GetNPackets() > 0)
    {
      //FORWARD RECEIVED PACKETS TO THE DEVICE
      GetDevice()->ReceivePacketBurst(p);
    }

  delete txSignal;
}

void
EnbLtePhy::SendIdealControlMessage (IdealControlMessage *msg)
{
  ENodeB *enb = GetDevice ();
  switch( msg->GetMessageType() )
    {
    case IdealControlMessage::ALLOCATION_MAP:
        {
          PdcchMapIdealControlMessage *pdcchMsg =  (PdcchMapIdealControlMessage*)msg;
          for (auto record : *pdcchMsg->GetMessage())
            {
              record.m_ue->GetPhy ()->ReceiveIdealControlMessage (msg);
            }
        }
      break;

    case IdealControlMessage::NB_IOT_ALLOCATION_MAP:
        {
          NbIoTMapIdealControlMessage *pdcchMsg =  (NbIoTMapIdealControlMessage*)msg;
          for (auto record : *pdcchMsg->GetMessage())
            {
              record.m_ue->GetPhy ()->ReceiveIdealControlMessage (msg);
            }
        }
      break;

    case IdealControlMessage::RA_RESPONSE:
    case IdealControlMessage::RA_CONNECTION_RESOLUTION:
      msg->GetDestinationDevice()->GetPhy ()->ReceiveIdealControlMessage (msg);
      break;

    default:
      cout << "Error in EnbLtePhy::SendIdealControlMessage: unknown message type (" << msg->GetMessageType() << ")" << endl;
      exit(1);
    }

  delete msg;
}

void
EnbLtePhy::ReceiveIdealControlMessage (IdealControlMessage *msg)
{
  ENodeB* enb = GetDevice();
  enb->GetMacEntity()->ReceiveIdealControlMessage(msg);
}

void
EnbLtePhy::ReceiveReferenceSymbols (NetworkNode* n, TransmittedSignal* s)
{
  ENodeB::UserEquipmentRecord* user = ((UserEquipment*) n)->GetTargetNodeRecord();
  ReceivedSignal* rxSignal;
  if (GetUlChannel ()->GetPropagationLossModel () != nullptr)
    {
      rxSignal = GetUlChannel ()->GetPropagationLossModel ()->
                 AddLossModel (n, GetDevice (), s);
    }
  else
    {
      rxSignal = s->Copy ();
    }
  AMCModule* amc = user->GetUE()->GetProtocolStack ()->GetMacEntity ()->GetAmcModule ();
  vector<double> ulQuality;
  vector<double> rxSignalValues = rxSignal->GetValues ().at(0);
  delete rxSignal;
  double noise_interference = 10. * log10 (pow(10., GetThermalNoise()/10)); // dB
  for (auto power : rxSignalValues)
    {
      ulQuality.push_back (power - noise_interference - UL_INTERFERENCE);
    }


DEBUG_LOG_START_1(LTE_SIM_TEST_UL_SINR)
  double effectiveSinr = GetMiesmEffectiveSinr (ulQuality);
  if (effectiveSinr > 40) effectiveSinr = 40;
  int mcs = amc->GetMCSFromCQI (amc->GetCQIFromSinr(effectiveSinr));
  cout << "UL_SINR " << n->GetIDNetworkNode () << " "
            << n->GetMobilityModel ()->GetAbsolutePosition()->GetCoordinateX () << " "
            << n->GetMobilityModel ()->GetAbsolutePosition()->GetCoordinateY () << " "
            << effectiveSinr << " " << mcs << endl;
DEBUG_LOG_END


  user->SetUplinkChannelStatusIndicator (ulQuality);
}


ENodeB*
EnbLtePhy::GetDevice(void)
{
  LtePhy* phy = (LtePhy*)this;
  return (ENodeB*)phy->GetDevice();
}
