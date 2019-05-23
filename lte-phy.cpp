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


#include "lte-phy.h"
#include "../device/NetworkNode.h"
#include "../channel/LteChannel.h"
#include "../core/spectrum/bandwidth-manager.h"
#include "../protocolStack/packet/packet-burst.h"
#include "../core/spectrum/transmitted-signal.h"
#include "interference.h"
#include "error-model.h"

LtePhy::LtePhy()
{
  m_device = nullptr;
  m_dlChannel = nullptr;
  m_dlMcChannel = nullptr;
  m_ulChannel = nullptr;
  m_bandwidthManager = nullptr;
  m_carrierFrequency = 2000;
  m_txSignal = nullptr;
  m_antennaParameters = new AntennaParameters ();
  SetNoiseFigure(2.5);
  m_averageBuildingHeight = 10;

  m_antennaParameters->SetFeederLoss(0);

  // The following parameters are only meaningful with 3-sector antennas
  m_antennaParameters->SetGain (0);
  m_antennaParameters->SetHorizontalBeamwidth3db(70);
  m_antennaParameters->SetVerticalBeamwidth3db(10);
  m_antennaParameters->SetMaxHorizontalAttenuation(20);
  m_antennaParameters->SetMaxVerticalAttenuation(20);

  m_waveform = WAVEFORM_TYPE_OFDM;
  m_useSrtaPi = false;

}

LtePhy::~LtePhy()
{
  delete m_txSignal;
}

void
LtePhy::Destroy (void)
{
  delete m_txSignal;
  delete m_interference;
  delete m_errorModel;
}


void
LtePhy::SetDevice (NetworkNode* d)
{
  m_device = d;
}

NetworkNode*
LtePhy::GetDevice (void)
{
  return m_device;
}

void
LtePhy::SetDlChannel (LteChannel* c)
{
  m_dlChannel = c;
}

void
LtePhy::SetDlMcChannel (LteChannel* c)
{
  m_dlMcChannel = c;
}

LteChannel*
LtePhy::GetDlChannel (void)
{
  return m_dlChannel;
}

LteChannel*
LtePhy::GetDlMcChannel (void)
{
  return m_dlMcChannel;
}

void
LtePhy::SetUlChannel (LteChannel* c)
{
  m_ulChannel = c;
}

LteChannel*
LtePhy::GetUlChannel (void)
{
  return m_ulChannel;
}

void
LtePhy::SetBandwidthManager (BandwidthManager* s)
{
  m_bandwidthManager = s;
  if (s != nullptr)
    DoSetBandwidthManager ();
}

BandwidthManager*
LtePhy::GetBandwidthManager (void)
{
  return m_bandwidthManager;
}

void
LtePhy::SetCarrierFrequency (double frequency)
{
  m_carrierFrequency = frequency;
}

double
LtePhy::GetCarrierFrequency (void)
{
  return m_carrierFrequency;
}

void
LtePhy::SetTxPower (double p)
{
  m_txPower = p;
}

double
LtePhy::GetTxPower (void)
{
  return m_txPower;
}

void
LtePhy::SetTxSignal (TransmittedSignal* txSignal)
{
  m_txSignal = txSignal;
}

TransmittedSignal*
LtePhy::GetTxSignal (void)
{
  return m_txSignal;
}


void
LtePhy::SetInterference (Interference* i)
{
  m_interference = i;
}

void
LtePhy::SetErrorModel (ErrorModel* e)
{
  m_errorModel = e;
}

void
LtePhy::SetTxAntennas (int n)
{
  m_txAntennas = n;
}

int
LtePhy::GetTxAntennas (void)
{
  return m_txAntennas;
}

void
LtePhy::SetRxAntennas (int n)
{
  m_rxAntennas = n;
}

int
LtePhy::GetRxAntennas (void)
{
  return m_rxAntennas;
}

void
LtePhy::SetHeight (double height)
{
  GetDevice ()->GetMobilityModel ()->GetAbsolutePosition ()->SetCoordinateZ (height);
}

