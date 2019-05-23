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
 * Author: Sergio Martiradonna <sergio.martiradonna@poliba.it>
 */

#include "../channel/LteChannel.h"
#include "../phy/enb-lte-phy.h"
#include "../phy/ue-lte-phy.h"
#include "../core/spectrum/bandwidth-manager.h"
#include "../networkTopology/Cell.h"
#include "../protocolStack/packet/packet-burst.h"
#include "../protocolStack/packet/Packet.h"
#include "../core/eventScheduler/simulator.h"
#include "../flows/application/InfiniteBuffer.h"
#include "../flows/application/VoIP.h"
#include "../flows/application/CBR.h"
#include "../flows/application/TraceBased.h"
#include "../device/IPClassifier/ClassifierParameters.h"
#include "../flows/QoS/QoSParameters.h"
#include "../flows/QoS/QoSForEXP.h"
#include "../flows/QoS/QoSForFLS.h"
#include "../flows/QoS/QoSForM_LWDF.h"
#include "../componentManagers/FrameManager.h"
#include "../utility/RandomVariable.h"
#include "../channel/propagation-model/channel-realization.h"
#include "../phy/wideband-cqi-eesm-error-model.h"
#include "../phy/simple-error-model.h"
#include "../load-parameters.h"

#include "../device/UserEquipment.h"
#include "../device/NetworkNode.h"
#include "../device/NetworkNode.h"
#include "../device/ENodeB.h"
#include "../device/CqiManager/cqi-manager.h"
#include "../device/CqiManager/fullband-cqi-manager.h"
#include "../device/CqiManager/wideband-cqi-manager.h"
#include "../channel/propagation-model/propagation-loss-model.h"

#include "../protocolStack/mac/random-access/ue-random-access.h"
#include "../protocolStack/mac/random-access/enb-random-access.h"

#include <iostream>
#include <queue>
#include <fstream>
#include <stdlib.h>
#include <cstring>
#include <math.h>


