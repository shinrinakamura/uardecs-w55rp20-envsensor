// 開発メモ
// UECSclient=UECSlogserver.available();を使用しないようにした

#include "Uardecs_RP20.h"

// #define DEBUG_MOD	// デバッグを行いたいときはコメントアウトを外してください　commented by S.Nakamura

char UECSbuffer[BUF_SIZE];//main buffer
char UECStempStr20[MAX_TYPE_CHAR];//sub buffer
EthernetUDP UECS_UDP16520;
EthernetUDP UECS_UDP16529;
EthernetUDP UECS_UDP16521;
EthernetServer UECSlogserver(80);
EthernetClient UECSclient;

struct UECSTEMPCCM UECStempCCM;

unsigned char UECSsyscounter60s;
unsigned long UECSsyscounter1s;
unsigned long UECSnowmillis;
unsigned long UECSlastmillis;

//bool flag_programUpdate;



//Form CCM.cpp
//##############################################################################
//##############################################################################


const char *UECSattrChar[] = { UECSccm_ROOMTXT, UECSccm_REGIONTXT, UECSccm_ORDERTXT, UECSCCM_PRIOTXT,};
const char *UECSCCMLEVEL[]={UECS_A1S0, UECS_A1S1, UECS_A10S0, UECS_A10S1, UECS_A1M0, UECS_A1M1, UECS_S1S, UECS_S1M, UECS_B0_, UECS_B1_, };

boolean UECSparseRec( struct UECSTEMPCCM *_tempCCM,int *matchCCMID){
	
	int i;
	int progPos = 0;
	int startPos = 0;
	short shortValue=0;
	
	if(!UECSFindConstChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;
	
	if(UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){

		startPos+=progPos;
		//E10 packet
	}
	else{
		//other ver packet
		if(!(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK)){return false;}
	}
	
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_DATATYPE[0],&progPos)){return false;}
	
	startPos+=progPos;
	//copy ccm type string
	for(i=0;i<MAX_TYPE_CHAR;i++){

		_tempCCM->type[i]=UECSbuffer[startPos+i];
		if(_tempCCM->type[i]==ASCIICODE_DQUOT || _tempCCM->type[i]=='\0'){
			_tempCCM->type[i]='\0';break;
		}
	}
	
	_tempCCM->type[MAX_CCMTYPESIZE]='\0';
	
	startPos=startPos+i;
	//In a practical environment ,packets of 99% are irrelevant CCM.
	//matching top 3 chars for acceleration 
	*matchCCMID=-1;
	for(i=0;i<U_MAX_CCM;i++){

		//check receive ccm
		if(U_ccmList[i].ccmLevel != NONE && U_ccmList[i].sender == false){

			if(_tempCCM->type[0]==U_ccmList[i].typeStr[0] && 
				_tempCCM->type[1]==U_ccmList[i].typeStr[1] && 
				_tempCCM->type[2]==U_ccmList[i].typeStr[2]){
					
				*matchCCMID=i;
				break;
			}
		}
	}

	if(*matchCCMID<0){return false;}//my ccm was not match ->cancel packet reading


	for(i=0;i<MAX_ATTR_NUMBER;i++){

		if(!UECSGetValPGMStrAndChr(&UECSbuffer[startPos],UECSattrChar[i],'\"',&(_tempCCM->attribute[i]),&progPos)){return false;}
		startPos+=progPos;
	}

	//find tag end
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_CLOSETAG[0],&progPos)){return false;}
	startPos+=progPos;
	
	//get value
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&(_tempCCM->value),&(_tempCCM->decimal),&progPos)){return false;}
	startPos+=progPos;
	
	//ip tag
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_IPTAG[0],&progPos)){

		//ip tag not found(old type packet)
		//ip address is already filled
		if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_FOOTER0[0],&progPos)){return false;}
		return true;
	}
	startPos+=progPos;
	
	unsigned char ip[4];
	if(!UECSGetIPAddress(&UECSbuffer[startPos],ip,&progPos)){return false;}

	_tempCCM->address[0]=ip[0];
	_tempCCM->address[1]=ip[1];
	_tempCCM->address[2]=ip[2];
	_tempCCM->address[3]=ip[3];
	
	startPos+=progPos;

	if(U_orgAttribute.flags&ATTRFLAG_LOOSELY_VERCHECK){return true;}//Ignore information after ip
	
	//check footer
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_FOOTER1[0],&progPos)){return false;}
	/*
	Serial.println("type");
	Serial.println(_tempCCM->type);
	Serial.println("attribute[AT_ROOM]");
	Serial.println(_tempCCM->attribute[AT_ROOM]);
	Serial.println("attribute[AT_REGI]");
	Serial.println(_tempCCM->attribute[AT_REGI]);
	Serial.println("attribute[AT_ORDE]");
	Serial.println(_tempCCM->attribute[AT_ORDE]);
	Serial.println("attribute[AT_PRIO]");
	Serial.println(_tempCCM->attribute[AT_PRIO]);
	Serial.println("value");
	Serial.println(_tempCCM->value);
	Serial.println("decimal");
	Serial.println(_tempCCM->decimal);
	Serial.println("ip[0]");
	Serial.println(_tempCCM->address[0]);
	Serial.println("ip[1]");
	Serial.println(_tempCCM->address[1]);
	Serial.println("ip[2]");
	Serial.println(_tempCCM->address[2]);
	Serial.println("ip[3]");
	Serial.println(_tempCCM->address[3]);
	*/
	return true;
}

//----------------------------------------------------------------------------
// ccmパケットを作成して送信する
void UECSCreateCCMPacketAndSend( struct UECSCCM* _ccm){
	ClearMainBuffer();	// バッファをクリアする
	UDPAddConstCharToBuffer(&(UECSccm_XMLHEADER[0]));
	UDPAddConstCharToBuffer(&(UECSccm_UECSVER_E10[0]));
	UDPAddConstCharToBuffer(&(UECSccm_DATATYPE[0]));
	UDPAddCharToBuffer(_ccm->typeStr);

	for(int i=0;i<4;i++){
		UDPAddConstCharToBuffer(UECSattrChar[i]); 
		UDPAddValueToBuffer(_ccm->baseAttribute[i]);
	}

	UDPAddConstCharToBuffer(&(UECSccm_CLOSETAG[0]));
  dtostrf(((double)_ccm->value) / pow(10, _ccm->decimal), -1, _ccm->decimal, UECStempStr20);
	UDPAddCharToBuffer(UECStempStr20);
	UDPAddConstCharToBuffer(&(UECSccm_IPTAG[0]));

	if(U_orgAttribute.status & STATUS_SAFEMODE){
		UDPAddConstCharToBuffer(&(UECSdefaultIPAddress[0]));
	}
	else{
		sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2],U_orgAttribute.ip[3]);
		UDPAddCharToBuffer(UECStempStr20);
	}

	UDPAddConstCharToBuffer(&(UECSccm_FOOTER1[0]));

	// デバッグダンプ(確認用の表示)
	// Serial.println("=== UECSbuffer dump after IP ===");
	// Serial.println(UECSbuffer);

	//send ccm
  UECS_UDP16520.beginPacket(_ccm->address, 16520);
  UECS_UDP16520.write(UECSbuffer);
  
  if(UECS_UDP16520.endPacket()==0){
	
		UECSresetEthernet();//when udpsend failed,reset ethernet status
	}
}

// 受信したCCM情報を既存のCCM構造体に反映
void UECSupRecCCM(UECSCCM* _ccm, UECSTEMPCCM* _ccmRec){
  
	// boolean success = false;	// 使用されていない

	int i;
	for(i=0;i<MAX_ATTR_NUMBER;i++){
		_ccm->attribute[i] = _ccmRec->attribute[i];
	}

	for(i=0;i<MAX_IPADDRESS_NUMBER;i++){
		_ccm->address[i] = _ccmRec->address[i];
	}

	// 値の小数点を合わせる
	int dif_decimal=_ccm->decimal-_ccmRec->decimal;
	if(dif_decimal>=0){
		_ccm->value = _ccmRec->value*pow(10,dif_decimal);		// 10^x
	}
	else{
		_ccm->value = _ccmRec->value/pow(10,-dif_decimal);	// 1/10^x
	}
    
	_ccm->recmillis = 0;						// 最後に受信してからの時間をリセット
	_ccm->validity=true;						// 有効なデータとしてフラグを立てる
	_ccm->flagStimeRfirst = true;		// 初回受信済出あることを示す
}

//---------------------------------------------------------------
// 一次的に受信した_tempCCMを既存のCCMリストと比較して必要があれば更新する
// 第一引数：一次的な受信データ
// 第二引数：現在の時間（millis()）
// 第三引数：検索開始ID
void UECScheckUpDate(UECSTEMPCCM* _tempCCM, unsigned long _time,int startid){

    for(int i = startid; i < U_MAX_CCM; i++){
			// 無効なCCMと送信者として動作しているCCMは対象外
      if(U_ccmList[i].ccmLevel == NONE || U_ccmList[i].sender == true){continue;}
      
      
			if(!((_tempCCM->attribute[AT_ROOM] == 0 || U_ccmList[i].baseAttribute[AT_ROOM]==0 || _tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]) &&
				(_tempCCM->attribute[AT_REGI] == 0 || U_ccmList[i].baseAttribute[AT_REGI]==0 || _tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI]) &&
				(_tempCCM->attribute[AT_ORDE] == 0 || U_ccmList[i].baseAttribute[AT_ORDE]==0 || _tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE]))){
					continue;
				}

        //type 
        if(strcmp(U_ccmList[i].typeStr, _tempCCM->type) != 0){
					continue;
				}
        
				// 更新条件の判定
				boolean up = false;
				if(U_ccmList[i].ccmLevel==B_0 || U_ccmList[i].ccmLevel==B_1){
					up = true;
				}
				else if(!U_ccmList[i].validity){
					up = true;
				} 
				else if(_tempCCM->attribute[AT_PRIO] < U_ccmList[i].attribute[AT_PRIO]){
					up = true;
				}//lower priority number is strong

				// Room→Region→Order→IPアドレスが一致したときに更新
				// 優先度が同等の時にはIPアドレスが小さい方が優先
      	else{
					
					if(_tempCCM->attribute[AT_ROOM] == U_ccmList[i].attribute[AT_ROOM]){
                    
						if(_tempCCM->attribute[AT_REGI] == U_ccmList[i].attribute[AT_REGI]){

							if(_tempCCM->attribute[AT_ORDE] == U_ccmList[i].attribute[AT_ORDE]){
								
								//if(_tempCCM->address <= U_ccmList[i].address)
								//convert big endian
								unsigned long address_t=_tempCCM->address[0];
								address_t=(address_t<<8)|_tempCCM->address[1];
								address_t=(address_t<<8)|_tempCCM->address[2];
								address_t=(address_t<<8)|_tempCCM->address[3];
								unsigned long address_b=U_ccmList[i].address[0];
								address_b=(address_b<<8)|U_ccmList[i].address[1];
								address_b=(address_b<<8)|U_ccmList[i].address[2];
								address_b=(address_b<<8)|U_ccmList[i].address[3];
								
								if(address_t<=address_b){
									up = true;
								} 
							}
          	else if (_tempCCM->attribute[AT_ORDE] == U_ccmList[i].baseAttribute[AT_ORDE] || U_ccmList[i].baseAttribute[AT_ORDE]==0){
							up = true;
						}                         
					}
					else if (_tempCCM->attribute[AT_REGI] == U_ccmList[i].baseAttribute[AT_REGI] || U_ccmList[i].baseAttribute[AT_REGI]==0){
						up = true;
					}                       
				}
				else if (_tempCCM->attribute[AT_ROOM] == U_ccmList[i].baseAttribute[AT_ROOM]  || U_ccmList[i].baseAttribute[AT_ROOM]==0){
					up = true;
				} 
			}

		if(up){UECSupRecCCM(&U_ccmList[i], _tempCCM);}
	}  
}

