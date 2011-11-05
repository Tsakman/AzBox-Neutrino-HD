#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/types.h>

#include <linux/dvb/ca.h>
#include <poll.h>
 
#include <list>
#include <queue>

#include "dvb-ci.h"
#include "dvbci_session.h"
#include "dvbci_appmgr.h"
#include "dvbci_camgr.h"

#include "dvbci_mmi.h"

#include <neutrinoMessages.h>
#include <driver/rcinput.h>

#include <connection/messagetools.h>   /* get_length_field_size */
#include <zapit/include/zapit/ci.h>

static const char * FILENAME = "dvb-ci.cpp";

#define TAG_MENU_ANSWER                      0x9f880b
#define TAG_ENTER_MENU                       0x9f8022

extern CRCInput *g_RCInput;

bool cDvbCi::checkQueueSize(tSlot* slot)
{
	return (slot->sendqueue.size() > 0);
}

void cDvbCi::CI_MenuAnswer(unsigned char bSlotIndex,unsigned char choice)
{
	printf("%s:%s: %d %c\n", FILENAME, __FUNCTION__, bSlotIndex, choice);

	std::list<tSlot*>::iterator it;
	
        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
            if ((*it)->slot == bSlotIndex) 
	    {
	       if ((*it)->hasMMIManager)
                  (*it)->mmiSession->answerText((int) choice);
	    }
	}

}

void cDvbCi::CI_Answer(unsigned char bSlotIndex,unsigned char *pBuffer,unsigned char nLength)
{
	printf("%s:%s: %d\n", FILENAME, __FUNCTION__, bSlotIndex);

	std::list<tSlot*>::iterator it;
	
//fixme: currently not tested
	
        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
            if ((*it)->slot == bSlotIndex) 
	    {
	       if ((*it)->hasMMIManager)
                  (*it)->mmiSession->answerEnq((char*) pBuffer, nLength);

	       break;
	    }
	}
}

void cDvbCi::CI_CloseMMI(unsigned char bSlotIndex)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__, bSlotIndex);

	std::list<tSlot*>::iterator it;
	

        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
            if ((*it)->slot == bSlotIndex) 
	    {
	       if ((*it)->hasMMIManager)
                  (*it)->mmiSession->stopMMI();
	       break;
	    }
	}
}

void cDvbCi::CI_EnterMenu(unsigned char bSlotIndex)
{
	printf("%s:%s %d\n", FILENAME, __FUNCTION__, bSlotIndex);

	std::list<tSlot*>::iterator it;
	
        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
	    if ((strstr((*it)->name, "unknown module") != NULL) &&
	        ((*it)->slot == bSlotIndex))
	    {
	       //the module has no real name, this is the matter if something while initializing went wrong
	       //so let this take as a reset action for the module so we do not need to add a reset
	       //feature to the neutrino menu
	       reset(bSlotIndex);

	       return;
	    }

            if ((*it)->slot == bSlotIndex) 
	    {
	       if ((*it)->hasAppManager)
                  (*it)->appSession->startMMI();

	       break;
	    }
	}
}


/* from dvb-apps
 */
int asn_1_decode(uint16_t * length, unsigned char * asn_1_array,
		 uint32_t asn_1_array_len)
{
	uint8_t length_field;

	if (asn_1_array_len < 1)
		return -1;
	length_field = asn_1_array[0];

	if (length_field < 0x80) {
		// there is only one word
		*length = length_field & 0x7f;
		return 1;
	} else if (length_field == 0x81) {
		if (asn_1_array_len < 2)
			return -1;

		*length = asn_1_array[1];
		return 2;
	} else if (length_field == 0x82) {
		if (asn_1_array_len < 3)
			return -1;

		*length = (asn_1_array[1] << 8) | asn_1_array[2];
		return 3;
	}

	return -1;
}


