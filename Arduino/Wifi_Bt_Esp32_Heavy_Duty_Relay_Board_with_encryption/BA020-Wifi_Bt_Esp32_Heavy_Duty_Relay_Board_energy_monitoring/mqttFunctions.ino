boolean connectMQTT()
{
  if (mqttClient.connected()){
    return true;
  }  
  
  Serial.print("Connecting to MQTT server ");
  Serial.print(mqttServer);
  Serial.print(" as ");
  Serial.println(host);

  String temp_will_msg=(String)firstName+" "+(String)mqtt_will_msg;  //added on 28/07/2017
  //connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) //format of connect function
  
  if (mqttClient.connect(host,(char*)mqtt_user.c_str(),(char*)mqtt_passwd.c_str(),(char*)pubTopic.c_str(),0,0,(char*)temp_will_msg.c_str())) //added on 28/07/18
  {
     mqttClient.loop();
    Serial.println("Connected to MQTT broker with authentication");
     mqttClient.publish((char*)pubTopic.c_str(), (char*)BOARD_NAME .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)FIRMWARE_VERSION .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)HARDWARE_VERSION .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)MQTT_COMMANDS_AVAILABLE .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)MQTT_TOPICS_AVAILABLE.c_str());
     
    if(mqttClient.subscribe((char*)subTopic.c_str()))
    {
      Serial.println("Subsribed to topic.1");
    } 
    else 
    {
      Serial.println("NOT subsribed to topic 1");      
    }
    return true;
   }  
  else if (mqttClient.connect(host))//added on 31/07/18
  {
    Serial.println("Connected to MQTT broker without authentication");
     mqttClient.loop();
    mqttClient.publish((char*)pubTopic.c_str(), (char*)BOARD_NAME .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)FIRMWARE_VERSION .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)HARDWARE_VERSION .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)MQTT_COMMANDS_AVAILABLE .c_str());
     delay(10);
     mqttClient.publish((char*)pubTopic.c_str(), (char*)MQTT_TOPICS_AVAILABLE.c_str());
     
    if(mqttClient.subscribe((char*)subTopic.c_str()))
    {
      Serial.println("Subsribed to topic.1");
    } 
    else 
    {
      Serial.println("NOT subsribed to topic 1");      
    }
    return true;
   } 
  else 
  {
    Serial.println("MQTT connect failed! ");
    //digitalWrite(LED_R, HIGH);
    return false;
  }
  return false;
}

 
void disconnectMQTT(){
  mqttClient.disconnect();
}

void mqtt_handler()
{
  mqttClient.loop();
  delay(1); //let things happen in background
 if (toPub_1 == 1 || toPub_2 == 1 ) 
 {
    Debugln("DEBUG: Publishing state via MQTT");
    if (pubState_1()) 
    {
      toPub_1 = 0; 
    }
    if (pubState_2()) 
    {
      toPub_2 = 0;  
    }
 }
  delay(1); //let things happen in background
}

void mqtt_arrived(char* subTopic, byte* payload, unsigned int length) { // handle messages arrived 
  int temp_mqtt_buffer = 0;
  Serial.print("MQTT message arrived:  topic: " + String(subTopic));
    // create character buffer with ending null terminator (string)
    
  for(temp_mqtt_buffer=0; temp_mqtt_buffer<length; temp_mqtt_buffer++) {    
    buf[temp_mqtt_buffer] = payload[temp_mqtt_buffer];
  }
  buf[temp_mqtt_buffer] = '\0';
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
  if (msgString == "R01_ON" && !digitalRead(OUTPIN_12)) 
  {
    Serial.print("Output1 is ");
    Serial.println(digitalRead(OUTPIN_12));
    Serial.print("Switching Output1 to ");
    Serial.println("high");
    state_out_1=state_out_1^1;
  } 
  else if (msgString == "R01_OFF" && digitalRead(OUTPIN_12)) 
  {
    Serial.print("Output1 is ");
    Serial.println(digitalRead(OUTPIN_12));
    Serial.print("Switching Output1 to ");
    Serial.println("low");
    state_out_1=state_out_1^1;
  }
  else if (msgString == "R02_ON" && !digitalRead(OUTLED)) 
  {
    Serial.print("Output2 is ");
    Serial.println(digitalRead(OUTLED));
    Serial.print("Switching Output2 to ");
    Serial.println("high");
    state_out_2 =state_out_2^1;
    //digitalWrite(OUTPIN_2, HIGH);
  } 
  else if (msgString == "R02_OFF" && digitalRead(OUTLED)) 
  {
    Serial.print("Output2 is ");
    Serial.println(digitalRead(OUTLED));
    Serial.print("Switching Output2 to ");
    Serial.println("low");
    state_out_2 =state_out_2^1;
    //digitalWrite(OUTPIN_2, LOW);
  }
   else if (msgString.substring(0,6) == "SetPV:") 
  {
  String  tempString_mqtt =msgString;
  //float P,V;
  int ind1; 
  int ind2;
  ind1=tempString_mqtt.indexOf(':');
  ind2=tempString_mqtt.indexOf(',');
  P=tempString_mqtt.substring(ind1+1,ind2+1).toFloat();
  V=tempString_mqtt.substring(ind2+1).toFloat();
  //Serial.println(P);
  //Serial.println(V);
  calflag=1;
  }
   else if (msgString == "Reset") 
  {
    clearConfig();
    delay(10);
    Serial.println("Done, restarting!");
    esp_restart();
  }


}



boolean pubState_1() 
{
  if (!connectMQTT()) 
  {
    delay(100);
    if (!connectMQTT) 
    {
      Serial.println("Could not connect MQTT.");
      Serial.println("Publish state NOK");
      return false;
    }
  }
  if (mqttClient.connected()) 
  {
    Serial.println("To publish state " + o1_state_pub);
    String pubTopic_R1=pubTopic+"/R01";
    
    if(toPub_1==1)
    {
     if (mqttClient.publish((char*)pubTopic_R1.c_str(), (char*) o1_state_pub.c_str())) 
     {
      Serial.println("Publish state OK");
      return true;
     } 
     else 
     {
      Serial.println("Publish state NOK");
      return false;
     }
   } 
  }
  else 
  {
    Serial.println("Publish state NOK");
    Serial.println("No MQTT connection.");
  }
}

boolean pubState_2() 
{ 
  if (!connectMQTT()) 
  {
    delay(100);
    if (!connectMQTT) 
    {
      Serial.println("Could not connect MQTT.");
      Serial.println("Publish state NOK");
      return false;
    }
  }
  if (mqttClient.connected()) 
  {
    Serial.println("To publish state "+o2_state_pub);
    String pubTopic_R2=pubTopic+"/R02";
    if(toPub_2==1)
    {
     if (mqttClient.publish((char*)pubTopic_R2.c_str(), (char*) o2_state_pub.c_str())) 
     {
      Serial.println("Publish state OK");
      return true;
     } 
     else 
     {
      Serial.println("Publish state NOK");
      return false;
     }
   } 
  }
  else 
  {
    Serial.println("Publish state NOK");
    Serial.println("No MQTT connection.");
  }
}


