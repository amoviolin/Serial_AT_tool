#define uc unsigned char
extern "C"
{
__int16 __stdcall dc_getver(HANDLE icdev,unsigned char *sver);

__int16 __stdcall dc_update(HANDLE icdev);
//1.
HANDLE  __stdcall  dc_init(__int16 port,long baud);
//2.
__int16  __stdcall dc_exit(HANDLE icdev);
//3.
__int16  __stdcall dc_config(HANDLE icdev,unsigned char _Mode,unsigned char _Baud);
//4.
__int16  __stdcall dc_request(HANDLE icdev,unsigned char _Mode,unsigned __int16  *TagType);
//5.
__int16  __stdcall  dc_anticoll(HANDLE icdev,unsigned char _Bcnt,unsigned long *_Snr);
//6.
__int16  __stdcall dc_select(HANDLE icdev,unsigned long _Snr,unsigned char *_Size);
//7.
__int16  __stdcall dc_authentication(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr);
//8.
__int16  __stdcall dc_halt(HANDLE icdev);
//9
__int16  __stdcall dc_read(HANDLE icdev,unsigned char _Adr,unsigned char *_Data);
//10.
__int16  __stdcall dc_read_hex(HANDLE icdev,unsigned char _Adr,char *_Data);
//11.
__int16  __stdcall dc_write(HANDLE icdev,unsigned char _Adr,unsigned char *_Data);
//12.
__int16  __stdcall dc_write_hex(HANDLE icdev,unsigned char _Adr,char *_Data);
//13.
__int16  __stdcall dc_load_key(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr,unsigned char *_NKey);
//14.
__int16  __stdcall dc_load_key_hex(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr,char *_NKey);
//15.
__int16  __stdcall dc_increment(HANDLE icdev,unsigned char _Adr,unsigned long _Value);
//16.
__int16  __stdcall dc_decrement(HANDLE icdev,unsigned char _Adr,unsigned long _Value);
//17
__int16  __stdcall dc_decrement_ml(HANDLE icdev,unsigned __int16   _Value);
//18.
__int16  __stdcall dc_restore(HANDLE icdev,unsigned char _Adr);
//19
__int16  __stdcall dc_transfer(HANDLE icdev,unsigned char _Adr);
//20.
__int16  __stdcall dc_card(HANDLE icdev,unsigned char _Mode,unsigned long *_Snr);
//21.
__int16  __stdcall dc_initval(HANDLE icdev,unsigned char _Adr,unsigned long _Value);
//22
__int16  __stdcall dc_initval_ml(HANDLE icdev,unsigned __int16   _Value);
//23.
__int16  __stdcall dc_readval(HANDLE icdev,unsigned char _Adr,unsigned long *_Value);
//24
__int16  __stdcall dc_readval_ml(HANDLE icdev,unsigned __int16   *_Value);
//25.
__int16  __stdcall dc_changeb3(HANDLE icdev,unsigned char _SecNr,unsigned char *_KeyA,unsigned char _B0,unsigned char _B1,unsigned char _B2,unsigned char _B3,unsigned char _Bk,unsigned char *_KeyB);
//26.
__int16  __stdcall dc_get_status(HANDLE icdev,unsigned char *_Status);
//27.
__int16  __stdcall dc_clr_control_bit(HANDLE icdev,unsigned char _b);
//28.
__int16  __stdcall dc_set_control_bit(HANDLE icdev,unsigned char _b);
//29.
__int16  __stdcall dc_reset(HANDLE icdev,unsigned __int16   _Msec);
//30.
__int16  __stdcall dc_HL_decrement(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr,unsigned long _Value,unsigned long _Snr,unsigned long *_NValue,unsigned long *_NSnr);
//31.
__int16  __stdcall dc_HL_increment(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr,unsigned long _Value,unsigned long _Snr,unsigned long *_NValue,unsigned long *_NSnr);
//32.
__int16  __stdcall dc_HL_write(HANDLE icdev,unsigned char _Mode,unsigned char _Adr,unsigned long *_Snr,unsigned char *_Data);
//33
__int16  __stdcall dc_HL_writehex(HANDLE icdev,unsigned char _Mode,unsigned char _Adr,unsigned long *_Snr,unsigned char *_Data);
//34.
__int16  __stdcall dc_HL_read(HANDLE icdev,unsigned char _Mode,unsigned char _Adr,unsigned long _Snr,unsigned char *_Data,unsigned long *_NSnr);
//35
__int16  __stdcall dc_HL_readhex(HANDLE icdev,unsigned char _Mode,unsigned char _Adr,unsigned long _Snr,unsigned char *_Data,unsigned long *_NSnr);
//36.
__int16  __stdcall dc_HL_initval(HANDLE icdev,unsigned char _Mode,unsigned char _SecNr,unsigned long _Value,unsigned long *_Snr);
//37.
__int16  __stdcall dc_beep(HANDLE icdev,unsigned short _Msec);
//38.
__int16  __stdcall dc_disp_str(HANDLE icdev,char *dispstr);
//40.
__int16  __stdcall dc_HL_authentication(HANDLE icdev,unsigned char reqmode,unsigned long snr,unsigned char authmode,unsigned char secnr);
//43
__int16  __stdcall dc_srd_eeprom(HANDLE icdev,__int16   offset,__int16   lenth,unsigned char *rec_buffer);
//44
__int16  __stdcall dc_swr_eeprom(HANDLE icdev,__int16   offset,__int16   lenth,unsigned char* send_buffer);
//45
__int16 __stdcall  swr_alleeprom(HANDLE icdev,__int16 offset,__int16 lenth,unsigned char* snd_buffer);

__int16  __stdcall dc_srd_eepromhex(HANDLE icdev,__int16   offset,__int16   lenth,unsigned char *rec_buffer);
//44
__int16  __stdcall dc_swr_eepromhex(HANDLE icdev,__int16   offset,__int16   lenth,unsigned char* send_buffer);
//
__int16 __stdcall  srd_alleeprom(HANDLE icdev,__int16 offset,__int16 lenth,unsigned char *receive_buffer);

//46
__int16  __stdcall dc_check_write(HANDLE icdev,unsigned long Snr,unsigned char authmode,unsigned char Adr,unsigned char * _data);
//47
__int16  __stdcall dc_check_writehex(HANDLE icdev,unsigned long Snr,unsigned char authmode,unsigned char Adr,unsigned char * _data);
//48
__int16  __stdcall dc_authentication_2(HANDLE icdev,unsigned char _Mode,unsigned char KeyNr,unsigned char Adr);
//49
//50
//52
__int16  __stdcall dc_gettime(HANDLE icdev,unsigned char *time);
//53
__int16  __stdcall dc_gettimehex(HANDLE icdev,char *time);
//54
__int16  __stdcall dc_settime(HANDLE icdev,unsigned char *time);
//55
__int16  __stdcall dc_settimehex(HANDLE icdev,char *time);
//56
__int16  __stdcall dc_setbright(HANDLE icdev,unsigned char bright);
//57
__int16  __stdcall dc_ctl_mode(HANDLE icdev,unsigned char mode);
//58
__int16  __stdcall dc_disp_mode(HANDLE icdev,unsigned char mode);
//59
//60
__int16  __stdcall dcdeshex(unsigned char *key,unsigned char *sour,unsigned char *dest,__int16 m);

__int16 __stdcall dc_light(HANDLE icdev,unsigned short _OnOff);

__int16 __stdcall dc_high_disp(HANDLE icdev,unsigned char offset,unsigned char displen,unsigned char *dispstr);

__int16 __stdcall dc_setcpu(HANDLE icdev,unsigned char _Byte);
__int16 __stdcall dc_cpureset(HANDLE icdev,unsigned char *rlen,unsigned char *databuffer);
__int16 __stdcall dc_cpuapdusource(HANDLE icdev,unsigned char slen,unsigned char * sendbuffer,unsigned char *rlen,unsigned char * databuffer);
__int16 __stdcall dc_cpuapdu(HANDLE icdev,unsigned char slen,unsigned char * sendbuffer,unsigned char *rlen,unsigned char * databuffer);
__int16 __stdcall dc_cpureset_hex(HANDLE icdev,unsigned char *rlen, char *databuffer);
__int16 __stdcall dc_cpuapdusource_hex(HANDLE icdev,unsigned char slen, char * sendbuffer,unsigned char *rlen, char * databuffer);
__int16 __stdcall dc_cpuapdu_hex(HANDLE icdev,unsigned char slen, char * sendbuffer,unsigned char *rlen, char * databuffer);
__int16 __stdcall dc_cpudown(HANDLE icdev);

__int16 __stdcall dc_setbptime(HANDLE icdev,unsigned char _Byte);

__int16 __stdcall dc_card_hex(HANDLE icdev,unsigned char _Mode,unsigned char *snrstr);

__int16 __stdcall dc_set_addr(unsigned char saddr);

HANDLE __stdcall dc_init_485(__int16 port,long baud);

__int16 __stdcall dc_change_addr(HANDLE icdev,unsigned char saddr);

__int16  __stdcall dc_read_all(HANDLE icdev,unsigned char *_Data);

__int16  __stdcall dc_write_all(HANDLE icdev,unsigned char *_Data);

__int16  __stdcall dc_read_allhex(HANDLE icdev, char *_Data);

__int16  __stdcall dc_write_allhex(HANDLE icdev, char *_Data);


__int16 __stdcall dc_set_autoflag(HANDLE icdev,unsigned char _Byte);

__int16  __stdcall dc_read_one(HANDLE icdev,unsigned char _SecNr,unsigned char *_Data);

__int16  __stdcall dc_write_one(HANDLE icdev,unsigned char _SecNr,unsigned char *_Data);

__int16  __stdcall dc_read_onehex(HANDLE icdev,unsigned char _SecNr,char *_Data);

__int16  __stdcall dc_write_onehex(HANDLE icdev,unsigned char _SecNr,char *_Data);

__int16  __stdcall dc_read_auto(HANDLE icdev,unsigned char *_Data);

__int16  __stdcall dc_read_autohex(HANDLE icdev,char *_Data);

__int16  __stdcall  dc_anticoll2(HANDLE icdev,unsigned char _Bcnt,unsigned long *_Snr);
//6.
__int16  __stdcall dc_select2(HANDLE icdev,unsigned long _Snr,unsigned char *_Size);

__int16 __stdcall dc_pro_reset(HANDLE icdev,unsigned char *rlen,unsigned char *receive_data);

__int16 __stdcall dc_pro_command(HANDLE idComDev,unsigned char slen,unsigned char * sendbuffer,unsigned char *rlen,unsigned char * databuffer,unsigned char timeout);

__int16 __stdcall dc_open_door(HANDLE icdev,unsigned char cflag);

__int16 __stdcall dc_open_timedoor(HANDLE icdev,unsigned int utime);

__int16 __stdcall dc_request_shc1102(HANDLE icdev,unsigned char _Mode,unsigned __int16 *TagType);

__int16 __stdcall dc_auth_shc1102(HANDLE icdev,unsigned char *_Data);

__int16 __stdcall dc_read_shc1102(HANDLE icdev,unsigned char _Adr,unsigned char *_Data);

__int16 __stdcall dc_write_shc1102(HANDLE icdev,unsigned char _Adr,unsigned char *_Data);

__int16 __stdcall dc_halt_shc1102(HANDLE icdev);

__int16 __stdcall dc_pro_resethex(HANDLE icdev,unsigned char *rlen, char *receive_data);

__int16 __stdcall dc_pro_commandhex(HANDLE idComDev,unsigned char slen, char * sendbuffer,unsigned char *rlen, char * databuffer,unsigned char timeout);

__int16 __stdcall dc_pro_commandsource(HANDLE idComDev,unsigned char slen,unsigned char * sendbuffer,unsigned char *rlen,unsigned char * databuffer,unsigned char timeout);

__int16 __stdcall dc_pro_halt(HANDLE icdev);


//void hex_a(unsigned char *hex,unsigned char *a,__int16 length);

//__int16 a_hex(unsigned char *a,unsigned char *hex,__int16 len);

__int16 __stdcall dc_config_card(HANDLE icdev,unsigned char cardtype);

__int16 __stdcall dc_request_b(HANDLE icdev,unsigned char _Mode,
							   unsigned char AFI, 
		                       unsigned char N, 
		                       unsigned char *ATQB);

__int16 __stdcall dc_slotmarker(HANDLE icdev,unsigned char N, unsigned char *ATQB);

__int16 __stdcall dc_attrib(HANDLE icdev,unsigned char *PUPI, unsigned char CID);

__int16 __stdcall dc_changebaud_485(HANDLE icdev,long baud);


__int16 __stdcall dc_read_random(HANDLE icdev, unsigned char *data);

__int16 __stdcall dc_write_random(HANDLE icdev,int len, unsigned char *data);

__int16 __stdcall dc_read_random_hex(HANDLE icdev, unsigned char *data);

__int16 __stdcall dc_write_random_hex(HANDLE icdev,int len, unsigned char *data);

__int16 __stdcall dc_erase_random(HANDLE icdev,int len);

}