//wait for a while for some data und read it if some
eData waitData(int fd, unsigned char* buffer, int* len)
{
      int        retval;
      struct      pollfd fds;

      //printf("%s: %d\n", __func__, *len);
      fds.fd = fd;
      fds.events = POLLOUT | POLLPRI | POLLIN;
      
      retval = poll(&fds, 1, 100);

      if (retval < 0)
      {
         printf("data error\n");
         return eDataError;
      }
      else
      if (retval == 0)
      {
         return eDataTimeout;
      }
      else
      if (retval > 0)
      {
          if (fds.revents & POLLIN)
	  { 
	     int n = read (fd, buffer, *len);
          
	     if (n > 0)
	     {
	        *len = n;
                return eDataReady;
             }
             *len = 0;
	     return eDataError;
	  } else
          if (fds.revents & POLLOUT)
	  { 
	      return eDataWrite;
	  } else
          if (fds.revents & POLLPRI)
	  { 
	      return eDataStatusChanged;
	  }
      }

      return eDataError;
      
}

//send some data on an fd, for a special slot and connection_id
eData sendData(tSlot* slot, unsigned char* data, int len)
{
#ifdef x_debug	
        printf("%s: %p, %d\n", __func__, data, len);
#endif
       
	unsigned char *d = (unsigned char*) malloc(len + 5);
		
	// only poll connection if we are not awaiting an answer
	slot->pollConnection = false;	
		
	/* should we send a data last ?
	   */
	if (data != NULL)
	{
	   if ((data[2] >= T_SB) && (data[2] <= T_NEW_T_C))
	   {
	      memcpy(d, data, len);
	   } else
	   {
	      //send data_last and data
	      memcpy(d + 5, data, len);

	      d[0] = slot->slot;
	      d[1] = slot->connection_id;
	      d[2] = T_DATA_LAST; 	
	      d[3] = len + 1; 		/* len */
	      d[4] = slot->connection_id; 	/* transport connection identifier*/

	      len += 5;	
	   }
	}
	else
	{
	      //send a data last only
	      d[0] = slot->slot;
	      d[1] = slot->connection_id;
	      d[2] = T_DATA_LAST; 	
	      d[3] = len + 1; 		/* len */
	      d[4] = slot->connection_id; 	/* transport connection identifier*/

	      len = 5;	
	}

#ifdef x_debug
	printf("write (%d): > ", slot->slot);
	for(int i=0; i < len; i++)
	   printf("%02x ",d[i]);
	printf("\n");
#endif

#ifdef direct_write
	res = write(slot->fd, d, len); 

	free(d);
	if (res < 0 || res != len) 
	{ 
            printf("error writing data to fd %d, slot %d: %m\n", slot->fd, slot->slot);
            return eDataError; 
	}
#else
	slot->sendqueue.push( queueData(d, len) );
#endif	 
	
	return eDataReady;
}

//send a transport connection create request
bool sendCreateTC(tSlot* slot)
{
   //printf("%s:%s >\n", FILENAME, __FUNCTION__);
   unsigned char* data = (unsigned char*) malloc(sizeof(char) * 5);
   
   data[0] = slot->slot;
   data[1] = slot->slot + 1; 	/* conid */
   data[2] = T_CREATE_T_C;
   data[3] = 1;
   data[4] = slot->slot + 1 	/*conid*/;

   sendData(slot, data, 5);

   //printf("%s:%s <\n", FILENAME, __FUNCTION__);
   return true;
}