/********************************/
/* 16529 Response   *************/
/********************************/
// ノードスキャンまたはCCMスキャンのレスポンス
// 返り値：true：応答の生成、false：該当処理なし
boolean UECSresNodeScan(){
	
	int i;
	int progPos = 0;
	int startPos = 0;
	
	if(!UECSFindConstChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){
		return false;
	}
	startPos+=progPos;

	// バージョン管理
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){
		
		return false;
	}
	startPos+=progPos;
	
	// NODESCANへのレスポンス
	if(UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_NODESCAN1[0],&progPos) || UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_NODESCAN2[0],&progPos)){
		
    //NODESCAN response
		ClearMainBuffer();
    UDPAddConstCharToBuffer(&(UECSccm_XMLHEADER[0]));
    UDPAddConstCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    UDPAddConstCharToBuffer(&(UECSccm_NODENAME[0])); 
    UDPAddConstCharToBuffer(&(U_name[0]));
    UDPAddConstCharToBuffer(&(UECSccm_VENDER[0])); 
    UDPAddConstCharToBuffer(&(U_vender[0]));
    UDPAddConstCharToBuffer(&(UECSccm_UECSID[0])); 
    UDPAddConstCharToBuffer(&(U_uecsid[0]));
    UDPAddConstCharToBuffer(&(UECSccm_UECSID_IP[0]));
    
    // IPアドレスの確認
    if(U_orgAttribute.status & STATUS_SAFEMODE){
			UDPAddConstCharToBuffer(&(UECSdefaultIPAddress[0]));
		}
		else{
			sprintf(UECStempStr20, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
			UDPAddCharToBuffer(UECStempStr20);
		}
    
    UDPAddConstCharToBuffer(&(UECSccm_MAC[0]));
    sprintf(UECStempStr20, "%02X%02X%02X%02X%02X%02X", U_orgAttribute.mac[0], U_orgAttribute.mac[1], U_orgAttribute.mac[2], U_orgAttribute.mac[3], U_orgAttribute.mac[4], U_orgAttribute.mac[5]);
    UDPAddCharToBuffer(UECStempStr20);
    UDPAddConstCharToBuffer(&(UECSccm_NODECLOSE[0]));
    
    return true;
	}
    
	// CCMSCANの確認（ページ数）
	if(UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_CCMSCAN[0],&progPos)){

		short pageNum=0;
		startPos+=progPos;
			
		//format of page number written type
		if(UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_PAGE[0],'\"',&pageNum,&progPos)){

			startPos+=progPos;
			//check close tag
			if(!(UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE2[0],&progPos))){return false;}
		}

		//format of page number abridged type（簡略形式）
		else if(UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_CCMSCANCLOSE0[0],&progPos)){
			pageNum=1;
		}
		else{
			return false;
		}
		
		//CCMSCAN response
		ClearMainBuffer();
		UDPAddConstCharToBuffer(&(UECSccm_XMLHEADER[0]));
		UDPAddConstCharToBuffer(&(UECSccm_UECSVER_E10[0]));
		UDPAddConstCharToBuffer(&(UECSccm_CCMNUM[0]));
		
		//count enabled ccm
		short enabledCCMNum=0;
		short returnCCMID=-1;
		for(i = 0; i < U_MAX_CCM; i++){
			
			if(U_ccmList[i].ccmLevel != NONE){
			
				enabledCCMNum++;
				if(enabledCCMNum==pageNum){returnCCMID=i;}
			}
		}
				
		if(enabledCCMNum==0 || returnCCMID<0){return false;}//page num over
		
		UDPAddValueToBuffer(pageNum);
		UDPAddConstCharToBuffer(&(UECSccm_TOTAL[0]));
		UDPAddValueToBuffer(enabledCCMNum);
		UDPAddConstCharToBuffer(&(UECSccm_CLOSETAG[0]));
		UDPAddValueToBuffer(1);//Column number is always 1
		UDPAddConstCharToBuffer(&(UECSccm_CCMNO[0]));
		UDPAddValueToBuffer(pageNum);//page number = ccm number
		
		// 属性の追加
		for(i=0;i<3;i++){

			UDPAddConstCharToBuffer(UECSattrChar[i]);
			UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[i]);
		}
		UDPAddConstCharToBuffer(UECSattrChar[AT_PRIO]);
		UDPAddValueToBuffer(U_ccmList[returnCCMID].baseAttribute[AT_PRIO]);
		
		// 小数点の桁数
		UDPAddConstCharToBuffer(&(UECSccm_CAST[0]));
		UDPAddValueToBuffer(U_ccmList[returnCCMID].decimal);
		// 単位
		UDPAddConstCharToBuffer(&(UECSccm_UNIT[0]));                          
		UDPAddConstCharToBuffer((U_ccmList[returnCCMID].unit));
		
		// 送受信の把握
		UDPAddConstCharToBuffer(&(UECSccm_SR[0]));
		if(U_ccmList[returnCCMID].sender){
			UDPAddConstCharToBuffer(UECSTxtPartS);
		}
		else{
			UDPAddConstCharToBuffer(UECSTxtPartR);
		}              
		// レベル
		UDPAddConstCharToBuffer(&(UECSccm_LV[0]));                          
		UDPAddConstCharToBuffer((UECSCCMLEVEL[U_ccmList[returnCCMID].ccmLevel]));
		// タイプ
		UDPAddConstCharToBuffer(&(UECSccm_CLOSETAG[0]));
		UDPAddCharToBuffer(U_ccmList[returnCCMID].typeStr);
		UDPAddConstCharToBuffer(&(UECSccm_CCMRESCLOSE[0]));  
			
		return true;
	}
    
	return false;
}


//------------------------------------------------------------------
// 自動送信のタイミングの管理と有効性のチェック
void UECSautomaticSendManager(){

	/*
	UDP transmission load balancing
	Improved UDP send timing not to be concentrated in Ver2.0 or later.
	Packets sent at 10-second and 60-second intervals were sent together in past versions, but are sent separately in the new version.
	Transmission timing is distributed according to the order of registration of the lower one of the IP addresses and the CCM.
	However, this load balancing is ineffective for packets sent every second.
	*/
	int timing_count;

	for(int id=0;id<U_MAX_CCM;id++){
  
		if(U_ccmList[id].ccmLevel == NONE || !U_ccmList[id].sender){
			continue;
		}

		//Timeout judgment of the sending CCM
		if(U_ccmList[id].flagStimeRfirst == true){
			
			U_ccmList[id].recmillis=0;
			U_ccmList[id].validity=true;
		}
		else{
			
			if((U_ccmList[id].ccmLevel==A_1S_0 || U_ccmList[id].ccmLevel==A_1S_1)&& U_ccmList[id].recmillis>1000){U_ccmList[id].validity=false;}
			else if((U_ccmList[id].ccmLevel==A_10S_0 || U_ccmList[id].ccmLevel==A_10S_1)&& U_ccmList[id].recmillis>10000){U_ccmList[id].validity=false;}
			else if((U_ccmList[id].ccmLevel==A_1M_0 || U_ccmList[id].ccmLevel==A_1M_1)&& U_ccmList[id].recmillis>60000){U_ccmList[id].validity=false;}
		}
			
		if(U_ccmList[id].recmillis<36000000){
			U_ccmList[id].recmillis+=1000;
		}

		//Determination of CCM transmission timing
	  timing_count=U_orgAttribute.ip[3]+id;
	  if((U_ccmList[id].ccmLevel == A_10S_1 || U_ccmList[id].ccmLevel == A_1M_1 ) && U_ccmList[id].old_value!=U_ccmList[id].value){
  	  
			U_ccmList[id].flagStimeRfirst = true;
  	}
		else if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0){
			U_ccmList[id].flagStimeRfirst = true;
		}
    else if((UECSsyscounter60s % 10 == (timing_count % 10)) && (U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1)){
			
			U_ccmList[id].flagStimeRfirst = true;
		}
		else if(UECSsyscounter60s == (timing_count % 60) && (U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0)){
			U_ccmList[id].flagStimeRfirst = true;
    }
    else{
			U_ccmList[id].flagStimeRfirst = false;
		}

		U_ccmList[id].old_value=U_ccmList[id].value;
	}
}

//----------------------------------------------------------------------
// CCM受信の有効性を管理する
// 引数：前回チェックからの経過時間（ms）
void UECSautomaticValidManager(unsigned long td)
{
	for(int id=0;id<U_MAX_CCM;id++){

		// 無効なCCMまたは送信用ののノードの場合はスキップ
		if(U_ccmList[id].ccmLevel == NONE|| U_ccmList[id].sender){
			continue;
		}
				
		//over 24hour
		if(U_ccmList[id].recmillis>86400000){
			continue;//stop count（無効にはしないがこれ以上カウントはされない）
		}

		// 経過時間を加算
		U_ccmList[id].recmillis+=td;//count time(ms) since last recieved
		
		// 有効とみなす最大経過時間の把握（指定周期の3倍）
		unsigned long validmillis=0;		
		if(U_ccmList[id].ccmLevel == A_1S_0 || U_ccmList[id].ccmLevel == A_1S_1 || U_ccmList[id].ccmLevel == S_1S_0){
			validmillis = 3000;
		}
		else if(U_ccmList[id].ccmLevel == A_10S_0 || U_ccmList[id].ccmLevel == A_10S_1){				
			validmillis = 30000;
		}
		else if(U_ccmList[id].ccmLevel == A_1M_0 || U_ccmList[id].ccmLevel == A_1M_1 || U_ccmList[id].ccmLevel == S_1M_0){
			validmillis = 180000;
		}

		// 有効期間を超えたとき、または一度も受信していないときは無効化 
		if(U_ccmList[id].recmillis > validmillis || U_ccmList[id].flagStimeRfirst == false){
			U_ccmList[id].validity = false;  
		}
	}
}

//##############################################################################
//##############################################################################
void UECS_EEPROM_writeLong(int ee, long value){
	byte* p = (byte*)(void*)&value;
	for (unsigned int i = 0; i < sizeof(value); i++){
		
		//same value skip
		if(EEPROM.read(ee)!=p[i]){

			EEPROM.write(ee, p[i]);
		}
		ee++;
	}

#ifdef DEBUG_MOD
	// 確認のための表示
	Serial.println("");
	Serial.print("write long eeprom");
	Serial.println(value);
	Serial.println("");
#endif
	// フラッシュに書き込む
	EEPROM.commit();
}

long UECS_EEPROM_readLong(int ee){

	long value = 0;
	byte* p = (byte*)(void*)&value;
	for (unsigned int i = 0; i < sizeof(value); i++)
	*p++ = EEPROM.read(ee++);

#ifdef DEBUG_MOD
	// テスト用の表示
	Serial.println("");
	Serial.print("eeprom read long");
	Serial.println(value);
	Serial.println("");
#endif
	return value;
}

// HTMLのinput要素の生成
// 引数：入力欄の初期値
void HTTPsetInput(short _value){
	HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
	HTTPAddValueToBuffer(_value);
	HTTPAddConstCharToBuffer(&(UECShtmlINPUTCLOSE3[0]));
}

// HTMLにIPアドレスを表示
void HTTPprintIPtoHtml(byte address[]){

  for(int i = 0; i < 3; i++){ 
   HTTPsetInput(address[i]);
   HTTPAddConstCharToBuffer(UECSTxtPartColon);	//IPアドレスの区切りのコロンの表示
  }      
  HTTPsetInput(address[3]);
  HTTPAddConstCharToBuffer(&(UECSbtrag[0]));		// <br>
}

// ccm editの内容をリダイレクト
void HTTPPrintRedirect(char page){
	ClearMainBuffer();
	HTTPAddConstCharToBuffer(&(UECShttpHead200_OK[0]));
	HTTPAddConstCharToBuffer(&(UECShttpHeadContentType[0]));
	HTTPAddConstCharToBuffer(&(UECShttpHeadConnection[0]));
	HTTPAddConstCharToBuffer(&(UECShtmlHEADER[0]));
 	if(page==3){
 		HTTPAddConstCharToBuffer(&(UECShtmlREDIRECT3[0]));
	}
 	else{
	 	HTTPAddConstCharToBuffer(&(UECShtmlREDIRECT1[0]));
	}
 
 	HTTPCloseBuffer();
}

/*
void HTTPPrintRedirectP3(){
 ClearMainBuffer();
 HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
 HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlHEADER[0]));
 HTTPAddPGMCharToBuffer(&(UECShtmlREDIRECT3[0]));
 HTTPCloseBuffer();
}*/
//-----------------------------------------------------------
// HTTPレスポンスとヘッダーをバッファに詰める
void HTTPPrintHeader(){

 	ClearMainBuffer();
	// HTTPレスポンス
	HTTPAddConstCharToBuffer(&(UECShttpHead200_OK[0]));
	HTTPAddConstCharToBuffer(&(UECShttpHeadContentType[0]));
	HTTPAddConstCharToBuffer(&(UECShttpHeadConnection[0]));
 
	HTTPAddConstCharToBuffer(&(UECShtmlHEADER[0]));
	HTTPAddConstCharToBuffer(&(UECShtmlNORMAL[0]));

 	if(U_orgAttribute.status & STATUS_MEMORY_LEAK){
		HTTPAddConstCharToBuffer(&(UECShtmlATTENTION_INTERR[0]));
	}

 	if(U_orgAttribute.status & STATUS_SAFEMODE){
		HTTPAddConstCharToBuffer(&(UECShtmlATTENTION_SAFEMODE[0]));
	}

	HTTPAddConstCharToBuffer(&(U_name[0]));
	HTTPAddConstCharToBuffer(&(UECShtmlTITLECLOSE[0]));
	HTTPAddConstCharToBuffer(&(UECShtml1A[0]));   // </script></HEAD><BODY><CENTER><H1>
}
//-----------------------------------------------------------
// エラーページの表示
void HTTPsendPageError(){

  HTTPPrintHeader();
  HTTPAddConstCharToBuffer(&(UECSpageError[0]));      
  HTTPAddConstCharToBuffer(&(UECShtmlH1CLOSE[0]));
  HTTPAddConstCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
  HTTPCloseBuffer();
}
//-------------------------------------------------------------
// ccm editページ
void HTTPsendPageIndex(){

  HTTPPrintHeader();
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddConstCharToBuffer(&(UECShtmlH1CLOSE[0]));
  HTTPAddConstCharToBuffer(&(UECShtmlIndex[0]));
  HTTPAddConstCharToBuffer(&(UECShtmlHR[0]));
  HTTPAddConstCharToBuffer(&(U_footnote[0]));

	HTTPAddConstCharToBuffer(&(LastUpdate[0]));
	HTTPAddConstCharToBuffer(&(ProgramDate[0]));
	HTTPAddConstCharToBuffer(&(UECSTxtPartHyphen[0]));
	HTTPAddConstCharToBuffer(&(ProgramTime[0]));

  HTTPAddConstCharToBuffer(&(UECShtmlHTMLCLOSE[0]));
  HTTPCloseBuffer();

#ifdef DEBUG_MOD
  // バッファ内容を確認
  Serial.println("=== UECSbuffer dump ===");
  Serial.println(UECSbuffer);
#endif

#if 0	// サーバーとして動作していることを確認する為の簡単な表示 commented by S.Nakamura
	// thmlを返信できる下処理
	ClearMainBuffer();
	// httpレスポンス  
	HTTPAddPGMCharToBuffer(&(UECShttpHead200_OK[0]));
	HTTPAddPGMCharToBuffer(&(UECShttpHeadContentType[0]));
	HTTPAddPGMCharToBuffer(&(UECShttpHeadConnection[0]));
	
	String html = "";
	html  = "<!DOCTYPE html>";
	html += "<html lang=\"en\">";
	html += "<head>";
	html += "	<meta charset=\"UTF-8\">";
	html += "	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
	html += "	<title>testdisplay</title>";
	html += "</head>";
	html += "<body>";
	html += "	<h1>test indicate page</h1>";
	html += "	<p>this is test page</p>";
	html += "	<a href=\"/\">test link</a>";
	html += "</body>";
	html += "</html>";
	
	// 何が送信されるかダンプして確認
	Serial.println("=== html display dump ===");
  Serial.println(html);

	HTTPAddPGMCharToBuffer(html.c_str());
  
  HTTPCloseBuffer();
#endif	
}