static void nbCell (int argc, char *argv[])
{
  int schedType = atoi(argv[2]);
  double dur = atoi(argv[3]);
  double radius = atof(argv[4]);
  int nbUE = atoi(argv[5]);
  double bandwidth = atof(argv[6]);
  int carriers = atoi(argv[7]);
  double spacing = atof(argv[8]);
  int tones = atoi(argv[9]);
  int CBR_interval = atoi(argv[10]);
  int CBR_size = atoi(argv[11]);
  int seed;
  if (argc==13)
    {
      seed = atoi(argv[12]);
    }
  else
    {
      seed = -1;
    }



  // define simulation times
  double duration = dur;
  double flow_duration = dur;

  UeRandomAccess::RandomAccessType m_UeRandomAccessType = UeRandomAccess::RA_TYPE_NB_IOT;
  EnbRandomAccess::RandomAccessType m_EnbRandomAccessType = EnbRandomAccess::RA_TYPE_NB_IOT;

  // CREATE COMPONENT MANAGER
  Simulator *simulator = Simulator::Init();
  FrameManager *frameManager = FrameManager::Init();
  NetworkManager* networkManager = NetworkManager::Init();

  // CONFIGURE SEED
  if (seed >= 0)
    {
      srand (seed);
    }
  else
    {
      srand (time(NULL));
    }
  cout << "Simulation with SEED = " << seed << endl;
  cout << "Duration: " << duration << " flow: " << flow_duration << endl;



  // SET FRAME STRUCTURE
  frameManager->SetFrameStructure(FrameManager::FRAME_STRUCTURE_FDD);


  //Define Application Container
  CBR CBRApplication[nbUE];
  int cbrApplication = 0;
  int destinationPort = 101;
  int applicationID = 0;


  // CREATE CELL
  Cell *cell = new Cell (0, radius, 0.005, 0, 0);

  networkManager->GetCellContainer ()->push_back (cell);

  // CREATE CHANNELS and propagation loss model
  LteChannel *dlCh = new LteChannel ();
  LteChannel *ulCh = new LteChannel ();

  // CREATE SPECTRUM
  BandwidthManager* spectrum = new BandwidthManager (bandwidth, bandwidth, 0, 0);
  spectrum->CreateNbIoTspectrum(carriers, spacing, tones);


  cout << "TTI Length: ";
  frameManager->setTTILength(tones, spacing);
  cout << frameManager->getTTILength() << "ms " << endl;

DEBUG_LOG_START_1(LTE_SIM_SCHEDULER_DEBUG_NB)
  spectrum->Print();
DEBUG_LOG_END


  ENodeB::ULSchedulerType uplink_scheduler_type;
  switch (schedType)
    {
    case 0:
      uplink_scheduler_type = ENodeB::ULScheduler_TYPE_NB_IOT_FIFO;
      cout << "Scheduler NB FIFO "<< endl;
      break;
    case 1:
      uplink_scheduler_type = ENodeB::ULScheduler_TYPE_NB_IOT_ROUNDROBIN;
      cout << "Scheduler NB RR "<< endl;
      break;
    default:
      uplink_scheduler_type = ENodeB::ULScheduler_TYPE_NB_IOT_FIFO;
      cout << "Scheduler NB FIFO "<< endl;
      break;
    }



  //Create ENodeB
  ENodeB* enb = new ENodeB (1, cell, 0, 0);
  enb->SetRandomAccessType(m_EnbRandomAccessType);
  enb->GetPhy ()->SetDlChannel (dlCh);
  enb->GetPhy ()->SetUlChannel (ulCh);
  enb->GetPhy ()->SetBandwidthManager (spectrum);

  WidebandCqiEesmErrorModel *errorModel = new WidebandCqiEesmErrorModel ();
  enb->GetPhy ()->SetErrorModel (errorModel);

  ulCh->AddDevice (enb);
  enb->SetDLScheduler (ENodeB::DLScheduler_TYPE_PROPORTIONAL_FAIR);
  enb->SetULScheduler(uplink_scheduler_type);
  networkManager->GetENodeBContainer ()->push_back (enb);
  cout << "Created eNB - id 1 position (0;0)"<< endl;

  //Create UEs
  int idUE = 2;
  double speedDirection = 0;

  double posX = 0;
  double posY = 0;

  int nbOfZones;
  if (tones==1)
    {
      nbOfZones=11;
    }
  else
    {
      nbOfZones=14;
    }
  int zone;
  double zoneWidth = radius*1000 / nbOfZones;
  double edges[nbOfZones+1];
  for (int i=0; i <= nbOfZones; i++)
    {
      edges[i]= i*zoneWidth;
    }

  int high, low, sign;
  double distance, random;

  for (int i = 0; i < nbUE; i++)
    {
      zone = (rand() % static_cast<int>(nbOfZones));
      cout << "ZONE " << zone;
      low = edges[nbOfZones - 1 - zone];
      cout << " LOW EDGE " << low;
      random = ((double) rand()) / (double) RAND_MAX;
      random = random * zoneWidth;
      distance = random + (double) low;
      cout << " DISTANCE " << distance;
      cout << endl;

      sign = (rand() % 2) * 2 - 1;
      posX=distance / sqrt(2) * sign;
      sign = (rand() % 2) * 2 - 1;
      posY=distance / sqrt(2) * sign;
      /*
       * double posX = 0;
      double posY = 0;
      do
        {
          posX = (double)rand()/RAND_MAX;
          posX = (double)(0.95 * (((2000*radius)*posX) - (radius*1000)));
          posY = (double)rand()/RAND_MAX;
          posY = (0.95 * (((2*radius*1000)*posY) - (radius*1000)));
        } while (sqrt(pow(posX,2) + pow(posY,2)) > radius*1000);
       */
      UserEquipment* ue = new UserEquipment (idUE,
                                             posX, posY, 3, speedDirection,			// by zyb
                                             cell,
                                             enb,
                                             0, //handover false!
                                             Mobility::CONSTANT_POSITION);

      cout << "Created UE - id " << idUE << " position " << posX << " " << posY << endl;

      ue->SetRandomAccessType(m_UeRandomAccessType);
      ue->GetPhy ()->SetDlChannel (dlCh);
      ue->GetPhy ()->SetUlChannel (ulCh);

      networkManager->GetUserEquipmentContainer ()->push_back (ue);

      // register ue to the enb
      enb->RegisterUserEquipment (ue);


      //enb->GetPhy()->GetUlChannel()->SetPropagationLossModel(NULL);			// by zyb
      ChannelRealization* c_ul = new ChannelRealization (ue, enb, ChannelRealization::CHANNEL_MODEL_MACROCELL_URBAN);	// by zyb
      enb->GetPhy ()->GetUlChannel ()->GetPropagationLossModel ()->AddChannelRealization (c_ul);						// by zyb

DEBUG_LOG_START_1(LTE_SIM_SCHEDULER_DEBUG_LOG)
      CartesianCoordinates* userPosition = ue->GetMobilityModel()->GetAbsolutePosition();
      CartesianCoordinates* cellPosition = cell->GetCellCenterPosition();
      double distance = userPosition->GetDistance(cellPosition)/1000;

      double zoneWidth = (double) (radius/nbOfZones);
      double edges[nbOfZones+1];
      for (int i=0; i <= nbOfZones; i++)
        {
          edges[i]= i*zoneWidth;
        }
      if (distance >= edges[nbOfZones])
        {
          zone = 0;
        }
      for (int i= 0; i < nbOfZones; i++)
        {
          if (distance >= edges[i] && distance <= edges[i+1])
            {
              zone=nbOfZones-1-i;
            }
        }
      cout << "LOG_ZONE UE " << idUE
           << " DISTANCE " << distance
           << " WIDTH " << zoneWidth
           << " ZONE " << zone
           << endl;
DEBUG_LOG_END

      //CREATE UPLINK APPLICATION FOR THIS UE
      double start_time = 1 + (double)rand()/RAND_MAX * CBR_interval;
      //double start_time = CBR_interval;
      //double start_time = 1+  (rand() % static_cast<int>(CBR_interval));
      double duration_time = start_time + flow_duration;

      // *** cbr application
      // create application
      CBRApplication[cbrApplication].SetSource (ue);
      CBRApplication[cbrApplication].SetDestination (enb);
      CBRApplication[cbrApplication].SetApplicationID (applicationID);
      CBRApplication[cbrApplication].SetStartTime(start_time);
      CBRApplication[cbrApplication].SetStopTime(duration_time);

      CBRApplication[cbrApplication].SetInterval ((double) CBR_interval);
      CBRApplication[cbrApplication].SetSize (CBR_size);

//------------------------------------------------------------------------------------------------------------- create qos parameters????

      QoSParameters *qosParameters = new QoSParameters ();
      qosParameters->SetMaxDelay (flow_duration);
      CBRApplication[cbrApplication].SetQoSParameters (qosParameters);


      //create classifier parameters
      ClassifierParameters *cp = new ClassifierParameters (ue->GetIDNetworkNode(),
                                                           enb->GetIDNetworkNode(),
                                                           0,
                                                           destinationPort,
                                                           TransportProtocol::TRANSPORT_PROTOCOL_TYPE_UDP);
      CBRApplication[cbrApplication].SetClassifierParameters (cp);

      cout << "CREATED CBR APPLICATION, ID " << applicationID << endl;

      //update counter
      //destinationPort++;
      applicationID++;
      cbrApplication++;
      idUE++;
    }

  simulator->SetStop(duration);
  simulator->Run ();
}