double
LtePhy::GetHeight (void)
{
  return GetDevice ()->GetMobilityModel ()->GetAbsolutePosition ()->GetCoordinateZ ();
}

Interference*
LtePhy::GetInterference (void)
{
  return m_interference;
}


ErrorModel*
LtePhy::GetErrorModel (void)
{
  return m_errorModel;
}

LtePhy::AntennaParameters* LtePhy::GetAntennaParameters (void)
{
  return m_antennaParameters;
}

void
LtePhy::AntennaParameters::SetEtilt(int etilt)
{
  m_etilt = etilt;
}

double
LtePhy::AntennaParameters::GetEtilt(void)
{
  return m_etilt;
}

void
LtePhy::AntennaParameters::SetType(AntennaType t)
{
  m_type = t;
}

LtePhy::AntennaParameters::AntennaType
LtePhy::AntennaParameters::GetType(void)
{
  return m_type;
}

void
LtePhy::AntennaParameters::SetBearing(double bearing)
{
  m_bearing = bearing;
}

double
LtePhy::AntennaParameters::GetBearing(void)
{
  return m_bearing;
}

void
LtePhy::AntennaParameters::SetGain(double gain)
{
  m_gain = gain;
}

double
LtePhy::AntennaParameters::GetGain(void)
{
  return m_gain;
}

void
LtePhy::AntennaParameters::SetHorizontalBeamwidth3db(double beamwidth)
{
  m_horizontalBeamwidth3dB = beamwidth;
}

double
LtePhy::AntennaParameters::GetHorizontalBeamwidth3db(void)
{
  return m_horizontalBeamwidth3dB;
}

void
LtePhy::AntennaParameters::SetVerticalBeamwidth3db(double beamwidth)
{
  m_verticalBeamwidth3dB = beamwidth;
}

double
LtePhy::AntennaParameters::GetVerticalBeamwidth3db(void)
{
  return m_verticalBeamwidth3dB;
}

void
LtePhy::AntennaParameters::SetMaxHorizontalAttenuation(double attenuation)
{
  m_maxHorizontalAttenuation = attenuation;
}

double
LtePhy::AntennaParameters::GetMaxHorizontalAttenuation(void)
{
  return m_maxHorizontalAttenuation;
}

void
LtePhy::AntennaParameters::SetMaxVerticalAttenuation(double attenuation)
{
  m_maxVerticalAttenuation = attenuation;
}

double
LtePhy::AntennaParameters::GetMaxVerticalAttenuation(void)
{
  return m_maxVerticalAttenuation;
}


void
LtePhy::AntennaParameters::SetFeederLoss(double loss)
{
  m_feederLoss = loss;
}

double
LtePhy::AntennaParameters::GetFeederLoss(void)
{
  return m_feederLoss;
}

void
LtePhy::SetNoiseFigure(double nf)
{
  m_noiseFigure = nf;
  /*
   * Noise is computed as follows:
   *  - n0 = -174 dBm
   *  - sub channel bandwidth = 180 kHz
   *
   *  noise_db = noise figure + n0 + 10log10 (180000) - 30
   */
  m_thermalNoise = m_noiseFigure - 174 + 10*log10(180000) - 30;
}

double
LtePhy::GetNoiseFigure(void)
{
  return m_noiseFigure;
}

double
LtePhy::GetThermalNoise(void)
{
  return m_thermalNoise;
}

void
LtePhy::SetAverageBuildingHeight(double height)
{
  m_averageBuildingHeight = height;
}

double
LtePhy::GetAverageBuildingHeight(void)
{
  return m_averageBuildingHeight;
}


LtePhy::WaveformType
LtePhy::GetWaveformType (void)
{
  return m_waveform;
}

void
LtePhy::SetWaveformType (LtePhy::WaveformType w)
{
  m_waveform = w;
}

bool
LtePhy::GetSrtaPi (void)
{
  return m_useSrtaPi;
}

void
LtePhy::SetSrtaPi (bool b)
{
  m_useSrtaPi = b;
}

