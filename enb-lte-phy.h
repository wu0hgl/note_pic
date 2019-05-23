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
 */


#ifndef ENB_LTE_PHY_H_
#define ENB_LTE_PHY_H_

#include "lte-phy.h"

class IdealControlMessage;
class ENodeB;

class EnbLtePhy :public LtePhy
{
public:
  EnbLtePhy();
  virtual ~EnbLtePhy();

  virtual void DoSetBandwidthManager (void);

  virtual void StartTx (shared_ptr<PacketBurst> p);
  virtual void StartRx (shared_ptr<PacketBurst> p, TransmittedSignal* txSignal);
  virtual void StartRx (shared_ptr<PacketBurst> p, TransmittedSignal* txSignal, NetworkNode* src);	// by zyb

  virtual void SendIdealControlMessage (IdealControlMessage *msg);
  virtual void ReceiveIdealControlMessage (IdealControlMessage *msg);

  void ReceiveReferenceSymbols (NetworkNode* n, TransmittedSignal* s);

  ENodeB* GetDevice(void);
private:
  vector<int> m_mcsIndexForRx;

};

#endif /* ENB_LTE_PHY_H_ */