void cDvbCi::process_tpdu(tSlot* slot, unsigned char tpdu_tag, __u8* data, int asn_data_length, int con_id)
{
      switch (tpdu_tag) 
      {
	 case T_C_T_C_REPLY:
	      printf("Got CTC Replay (slot %d, con %d)\n", slot->slot, slot->connection_id);

	      //answer with data last (and if we have with data)
	      sendData(slot, NULL, 0);
	      
	      break;
	 case T_DELETE_T_C:
//FIXME: close sessions etc; slot->reset ?
//we must answer here with t_c_replay

	      printf("Got \"Delete Transport Connection\" from module ->currently not handled!\n");
	      
	      break;
	 case T_D_T_C_REPLY:

	      printf("Got \"Delete Transport Connection Replay\" from module!\n");
	      break;

	 case T_REQUEST_T_C:

	      printf("Got \"Request Transport Connection\" from Module ->currently not handled!\n");

	      break;
	 case T_DATA_MORE:
	 {
		int new_data_length = slot->receivedLen + asn_data_length;

	        printf("Got \"Data More\" from Module\n");

		__u8 *new_data_buffer = (__u8*)
	    		realloc(slot->receivedData, new_data_length);
	
		slot->receivedData = new_data_buffer;

		memcpy(slot->receivedData + slot->receivedLen, data, asn_data_length);
		
		slot->receivedLen = new_data_length;
					
	      break;
	 }
	 case T_DATA_LAST:
	 
#ifdef x_debug	
	        printf("Got \"Data Last\" from Module\n");
#endif
		
		/* single package */
	        if (slot->receivedData == NULL) 
		{

	        	printf("->single package\n");
#ifdef x_debug	
			printf("calling receiveData with data (len %d)> ", asn_data_length);
			for(int i = 0;i < asn_data_length; i++)
				printf("%02x ", data[i]);
                        printf("\n");
#endif

			eDVBCISession::receiveData(slot, data, asn_data_length);
			eDVBCISession::pollAll();
		} else 
		{
			/* chained package */
			int new_data_length = slot->receivedLen + asn_data_length;

	        	printf("->chained data\n");

			__u8 *new_data_buffer =
	    			(__u8*) realloc(slot->receivedData, new_data_length);
	
			slot->receivedData = new_data_buffer;

			memcpy(slot->receivedData + slot->receivedLen, data, asn_data_length);
		
			slot->receivedLen = new_data_length;
					
#ifdef x_debug	
			printf("calling receiveData with data (len %d)> ", asn_data_length);
			for(int i = 0;i < slot->receivedLen; i++)
				printf("%02x ", slot->receivedData[i]);
                        printf("\n");
#endif

			eDVBCISession::receiveData(slot, slot->receivedData, slot->receivedLen);
			eDVBCISession::pollAll();

//fixme: must also be moved in e2 behind the data processing ;) 

			free(slot->receivedData);
			slot->receivedData = NULL;
			slot->receivedLen = 0;
		}
	      break;
	 case T_SB:
	 {
#ifdef x_debug	
	      printf("Got \"SB\" from Module\n");
#endif

	      if (data[0] & 0x80)
	      {
	      	printf("->data ready (%d)\n", slot->slot);
	
		//we now wait for an answer so dont poll
		slot->pollConnection = false;

		// send the RCV and ask for the data
		unsigned char send_data[5];

		send_data[0] = slot->slot;
		send_data[1] = slot->connection_id;
		send_data[2] = T_RCV;
		send_data[3] = 1;
		send_data[4] = slot->connection_id;

		write(slot->fd, send_data, 5);
	      } else
	      {
	        //if the queue is not empty we dont need to send
		//a polling to the module.
		//if (checkQueueSize(slot) == false)
		//    slot->pollConnection = true;
	      }
	      break;
	 }
	 default:
	      printf("unhandled tpdu_tag 0x%0x\n", tpdu_tag);
      }
}


