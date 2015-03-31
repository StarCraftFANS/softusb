/* Name: main.c

 */

#define F_CPU   12000000L    /* evaluation board runs on 4MHz */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>

#include "usbdrv.h"
#include "oddebug.h"
#include "autokey.h"

/* ----------------------- hardware I/O abstraction ------------------------ */

/* pin assignments:
PB0	Key 1
PB1	Key 2
PB2	Key 3
PB3	Key 4
PB4	Key 5
PB5 Key 6

PC0	Key 7
PC1	Key 8
PC2	Key 9
PC3	Key 10
PC4	Key 11
PC5	Key 12

PD0	USB-
PD1	debug tx
PD2	USB+ (int0)
PD3	Key 13
PD4	Key 14
PD5	Key 15
PD6	Key 16
PD7	Key 17
*/

static void hardwareInit(void)
{
uchar	i, j;

    PORTB = 0xff;   /* activate all pull-ups */
    DDRB = 0;       /* all pins input */
    PORTC = 0xff;   /* activate all pull-ups */
    DDRC = 0;       /* all pins input */
    PORTD = 0xfa;   /* 1111 1010 bin: activate pull-ups except on USB lines */
    DDRD = 0x07;    /* 0000 0111 bin: all pins input except USB (-> USB reset) */
	j = 0;
	while(--j){     /* USB Reset by device only required on Watchdog Reset */
		i = 0;
		while(--i); /* delay >10ms for USB reset */
	}
    DDRD = 0x02;    /* 0000 0010 bin: remove USB reset condition */
    /* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
	TCNT0 = 21;     // 20 ms
    TCCR0B = 5;      /* timer 0 prescaler: 1024 */
	TCCR0A =0;
}

/* ------------------------------------------------------------------------- */

#define NUM_KEYS    17

/* The following function returns an index for the first key pressed. It
 * returns 0 if no key is pressed.
 */
static uchar    keyPressed(void)
{
uchar   i, mask, x;

    x = PINC;
    mask = 1;
    for(i=0;i<6;i++){
        if((x & mask) == 0)
            return i + 1;
        mask <<= 1;
    }
    x = PINB;
    mask = 1;
    for(i=0;i<6;i++){
        if((x & mask) == 0)
            return i + 7;
        mask <<= 1;
    }
    x = PIND;
    mask = 1 << 3;
    for(i=0;i<5;i++){
        if((x & mask) == 0)
            return i + 13;
        mask <<= 1;
    }
    return 0;
}


const unsigned char ee_zero[] __attribute__((section(".eeprom"))) = {0,0,0,0,0,0};


/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

static uchar    reportBuffer[2];    /* buffer for HID reports */
static uchar    idleRate;           /* in 4 ms units */

PROGMEM char usbHidReportDescriptor[35] = { /* USB report descriptor */
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};
/* We use a simplifed keyboard report descriptor which does not support the
 * boot protocol. We don't allow setting status LEDs and we only allow one
 * simultaneous key press (except modifiers). We can therefore use short
 * 2 byte input reports.
 * The report descriptor has been created with usb.org's "HID Descriptor Tool"
 * which can be downloaded from http://www.usb.org/developers/hidpage/.
 * Redundant entries (such as LOGICAL_MINIMUM and USAGE_PAGE) have been omitted
 * for the second INPUT item.
 */

/* Keyboard usage values, see usb.org's HID-usage-tables document, chapter
 * 10 Keyboard/Keypad Page for more codes.
 */
#define MOD_CONTROL_LEFT    (1<<0)
#define MOD_SHIFT_LEFT      (1<<1)
#define MOD_ALT_LEFT        (1<<2)
#define MOD_GUI_LEFT        (1<<3)
#define MOD_CONTROL_RIGHT   (1<<4)
#define MOD_SHIFT_RIGHT     (1<<5)
#define MOD_ALT_RIGHT       (1<<6)
#define MOD_GUI_RIGHT       (1<<7)

