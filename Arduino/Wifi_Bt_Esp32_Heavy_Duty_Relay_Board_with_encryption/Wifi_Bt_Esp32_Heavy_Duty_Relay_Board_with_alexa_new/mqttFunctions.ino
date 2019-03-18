boolean connectMQTT(){
  if (mqttClient.connected()){
    return true;
  }  
  
  Serial.print("Connecting to MQTT server ");
  Serial.print(mqttServer);
  Serial.print(" as ");
  Serial.println(host);

  String temp_will_msg=(String)firstName+(String)mqtt_will_msg;  //added on 28/07/2017
  //connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) //format of connect function
  
  if (mqttClient.connect(host,(char*)mqtt_user.c_str(),(char*)mqtt_passwd.c_str(),(char*)pubTopic.c_str(),0,0,(char*)temp_will_msg.c_str())) //added on 28/07/18
  {
    Serial.println("Connected to MQTT broker with authentication");
    if(mqttClient.subscribe((char*)subTopic.c_str()))
    {
      Serial.println("Subsribed to topic.1");
    } 
    else 
    {
      Serial.println("NOT subsribed to topic 1");      
    }
    mqttClient.loop();


    return true;
   }  
  else if (mqttClient.connect(host))//added on 31/07/18
  {
    Serial.println("Connected to MQTT broker without authentication");
    if(mqttClient.subscribe((char*)subTopic.c_str()))
    {
      Serial.println("Subsribed to topic.1");
    } 
    else 
    {
      Serial.println("NOT subsribed to topic 1");      
    }
    mqttClient.loop();


    return true;
   } 
  else 
  {
    Serial.println("MQTT connect failed! ");
    //digitalWrite(LED_R, HIGH);
    return false;
  }
}

  
//  if (mqttClient.connect(host)) {
//    Serial.println("Connected to MQTT broker");
//    if(mqttClient.subscribe((char*)subTopic.c_str())){
//      Serial.println("Subsribed to topic.");
//    } else {
//      Serial.println("NOT subsribed to topic!");      
//    }
//    return true;
//  }
//  else {
//    Serial.println("MQTT connect failed! ");
//    return false;
//  }
//}

void disconnectMQTT(){
  mqttClient.disconnect();
}

void mqtt_handler(){
  if (toPub==1){
    Debugln("DEBUG: Publishing state via MQTT");
    if(pubState()){
     toPub=0; 
    }
  }
  mqttClient.loop();
  delay(100); //let things happen in background
}

void mqtt_arrived(char* subTopic, byte* payload, unsigned int length) { // handle messages arrived 
  int i = 0;
  Serial.print("MQTT message arrived:  topic: " + String(subTopic));
    // create character buffer with ending null terminator (string)
    
  for(i=0; i<length; i++) {    
    buf[i] = payload[i];
  }
  buf[i] = '\0';
  String msgString = String(buf);
  Serial.println(" message: " + msgString);
  


//..................ZDC STATUS...................................
  if(msgString == "Status")
  { String Status; 
    if(phase_detect() == 1)
    {
      Status = "ON";
    }
    else if(phase_detect() == 0)
    {
      Status = "OFF";
    }
  mqttClient.publish((char*)pubTopic.c_str(), (char*) Status.c_str()); 
  }

//..............................................................
  if (msgString == "R13_ON")
  {
#ifdef ARDUINO_ARCH_ESP32
      if(switch_status_12==1)
      { 
       wifi_input_status_12=0;
      }
      else
      { 
       wifi_input_status_12=1;
      }
      send_status_12=1;
#else
    digitalWrite(OUTPIN_12, HIGH);
#endif
  } 
  else if (msgString == "R13_OFF")
  {
#ifdef ARDUINO_ARCH_ESP32
      if(switch_status_12==0)
      { 
       wifi_input_status_12=0;
      }
      else
      { 
       wifi_input_status_12=1;
      }
      send_status_12=1;
#else
    digitalWrite(OUTPIN_12, LOW);
#endif
  }   

       


    
   if (msgString == "Led_on")
   {
      Serial.print("Led is ");
      Serial.println(digitalRead(OUTLED));      
      Serial.print("Switching light to "); 
      Serial.println("high");
      digitalWrite(OUTLED, 1); 
  } 
  else if (msgString == "Led_off")
  {
      Serial.print("Led is ");
      Serial.println(digitalRead(OUTLED));    
      Serial.print("Switching light to "); 
      Serial.println("low");
      digitalWrite(OUTLED, 0); 
  } 
   if (msgString == "Reset")
   {
     clearConfig();
     delay(10);
     Serial.println("Done, restarting!");
#ifdef ARDUINO_ARCH_ESP32
     esp_restart();
#else
     ESP.reset();
#endif 
   }
}

boolean pubState(){ //Publish the current state of the light   
  boolean publish_ok; 
  if (!connectMQTT()){
      delay(100);
      if (!connectMQTT){                            
        Serial.println("Could not connect MQTT.");
        Serial.println("Publish state NOK");
        publish_ok=false;
        return publish_ok;
      }
    }
    if (mqttClient.connected()){      
        //String state = (digitalRead(OUTPIN))?"1":"0";
        
#ifdef ARDUINO_ARCH_ESP32

      if ( send_status_12 == 1) {
        mqttClient.publish((char*)pubTopic.c_str(), (char*) state2.c_str()); 
        send_status_12=0;
        Serial.println("Publish state2 OK");        
        publish_ok=true;
      } 

      send_status_12=0;

      return  publish_ok;
 #else
      if (mqttClient.publish((char*)pubTopic.c_str(), (char*) state.c_str())) {
        Serial.println("Publish state OK");        
        return true;
      }
     else {
        Serial.println("Publish state NOK");        
        return false;
      }
 #endif     
        
     } else {
         Serial.println("Publish state NOK");
         Serial.println("No MQTT connection.");        
     } 
    
}