//--------------------------------------------------
// LAN設定ページの表示
void HTTPsendPageLANSetting(){

	// 確認用の表示
	// Serial.println("HTTPsendPageLANSetting satart");
  UECSinitOrgAttribute();

  HTTPPrintHeader();
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddConstCharToBuffer(UECShtmlH1CLOSE); 
  HTTPAddConstCharToBuffer(UECShtmlH2TAG);
  HTTPAddConstCharToBuffer(UECShtmlLANTITLE);
  HTTPAddConstCharToBuffer(UECShtmlH2CLOSE);  
  HTTPAddConstCharToBuffer(&(UECShtmlLAN2[0]));  // <form action=\"./h2.htm\" name=\"f\"><p>
  HTTPAddConstCharToBuffer(&(UECShtmlLAN3A[0]));   // address: 
 byte UECStempByte[4];
  for(int i = 0; i < 4; i++){
    UECStempByte[i] = U_orgAttribute.ip[i];
  }
  HTTPprintIPtoHtml(UECStempByte); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddConstCharToBuffer(&(UECShtmlLAN3B[0]));  // subnet: 
  HTTPprintIPtoHtml( U_orgAttribute.subnet); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddConstCharToBuffer(&(UECShtmlLAN3C[0]));  // gateway: 
  HTTPprintIPtoHtml( U_orgAttribute.gateway); // XXX:XXX:XXX:XXX <br>
  
  HTTPAddConstCharToBuffer(&(UECShtmlLAN3D[0]));  // dns: 
  HTTPprintIPtoHtml( U_orgAttribute.dns); // XXX:XXX:XXX:XXX <br>

  HTTPAddConstCharToBuffer(&(UECShtmlLAN3E[0]));  // mac:
	// macアドレスの表示 
  sprintf(UECStempStr20, "%02X%02X%02X%02X%02X%02X", U_orgAttribute.mac[0], U_orgAttribute.mac[1], U_orgAttribute.mac[2], U_orgAttribute.mac[3], U_orgAttribute.mac[4], U_orgAttribute.mac[5]);
  UDPAddCharToBuffer(UECStempStr20);

  /*
  HTTPAddPGMCharToBuffer(UECShtmlH2TAG);// <H2>
  HTTPAddPGMCharToBuffer(UECShtmlUECSTITLE); // UECS
  HTTPAddPGMCharToBuffer(UECShtmlH2CLOSE); // </H2>
  
  HTTPAddPGMCharToBuffer(UECShtmlRoom);   
  HTTPsetInput( U_orgAttribute.room);  
  HTTPAddPGMCharToBuffer(UECShtmlRegion);
  HTTPsetInput( U_orgAttribute.region);  
  HTTPAddPGMCharToBuffer(UECShtmlOrder);
  HTTPsetInput( U_orgAttribute.order);
  HTTPAddPGMCharToBuffer(&(UECSbtrag[0]));
  */

  HTTPAddConstCharToBuffer(&(UECShtmlUECSID[0]));  // uecsid:
  UDPAddConstCharToBuffer(&(U_uecsid[0]));
  
  HTTPAddConstCharToBuffer(UECShtmlH2TAG);// <H2>
  HTTPAddConstCharToBuffer(UECShtmlNAMETITLE); //Node Name
  HTTPAddConstCharToBuffer(UECShtmlH2CLOSE); // </H2>
  
  HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
  HTTPAddCharToBuffer(U_nodename);
  HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE19);
  HTTPAddConstCharToBuffer(&(UECSbtrag[0]));  
  
  HTTPAddConstCharToBuffer(&(UECShtmlSUBMIT[0])); // <input name=\"b\" type=\"submit\" value=\"send\">
  HTTPAddConstCharToBuffer(&(UECSformend[0]));   //</form>
  
	// 再起動が必要な際は表示
	if(U_orgAttribute.status & STATUS_NEEDRESET){
		HTTPAddConstCharToBuffer(&(UECShtmlATTENTION_RESET[0]));
	}
		 
  
  HTTPAddConstCharToBuffer(&(UECShtmlRETURNINDEX[0]));
  HTTPAddConstCharToBuffer(&(UECShtmlHTMLCLOSE[0]));    //</BODY></HTML>
  HTTPCloseBuffer();
	// Serial.println("HTTPsendPageLANSetting done");	// 確認用の表示

}

//--------------------------------------------------
// ccmのステータスの表示
void HTTPsendPageCCM(){

 	HTTPPrintHeader();
	HTTPAddCharToBuffer(U_nodename); 

	HTTPAddConstCharToBuffer(&(UECShtmlCCMRecRes0[0])); // </H1><H2>CCM Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Info</TH><TH>S/R</TH><TH>Type</TH><TH>SR Lev</TH><TH>Value</TH><TH>Valid</TH><TH>Sec</TH><TH>Atr</TH><TH>IP</TH></TR>

	// 各CCMをテーブルとして表示
	for(int i = 0; i < U_MAX_CCM; i++){
		if(U_ccmList[i].ccmLevel != NONE){
 	
			HTTPAddConstCharToBuffer(&(UECStrtd[0])); //<tr><td>
			// ccmの名前を出力
			HTTPAddConstCharToBuffer(U_ccmList[i].name); 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>
			
			// sender / receiver
			if(U_ccmList[i].sender){
				HTTPAddConstCharToBuffer(UECSTxtPartS);
			}else{
				HTTPAddConstCharToBuffer(UECSTxtPartR);
			} 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>

			// type文字列
			HTTPAddCharToBuffer(U_ccmList[i].typeStr);
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td> 
			// CCMレベル
			HTTPAddConstCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel])); 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td> 

			// value
			dtostrf(((double)U_ccmList[i].value) / pow(10, U_ccmList[i].decimal), -1, U_ccmList[i].decimal,UECStempStr20);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>

			// validフラグ(受信モードのとき表示)
			if(U_ccmList[i].sender){
				/*
				if(U_ccmList[i].validity)
				{HTTPAddPGMCharToBuffer(UECSTxtPartSend);}
				else
				{HTTPAddPGMCharToBuffer(UECSTxtPartStop);}
				*/
			}
			else{
				if(U_ccmList[i].validity){
					HTTPAddConstCharToBuffer(UECSTxtPartOK);
				}
				else{
					HTTPAddConstCharToBuffer(UECSTxtPartHyphen);
				}
			} 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td> 

			// 最終受信からの経過秒数（１０時間以内）
			if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false){

				if(U_ccmList[i].recmillis<36000000){
					HTTPAddValueToBuffer(U_ccmList[i].recmillis / 1000);
				}
			}
			else{
			//over 10hour
			} 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td> 

			// 属性の表示
			if(U_ccmList[i].flagStimeRfirst && U_ccmList[i].sender == false){

				sprintf(UECStempStr20, "%d-%d-%d-%d", U_ccmList[i].attribute[AT_ROOM], U_ccmList[i].attribute[AT_REGI], U_ccmList[i].attribute[AT_ORDE], U_ccmList[i].attribute[AT_PRIO]);
				HTTPAddCharToBuffer(UECStempStr20); 
			}
			sprintf(UECStempStr20, "(%d-%d-%d)", U_ccmList[i].baseAttribute[AT_ROOM], U_ccmList[i].baseAttribute[AT_REGI], U_ccmList[i].baseAttribute[AT_ORDE]); 
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td> 

			// ipアドレスをリンク付きで表示
			HTTPAddConstCharToBuffer(&(UECSAHREF[0])); 
			sprintf(UECStempStr20, "%d.%d.%d.%d", 
							U_ccmList[i].address[0], 
							U_ccmList[i].address[1], 
							U_ccmList[i].address[2], 
							U_ccmList[i].address[3]); 
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECSTagClose[0]));
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECSSlashA[0]));

			HTTPAddConstCharToBuffer(&(UECStdtr[0])); //</td></tr> 

		} //NONE route
 	}
 
	HTTPAddConstCharToBuffer(&(UECShtmlTABLECLOSE[0])); //</TBODY></TABLE>
 
	// ユーザー定義データ表示
	if(U_HtmlLine>0) {
		HTTPAddConstCharToBuffer(&(UECShtmlUserRes0[0])); // </H1><H2>Status</H2><TABLE border=\"1\"><TBODY align=\"center\"><TR><TH>Name</TH><TH>Val</TH><TH>Unit</TH><TH>Detail</TH></TR>

		for(int i = 0; i < U_HtmlLine; i++){
			
			HTTPAddConstCharToBuffer(&(UECStrtd[0]));
			HTTPAddConstCharToBuffer(U_html[i].name);
			HTTPAddConstCharToBuffer(&(UECStdtd[0]));
			// only value
	
			if(U_html[i].pattern == UECSSHOWDATA){

				if(U_html[i].decimal != 0){
					dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);
				}else{
					sprintf(UECStempStr20, "%ld", *(U_html[i].data));
				}
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddConstCharToBuffer(&(UECShtmlInputHidden0[0]));
			}
			else if(U_html[i].pattern == UECSINPUTDATA){

				HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
				dtostrf(((double)*U_html[i].data) / pow(10, U_html[i].decimal), -1, U_html[i].decimal,UECStempStr20);
				HTTPAddCharToBuffer(UECStempStr20);
				HTTPAddConstCharToBuffer(UECSSlashTagClose);
			}
			else if(U_html[i].pattern == UECSSELECTDATA){

				HTTPAddConstCharToBuffer(&(UECShtmlSelect[0]));

				for(int j = 0; j < U_html[i].selectnum; j++){
			
					HTTPAddConstCharToBuffer(&(UECShtmlOption[0]));
					HTTPAddValueToBuffer(j);
					
					if((int)(*U_html[i].data) == j){
						HTTPAddConstCharToBuffer(UECSTxtPartSelected);
					}
					else{ 
						HTTPAddConstCharToBuffer(UECSTagClose);
					}
					HTTPAddConstCharToBuffer(U_html[i].selectname[j]);//************
				} 
				HTTPAddConstCharToBuffer(&(UECShtmlSelectEnd[0])); 
			}
			else if(U_html[i].pattern == UECSSHOWSTRING){
				HTTPAddConstCharToBuffer(U_html[i].selectname[(int)*(U_html[i].data)]);//************
				HTTPAddConstCharToBuffer(&(UECShtmlInputHidden0[0])); 
			}

			// 単位
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); 
			HTTPAddConstCharToBuffer(U_html[i].unit); 
			HTTPAddConstCharToBuffer(&(UECStdtd[0])); 
			HTTPAddConstCharToBuffer(U_html[i].detail); 
			HTTPAddConstCharToBuffer(&(UECStdtr[0])); //</td></tr> 
		}

		HTTPAddConstCharToBuffer(&(UECShtmlTABLECLOSE[0])); //</TBODY></TABLE>
		HTTPAddConstCharToBuffer(&(UECShtmlSUBMIT[0])); // <input name=\"b\" type=\"submit\" value=\"send\">
		HTTPAddConstCharToBuffer(&(UECSformend[0])); //</form> 
 	}
	HTTPAddConstCharToBuffer(&(UECShtmlRETURNINDEX[0])); //<P align=\"center\">return <A href=\"index.htm\">Top</A></P>
	HTTPAddConstCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
 
 	HTTPCloseBuffer();
}

