#ifndef __DVBCI_H
#define __DVBCI_H

#include <asm/types.h>

#include <pthread.h>

#include <queue>

#include <list>

#include <zapit/include/zapit/ci.h>

#define MAX_MMI_ITEMS 20
#define MAX_MMI_TEXT_LEN 255
#define MAX_MMI_CHOICE_TEXT_LEN 255

typedef struct
{
        int slot;
        int choice_nb;
        char title[MAX_MMI_TEXT_LEN];
        char subtitle[MAX_MMI_TEXT_LEN];
        char bottom[MAX_MMI_TEXT_LEN];
        char choice_item[MAX_MMI_ITEMS][MAX_MMI_CHOICE_TEXT_LEN];
} MMI_MENU_LIST_INFO;

typedef struct
{
        int slot;
        int blind;
        int answerlen;
        char enguiryText[MAX_MMI_TEXT_LEN];

} MMI_ENGUIRY_INFO;

/* ********************************** */
/* constants taken from dvb-apps 
 */
#define T_SB                0x80	// sb                           primitive   h<--m
#define T_RCV               0x81	// receive                      primitive   h-->m
#define T_CREATE_T_C        0x82	// create transport connection  primitive   h-->m
#define T_C_T_C_REPLY       0x83	// ctc reply                    primitive   h<--m
#define T_DELETE_T_C        0x84	// delete tc                    primitive   h<->m
#define T_D_T_C_REPLY       0x85	// dtc reply                    primitive   h<->m
#define T_REQUEST_T_C       0x86	// request transport connection primitive   h<--m
#define T_NEW_T_C           0x87	// new tc / reply to t_request  primitive   h-->m
#define T_T_C_ERROR         0x77	// error creating tc            primitive   h-->m
#define T_DATA_LAST         0xA0	// convey data from higher      constructed h<->m
				 // layers
#define T_DATA_MORE         0xA1	// convey data from higher      constructed h<->m
				 // layers

typedef enum {eDataTimeout, eDataError, eDataReady, eDataWrite, eDataStatusChanged} eData;

typedef enum {eStatusNone, eStatusWait} eStatus;

struct queueData
{
	__u8 prio;
	unsigned char *data;
	unsigned int len;
	queueData( unsigned char *data, unsigned int len, __u8 prio = 0 )
		:prio(prio), data(data), len(len)
	{

	}
	bool operator < ( const struct queueData &a ) const
	{
		return prio < a.prio;
	}
};

class eDVBCIMMISession;
class eDVBCIApplicationManagerSession;
class eDVBCICAManagerSession;

typedef struct
{
        pthread_t   slot_thread;
	int          slot;
	int          fd;
	int          connection_id;
        eStatus     status;  

        int          receivedLen;
	unsigned char* receivedData;
	
	void*        pClass;
        
	bool        pollConnection;
	bool        camIsReady;
	
	eDVBCIMMISession* mmiSession;
	eDVBCIApplicationManagerSession* appSession;
	eDVBCICAManagerSession* camgrSession;
	
	bool	    hasAppManager;
	bool        hasMMIManager;
	bool        hasCAManager;
	bool        hasDateTime;
	
	bool        mmiOpened;

	char         name[512];

        bool        init;
 
	std::priority_queue<queueData> sendqueue;

        CCaPmt      *caPmt;

} tSlot;


eData sendData(tSlot *slot, unsigned char* data, int len);


typedef void (*SEND_MSG_HOOK) (unsigned int msg, unsigned int data);

class cDvbCi {
	private:
		int slots;
		bool init;
		int pmtlen;
		unsigned char * pmtbuf;
		void SendPMT();
		pthread_mutex_t ciMutex;

#if defined __sh__ || AZBOX_GEN_1
	        std::list<tSlot*> slot_data;
                pthread_t     slot_thread;
#endif
	public:
		bool Init(void);
		bool SendPMT(unsigned char *data, int len);
                bool SendCaPMT(CCaPmt *caPmt);

		bool SendDateTime(void);
#if defined __sh__ || AZBOX_GEN_1
                void slot_pollthread(void *c);
#endif
 
		//
		cDvbCi(int Slots);
		~cDvbCi();
		static cDvbCi * getInstance();
		SEND_MSG_HOOK SendMessage;
		void SetHook(SEND_MSG_HOOK _SendMessage) { SendMessage = _SendMessage; };
		bool CamPresent(int slot);
		bool GetName(int slot, char * name);

                void CI_MenuAnswer(unsigned char bSlotIndex,unsigned char choice);
                void CI_Answer(unsigned char bSlotIndex,unsigned char *pBuffer,unsigned char nLength);
                void CI_CloseMMI(unsigned char bSlotIndex);
                void CI_EnterMenu(unsigned char bSlotIndex);
                bool checkQueueSize(tSlot* slot);
                void process_tpdu(tSlot* slot, unsigned char tpdu_tag, __u8* data, int asn_data_length, int con_id);
                void reset(int slot);
};

#endif //__DVBCI_H