void cDvbCi::slot_pollthread(void *c)
{
	ca_slot_info_t info;
	unsigned char data[1024];
	tSlot* slot = (tSlot*) c;
	
	while (1)
	{
            int len = 1024;
            unsigned char* d;
            eData status;
		    
	    switch (slot->status)
	    {
	        case eStatusNone:
		{
		   if (slot->camIsReady)
		   {
		      if (sendCreateTC(slot))
		      {
			  slot->status = eStatusWait;
			  slot->camIsReady = true;
		      } else
		      {
			  usleep(100000);
		      }
                   } else
		   {
		       /* wait for pollpri */
		       status = waitData(slot->fd, data, &len);
		       if (status == eDataStatusChanged)
		       {
                          info.num = slot->slot;

		          if (ioctl(slot->fd, CA_GET_SLOT_INFO, &info) < 0)
			     printf("IOCTL CA_GET_SLOT_INFO failed for slot %d\n", slot->slot);

                          printf("flags %d %d %d ->slot %d\n", info.flags, CA_CI_MODULE_READY, info.flags & CA_CI_MODULE_READY, slot->slot);

	                  if (info.flags & CA_CI_MODULE_READY)
			  {
		              printf("1. cam (%d) status changed ->cam now present\n", slot->slot);

		              slot->mmiSession = NULL;
		              slot->hasMMIManager = false;
		              slot->hasCAManager = false;
		              slot->hasDateTime = false;
		              slot->hasAppManager = false;

	                      slot->mmiOpened = false;

	                      slot->init = false;

        	              sprintf(slot->name, "unknown module %d", slot->slot);

		              slot->status = eStatusNone;

                              if (g_RCInput)
                        	g_RCInput->postMsg(NeutrinoMessages::EVT_CI_INSERTED, slot->slot);

		              slot->camIsReady = true;
                	  } else
			  {
                             //noop
                          }
		       }
		   }
		} /* case statusnone */
		break;
		case eStatusWait:
                {    
		    status = waitData(slot->fd, data, &len);
		    if (status == eDataReady)
		    {
		       //int s_id = data[0];
		       //int c_id = data[1];

		       slot->pollConnection = false;
		       
		       //printf("%d: s_id = %d, c_id = %d\n", slot->slot, s_id, c_id);
                       
		       d = data;

		       /* taken from the dvb-apps */
		       int data_length = len - 2;
		       d += 2; /* remove leading slot and connection id */
		       while (data_length > 0)
		       {
			       unsigned char tpdu_tag = d[0];
			       unsigned short asn_data_length;
			       int length_field_len;

			       if ((length_field_len = asn_1_decode(&asn_data_length, d + 1, data_length - 1)) < 0) 
			       {
				       printf("Received data with invalid asn from module on slot %02x\n", slot->slot);
				       break;
			       }

			       if ((asn_data_length < 1) ||
		    		   (asn_data_length > (data_length - (1 + length_field_len)))) 
			       {
				       printf("Received data with invalid length from module on slot %02x\n", slot->slot);
				       break;
			       }

			       slot->connection_id = d[1 + length_field_len];

			       //printf("Setting connection_id from received data to %d\n", slot->connection_id);

			       d += 1 + length_field_len + 1;
			       data_length -= (1 + length_field_len + 1);
			       asn_data_length--;

			       process_tpdu(slot, tpdu_tag, d, asn_data_length, slot->connection_id);

			       // skip over the consumed data
			       d += asn_data_length;
			       data_length -= asn_data_length;

		       } // while (data_length)
		    } /* data ready */
		    else if (status == eDataWrite)
		    {
		        if (!slot->sendqueue.empty()) 
			{
			     const queueData &qe = slot->sendqueue.top();
			     
			     int res = write(slot->fd, qe.data, qe.len);
			     if (res >= 0 && (unsigned int)res == qe.len)
			     {
				delete [] qe.data;
				slot->sendqueue.pop();
			     }
			     else
			     {
				printf("r = %d, %m\n", res);
			     }			
		         }
		         else
		         {
		              //printf("sendqueue emtpy\n");
		              if ((checkQueueSize(slot) == false) && ((!slot->hasCAManager) || (slot->mmiOpened)))
				 slot->pollConnection = true;
		         }
		    }
		    else if (status == eDataStatusChanged)
		    {
                       info.num = slot->slot;

		       if (ioctl(slot->fd, CA_GET_SLOT_INFO, &info) < 0)
			  printf("IOCTL CA_GET_SLOT_INFO failed for slot %d\n", slot->slot);

                       printf("flags %d %d %d ->slot %d\n", info.flags, CA_CI_MODULE_READY, info.flags & CA_CI_MODULE_READY, slot->slot);

	               if ((slot->camIsReady == false) && (info.flags & CA_CI_MODULE_READY))
		       {
		           printf("2. cam (%d) status changed ->cam now present\n", slot->slot);

		           slot->mmiSession = NULL;
		           slot->hasMMIManager = false;
		           slot->hasCAManager = false;
		           slot->hasDateTime = false;
		           slot->hasAppManager = false;

	                   slot->mmiOpened = false;

	                   slot->init = false;

        	           sprintf(slot->name, "unknown module %d", slot->slot);

		           slot->status = eStatusNone;

                           if (g_RCInput)
                             g_RCInput->postMsg(NeutrinoMessages::EVT_CI_INSERTED, slot->slot);

		           slot->camIsReady = true;
                       } else
	               if ((slot->camIsReady == true) && (!(info.flags & CA_CI_MODULE_READY)))
		       {
		             printf("cam (%d) status changed ->cam now _not_ present\n", slot->slot);

	 	             eDVBCISession::deleteSessions(slot);

		             slot->mmiSession = NULL;
		             slot->hasMMIManager = false;
		             slot->hasCAManager = false;
		             slot->hasDateTime = false;
		             slot->hasAppManager = false;

	                     slot->mmiOpened = false;

	                     slot->init = false;

        	             sprintf(slot->name, "unknown module %d", slot->slot);

			     slot->status = eStatusNone;

                             if (g_RCInput)
                        	g_RCInput->postMsg(NeutrinoMessages::EVT_CI_REMOVED, slot->slot);

	                     while(slot->sendqueue.size())
	                     {
		        	delete [] slot->sendqueue.top().data;
		        	slot->sendqueue.pop();
	                     }

		             slot->camIsReady = false;
		          usleep(100000);		
                       }
		    }

                    if (!checkQueueSize(slot) && slot->pollConnection)
		    {
                        //printf("poll\n");
		        sendData(slot, NULL, 0);
                    }
		}
		break;
		default:
                   printf("unknown state %d\n", slot->status);		
		break;
	    }
	   
	    if (slot->hasCAManager && slot->hasAppManager && !slot->init) //declare this as init, but remeber we are still not complete!
	    {
	         slot->init = true;
	         if (g_RCInput)
                       g_RCInput->postMsg(NeutrinoMessages::EVT_CI_INIT_OK, slot->slot);
	    
	         //resend a capmt if we have one. this is not very proper but I cant any mechanism in
		 //neutrino currently. so if a cam is inserted a pmt is not resend
		 
		 if (slot->caPmt != NULL)
		 {
		     SendCaPMT(slot->caPmt);
		 }
	    }
	}
}