//--------------------------------------------------
// 指定されたCCM IDの編集フォームを含むHTMLページの表示
void HTTPsendPageEDITCCM(int ccmid){

	HTTPPrintHeader();
	HTTPAddCharToBuffer(U_nodename);

	HTTPAddConstCharToBuffer(&(UECShtmlEditCCMTableHeader[0]));

	// 各CCMエントリを表示
	for(int i = 0; i < U_MAX_CCM; i++){
	
		HTTPAddConstCharToBuffer(&(UECStrtd[0])); //<tr><td>

		//ccm name
		HTTPAddConstCharToBuffer(U_ccmList[i].name);
		HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>
		// 送信受信判断
	 	if(U_ccmList[i].sender){
	 		HTTPAddConstCharToBuffer(UECSTxtPartS);
	 	}else{
	 		HTTPAddConstCharToBuffer(UECSTxtPartR);
	 	} 
		//level
		HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>
		HTTPAddConstCharToBuffer((UECSCCMLEVEL[U_ccmList[i].ccmLevel]));
		
		//unit
		HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>
		HTTPAddConstCharToBuffer(U_ccmList[i].unit);
		HTTPAddConstCharToBuffer(&(UECStdtd[0])); //</td><td>
	
		// 編集対象行（ccmidが一致してccmが有効な場合）
		if(i==ccmid && U_ccmList[i].ccmLevel != NONE){

			HTTPAddConstCharToBuffer(&(UECShtmlInputHiddenValue[0])); //hidden value(ccmid)
			sprintf(UECStempStr20,"%d",i);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECSSlashTagClose[0]));
				
			//room region order
			HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
			sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_ROOM]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE3);

			HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
			sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_REGI]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE3);

			HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
			sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_ORDE]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE3);
			
			//priority
			HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
			sprintf(UECStempStr20,"%d",U_ccmList[i].baseAttribute[AT_PRIO]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE3);
			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			
			//TypeInput
			HTTPAddConstCharToBuffer(&(UECShtmlInputText[0]));
			HTTPAddCharToBuffer(U_ccmList[i].typeStr);
			HTTPAddConstCharToBuffer(UECShtmlINPUTCLOSE19);

			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			//default
			HTTPAddConstCharToBuffer(U_ccmList[i].type);

			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			//submit btn
			HTTPAddConstCharToBuffer(&(UECShtmlSUBMIT[0])); 
		}
		else{
			
			sprintf(UECStempStr20,"%d-%d-%d-%d",U_ccmList[i].baseAttribute[AT_ROOM],U_ccmList[i].baseAttribute[AT_REGI],U_ccmList[i].baseAttribute[AT_ORDE],U_ccmList[i].baseAttribute[AT_PRIO]);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			HTTPAddCharToBuffer(U_ccmList[i].typeStr);
			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			HTTPAddConstCharToBuffer(U_ccmList[i].type);
			HTTPAddConstCharToBuffer(&(UECStdtd[0]));//</td><td>
			
			//Edit Link
			HTTPAddConstCharToBuffer(&(UECSAHREF3[0]));
			sprintf(UECStempStr20,"%d",i);
			HTTPAddCharToBuffer(UECStempStr20);
			HTTPAddConstCharToBuffer(&(UECSTagClose[0]));
			HTTPAddConstCharToBuffer(&(UECShtmlEditCCMEditTxt[0]));
			HTTPAddConstCharToBuffer(&(UECSSlashA[0])); //</a> 
		}
		HTTPAddConstCharToBuffer(&(UECStdtr[0])); //</td><tr>

	}

	HTTPAddConstCharToBuffer(&(UECShtmlTABLECLOSE[0])); 
	HTTPAddConstCharToBuffer(&(UECSformend[0]));

	// 編集リンクボタン
	HTTPAddConstCharToBuffer(&(UECShtmlEditCCMCmdBtn1[0]));
	
	// 属性の表示
	sprintf(UECStempStr20,"%d-%d-%d-%d",
					U_ccmList[ccmid].baseAttribute[AT_ROOM],
					U_ccmList[ccmid].baseAttribute[AT_REGI],
					U_ccmList[ccmid].baseAttribute[AT_ORDE],
					U_ccmList[ccmid].baseAttribute[AT_PRIO]);
	HTTPAddCharToBuffer(UECStempStr20);
	HTTPAddConstCharToBuffer(&(UECShtmlEditCCMCmdBtn2[0]));

	// topページに戻るリンク
	HTTPAddConstCharToBuffer(&(UECShtmlRETURNINDEX[0])); 

	//Javascript
	HTTPAddConstCharToBuffer(&(UECShtmlEditCCMCmdBtnScript1[0])); 
	sprintf(UECStempStr20,"%d",ccmid+100);
	HTTPAddCharToBuffer(UECStempStr20);
	HTTPAddConstCharToBuffer(&(UECShtmlEditCCMCmdBtnScript2[0])); 

	HTTPAddConstCharToBuffer(&(UECShtmlHTMLCLOSE[0])); 
  
 	HTTPCloseBuffer();
}
//---------------------------------------------####################
//---------------------------------------------####################
//---------------------------------------------####################
// CCMの設定ページ
void HTTPGetFormDataCCMPage(){

	if(U_HtmlLine==0){return;}

	int i;
	int startPos=0;
	int progPos=0;
	long tempValue;
	unsigned char tempDecimal;

	// HTML構成で定義された行数分ループ
	for(i=0;i<U_HtmlLine;i++){

		// Lのようなキーワードをバッファから検索
		if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){continue;}
		startPos+=progPos;

		// 入力欄の型が数値入力の場合
		if(U_html[i].pattern == UECSINPUTDATA){

			// フォームから数値を取得（小数も対応）
			if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){return;}
			startPos+=progPos;
			// &区切りが無ければ不正
			if(UECSbuffer[startPos]!='&'){return;}//last '&' not found

			// 桁数調整
			int dif_decimal=U_html[i].decimal-tempDecimal;
			if(dif_decimal>=0){
				*U_html[i].data = tempValue*pow(10,dif_decimal);
			}
			else{
				*U_html[i].data = tempValue/pow(10,-dif_decimal);
			}
			// 入力値を最小値・最大値で制限
			if(*U_html[i].data<U_html[i].minValue){
				*U_html[i].data=U_html[i].minValue;
			}
			if(*U_html[i].data>U_html[i].maxValue){
				*U_html[i].data=U_html[i].maxValue;
			}
		}
		// 入力用のセレクトボックス（整数しか許可しない）
		else if(U_html[i].pattern == UECSSELECTDATA){
					
			if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&tempValue,&tempDecimal,&progPos)){return;}
				startPos+=progPos;
					
			// &が無ければ不正
			if(UECSbuffer[startPos]!='&'){return;}//last '&' not found
			
			if(tempDecimal!=0){return;}
			*U_html[i].data=tempValue%U_html[i].selectnum;//cut too big value
		}	
	}
	
	// webフォームから入力されたときに実行される関数
	OnWebFormRecieved();
	
	// 値をEEPROMに保存
	for(i = 0; i < U_HtmlLine; i++){
		UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));
	}
}

// CCMの属性を変更
int HTTPGetFormDataEDITCCMPage(){

	int i;
	int startPos=0;	// バッファ内の現在位置を確認
	int progPos=0;	// パースで進んだバイト数
	unsigned char tempDecimal;
	long ccmid,room,region,order,priority;

	// 最初のキー（ccmid）の 'L=' を探す
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return -1;}
	startPos+=progPos;

	// CCCMIDの値を取得（整数のみを許容）
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){return -1;}
	startPos+=progPos;
	//if(UECSbuffer[startPos]!='&'){return -1;}//last '&' not found
	if(tempDecimal!=0){return -1;}	// 整数以外はエラー処理
	
	//Room
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&room,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	room=constrain(room,0,127);	// 範囲制限
	
	//Region
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&region,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	region=constrain(region,0,127);
	
	//Order
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&order,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	order=constrain(order,0,30000);
	
	//Priority
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&priority,&tempDecimal,&progPos)){return ccmid;}
	startPos+=progPos;
	if(UECSbuffer[startPos]!='&'){return ccmid;}//last '&' not found
	if(tempDecimal!=0){return ccmid;}
	priority=constrain(priority,0,30);
	
	// 取得した値をccmにセット
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=room;
	U_ccmList[ccmid].baseAttribute[AT_REGI]=region;
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=order;
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=priority;
	
	// EEPROMに保存
	UECS_EEPROM_SaveCCMAttribute(ccmid);
	
	 //---------------------------Type
	// typestrの取得
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ccmid;}
	startPos+=progPos;
	
	//copy type
	// 文字列のコピー英数字と `.` `_` のみ許容？）
	int count=0;
	for(i=0;i<MAX_CCMTYPESIZE;i++){
		if(UECSbuffer[startPos+i]=='&'){break;}	// フィールド終端
		if(UECSbuffer[startPos+i]=='\0' || i==MAX_CCMTYPESIZE){return ccmid;}//���[���Ȃ�

		// 許可された文字列のみを通す
		if( (UECSbuffer[startPos+i]>='0' && UECSbuffer[startPos+i]<='9')||
			(UECSbuffer[startPos+i]>='A' && UECSbuffer[startPos+i]<='Z')||
			(UECSbuffer[startPos+i]>='a' && UECSbuffer[startPos+i]<='z')||
			 UECSbuffer[startPos+i]=='.' || UECSbuffer[startPos+i]=='_' ){
		}
		else{
			UECSbuffer[startPos+i]='x';	// 不正文字はxに置き換え
		}		

		UECStempStr20[i]=UECSbuffer[startPos+i];
		count++;
	}
		
	UECStempStr20[i]='\0';//set end code　（null終端）

	// 最小長3、最大長19の間を有効とみなす
	if(count>=3 && count<=19){
		
		strcpy(U_ccmList[ccmid].typeStr,UECStempStr20);
		UECS_EEPROM_SaveCCMType(ccmid);
	}
	
	// 成功した時は処理したCCMIDを返す
	return ccmid;
}

//--------------------------------------------------------------------------
// ネットワークの設定が変更されたときに反映する
void HTTPGetFormDataLANSettingPage(){

	int i;
	int startPos=0;	// uecsbuffer内での位置の把握
	int progPos=0;	// パターンマッチングで使用する一次的な場所
	long UECStempValue[16];	// IPアドレスを一次的に格納する配列
	unsigned char tempDecimal;	// 小数点以下の把握（利用しない）
  
	//MYIP      4
	//SUBNET    4
	//GATEWAY   4
	//DNS       4
	//-------------
	//total    16

	// IPアドレス、サブネットマスク、ゲートウェイ、DNSの各4バイト（計16バイト）を取得
	for(i=0;i<16;i++){
		// "L=" というパラメータ識別子を検索
		if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ;}
		startPos+=progPos;
		// 数値を取得し、tempDecimalに小数点以下の値を格納（ここでは整数のみを想定）
		if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&UECStempValue[i],&tempDecimal,&progPos)){return ;}
		startPos+=progPos;
		// '&' で区切られていることを確認
		if(UECSbuffer[startPos]!='&'){return ;}//last '&' not found
		// 小数点以下の値が0であることを確認（整数であることを期待）
		if(tempDecimal!=0){
			// Serial.println("It has a decimal point.");  // 確認用の表示
			return ;
		}
		
		// 値が0〜255の範囲内であることを確認（IPアドレスの各オクテットの範囲）
		if(UECStempValue[i]<0 || UECStempValue[i]>255){
			// Serial.println("octet out of range");   // 確認用の表示
			return ;
		}
	}

	// EEPROMに保存されている値と比較し、異なる場合のみ書き込みを行う
	for(int i = 0; i < 16; i++){

		//skip same value
		if(EEPROM.read(EEPROM_DATATOP + i)!=(unsigned char)UECStempValue[i]){
			EEPROM.write(EEPROM_DATATOP + i, (unsigned char)UECStempValue[i]);
			U_orgAttribute.status |= STATUS_NEEDRESET;	// 設定変更を表示すフラグを設定       	        
		}		
	}
	EEPROM.commit();	// フラッシュへの書き込み（場合分けが必要？）
	
	// ノード名の取得
	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return ;}
	startPos+=progPos;
	
	// ノード名を一時的な文字列にコピー
	for(i=0;i<20;i++){
	
		// HTMLタグの開始・終了文字を無効化
		if(UECSbuffer[startPos+i]=='<' || UECSbuffer[startPos+i]=='>'){
			UECSbuffer[startPos+i]='*';
		}

		// '&' で区切られていることを確認
		if(UECSbuffer[startPos+i]=='&'){break;}

			// 文字列の終端または最大長に達した場合は終了
		if(UECSbuffer[startPos+i]=='\0' || i==19){return;}//�I�[�������̂Ŗ���
		
		// UTF-8のマルチバイト文字の途中で切れないようにする
		if(i>=16 && (unsigned char)UECSbuffer[startPos+i]>=0xC0){

			break;
		}

		UECStempStr20[i]=UECSbuffer[startPos+i];
	}

	UECStempStr20[i]='\0';// set end code
	
	// ノード名をEEPROMに保存
	for(int i = 0; i < 20; i++){
		U_nodename[i]=UECStempStr20[i];
		
		//skip same value
		if(EEPROM.read(EEPROM_NODENAME + i)!=U_nodename[i]){
			EEPROM.write(EEPROM_NODENAME + i, U_nodename[i]);
		}
			
	}
	EEPROM.commit();	// フラッシュへの書き込み

	// Serial.println("finish lan setting page");   // 確認用の表示
	return ;

}

