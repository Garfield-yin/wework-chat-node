// All Rights Reserved.
// *File £º WeWorkFinanceSdk_C.h
// @Brief£ºÀ­È¡ÆóÒµÁÄÌì¼ÇÂ¼ÓëÃ½ÌåÏûÏ¢sdkÍ·ÎÄ¼þ

#pragma once
//·µ»ØÂë	´íÎóËµÃ÷
//10000	²ÎÊý´íÎó£¬ÇëÇó²ÎÊý´íÎó
//10001	ÍøÂç´íÎó£¬ÍøÂçÇëÇó´íÎó
//10002	Êý¾Ý½âÎöÊ§°Ü
//10003	ÏµÍ³Ê§°Ü
//10004	ÃÜÔ¿´íÎóµ¼ÖÂ¼ÓÃÜÊ§°Ü
//10005	fileid´íÎó
//10006	½âÃÜÊ§°Ü
//10007 ÕÒ²»µ½ÏûÏ¢¼ÓÃÜ°æ±¾µÄË½Ô¿£¬ÐèÒªÖØÐÂ´«ÈëË½Ô¿¶Ô
//10008 ½âÎöencrypt_key³ö´í
//10009 ip·Ç·¨
//10010 Êý¾Ý¹ýÆÚ
//10011	Ö¤Êé´íÎó

typedef struct WeWorkFinanceSdk_t WeWorkFinanceSdk_t;

// Êý¾Ý
typedef struct Slice_t {
    char* buf;
    int len;
} Slice_t;

typedef struct MediaData {
    char* outindexbuf;
    int out_len;
    char* data;    
    int data_len;
    int is_finish;
} MediaData_t;