/* helper function to call the cpp thread loop */
void* execute_thread(void *c)
{
   tSlot* slot = (tSlot*) c;
   cDvbCi *obj=(cDvbCi*)slot->pClass;

   obj->slot_pollthread(c);

   return NULL;
}

/* ********************************** */

bool cDvbCi::Init(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
        return true;
}

bool cDvbCi::SendPMT(unsigned char *data, int len)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

//Dagobert: noop here, we need do deal with capmt rather then
//with raw pmt
        return true;
}

bool cDvbCi::SendCaPMT(CCaPmt *caPmt)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	std::list<tSlot*>::iterator it;

        for(it = slot_data.begin(); it != slot_data.end(); it++)
        {
//fixme: ask camagr if module can handle this caids
	       if (((*it)->fd > 0) && ((*it)->camIsReady)) 
	       {
	           unsigned int size = caPmt->getLength();
	           unsigned char buffer[3 + get_length_field_size(size) + size];
	           
                   printf(" %d, %d\n", get_length_field_size(size), size);
		   int len = caPmt->writeToBuffer(buffer, 0, 0xff);

         	   printf("capmt(%d): > ", len);
	
#ifdef x_debug
		   for(int i=0; i < len; i++)
	               printf("%02x ",buffer[i]);
	           printf("\n");
#endif

                   if ((*it)->hasCAManager)
                      (*it)->camgrSession->sendSPDU(0x90, 0, 0, buffer, len);

//fixme: hmmm is this a copy or did I only save the pointer.
//in copy case I must do some delete etc before if set and in destruct case
                   (*it)->caPmt = caPmt;
               }
	}
        return true;
}