#define KEY_A       4
#define KEY_B       5
#define KEY_C       6
#define KEY_D       7
#define KEY_E       8
#define KEY_F       9
#define KEY_G       10
#define KEY_H       11
#define KEY_I       12
#define KEY_J       13
#define KEY_K       14
#define KEY_L       15
#define KEY_M       16
#define KEY_N       17
#define KEY_O       18
#define KEY_P       19
#define KEY_Q       20
#define KEY_R       21
#define KEY_S       22
#define KEY_T       23
#define KEY_U       24
#define KEY_V       25
#define KEY_W       26
#define KEY_X       27
#define KEY_Y       28
#define KEY_Z       29
#define KEY_1       30
#define KEY_2       31
#define KEY_3       32
#define KEY_4       33
#define KEY_5       34
#define KEY_6       35
#define KEY_7       36
#define KEY_8       37
#define KEY_9       38
#define KEY_0       39

#define KEY_F1      58
#define KEY_F2      59
#define KEY_F3      60
#define KEY_F4      61
#define KEY_F5      62
#define KEY_F6      63
#define KEY_F7      64
#define KEY_F8      65
#define KEY_F9      66
#define KEY_F10     67
#define KEY_F11     68
#define KEY_F12     69



static void buildReport(uchar key)
{
/* This (not so elegant) cast saves us 10 bytes of program memory */
//    *(int *)reportBuffer = pgm_read_word(keyReport[key]);
reportBuffer[0]=0;
reportBuffer[1]=key;
}


uchar   key, lastKey = 0, keyDidChange = 0;
uchar   idleCounter = 0;


//static const uchar faketime[7]={23,23,46,69,92,114,138,};
static const uchar faketime[7]={10,10,20,30,40,50,60,};
KEYDATA_T Key_list[64];
uchar k_ist_length;
uchar current_key_i;
uchar m_count=0;
uchar out_key;
uchar commontime;
uchar auto_key;

uchar record_key;
uchar span_index;




uchar KeyOut()
{
	if(auto_key!=0)
	{
		if(m_count>=Key_list[current_key_i].time)
		{
		out_key = Key_list[current_key_i].key;
		current_key_i++;
		if(current_key_i>=k_ist_length){current_key_i=0;}
		m_count=0;
		keyDidChange=1;
		}else{out_key=0;keyDidChange=1;}
	m_count++;
	}
	return out_key;
}



void init_autokey()
{
    k_ist_length= 0;
	eeprom_busy_wait(); 
	k_ist_length=eeprom_read_byte(0);
	eeprom_busy_wait(); 
	uint16_t tmp=0;


	if(k_ist_length!=0) 
	{
		if(k_ist_length<0x48) // progisp 写 eeprom有问题，暂时不初始化，在程序里判定
		{
			uchar i=0;
				for(;i<k_ist_length;i++)
				{
					eeprom_busy_wait();
					tmp = eeprom_read_word(2+i*2);
					eeprom_busy_wait();
					copy_word(&Key_list[i],&tmp);
	
				}
		}else
		{
		k_ist_length=0;
		}
	}

	current_key_i=0;
	m_count=0;
	out_key=0;
	commontime=1;
	auto_key=0;
	
	record_key=0x04;
	
	span_index=0;
	
	
}