//--------------------------------------------------------------------
// CCMの設定の変更を反映
void HTTPGetFormDataFillCCMAttributePage(){
	
	int i;
	int startPos=0;
	int progPos=0;
	unsigned char tempDecimal;
	long ccmid;

	if(!UECSFindConstChar(&UECSbuffer[startPos],UECSaccess_LEQUAL,&progPos)){return;}
	startPos+=progPos;
	if(!UECSGetFixedFloatValue(&UECSbuffer[startPos],&ccmid,&tempDecimal,&progPos)){return;}
	startPos+=progPos;
	if(tempDecimal!=0){return;}
	ccmid-=100;
	if(ccmid<0 || ccmid>=U_MAX_CCM){return;}
	
	for(i=0;i<U_MAX_CCM;i++){
		U_ccmList[i].baseAttribute[AT_ROOM]=U_ccmList[ccmid].baseAttribute[AT_ROOM];
		U_ccmList[i].baseAttribute[AT_REGI]=U_ccmList[ccmid].baseAttribute[AT_REGI];
		U_ccmList[i].baseAttribute[AT_ORDE]=U_ccmList[ccmid].baseAttribute[AT_ORDE];
		U_ccmList[i].baseAttribute[AT_PRIO]=U_ccmList[ccmid].baseAttribute[AT_PRIO];
		UECS_EEPROM_SaveCCMAttribute(i);
	}
}

//---------------------------------------------####################
// HTTPリクエストのチェック
void HTTPcheckRequest(){
	// UECSclient=UECSlogserver.available();	// comment out by S.Nakamura
	UECSclient=UECSlogserver.accept();				// added by S.Nakamura

	// 最大で299文字しか受信できず、それ以上は無視される
	if(UECSclient){
			
		// バッファにhttpリクエストを読み込んで、終端にnullを付ける
		// 例外処理を入れる必要がある
		UECSbuffer[UECSclient.read((uint8_t *)UECSbuffer,BUF_SIZE-1)]='\0';
#ifdef DEBUG_MOD
		// HTTPリクエスト全文をシリアル出力でダンプ ★★★
		Serial.println("");
		Serial.println("=== HTTP Request Dump Start ===");
		Serial.println(UECSbuffer);
		Serial.println("=== HTTP Request Dump End ===");
#endif
		// リクエストの先頭行を抽出し、ノイズや不要な文字列を削除
		HTTPFilterToBuffer();//Get first line before "&S=" and eliminate unnecessary code
		int progPos = 0;

		if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP0[0]),&progPos)){
			HTTPsendPageIndex();
		}
 
		// CCMステータス表示ページ
		else if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1[0]),&progPos)){
			HTTPsendPageCCM();
		}

		// LAN設定表示ページ
		else if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2[0]),&progPos)){
			HTTPsendPageLANSetting();
		}

		// CCM設定フォームの送信結果を処理し、リダイレクト
		else if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP1A[0]),&progPos)){
			HTTPGetFormDataCCMPage();
			HTTPPrintRedirect(1);
		}
		
		// LAN設定フォームの送信結果を表示し、設定画面を再表示
		else if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP2A[0]),&progPos)){
			// Serial.println("");	// デバッグ用のメッセージ
			// Serial.println("************get form lan setting page start*******");	// デバッグ用のメッセージ
			HTTPGetFormDataLANSettingPage();
			HTTPsendPageLANSetting();
		}
		
		// CCM編集ページからのフォーム処理とリダイレクト
		else if(UECSFindConstChar(UECSbuffer,&(UECSaccess_NOSPC_GETP3A[0]),&progPos)){

			int ccmid=HTTPGetFormDataEDITCCMPage();
			
			// タイプの全リセット(ccmidが999の時)
			if(ccmid==999){
				for(int i=0;i<U_MAX_CCM;i++){
					strcpy_P(U_ccmList[i].typeStr, U_ccmList[i].type);
					UECS_EEPROM_SaveCCMType(i);
				}
				HTTPPrintRedirect(3);
			}
			//Attribute Reset
			// 指定されたCCM IDの属性を他に適用
			else if(ccmid-100>=0 && ccmid-100<U_MAX_CCM){
				HTTPGetFormDataFillCCMAttributePage();
				HTTPPrintRedirect(3);
			}
			
			// 不正なCCM ID
			else if(ccmid<0 || ccmid>=U_MAX_CCM){HTTPsendPageError();}
			// CCMの編集ページの表示
			else{HTTPsendPageEDITCCM(ccmid);}
			
		}
		
		else {
			HTTPsendPageError();
		}
	}
	// クライアント接続を閉じる
	UECSclient.stop();
}



//----------------------------------
// UECS CCM受信
// 第一引数：一次的に受信する受信データ構造体
// 第二引数：システム次項
void UECSupdate16520portReceive( UECSTEMPCCM* _tempCCM, unsigned long _millis){
  
	// UDPのパケットサイズの把握
	int packetSize = UECS_UDP16520.parsePacket();
  int matchCCMID=-1;
  if(packetSize){
  	  
   	ClearMainBuffer();
    _tempCCM->address = UECS_UDP16520.remoteIP();		// 送信元のIPアドレスを取得し、受信CCMのアドレスとして保存
    UECSbuffer[UECS_UDP16520.read(UECSbuffer, BUF_SIZE-1)]='\0';	// パケットの読み込み
    UDPFilterToBuffer();	// 不要な文字をフィルタ

		// パースしたデータが有効であれば当該IDのCCMに更新を試みる
    if(UECSparseRec( _tempCCM,&matchCCMID)){
			UECScheckUpDate( _tempCCM, _millis,matchCCMID);
		}
  }
}
//----------------------------------
// ノードスキャン、CCMスキャンに対する対応
void UECSupdate16529port( UECSTEMPCCM* _tempCCM){

  int packetSize = UECS_UDP16529.parsePacket();
  
	if(packetSize){
   	   
		ClearMainBuffer();
		_tempCCM->address = UECS_UDP16529.remoteIP();   
		UECSbuffer[UECS_UDP16529.read(UECSbuffer, BUF_SIZE-1)]='\0';
		UDPFilterToBuffer();
	  
		// ノードスキャンかCCMスキャンか判断し応答メッセージを準備 
		if(UECSresNodeScan()){
			UECS_UDP16529.beginPacket(_tempCCM->address, 16529);
			UECS_UDP16529.write(UECSbuffer);
			
			// パケット送信の終了処理（失敗したときはイーサネットのリセット）
			if(UECS_UDP16529.endPacket()==0){
  			UECSresetEthernet(); //when udpsend failed,reset ethernet status
			}
		}     
	}
}

//----------------------------------
// CCMサーチに対するレスポンス
void UECSupdate16521port( UECSTEMPCCM* _tempCCM){

  int packetSize = UECS_UDP16521.parsePacket();
	if(packetSize){
   	   
		ClearMainBuffer();
		// CCMサーチへの返信（送信元を応答先として利用）
		_tempCCM->address = UECS_UDP16521.remoteIP();   
		UECSbuffer[UECS_UDP16521.read(UECSbuffer, BUF_SIZE-1)]='\0';
		UDPFilterToBuffer();
		UECSresCCMSearchAndSend(_tempCCM);
	}
}


//----------------------------------
#define         EEPROMAREA	4096	// W55RP20ではEEPROMを使用する際にサイズを指定する必要がある
																	// 4KB確保 comment by S.Nakamura


// テスト関数 added by S.Nakamura
// 保存されたデータをEEPROMから読み出して確認表示
void PrintSavedWebData() {
  // Serial.println("=== Saved Web Data from EEPROM ===");
  for (int i = 0; i < U_HtmlLine; i++) {
		
    long value = UECS_EEPROM_readLong(EEPROM_WEBDATA + i * 4);
	
		// 確認用の表示
		// Serial.println("print web data"); 
    // Serial.print("Index ");
    // Serial.print(i);
    // Serial.print(": ");
    // Serial.println(value);

  }
}

// -----------------------------------------------------------
void UECSsetup(){

  // Serial.println("uecs set up start");

	EEPROM.begin(EEPROMAREA);		// eepromの初期化
  // Serial.println("EEPROM initialized");
#ifdef DEBUG_MOD
	PrintSavedWebData();	// eepromのデータを読み出して確認
#endif
  UECSCheckProgramUpdate();
	delay(300);

	// safe mode 確認用
  pinMode(U_InitPin,INPUT_PULLUP);
  
	// safe modeかどうか確認する
	if(digitalRead(U_InitPin) == U_InitPin_Sense || UECS_EEPROM_readLong(EEPROM_IP)==-1){
	
		U_orgAttribute.status|=STATUS_SAFEMODE;
    // Serial.println("start safe mode");		// 確認用の出力
	}else{
		
		U_orgAttribute.status&=STATUS_SAFEMODE_MASK;//Release safemode
		// Serial.println("start musk safe mode");		// 確認用の出力
	}

  UECSinitOrgAttribute();
  UECSinitCCMList();
  UserInit();						// uecs ccmをセット
  UECSstartEthernet();	// イーサネットとサーバーを定義

  // Serial.println("uecs set up done");	// 確認用の出力
  // Serial.println("");
}
//---------------------------------------------
// プログラムが更新されたかどうかチェックする
// 最適化されたコード
void UECSCheckProgramUpdate() {
  // プログラムの日付・時刻をバッファに格納
  ClearMainBuffer();
  UDPAddConstCharToBuffer(ProgramDate);
  UDPAddConstCharToBuffer(ProgramTime);

  // 書き込みが必要かどうかをチェックするフラグ
  bool needCommit = false;

  // プログラム日付＋時刻をEEPROMに保存（違いがある場合のみ）
  for (int i = 0; i < (EEPROM_CCMTOP - EEPROM_PROGRAMDATETIME); i++) {
    byte currentEEPROM = EEPROM.read(i + EEPROM_PROGRAMDATETIME);
    byte newValue = UECSbuffer[i];

    // EEPROMの内容と異なれば更新が必要
    if (currentEEPROM != newValue) {
      EEPROM.write(i + EEPROM_PROGRAMDATETIME, newValue);
      needCommit = true;
      U_orgAttribute.status |= STATUS_PROGRAMUPDATE;
    }

    // NULL文字で終了（\0）
    if (newValue == '\0') break;
  }

  // 最後に一度だけcommit
  if (needCommit) {
    EEPROM.commit();  // 実際のフラッシュへの書き込み
		// 確認用の表示
		// Serial.println("uecs program update check eeprom commit");	// 確認用の出力
  }
}


//---------------------------------------------

//--------------------------------------------------------------
// 指定したCCM IDに対するCCMタイプ文字列をEEPROMに保存
// 指定した CCM ID に対応する typeStr（CCMタイプ文字列）を EEPROM に保存（commit は一括）
// ここが想定通りに動作しているかテストが必要
void UECS_EEPROM_SaveCCMType(int ccmid)
{

    // 書き込み先が EEPROM 範囲を超える場合は中止
    if (ccmid * EEPROM_L_CCM_TOTAL + EEPROM_L_CCM_TOTAL > EEPROM_CCMEND) {
        return;
    }


    bool needCommit = false;  // 変更があったかを記録するフラグ

    for (int i = 0; i <= MAX_CCMTYPESIZE; i++) {
        int addr = ccmid * EEPROM_L_CCM_TOTAL + EEPROM_CCMTOP + EEPROM_L_CCM_TYPETXT + i;

        // EEPROM内容と異なる場合だけ書き込み
        if (EEPROM.read(addr) != U_ccmList[ccmid].typeStr[i]) {
            EEPROM.write(addr, U_ccmList[ccmid].typeStr[i]);
            needCommit = true;  // 書き込みが発生したので commit が必要
        }

        // 文字列終端（null）で終了
        if (U_ccmList[ccmid].typeStr[i] == '\0') {
            break;
        }
    }

    // 一度だけ commit を実行（変更があった場合のみ）
    if (needCommit) {
        EEPROM.commit();
				// Serial.println("save ccm type eeprom commit");	// 確認用の出力
    }
}


//--------------------------------------------------------------
// CCMの属性をEEPROMに保存する
void UECS_EEPROM_SaveCCMAttribute(int ccmid){

	
	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
	

	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ROOM]));
	}

	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_REGI]));
	}

	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]));
	}

	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256)){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_ORDE]/256));
	}

	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO)!=(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO])){
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO,(unsigned char)(U_ccmList[ccmid].baseAttribute[AT_PRIO]));
	}
  EEPROM.commit();	// フラッシュへの書き込み
}