#ifdef __cplusplus
extern "C" {
#endif    

    WeWorkFinanceSdk_t* NewSdk();
                               

	/**
	 * ³õÊ¼»¯º¯Êý
	 * ReturnÖµ=0±íÊ¾¸ÃAPIµ÷ÓÃ³É¹¦
	 * 
	 * @param [in]  sdk			NewSdk·µ»ØµÄsdkÖ¸Õë
	 * @param [in]  corpid      µ÷ÓÃÆóÒµµÄÆóÒµid£¬ÀýÈç£ºwwd08c8exxxx5ab44d£¬¿ÉÒÔÔÚÆóÒµÎ¢ÐÅ¹ÜÀí¶Ë--ÎÒµÄÆóÒµ--ÆóÒµÐÅÏ¢²é¿´
	 * @param [in]  secret		ÁÄÌìÄÚÈÝ´æµµµÄSecret£¬¿ÉÒÔÔÚÆóÒµÎ¢ÐÅ¹ÜÀí¶Ë--¹ÜÀí¹¤¾ß--ÁÄÌìÄÚÈÝ´æµµ²é¿´
	 *						
	 *
	 * @return ·µ»ØÊÇ·ñ³õÊ¼»¯³É¹¦
	 *      0   - ³É¹¦
	 *      !=0 - Ê§°Ü
	 */
    int Init(WeWorkFinanceSdk_t* sdk, const char* corpid, const char* secret);

	/**
	 * À­È¡ÁÄÌì¼ÇÂ¼º¯Êý
	 * ReturnÖµ=0±íÊ¾¸ÃAPIµ÷ÓÃ³É¹¦
	 * 
	 *
	 * @param [in]  sdk				NewSdk·µ»ØµÄsdkÖ¸Õë
	 * @param [in]  seq				´ÓÖ¸¶¨µÄseq¿ªÊ¼À­È¡ÏûÏ¢£¬×¢ÒâµÄÊÇ·µ»ØµÄÏûÏ¢´Óseq+1¿ªÊ¼·µ»Ø£¬seqÎªÖ®Ç°½Ó¿Ú·µ»ØµÄ×î´óseqÖµ¡£Ê×´ÎÊ¹ÓÃÇëÊ¹ÓÃseq:0
	 * @param [in]  limit			Ò»´ÎÀ­È¡µÄÏûÏ¢ÌõÊý£¬×î´óÖµ1000Ìõ£¬³¬¹ý1000Ìõ»á·µ»Ø´íÎó
	 * @param [in]  proxy			Ê¹ÓÃ´úÀíµÄÇëÇó£¬ÐèÒª´«Èë´úÀíµÄÁ´½Ó¡£Èç£ºsocks5://10.0.0.1:8081 »òÕß http://10.0.0.1:8081
	 * @param [in]  passwd			´úÀíÕËºÅÃÜÂë£¬ÐèÒª´«Èë´úÀíµÄÕËºÅÃÜÂë¡£Èç user_name:passwd_123
	 * @param [in]  timeout			³¬Ê±Ê±¼ä£¬µ¥Î»Ãë
	 * @param [out] chatDatas		·µ»Ø±¾´ÎÀ­È¡ÏûÏ¢µÄÊý¾Ý£¬slice½á¹¹Ìå.ÄÚÈÝ°üÀ¨errcode/errmsg£¬ÒÔ¼°Ã¿ÌõÏûÏ¢ÄÚÈÝ¡£Ê¾ÀýÈçÏÂ£º

	 {"errcode":0,"errmsg":"ok","chatdata":[{"seq":196,"msgid":"CAQQ2fbb4QUY0On2rYSAgAMgip/yzgs=","publickey_ver":3,"encrypt_random_key":"ftJ+uz3n/z1DsxlkwxNgE+mL38H42/KCvN8T60gbbtPD+Rta1hKTuQPzUzO6Hzne97MgKs7FfdDxDck/v8cDT6gUVjA2tZ/M7euSD0L66opJ/IUeBtpAtvgVSD5qhlaQjvfKJc/zPMGNK2xCLFYqwmQBZXbNT7uA69Fflm512nZKW/piK2RKdYJhRyvQnA1ISxK097sp9WlEgDg250fM5tgwMjujdzr7ehK6gtVBUFldNSJS7ndtIf6aSBfaLktZgwHZ57ONewWq8GJe7WwQf1hwcDbCh7YMG8nsweEwhDfUz+u8rz9an+0lgrYMZFRHnmzjgmLwrR7B/32Qxqd79A==","encrypt_chat_msg":"898WSfGMnIeytTsea7Rc0WsOocs0bIAerF6de0v2cFwqo9uOxrW9wYe5rCjCHHH5bDrNvLxBE/xOoFfcwOTYX0HQxTJaH0ES9OHDZ61p8gcbfGdJKnq2UU4tAEgGb8H+Q9n8syRXIjaI3KuVCqGIi4QGHFmxWenPFfjF/vRuPd0EpzUNwmqfUxLBWLpGhv+dLnqiEOBW41Zdc0OO0St6E+JeIeHlRZAR+E13Isv9eS09xNbF0qQXWIyNUi+ucLr5VuZnPGXBrSfvwX8f0QebTwpy1tT2zvQiMM2MBugKH6NuMzzuvEsXeD+6+3VRqL"}]}

	 *
	 * @return ·µ»ØÊÇ·ñµ÷ÓÃ³É¹¦
	 *      0   - ³É¹¦
	 *      !=0 - Ê§°Ü	
	 */		
    int GetChatData(WeWorkFinanceSdk_t* sdk, unsigned long long seq, unsigned int limit, const char *proxy,const char* passwd, int timeout,Slice_t* chatDatas);

	/**
     * @brief ½âÎöÃÜÎÄ.ÆóÒµÎ¢ÐÅ×ÔÓÐ½âÃÜÄÚÈÝ
     * @param [in]  encrypt_key, getchatdata·µ»ØµÄencrypt_random_key,Ê¹ÓÃÆóÒµ×Ô³Ö¶ÔÓ¦°æ±¾ÃØÔ¿RSA½âÃÜºóµÄÄÚÈÝ
     * @param [in]  encrypt_msg, getchatdata·µ»ØµÄencrypt_chat_msg
     * @param [out] msg, ½âÃÜµÄÏûÏ¢Ã÷ÎÄ
	 * @return ·µ»ØÊÇ·ñµ÷ÓÃ³É¹¦
	 *      0   - ³É¹¦
	 *      !=0 - Ê§°Ü
     */
    int DecryptData(const char* encrypt_key, const char* encrypt_msg, Slice_t* msg);

	/**
	 * À­È¡Ã½ÌåÏûÏ¢º¯Êý
	 * ReturnÖµ=0±íÊ¾¸ÃAPIµ÷ÓÃ³É¹¦
	 * 
	 *
	 * @param [in]  sdk				NewSdk·µ»ØµÄsdkÖ¸Õë
	 * @param [in]  sdkFileid		´ÓGetChatData·µ»ØµÄÁÄÌìÏûÏ¢ÖÐ£¬Ã½ÌåÏûÏ¢°üÀ¨µÄsdkfileid
	 * @param [in]  proxy			Ê¹ÓÃ´úÀíµÄÇëÇó£¬ÐèÒª´«Èë´úÀíµÄÁ´½Ó¡£Èç£ºsocks5://10.0.0.1:8081 »òÕß http://10.0.0.1:8081
	 * @param [in]  passwd			´úÀíÕËºÅÃÜÂë£¬ÐèÒª´«Èë´úÀíµÄÕËºÅÃÜÂë¡£Èç user_name:passwd_123
	 * @param [in]  indexbuf		Ã½ÌåÏûÏ¢·ÖÆ¬À­È¡£¬ÐèÒªÌîÈëÃ¿´ÎÀ­È¡µÄË÷ÒýÐÅÏ¢¡£Ê×´Î²»ÐèÒªÌîÐ´£¬Ä¬ÈÏÀ­È¡512k£¬ºóÐøÃ¿´Îµ÷ÓÃÖ»ÐèÒª½«ÉÏ´Îµ÷ÓÃ·µ»ØµÄoutindexbufÌîÈë¼´¿É¡£
	 * @param [in]  timeout			³¬Ê±Ê±¼ä£¬µ¥Î»Ãë
	 * @param [out] media_data		·µ»Ø±¾´ÎÀ­È¡µÄÃ½ÌåÊý¾Ý.MediaData½á¹¹Ìå.ÄÚÈÝ°üÀ¨data(Êý¾ÝÄÚÈÝ)/outindexbuf(ÏÂ´ÎË÷Òý)/is_finish(À­È¡Íê³É±ê¼Ç)
	 
	 *
	 * @return ·µ»ØÊÇ·ñµ÷ÓÃ³É¹¦
	 *      0   - ³É¹¦
	 *      !=0 - Ê§°Ü
	 */
	int GetMediaData(WeWorkFinanceSdk_t* sdk, const char* indexbuf,
                     const char* sdkFileid,const char *proxy,const char* passwd, int timeout, MediaData_t* media_data);

    /**
     * @brief ÊÍ·Åsdk£¬ºÍNewSdk³É¶ÔÊ¹ÓÃ
     * @return 
     */
    void DestroySdk(WeWorkFinanceSdk_t* sdk);


    //--------------ÏÂÃæ½Ó¿ÚÎªÁËÆäËûÓïÑÔÀýÈçpythonµÈµ÷ÓÃc½Ó¿Ú£¬×ÃÇéÊ¹ÓÃ--------------
    Slice_t* NewSlice();

    /**
     * @brief ÊÍ·Åslice£¬ºÍNewSlice³É¶ÔÊ¹ÓÃ
     * @return 
     */
    void FreeSlice(Slice_t* slice);

    /**
     * @brief ÎªÆäËûÓïÑÔÌá¹©¶ÁÈ¡½Ó¿Ú
     * @return ·µ»ØbufÖ¸Õë
     *     !=NULL - ³É¹¦
     *     NULL   - Ê§°Ü
     */
    char* GetContentFromSlice(Slice_t* slice);
	int GetSliceLen(Slice_t* slice);

	// Ã½Ìå¼ÇÂ¼Ïà¹Ø¹¤¾ß

    MediaData_t*  NewMediaData();

    void FreeMediaData(MediaData_t* media_data);

    char* GetOutIndexBuf(MediaData_t* media_data);
    char* GetData(MediaData_t* media_data);
	int GetIndexLen(MediaData_t* media_data);
	int GetDataLen(MediaData_t* media_data);
    int IsMediaDataFinish(MediaData_t* media_data);

#ifdef __cplusplus
}
#endif    