void clear()
{
	auto_key=0;
	k_ist_length=0;
//	eeprom_busy_wait(); 
	eeprom_write_byte(0,k_ist_length);
}


	
void Set_Fuc()
{


	if(key){
	out_key=0;
		
		switch(key)
		{
			case K_KEY:
			record_key++;
			if(record_key>0x45){record_key=0x04;}
			out_key=record_key;
			break;
			case K_BACK:
				if(record_key<0x10){
				record_key=0x45;
				}else{
					record_key-=6;
				}
				out_key=record_key;
			break;			
			case K_FORWORD:
				if(record_key>0x3f){
				record_key=0x04;
				}else{
					record_key+=6;
				}
				out_key=record_key;
			break;
			
			case K_TIME:
			if(span_index>6){span_index=0;}
			span_index++;
			out_key=span_index+0x1d;			
			break;
			
			case K_RECORD:
			Key_list[k_ist_length].key=record_key;
			Key_list[k_ist_length].time=faketime[span_index];
			
			eeprom_busy_wait();
			eeprom_write_word(k_ist_length*2+2, *((uint16_t*) &(Key_list[k_ist_length]) ) );
			span_index=0;
			k_ist_length++;if(k_ist_length>63){k_ist_length=0;}
			out_key= 0x28;

			break;
			
			case K_CLEAR:
			clear();
			break;
			
			case K_AUTO:
			eeprom_write_byte(0,k_ist_length);
			eeprom_busy_wait(); 
			if(auto_key==0){auto_key=1;}else{auto_key=0;}
			break;
			
			case K_X:
			if(k_ist_length>0x48){
				out_key=0x09;
			}else{
				out_key=k_ist_length+0x1d;
				}
				

			break;
			
/*			
			case TIME_SPAN:
			if(span_index>6){span_index=0;}
			span_index++;
			out_key=span_index+0x1d;
			break;
			
			case RECORD:
			Key_list[k_ist_length].key=record_key;
			Key_list[k_ist_length].time=faketime[span_index-1];
			span_index=0;
			k_ist_length++;if(k_ist_length>63){k_ist_length=0;}
			tmp_key[0]=tmp_key_min[0];
			tmp_key[1]=tmp_key_min[1];
			tmp_key[2]=tmp_key_min[2];
			tmp_key[3]=tmp_key_min[3];
			tmp_key[4]=tmp_key_min[4];
			break;
			
			case COMMON_TIMES:
			commontime++;
			out_key=commontime+0x1d;
			if(commontime>6){commontime=1;}
			break;
			
			case AUTOKEY:
			if(auto_key==0){auto_key=1;}else{auto_key=0;}
			break;
			
			case CLEANUP:
			init_autokey();
			break;
*/
			default:
			out_key=0;
			break;
		}
		
	}else{
	if(auto_key==0){out_key=0;}
	}
	
}


uchar	usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;

    usbMsgPtr = reportBuffer;
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* class request type */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){  /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* we only have one report type, so don't look at wValue */
            buildReport(4);
            return sizeof(reportBuffer);
        }else if(rq->bRequest == USBRQ_HID_GET_IDLE){
            usbMsgPtr = &idleRate;
            return 1;
        }else if(rq->bRequest == USBRQ_HID_SET_IDLE){
            idleRate = rq->wValue.bytes[1];
        }
    }else{
        /* no vendor specific requests implemented */
    }
	return 0;
}


/* ------------------------------------------------------------------------- */
uchar t_count=0;
int	main(void)
{
unsigned int dcount=0;

	wdt_enable(WDTO_2S);
    hardwareInit();
	odDebugInit();
	usbInit();
	sei();
	init_autokey();
    DBG1(0x00, 0, 0);
	keyDidChange = 1;
	for(;;){	/* main event loop */
		wdt_reset();
		usbPoll();
        key = keyPressed();
		
		dcount++;
		if(dcount>40000){dcount=40000;}
        if(lastKey != key){     //实际上这里意味着一次keydown或者一次keyup 而不是表示一个键到另一个键的改变
			if(dcount>1000)  //去抖，实际上只是发送到PC的话，WINDOWS会去抖，但是这里单片机自己要使用按键，所以要去抖
			{
			Set_Fuc();   //取keydown来当做按键的标志。
            keyDidChange = 1;
			}
			lastKey = key;
			dcount=0;
        }

        if(TIFR0 & (1<<TOV0)){   /* 20 ms timer */
		TCNT0 = 21; 
            TIFR0 = 1<<TOV0;
			/*
            if(idleRate != 0){
                if(idleCounter > 4){
                    idleCounter -= 5;   // 22 ms in units of 4 ms 
                }else{
                    idleCounter = idleRate;
                    keyDidChange = 1;
                }
            }
			*/ 
			
			if(t_count>=5)  // 100ms timer 
			{
			t_count=0;
				KeyOut();
			}
			t_count++;
        }
		
      
	
         if(keyDidChange && usbInterruptIsReady()){
            keyDidChange = 0;

            buildReport(out_key);
            usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
        }
	
        
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