//--------------------------------------------------------------
// 指定されたccmidに対してtypestrと属性を読み出してccm listに格納
void UECS_EEPROM_LoadCCMSetting(int ccmid){

	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory

	int i;
	// typestrの読み出し
	for(i=0;i<MAX_CCMTYPESIZE;i++){

		// EEPROMから１バイトずつtypestrに読み出す
		U_ccmList[ccmid].typeStr[i]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i);
		// 未初期化の場合はxを代入して終了
		if(U_ccmList[ccmid].typeStr[i]==255){U_ccmList[ccmid].typeStr[i]='x';break;}
		if(U_ccmList[ccmid].typeStr[i]=='\0'){break;}	// null文字で終了
	}

	// 文字列終端を追加 
	U_ccmList[ccmid].typeStr[i]='\0';
	
	/*
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM) & 127;
	U_ccmList[ccmid].baseAttribute[AT_REGI]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI) & 127;
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=
		(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)+
		EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)*256) & 32767;
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO) & 31;
	U_ccmList[ccmid].attribute[AT_PRIO] =U_ccmList[ccmid].baseAttribute[AT_PRIO];
	*/
	// 各種属性の読み出し
	U_ccmList[ccmid].baseAttribute[AT_ROOM]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ROOM);
	U_ccmList[ccmid].baseAttribute[AT_REGI]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_REGI);
	U_ccmList[ccmid].baseAttribute[AT_ORDE]=(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_L)+EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_ORDE_H)*256);
	U_ccmList[ccmid].baseAttribute[AT_PRIO]=EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_PRIO);

	//Prepare the correct values for the Arduino where the data will be written for the first time.
	if(U_ccmList[ccmid].baseAttribute[AT_ROOM]==0xff){

		U_ccmList[ccmid].baseAttribute[AT_ROOM]=1;
		U_ccmList[ccmid].baseAttribute[AT_REGI]=1;
		U_ccmList[ccmid].baseAttribute[AT_ORDE]=1;
		U_ccmList[ccmid].baseAttribute[AT_PRIO]=U_ccmList[ccmid].attribute[AT_PRIO];
		UECS_EEPROM_SaveCCMAttribute(ccmid);
	}
	
	U_ccmList[ccmid].attribute[AT_PRIO] =U_ccmList[ccmid].baseAttribute[AT_PRIO];
}

//---------------------------------------------
// ユニークなボードIDを読み出してMacアドレスを生成する
void generate_mac_addr (__unused int idx, uint8_t buf[6]) {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    memcpy(buf, &board_id.id[2], 6);
    buf[0] &= (uint8_t)~0x1; // unicast
    buf[0] |= 0x2; // locally administered
}



void UECSstartEthernet(){
  
	resetEthernet();		// イーサネットのリセット

	// Macアドレスの取得
	byte mac[6];
	generate_mac_addr (0, (byte*)mac) ;
	// 確認用のコンソール出力
	// Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
	// 							mac[0],  mac[1], mac[2], mac[3], mac[4], mac[5] );
	
	U_orgAttribute.mac[0] = mac[0];
	U_orgAttribute.mac[1] = mac[1];
	U_orgAttribute.mac[2] = mac[2];
	U_orgAttribute.mac[3] = mac[3];
	U_orgAttribute.mac[4] = mac[4];
	U_orgAttribute.mac[5] = mac[5];

  // モードに応じたネットワークスタート 
  if(U_orgAttribute.status&STATUS_SAFEMODE){
		
  	// Serial.println("safe mode network start");	
  	Ethernet.begin(U_orgAttribute.mac, defaultip, defaultdns,defaultgateway,defaultsubnet);	// デフォルトのネットワーク設定
	}
  else{

		// Serial.println("masked safe mode network start");
		Ethernet.begin(U_orgAttribute.mac, U_orgAttribute.ip, U_orgAttribute.dns, U_orgAttribute.gateway, U_orgAttribute.subnet);
	}

	// サーバーとUDPのポートを開放する
  UECSlogserver.begin();
  UECS_UDP16520.begin(16520);
  UECS_UDP16529.begin(16529);
  UECS_UDP16521.begin(16521);
}


//---------------------------------------------------------
// イーサネットのリセット
void UECSresetEthernet(){

  	UECSstartEthernet();
}


//----------------------------------------------------------------------
// UECSのメイン処理
void UECSloop(){

  // network Check
  // 1. http request
  // 2. udp16520Receive
  // 3. udp16529Receive and Send
  // 4. udp16521Receive and Send
  // << USER MANAGEMENT >>
  // 5. udp16520Send

	// Ethernet.maintain();	// DHCPを使用する時は必要
													// 固定IPでは不要？
  HTTPcheckRequest();
  UECSupdate16520portReceive(&UECStempCCM, UECSnowmillis);
  UECSupdate16529port(&UECStempCCM);
  UECSupdate16521port(&UECStempCCM);
  UserEveryLoop();  

	// 1秒間隔の把握
 	UECSnowmillis = millis();
 if(UECSnowmillis==UECSlastmillis){return;}
 
	//how long elapsed?
	unsigned long td=UECSnowmillis-UECSlastmillis;

	//check overflow 
	// オーバーフローをまたいでからの経過時間も把握
	if(UECSnowmillis<UECSlastmillis){
		td=(4294967295-UECSlastmillis)+UECSnowmillis;
	}

	//over 1msec
	UECSsyscounter1s+=td;
	UECSlastmillis=UECSnowmillis;
	UECSautomaticValidManager(td);	// 有効期間のチェック

	// 1秒を経過していないときは処理を抜ける
  if(UECSsyscounter1s < 999){return;}
	
	//over 1000msec
	UECSsyscounter1s = 0;
	UECSsyscounter60s++;
    
	// 1分周期の処理
	if(UECSsyscounter60s >= 60){
		UserEveryMinute();
		UECSsyscounter60s = 0;  
	}   
    
    
	UECSautomaticSendManager();		// ロードバランシング？受信処理？
	UserEverySecond();
    
	// 条件が揃っているときはUECS CCMを送信する
	for(int i = 0; i < U_MAX_CCM; i++){
		
		if(U_ccmList[i].sender && U_ccmList[i].flagStimeRfirst && U_ccmList[i].ccmLevel != NONE){
			UECSCreateCCMPacketAndSend(&U_ccmList[i]);
		}
	}   
}

//------------------------------------------------------
// EEPROMから各種初期値を読み出しU_orgAttributeにセットする
void UECSinitOrgAttribute(){
	
	// Serial.println("UECSinitOrgAttribute start");
	// ネットワーク設定を読み出す
	for(int i = 0; i < 4; i++){

		U_orgAttribute.ip[i]		= EEPROM.read(i + EEPROM_IP);
		U_orgAttribute.subnet[i]	= EEPROM.read(i + EEPROM_SUBNET);
		U_orgAttribute.gateway[i]	= EEPROM.read(i + EEPROM_GATEWAY);
		U_orgAttribute.dns[i]		= EEPROM.read(i + EEPROM_DNS);
	}


	//reset web form
	for(int i = 0; i < U_HtmlLine; i++){

		*(U_html[i].data) = U_html[i].minValue;
	} 


//	  U_orgAttribute.room 	=  EEPROM.read(EEPROM_ROOM);
//	  U_orgAttribute.region =  EEPROM.read(EEPROM_REGION);
//	  U_orgAttribute.order 	=  EEPROM.read(EEPROM_ORDER_L)+ (unsigned short)(EEPROM.read(EEPROM_ORDER_H))*256;
//	 if(U_orgAttribute.order>30000){U_orgAttribute.order=30000;}

	//　セーフモードの場合、ノード名とwebフォームデータの読み込みは行わずに終了 
	if(U_orgAttribute.status & STATUS_SAFEMODE){return;}
	
	// ノード名の読み出し
	for(int i = 0; i < 20; i++){
		U_nodename[i] = EEPROM.read(EEPROM_NODENAME + i);
	}
		  
	// Serial.println("check HTML line"); // 確認用の表示
	// 各webフォーム項目の値をEEPROMから読み込む
	for(int i = 0; i < U_HtmlLine; i++){
		*(U_html[i].data) = UECS_EEPROM_readLong(EEPROM_WEBDATA + i * 4);
		// デバッグ用の表示
		// Serial.printf("check eeprom web form value %d :", i);
		// Serial.println(*U_html[i].data);
	} 
}

//------------------------------------------------------
// CCMのリストを初期化
void UECSinitCCMList(){
  for(int i = 0; i < U_MAX_CCM; i++){
    U_ccmList[i].ccmLevel = NONE;
    U_ccmList[i].validity = false;
    U_ccmList[i].flagStimeRfirst = false;
    U_ccmList[i].recmillis = 0; 
  }
}

// 指定したCCM番号に対してCCMの属性を初期化・設定する関数
void UECSsetCCM(boolean _sender, signed char _num, const char* _name, const char* _type, const char* _unit, unsigned short _priority, unsigned char _decimal, char _ccmLevel){
	
	// CCM番号が範囲外であれば何もせずに終了
	if(_num > U_MAX_CCM || _num < 0){
    return;
  }

	// 基本属性 
  U_ccmList[_num].sender = _sender;
  U_ccmList[_num].ccmLevel = _ccmLevel;
  U_ccmList[_num].name = _name;    
  U_ccmList[_num].type = _type;
  U_ccmList[_num].unit = _unit;
  U_ccmList[_num].decimal = _decimal;
  U_ccmList[_num].ccmLevel = _ccmLevel;
	
	// IPアドレスを255で初期化 
  U_ccmList[_num].address[0] = 255;
  U_ccmList[_num].address[1] = 255;
  U_ccmList[_num].address[2] = 255;
  U_ccmList[_num].address[3] = 255;
	// 実行時の属性を初期値に刷する（0）
  U_ccmList[_num].attribute[AT_ROOM] = 0;
  U_ccmList[_num].attribute[AT_REGI] = 0;
  U_ccmList[_num].attribute[AT_ORDE] = 0;
  U_ccmList[_num].attribute[AT_PRIO] = _priority;

	// 確認用の表示
	// Serial.print("SetCCM type: ");
	// Serial.println(_type);

  // U_ccmList[_num].baseAttribute[AT_PRIO] = _priority;
  // strcat_P(U_ccmList[_num].typeStr,U_ccmList[_num].type);
	// 安全に typeStr にコピー（strncpy + 終端保証）
	// 理屈的に確認する必要がある
	// 表示用のtype文字列を安全にコピー
  memset(U_ccmList[_num].typeStr, 0, sizeof(U_ccmList[_num].typeStr));
  strncpy(U_ccmList[_num].typeStr, _type, MAX_TYPE_CHAR - 1);
  U_ccmList[_num].typeStr[MAX_TYPE_CHAR - 1] = '\0';

#ifdef DEBUG_MOD
	// 確認用の表示
	Serial.println("");
	Serial.print("U_ccmList[_num].typeStr: ");
	Serial.println(U_ccmList[_num].typeStr);
#endif
	// ここが上手く動いていない可能性が高い
	// Serial.println("!! Typesetting is not working well. !!");
	// プルグラムアップデートフラグが立っている場合はEEPROMにtypeを保存
	if(U_orgAttribute.status&STATUS_PROGRAMUPDATE){
  	UECS_EEPROM_SaveCCMType(_num);
	}

	// EEPROMから属性を読み直してCCMに反映
	UECS_EEPROM_LoadCCMSetting(_num);

#ifdef DEBUG_MOD
	// 確認用の表示
	Serial.print("U_ccmList[_num].typeStr after: ");
	Serial.println(U_ccmList[_num].typeStr);
#endif
  return;
}


//####################################String Buffer control
static int wp;
// バッファをnullで初期化して内容のクリア
void ClearMainBuffer(){

	for(int i=0;i<BUF_SIZE;i++){
		UECSbuffer[i]='\0';
	}
	
	wp=0;	// 書き込みポインタを先頭にリセット
}

//-----------------------------------
// UDPをバッファに詰める
void UDPAddConstCharToBuffer(const char* word){

	for (int i = 0; i <= MAXIMUM_STRINGLENGTH; i++) {
		
		UECSbuffer[wp] = word[i];  // pgm_read_byte() を使わず、直接読み出す
		if (UECSbuffer[wp] == '\0') {
			break;
		}
		wp++;
	}

	MemoryWatching();
}


//-----------------------------------
// UDPをバッファに詰める
void UDPAddCharToBuffer(char* word){

	strcat(UECSbuffer,word);
	wp=strlen(UECSbuffer);
  
	MemoryWatching();
}
//-----------------------------------
// UDPに送信する値を入れる
void UDPAddValueToBuffer(long value){
	sprintf(&UECSbuffer[wp], "%ld", value);
	wp=strlen(UECSbuffer);
	
	MemoryWatching();
}
//-----------------------------------
// html出力の内容をバッファに詰める
void HTTPAddConstCharToBuffer(const char* word){

	for (int i = 0; i < MAXIMUM_STRINGLENGTH; i++){

		if (word[i] == '\0') break;

		UECSbuffer[wp++] = word[i];

		// バッファ上限に達したら自動送信
		if (wp >= BUF_HTTP_REFRESH_SIZE){
			
			// 確認用の表示
#ifdef DEBUG_MOD
			Serial.println("=== constchartoBuffer  dump ===");
			Serial.println(UECSbuffer);
			Serial.print("wp = ");
			Serial.println(wp);
#endif
			HTTPRefreshBuffer();  // wpを0にリセットする処理が中で行われている前提
		}
	}

	MemoryWatching();
}


//---------------------------------------------
// 出力するHTTPレスポンスをバッファに詰める
void HTTPAddCharToBuffer(char* word){

	for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++){

		UECSbuffer[wp]=word[i];
		if(UECSbuffer[wp]=='\0'){
			break;
		}
		wp++;
		//auto send
		if(wp>BUF_HTTP_REFRESH_SIZE){
			HTTPRefreshBuffer();
		}
	}

	MemoryWatching();
}