bool cDvbCi::SendDateTime(void)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

        return false;
}

//
cDvbCi::cDvbCi(int Slots) {
	printf("%s:%s %d\n", FILENAME, __FUNCTION__, Slots);

	int fd, i;
	char filename[128];

	for (i = 0; i < Slots; i++)
	{
	    sprintf(filename, "/dev/dvb/adapter0/ci%d", i);
	    fd = open(filename, O_RDWR | O_NONBLOCK);
	    
	    if (fd < 0)
	    {
                printf("failed to open %s ->%m", filename);
	    } 
	    	
	    tSlot* slot = (tSlot*) malloc(sizeof(tSlot));

	    slot->slot   = i;
	    slot->fd     = fd;
            slot->connection_id = 0;
	    slot->status = eStatusNone;
	    slot->receivedLen = 0;
	    slot->receivedData = NULL;
	    slot->pClass = this;
	    slot->pollConnection = false;
	    slot->camIsReady = false;

	    slot->hasMMIManager = false;
	    slot->hasCAManager = false;
	    slot->hasDateTime = false;
	    slot->hasAppManager = false;

	    slot->mmiOpened = false;

	    slot->init = false;
 
            slot->caPmt = NULL;

            sprintf(slot->name, "unknown module %d", i);

	    slot_data.push_back(slot);
            /* now reset the slot so the poll pri can happen in the thread */
            reset(i); 

	    /* create a thread for each slot */
	    if (fd > 0) 
	    {
	       
	       if (pthread_create(&slot->slot_thread, 0, execute_thread,  (void*)slot)) 
	       {
		  printf("pthread_create");
	       }
	    }
	}
}

cDvbCi::~cDvbCi()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
}


static cDvbCi* pDvbCiInstance = NULL;

cDvbCi * cDvbCi::getInstance()
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);
	
	if (pDvbCiInstance == NULL)
		pDvbCiInstance = new cDvbCi(2);
	
	return pDvbCiInstance;
}

bool cDvbCi::CamPresent(int slot)
{

#ifdef x_debug
	printf("%s:%s(slot %d)\n", FILENAME, __FUNCTION__, slot);
#endif

	std::list<tSlot*>::iterator it;
	
        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
            if ((*it)->slot == slot) 
	    {
	       return (*it)->camIsReady;
	       break;
	    }
	}
	
	return false; 
}

bool cDvbCi::GetName(int slot, char * name)
{
	printf("%s:%s\n", FILENAME, __FUNCTION__);

	std::list<tSlot*>::iterator it;
	
        for(it = slot_data.begin(); it != slot_data.end(); ++it)
        {
	    //printf("%d. name = %s, %p\n", (*it)->slot, (*it)->name, (*it));
            if ((*it)->slot == slot) 
	    {
	       strcpy(name, (*it)->name);
	       break;
	    }
	}
	
	return true;
}

void cDvbCi::reset(int slot)
{
     std::list<tSlot*>::iterator it;
     bool haveFound = false;

     for(it = slot_data.begin(); it != slot_data.end(); ++it)
     {
         if ((*it)->slot == slot) 
	 {
	    haveFound = true;
	    break;
	 }
     }

     if (haveFound)
     {
	if (ioctl((*it)->fd, CA_RESET, (*it)->slot) < 0)
		printf("IOCTL CA_RESET failed for slot %d\n", slot);
     }
}