//---------------------------------------------
// HTTPで出力する値をバッファに詰める
void HTTPAddValueToBuffer(long value){

	sprintf(&UECSbuffer[wp], "%ld", value);
	wp=strlen(UECSbuffer);

	if(wp>BUF_HTTP_REFRESH_SIZE){

		HTTPRefreshBuffer();
	}

	MemoryWatching();
}

// UECSバッファが上限に達した時はクライアントに送信する
void HTTPRefreshBuffer(void){

#ifdef DEBUG_MOD
  // バッファをコンソールに表示して確認
	Serial.println("=== HTTPRefreshBuffer dump ===");
  Serial.println(UECSbuffer);  // バッファの中身を確認
#endif
	UECSbuffer[wp]='\0';
	UECSclient.print(UECSbuffer);
	ClearMainBuffer();
}

// UECSバッファが残っている場合にクライアントに送信
void HTTPCloseBuffer(void){
	
	if(strlen(UECSbuffer)>0){
#ifdef DEBUG_MOD		
		// webページの内容をコンソールで確認
		Serial.println("=== HTTPCloseBuffer dump ===");
		Serial.println(UECSbuffer);  // これを追加
#endif
		UECSclient.print(UECSbuffer);
	}
}
//------------------------------------
//delete \r,\n and contorol code
//replace the continuous space character to 1 space
//Attention:first one character is ignored without delete.
//------------------------------------
// 受信バッファ（UECSバッファ）内の余分なスペースを削除する（XMLを成型）
void UDPFilterToBuffer(void){

	// UECSバッファの長さを取得
	int s_size=strlen(UECSbuffer);
	int write=0,read=0;
	int i,j;

	// バッファの１文字目はスキップして２文字目から処理開始
	for(i=1;i<s_size;i++){

		if(UECSbuffer[i]=='\0'){	//終端文字に達したら処理終了
			break;
		}

		// 制御文字又は連続スペースを検出したとき
		if(UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i]==ASCIICODE_SPACE && UECSbuffer[i-1]==ASCIICODE_SPACE)){
			
			write=i;
			//find end of space
			for(j=write;j<=s_size;j++){
				
				// 連続するスペースの終了位置（非空白文字の先頭）を探す
				if(UECSbuffer[j]>ASCIICODE_SPACE || UECSbuffer[j]=='\0'){

					read=j;
					break;
				}
			}

			// 空白をスキップしてそれ以降のデータを前方に詰めてコピー
			for(j=read;j<=s_size;j++){

				UECSbuffer[write+(j-read)]=UECSbuffer[j];
			}
		}
	}
	
	MemoryWatching();
}

//------------------------------------
//delete space and contorol code for http response
//\r,\n is regarded to end
//Attention:first one character is ignored without delete.
//decode URL like %nn to char code
//------------------------------------
// HTTP受信バッファをフィルタして不正な制御文字の削除、%エンコードのデコードおよび空白の除去を行う
void HTTPFilterToBuffer(void){

	int s_size=strlen(UECSbuffer);
	int write=0,read=0;
	int i,j;

  // %エンコードのデコード処理
  for(i=1;i<s_size;i++){

		// 制御コードや "&S=" で始まるセクションを検出して、バッファを切り捨て
		if((unsigned char)UECSbuffer[i]<ASCIICODE_SPACE || (UECSbuffer[i-1]=='&' && UECSbuffer[i]=='S' && UECSbuffer[i+1]=='=')){
			UECSbuffer[i]='\0';
			break;
		}
		
		// %で始まるURLエンコードを検出
		if(UECSbuffer[i]=='%'){

			unsigned char decode=0;
			// 1文字目をデコード
			if(UECSbuffer[i+1]>='A' && UECSbuffer[i+1]<='F'){
				decode=UECSbuffer[i+1]+10-'A';
			}
			else if(UECSbuffer[i+1]>='a' && UECSbuffer[i+1]<='f'){
				decode=UECSbuffer[i+1]+10-'a';
			}
			else if(UECSbuffer[i+1]>='0' && UECSbuffer[i+1]<='9'){
				decode=UECSbuffer[i+1]-'0';
			}

			// ２文字目をデコード
			if(decode!=0){

				decode=decode*16;
				if(UECSbuffer[i+2]>='A' && UECSbuffer[i+2]<='F'){
					decode+=UECSbuffer[i+2]+10-'A';
				}
				else if(UECSbuffer[i+2]>='a' && UECSbuffer[i+2]<='f'){
					decode+=UECSbuffer[i+2]+10-'a';
				}
				else if(UECSbuffer[i+2]>='0' && UECSbuffer[i+2]<='9'){
					decode+=UECSbuffer[i+2]-'0';
				}
				else {
					decode=0;
				}

				// 特殊文字(&)はデータ構造破壊を防ぐため*に置き換える
				if(decode!=0){

					if(decode=='&'){
						decode='*';
					}
					UECSbuffer[i]=decode;
					UECSbuffer[i+1]=' ';
					UECSbuffer[i+2]=' ';
				}
			}
		}
	}

	// デコード後の長さを再取得
  s_size=strlen(UECSbuffer);
  
	// 空白または制御文字の除去処理
	for(i=1;i<s_size;i++){

		//eliminate tag
		//if(UECSbuffer[i]=='<' || UECSbuffer[i]=='>'){UECSbuffer[i]=' ';}

		//find space
		if(UECSbuffer[i]<=ASCIICODE_SPACE){

			write=i;
			//find end of space
			for(j=write;j<=s_size;j++){

				if((unsigned char)(UECSbuffer[j])>ASCIICODE_SPACE || UECSbuffer[j]=='\0'){

				read=j;
				break;
				}
			}
			//copy str to fill space
			for(j=read;j<=s_size;j++){

				UECSbuffer[write+(j-read)]=UECSbuffer[j];
			}
		}
	}
  
  MemoryWatching();
}

//------------------------------------
// ターゲットバッファの中にpatternStrが存在しているか確認する関数
// W55RP20 (RP2040) 用: PROGMEM 非対応環境に合わせて修正
// 返り値：マッチした文字があるかないか
// マッチした文字列がある場合はマッチした位置の直後のインデックスを返す
bool UECSFindConstChar(char* targetBuffer, const char* patternStr, int* lastPos) {

	*lastPos = 0;         // マッチ終了位置の初期化
	int startPos = -1;    // マッチ開始位置の初期化

	int bufferLen = strlen(targetBuffer);
	int patternLen = strlen(patternStr);  

	if (bufferLen < patternLen) {
			return false; // 検索できる長さが不足している場合
	}

	int i, j;
	for (i = 0; i <= bufferLen - patternLen; i++) {
			// 先頭文字が一致しなければ次へ
			if (targetBuffer[i] != patternStr[0]) continue;

			// 残りの文字を順に比較
			for (j = 0; j < patternLen; j++) {
					if (targetBuffer[i + j] != patternStr[j]) {
							break; // 不一致があればスキップ
					}
			}

			if (j == patternLen) {
					startPos = i;
					break; // パターン一致
			}
	}

	if (startPos < 0) {
			return false; // マッチなし
	}

	*lastPos = startPos + patternLen;
	return true;
}




//------------------------------------
// 指定された文字列から整数値を探す
// L=から&の間で探す（web formからの情報を取り出す）
bool UECSGetValPGMStrAndChr(char* targetBuffer,const char *_romword_startStr, char end_asciiCode,short *shortValue,int *lastPos){

	int _targetBuffersize=strlen(targetBuffer);	// 解析対象バッファの長さを取得
	*lastPos=-1;																// 初期化:終了位置は未確定
	int startPos=-1;														// 初期化：検索開始位置
	int i;
	// 指定された開始キーワードをtargetbufferから検索
	if(!UECSFindConstChar(targetBuffer,_romword_startStr,&startPos)){
		return false;
	}

	// キーワード直後の文字が数字でなければ不正と判断
	if(targetBuffer[startPos]<'0' || targetBuffer[startPos]>'9'){
		return false;
	}

	// 終了文字列（例：&）を探す
	for(i=startPos;i<_targetBuffersize;i++){

		// 終端に達したのに終了記号がない
		if(targetBuffer[i]=='\0'){
			return false;
		}
		
		// 終了記号を見つけたら終了 
		if(targetBuffer[i]==end_asciiCode){
			break;
		}
		
	}

	// 終了記号が見つからないときは失敗
	if(i>=_targetBuffersize){
		return false;
	}

	// 終了記号の位置を記録
	*lastPos=i;

	// 文字列から整数(long)を抽出
	long longVal;
	unsigned char decimal;
	int progPos;
	// 文字列からlingを抽出（小数にも対応）
	if(!(UECSGetFixedFloatValue(&targetBuffer[startPos],&longVal,&decimal,&progPos))){
		return false;
	}

	// 小数が入っている場合は不正
	if(decimal!=0){
		return false;
	}

	// longをshortにキャストして保存
	*shortValue=(short)longVal;
	// オーバーフローチェック
	if(*shortValue != longVal){
		return false;
	}

	return true;
}

// 受信バッファからIPアドレスを取得して４バイトに分轄してip[]に格納する
bool UECSGetIPAddress(char *targetBuffer,unsigned char *ip,int *lastPos){
	
	int _targetBuffersize=strlen(targetBuffer);
	int i;
	int progPos=0;
	(*lastPos)=0;	// 最後に読み込んだ位置を初期化

	// 数字の開始位置（IPアドレスの開始位置）を探す
	for((*lastPos);i<_targetBuffersize;(*lastPos)++){

		if(targetBuffer[(*lastPos)]>='0' && targetBuffer[(*lastPos)]<='9'){
			break;
		}
	}

	// バッファ内に数字が見つからなければ終了
	if((*lastPos)==_targetBuffersize){return false;}//number not found

	// IPアドレスの各オクテットをパース（1～3）
	for(i=0;i<=2;i++){
		// 数値変換失敗
		if(!UECSAtoI_UChar(&targetBuffer[(*lastPos)],&ip[i],&progPos)){
			return false;
		}
		(*lastPos)+=progPos;
		// 残りの文字がピリオドでなければ不正
		if(targetBuffer[(*lastPos)]!='.'){
			return false;
		}
		(*lastPos)++;
	}

	// 最後のオクテット（第4オクテット）を取得
	if(!UECSAtoI_UChar(&targetBuffer[(*lastPos)],&ip[3],&progPos)){
		return false;
	}
	(*lastPos)+=progPos;

	return true;
}

//------------------------------------------------------------------------------
// 文字列から数値を読み取りunsigned char型に変換する関数
// 返り値：成功、失敗（非数値、オーバーフロー）
bool UECSAtoI_UChar(char *targetBuffer,unsigned char *ucharValue,int *lastPos){
	
	unsigned int newValue=0;
	bool validFlag=false;			// 少なくとも一桁は数字を読み込んだかを示すフラグ

	int i;

	// 最大桁までチェック
	for(i=0;i<MAX_DIGIT;i++){

		// 数時かどうかチェック
		if(targetBuffer[i]>='0' && targetBuffer[i]<='9'){
			validFlag=true;
			newValue=newValue*10;							// 桁を一つ上げる
			newValue+=(targetBuffer[i]-'0');	// 数値に変換して加算
			/* targetBuffer[i]-'0'は　
			文字としての数値を整数値に変換するためのテクニック*/
			
			if(newValue>255){return false;}//over flow!
			
			continue;		// 次の桁へ
		}

		break;				// 数字でない文字が出たら終了
	}

	*lastPos=i;						// 読み取った桁数を返す
	*ucharValue=newValue;	// 読み取った値を格納

	return validFlag;			// 数字を読み取ったかどうかを返す
}
//------------------------------------------------------------------------------
// UECSバッファから小数を読み取り、long型に変換する
bool UECSGetFixedFloatValue(char* targetBuffer,long *longVal,unsigned char *decimal,int *lastPos){

	*longVal=0;									// 結果の整数部分を初期化
	*decimal=0;									// 小数点以下の桁数（小数部の長さ）を初期化
	int i;
	int validFlag=0;						// 数値が1文字以上読み取れたかのフラグ
	bool floatFlag=false;				// 小数点が表れたかどうかのフラグ
	char signFlag=1;						// 符号フラグ：初期は正(1)、負の場合は(-1)
	unsigned long newValue=0;		// パース中の一次的な整数値
	unsigned long prvValue=0;		// オーバーフロー検出用に前回の値を記録


	for(i=0;i<MAX_DIGIT;i++){

		if(targetBuffer[i]=='.'){
			if(floatFlag){return false;}	// 小数点が2回以上出現したらエラー
			floatFlag=true;
			continue;
		}
		else if(targetBuffer[i]=='-'){
			// 符号が数値の後に表れる、または2回目異常ではエラー
			if(validFlag){return false;}//Symbol is after number
			if(signFlag<0){return false;}//Multiple symbols error
			signFlag=-1;
			continue;
		}
		else if(targetBuffer[i]>='0' && targetBuffer[i]<='9'){
		
			validFlag=true;
			// 小数点が出た後はdecimalのカウントを始める(小数の桁数を把握)
			if(floatFlag){(*decimal)++;}
			
			newValue=newValue*10;
			newValue+=(targetBuffer[i]-'0');
			// オーバーフロー検出
			if(prvValue>newValue || newValue>2147483646){return false;}//over flow!
			prvValue=newValue;
			continue;
		}

		break;	// 数値・記号以外の文字に遭遇した場合は処理を終了
	}

	// 結果を設定
	*longVal=((long)newValue) * signFlag;
	*lastPos=i;

	return validFlag;	// 一文字以上読み取れていればtrue
}


//------------------------------------
// メモリーのチェック
void MemoryWatching()
{
if(UECStempStr20[MAX_TYPE_CHAR-1]!=0 || UECSbuffer[BUF_SIZE-1]!=0)
	{U_orgAttribute.status|=STATUS_MEMORY_LEAK;}
}


//------------------------------------
// web設定値を変更し、対応するEEPROMにも保存する
// 元のプログラムでも未使用 2025-05-18 S.Nakamura
// bool ChangeWebValue(signed long* data,signed long value){
// 	for(int i=0;i<U_HtmlLine;i++){
		
// 		if(U_html[i].data==data){
// 			*(U_html[i].data)=value;
// 			UECS_EEPROM_writeLong(EEPROM_WEBDATA + i * 4, *(U_html[i].data));
			
// 			return true;
// 		}
// 	}
// 	return false;
// }

/********************************/
/* 16521 Response   *************/
/********************************/
// CCMサーチに対し条件に合致するCCM情報をUDPで応答する
boolean UECSresCCMSearchAndSend(UECSTEMPCCM* _tempCCM){
	
	//CCM provider search response
	/*
	In periodic transmission, only CCMs with a proven track record of transmission will return a response.
	Among regularly sent CCMs, CCMs that have not been sent at the specified frequency will not return a response even if they are registered. 
	This is to prevent accidental reference to broken sensors.
	*/
	int i;
	int progPos = 0;
	int startPos = 0;
	short room=0;
	short region=0;
	short order=0;
	
	// XMLヘッダ、バージョン、CCMサーチタグの存在確認
	if(!UECSFindConstChar(UECSbuffer,&UECSccm_XMLHEADER[0],&progPos)){return false;}
	startPos+=progPos;
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_UECSVER_E10[0],&progPos)){return false;}
	startPos+=progPos;
	if(!UECSFindConstChar(&UECSbuffer[startPos],&UECSccm_CCMSEARCH[0],&progPos)){return false;}
	startPos+=progPos;
	
	// CCM typeの抽出
	for(i=0;i<MAX_TYPE_CHAR;i++){
		UECStempStr20[i]=UECSbuffer[startPos+i];
		if(UECStempStr20[i]==ASCIICODE_DQUOT || UECStempStr20[i]=='\0'){
			UECStempStr20[i]='\0';break;
		}
	}
	UECStempStr20[MAX_CCMTYPESIZE]='\0';
	startPos=startPos+i;
	
	//Extract "room", "region", and "order". If not found, it is assumed to be zero.
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ROOM[0],'\"',&room,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_REGION[0],'\"',&region,&progPos);
	UECSGetValPGMStrAndChr(&UECSbuffer[startPos],&UECSccm_CCMSEARCH_ORDER[0],'\"',&order,&progPos);
	//Serial.print(room);Serial.print("-");
	//Serial.print(region);Serial.print("-");
	//Serial.print(order);Serial.print("\n");
	
	// 登録されているCCMリストから条件に一致するものを探す
	for(int id=0;id<U_MAX_CCM;id++){
		
		// 一致したCCMが見つかったら、レスポンスパケットを構築
		if(UECSCCMSimpleHitcheck(id,room,region,order)){

			//packet create
			ClearMainBuffer();
    	UDPAddConstCharToBuffer(&(UECSccm_XMLHEADER[0]));
    	UDPAddConstCharToBuffer(&(UECSccm_UECSVER_E10[0]));
    	UDPAddConstCharToBuffer(&(UECSccm_CCMSERVER[0]));
    	UDPAddCharToBuffer(UECStempStr20);
    	UDPAddConstCharToBuffer(&(UECSccm_ROOMTXT[0]));
			//room
			UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_ROOM]);
    	UDPAddConstCharToBuffer(&(UECSccm_REGIONTXT[0]));
			//region
			UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_REGI]);
    	UDPAddConstCharToBuffer(&(UECSccm_ORDERTXT[0]));
			//order
			UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_ORDE]);
    	UDPAddConstCharToBuffer(&(UECSCCM_PRIOTXT[0]));
			//priority
			UDPAddValueToBuffer(U_ccmList[id].baseAttribute[AT_PRIO]);
    	UDPAddConstCharToBuffer(&(UECSccm_CLOSETAG[0]));
			
			// IPアドレスの追加（safemodeとmusk safemodeでアドレスが変わる）
			char iptxt[20];
	    if(U_orgAttribute.status & STATUS_SAFEMODE){
				UDPAddConstCharToBuffer(&(UECSdefaultIPAddress[0]));
			}
			else{
				sprintf(iptxt, "%d.%d.%d.%d", U_orgAttribute.ip[0], U_orgAttribute.ip[1], U_orgAttribute.ip[2], U_orgAttribute.ip[3]);
				UDPAddCharToBuffer(iptxt);
			}
    	UDPAddConstCharToBuffer(&(UECSccm_CCMSERVERCLOSE[0]));

    	
    	//----------------------------------------------send 
			// 送信実行（UDP 16521ポートで返信）
			UECS_UDP16521.beginPacket(_tempCCM->address, 16521);
			UECS_UDP16521.write(UECSbuffer);
			// 送信失敗時はイーサネットのリセット
			if(UECS_UDP16521.endPacket()==0){
				UECSresetEthernet(); //when udpsend failed,reset ethernet status
			}
		}
	}

	return true;
}
//------------------------------------------------------------------------------
// 与えられた属性の条件が指定されたCCM(ccmid)がマッチするか判定する
boolean UECSCCMSimpleHitcheck(int ccmid,short room,short region,short order){

	// CCMがない、受信専用の時は対象外
	if(U_ccmList[ccmid].ccmLevel == NONE || !U_ccmList[ccmid].sender){return false;}
	// 型が一致しないときは対象外
	if(strcmp(U_ccmList[ccmid].typeStr,UECStempStr20) != 0){return false;}
	// 送信履歴がなく、かつ周期送信レベルが1分未満のものは対象外（未初期化センサ除外）
	if(!U_ccmList[ccmid].validity && U_ccmList[ccmid].ccmLevel<=A_1M_1){return false;}
	// 属性が一致しないときは対象外
	if(!(room==0 || U_ccmList[ccmid].baseAttribute[AT_ROOM]==0 || U_ccmList[ccmid].baseAttribute[AT_ROOM]==room)){return false;}
	if(!(region==0 || U_ccmList[ccmid].baseAttribute[AT_REGI]==0 || U_ccmList[ccmid].baseAttribute[AT_REGI]==region)){return false;}
	if(!(order==0 || U_ccmList[ccmid].baseAttribute[AT_ORDE]==0 || U_ccmList[ccmid].baseAttribute[AT_ORDE]==order)){return false;}
	
	// 上記の条件を全て満たしたときはヒットと判定
	return true;
}

// イーサネットのリセット added by S.Nakamura
void resetEthernet(){

  pinMode(ETH_RST_PIN, OUTPUT);
  digitalWrite(ETH_RST_PIN, LOW);
  delay(10);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(10);  
}


// arduinoのプログラムで使用していた関数------------------------------------

#if 0 	// progmemを使用する
void UDPAddPGMCharToBuffer(const char* _romword)
{
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);
    if(UECSbuffer[wp]=='\0'){break;}
    wp++;
  }
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif

}

// ターゲットバッファの中に_romword_startStrが存在しているか確認する関数	
// PROGMEMを使用する場合の関数
bool UECSFindPGMChar(char* targetBuffer,const char *_romword_startStr,int *lastPos){

	*lastPos=0;				// マッチした場合に返す終端位置を初期化
	int startPos=-1;	// マッチ開始位置

	// RAM上の文字列の長さを取得
	int _targetBuffersize=strlen(targetBuffer);
	// PROGMEM無いの文字列の長さを取得
	int _startStrsize=strlen_P(_romword_startStr);
	// 比較対象文字列が大きい場合は一致する可能性が無いためfalse
	if(_targetBuffersize<_startStrsize){
		return false;
	}


	int i,j;
	//-------------マッチ探索開始

	// PROGMEMに格納された文字列の先頭文字を取得
	unsigned char startchr=pgm_read_byte(&_romword_startStr[0]);

	// targetbufferの中でstartchrから始まるパターンを探す
	for(i=0;i<_targetBuffersize-_startStrsize+1;i++){
		
		// 先頭の文字が一致しなければ次へ
		if(targetBuffer[i]!=startchr){
			continue;
		}

		// 最初の1文字が一致したときに次の文字も比較する
		for(j=0;j<_startStrsize;j++){
			// 一致しなければそこで探索終了
			if(targetBuffer[i+j]!=pgm_read_byte(&_romword_startStr[j])){break;}//not hit!
		}

		// 全ての文字が一致した時に開始位置を保存して処理を抜ける
		if(j==_startStrsize){
			startPos=i;
			break;
		}
	}

	// 一致するパターンが無いときはfalseを返す
	if(startPos<0){
		return false;
	}

	// マッチした部分の終端を返す
	*lastPos=startPos+_startStrsize;

	return true;
}

// PROGMEMを使用しているもともとの方法
void HTTPAddPGMCharToBuffer(const char* _romword)
{
	// プログラムメモリから最大MAXIMUM_STRINGLENGTHを読み取る
   for(int i = 0; i <= MAXIMUM_STRINGLENGTH; i++)
   {
		// progmemに格納されている文字列を一文字ずつ読み出して、uecsbufferに書き込む
    UECSbuffer[wp]=pgm_read_byte(&_romword[i]);
		// nullで終了
    if(UECSbuffer[wp]=='\0'){break;}
    wp++;
    //auto send
		// バッファが一定サイズを超えたら、自動でクリアする
    if(wp>BUF_HTTP_REFRESH_SIZE)
    		{
			HTTPRefreshBuffer();	// 一次的なバッファ送信？
    		}
  }
  
  #if defined(_ARDUINIO_MEGA_SETTING)
      MemoryWatching();
  #endif
}

// イーサネットスタート
void UECSstartEthernet(){
  
  if(U_orgAttribute.status&STATUS_SAFEMODE)
  	{
  	byte defip[]     = {192,168,1,7};
  	//byte defsubnet[] = {255,255,255,0};
  	byte defdummy[] = {0,0,0,0};
  	Ethernet.begin(U_orgAttribute.mac, defip, defdummy,defdummy,defdummy);
  	}
  	else
    {
    Ethernet.begin(U_orgAttribute.mac, U_orgAttribute.ip, U_orgAttribute.dns, U_orgAttribute.gateway, U_orgAttribute.subnet);
    }

  UECSlogserver.begin();
  UECS_UDP16520.begin(16520);
  UECS_UDP16529.begin(16529);
  UECS_UDP16521.begin(16521);
}

// （W55RP20での使用は妥当ではない）
//Software reset command
// call 0
typedef int (*CALLADDR)(void);
// ソフトリセット関数
void SoftReset(void){
CALLADDR resetAddr=0;
(resetAddr)();
}


void UECSCheckProgramUpdate(){

	//Program Upadate Check
	ClearMainBuffer();
	// 現在のプログラムのビルド情報
	UDPAddConstCharToBuffer(&(ProgramDate[0]));
	UDPAddConstCharToBuffer(&(ProgramTime[0]));

	// バッファの内容とEEPROMの日時を比較して
	// 異なる場合はEEPROMを書き換えてフラグを立てる
	int i;
	for(i=0;i<(EEPROM_CCMTOP-EEPROM_PROGRAMDATETIME);i++){
	
		if(EEPROM.read(i+EEPROM_PROGRAMDATETIME)!=UECSbuffer[i]){
			U_orgAttribute.status|=STATUS_PROGRAMUPDATE;
			EEPROM.write(i+EEPROM_PROGRAMDATETIME,UECSbuffer[i]);
			EEPROM.commit();	// フラッシュへの書き込み
		}
		
	if(UECSbuffer[i]=='\0'){break;}
	}
}


#if defined(_ARDUINIO_MEGA_SETTING)

void MemoryWatching(){
if(UECStempStr20[MAX_TYPE_CHAR-1]!=0 || UECSbuffer[BUF_SIZE-1]!=0)
	{U_orgAttribute.status|=STATUS_MEMORY_LEAK;}
}

#endif


void UECS_EEPROM_SaveCCMType(int ccmid){

	#if defined(_ARDUINIO_MEGA_SETTING)
	// 書き込みがEEPROMの範囲を超える場合は中止
	if(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_L_CCM_TOTAL>EEPROM_CCMEND){return;}//out of memory
	#endif

	int i;
//type��������
for(i=0;i<=MAX_CCMTYPESIZE;i++)
	{
	if(EEPROM.read(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i)!=U_ccmList[ccmid].typeStr[i])
		{
		EEPROM.write(ccmid*EEPROM_L_CCM_TOTAL+EEPROM_CCMTOP+EEPROM_L_CCM_TYPETXT+i,U_ccmList[ccmid].typeStr[i]);
			EEPROM.commit();	// フラッシュへの書き込み
	}
	if(U_ccmList[ccmid].typeStr[i]=='\0'){break;}
	}
}

#endif
