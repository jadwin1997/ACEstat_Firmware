#include "voltammetry.h"


/***************CYCLIC VOLTAMMETRY**************/
void runCV(void){
  
  //printf("Zero voltage between 0000mV - 9999mV : ");
  printf("[:ZVI]");
  uint16_t cvZeroVolt = getParameter(4);

  //printf("Starting voltage between 0000mV - 9999mV : ");
  printf("[:SVI]");
  uint16_t cvStartVolt = getParameter(4);
  
  //printf("Vertex voltage between 0000mV - 9999mV : ");
  printf("[:VVI]");
  uint16_t cvVertexVolt = getParameter(4);
  
  //printf("Ending voltage between 0000mV - 9999mV : ");
  printf("[:EVI]");
  uint16_t cvEndVolt = getParameter(4);

  //printf("Current Limit:\n");
  //printf("0: 4.5mA\n1: 900uA\n2: 180uA\n3: 90uA\n4: 45uA\n5: 22.5uA\n6: 11.25uA\n7: 5.625uA\n");
  printf("[:CLI]");
  uint8_t RTIACHOICE = getParameter(1);
  //uint8_t* uBuffer = return_uart_buffer();
  //uint8_t RTIACHOICE = uBuffer[0];
  uint32_t RGAIN = RTIA_LOOKUP(RTIACHOICE-48); //PASS INT VAL RATHER THAN ASCII
  
  
  /*cv ramp setup*/
  AfePwrCfg(AFE_ACTIVE);  //set AFE power mode to active

  LPDacPwrCtrl(CHAN0,PWR_UP);
  LPDacCfg(CHAN0,LPDACSWNOR,VBIAS12BIT_VZERO6BIT,LPDACREF2P5);
  LPDacWr(CHAN0, 62, 62*64);

  /*LPTIA REQUIRED TO HAVE DAC OUTPUT*/
  /*power up PA,TIA,no boost, full power*/
  AfeLpTiaPwrDown(CHAN0,0);
  AfeLpTiaAdvanced(CHAN0,BANDWIDTH_NORMAL,CURRENT_NOR);
  AfeLpTiaSwitchCfg(CHAN0,SWMODE_SHORT);  /*short TIA feedback for Sensor setup*/
  AfeLpTiaCon(CHAN0,LPTIA_RLOAD_0,LPTIA_RGAIN_96K,LPTIA_RFILTER_1M);
  AfeLpTiaSwitchCfg(CHAN0,SWMODE_NORM);  /*TIA switch to normal*/
  /*LPTIA REQUIRED TO HAVE DAC OUTPUT*/

  hptia_setup_parameters(RGAIN);
  adcCurrentSetup_hptia();
  /*end cv ramp setup*/

  /*RAMP HERE*/
  //printf("CV sweep begin\n");
  cv_ramp_parameters(cvZeroVolt,cvStartVolt,cvVertexVolt,cvEndVolt,RGAIN);
  //printf("CV sweep end\n");
  /*END RAMP*/

  turn_off_afe_power_things_down();
  NVIC_SystemReset(); //ARM DIGITAL SOFTWARE RESET
  //DioTglPin(pADI_GPIO2,PIN4);           // Flash LED
}

void cv_ramp_parameters(uint16_t zeroV, uint16_t startV, uint16_t vertexV, uint16_t endV, uint32_t RGAIN){
  uint16_t SETTLING_DELAY = 5;
  uint16_t RAMP_STEP_DELAY = 10*mvStepDelay;          //14.7mS 68 loops to achieve 50mV 14.7mS*68 gives 50mV per second
  uint16_t cBias, cZero;
  uint16_t ADCRAW;

  uint16_t cStart = (startV-200)/0.54;
  uint16_t cVertex = (vertexV-200)/0.54;
  uint16_t cEnd =(endV-200)/0.54;
  int RTIA = RTIA_VAL_LOOKUP(RGAIN);
  
  cZero = (zeroV-200)/34.38;
  cBias = cStart;

  int sampleCount = 0;
  uint16_t* szADCSamples = return_adc_buffer();

  for (cBias = cStart; cBias < cVertex; ++cBias){
    LPDacWr(CHAN0, cZero, cBias);         // Set VBIAS/VZERO output voltages
    delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
    delay_10us(RAMP_STEP_DELAY);

    ADCRAW = pollReadADC();
    szADCSamples[sampleCount]=cBias;
    sampleCount++;
    szADCSamples[sampleCount]=ADCRAW;
    sampleCount++;
  }

  for (cBias = cVertex; cBias > cEnd; --cBias){
      LPDacWr(CHAN0, cZero, cBias);         // Set VBIAS/VZERO output voltages
      delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
      delay_10us(RAMP_STEP_DELAY);

      ADCRAW = pollReadADC();
      szADCSamples[sampleCount]=cBias;
      sampleCount++;
      if(sampleCount>MAX_BUFFER_LENGTH) {
        //printf("MEMORY OVERFLOW\n");
        printf("[ERR:MEMORY OVERFLOW]");
        sampleCount=0;
        break;
      }
      szADCSamples[sampleCount]=ADCRAW;
      sampleCount++;
      if(sampleCount>MAX_BUFFER_LENGTH) {
        //printf("MEMORY OVERFLOW\n");
        printf("[ERR:MEMORY OVERFLOW]");
        sampleCount=0;
        break;
      }
  }
  printCVResults(cZero,cStart,cVertex,cEnd,sampleCount,RTIA);
}

void printCVResults(float cZero, float cStart, float cVertex, float cEnd, int sampleCount, int RTIA){
  float zeroVoltage = 200+(cZero*34.38);
  //printf("RANGE IS %f to %f to %f\n", cStart*0.54+200-zeroVoltage, cVertex*0.54+200-zeroVoltage, cEnd*0.54+200-zeroVoltage);
  printf("[RANGE:%f,%f,%f]", cStart*0.54+200-zeroVoltage, cVertex*0.54+200-zeroVoltage, cEnd*0.54+200-zeroVoltage);
  //printf("RGAIN VALUE IS %i\n", RTIA);
  printf("[RGAIN:%i][RESULTS:", RTIA);
  uint16_t* szADCSamples = return_adc_buffer();
  float tc, vDiff;
  for(uint32_t i = 0; i < sampleCount; i+=2){
    vDiff = szADCSamples[i]*0.54+200-zeroVoltage;
    tc = calcCurrent_hptia(szADCSamples[i+1], RTIA);
    //printf("Volt:%f,Current:%f\n", vDiff,tc);
    printf("%f,%f\n", vDiff,tc);
  }
  printf("]");
}
/****************END CYCLIC VOLTAMMETRY**********************/

/****************SQUARE WAVE VOLTAMMETRY***************************/
void sqv_dep_time(uint16_t start, uint16_t time){
	uint16_t cBias, cZero;
	uint16_t cStart = (start-200)/0.54;

	cZero=32;
	cBias=cStart;

	LPDacWr(CHAN0, cZero, cBias);
	delay_10us(time*100*1000);
};

void sqv_ramp_parameters(uint16_t zeroV, uint16_t startV, uint16_t endV, uint32_t RGAIN, uint16_t amplitude, int dep){
  uint16_t SETTLING_DELAY = 5;
  uint16_t RAMP_STEP_DELAY = 10*mvStepDelay;          //14.7mS 68 loops to achieve 50mV 14.7mS*68 gives 50mV per second
  uint16_t cBias, cZero;
  uint16_t ADCRAW;

  uint16_t cStart = (startV-200)/0.54;
  uint16_t cEnd =(endV-200)/0.54;
  int RTIA = RTIA_VAL_LOOKUP(RGAIN);

  uint16_t amp = (amplitude-200)/0.54;

  cZero = (zeroV-200)/0.03438;
  cBias = cStart;

  int sampleCount = 0;
  uint16_t* szADCSamples = return_adc_buffer();

  /*palmsense device does 2 sweps for some reason only reports one*/
  for (cBias = cStart; cBias < cEnd; cBias+=10){
    LPDacWr(CHAN0, cZero, cBias+amp);
    delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
    delay_10us(1250); //delay 12.5ms
    LPDacWr(CHAN0, cZero, cBias-amp);
    delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
    delay_10us(1250); //delay 12.5ms
  }

  for (cBias = cStart; cBias < cEnd; cBias+=10){
    LPDacWr(CHAN0, cZero, cBias+amp);
    delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
    delay_10us(1250); //delay 12.5ms
    ADCRAW = pollReadADC();
    szADCSamples[sampleCount]=cBias;
    sampleCount++;
    if(sampleCount>MAX_BUFFER_LENGTH) {
        printf("MEMORY OVERFLOW\n");
        sampleCount=0;
        break;
      }
    szADCSamples[sampleCount]=ADCRAW;
    sampleCount++;
    if(sampleCount>MAX_BUFFER_LENGTH) {
        printf("MEMORY OVERFLOW\n");
        sampleCount=0;
        break;
      }

    LPDacWr(CHAN0, cZero, cBias-amp);
    delay_10us(SETTLING_DELAY);                  // allow LPDAC to settle
    delay_10us(1250); //delay 12.5ms
    ADCRAW = pollReadADC();
    szADCSamples[sampleCount]=ADCRAW;
    sampleCount++;
    if(sampleCount>MAX_BUFFER_LENGTH) {
        printf("MEMORY OVERFLOW\n");
        sampleCount=0;
        break;
      }
  }
  printSWVResults(cZero, cStart, cEnd, amplitude, sampleCount, RTIA, dep);
}

void runSWV(void){
  
  printf("Zero voltage between 0000mV - 9999mV : ");
  uint16_t swvZeroVolt = getParameter(4);

  printf("Starting voltage between 0000mV - 9999mV : ");
  uint16_t swvStartVolt = getParameter(4);
  
  printf("Ending voltage between 0000mV - 9999mV : ");
  uint16_t swvEndVolt = getParameter(4);
  
  printf("Squarewave amplitude between 000mV - 999mV : ");
  uint16_t swvAmp = getParameter(3);
  
  printf("Deposition time betwee 000s and 999s : ");
  uint16_t depTime = getParameter(3);
  
  printf("Current Limit:\n");
  printf("0: 4.5mA\n0001: 900uA\n2: 180uA\n3: 90uA\n4: 45uA\n5: 22.5uA\n6: 11.25uA\n7: 5.625uA\n");
  uint8_t RTIACHOICE = getParameter(1);
//  uint8_t* uBuffer = return_uart_buffer();
//  uint8_t RTIACHOICE = uBuffer[0];
  uint32_t RGAIN = RTIA_LOOKUP(RTIACHOICE);
  

  /*cv ramp setup*/
  AfePwrCfg(AFE_ACTIVE);  //set AFE power mode to active

  /*LPTIA REQUIRED TO HAVE DAC OUTPUT*/
  /*power up PA,TIA,no boost, full power*/
  AfeLpTiaPwrDown(CHAN0,0);
  AfeLpTiaAdvanced(CHAN0,BANDWIDTH_NORMAL,CURRENT_NOR);
  AfeLpTiaSwitchCfg(CHAN0,SWMODE_SHORT);  /*short TIA feedback for Sensor setup*/
  AfeLpTiaCon(CHAN0,LPTIA_RLOAD_0,LPTIA_RGAIN_96K,LPTIA_RFILTER_1M);
  AfeLpTiaSwitchCfg(CHAN0,SWMODE_NORM);  /*TIA switch to normal*/
  /*LPTIA REQUIRED TO HAVE DAC OUTPUT*/

  hptia_setup_parameters(RGAIN);
  adcCurrentSetup_hptia();
  /*end cv ramp setup*/

  /*RAMP HERE*/
  printf("SWV deposition begin\n");
  sqv_dep_time(swvStartVolt, depTime);
  printf("SWV deposition end, sweep begin\n");
  sqv_ramp_parameters(swvZeroVolt,swvStartVolt,swvEndVolt,RGAIN, swvAmp, depTime);
  printf("SWV sweep end\n");
  /*END RAMP*/

  turn_off_afe_power_things_down();
  //DioTglPin(pADI_GPIO2,PIN4);           // Flash LED
}

void printSWVResults(float cZero, float cStart, float cEnd, uint16_t amp, int sampleCount, int RTIA, int dep){
  float zeroVoltage = 200+(cZero*34.38);
  printf("Range is %f to %f\n", cStart*0.54+200-zeroVoltage, cEnd*0.54+200-zeroVoltage);
  printf("Rgain value is %i\n", RTIA);
  printf("Amplitude is %i\n", amp);
  printf("Deposition time is %i\n", dep);
  uint16_t* szADCSamples = return_adc_buffer();
  float vDiff, tc;

  for(uint32_t i = 0; i < sampleCount; i+=3){
    vDiff = szADCSamples[i]*0.54+200-zeroVoltage;
    tc = calcCurrent_hptia(szADCSamples[i+1]-szADCSamples[i+2], RTIA);
    //printf("Volt:%f,Current:%f\n", vDiff,tc);
    printf("%f,%f\n", vDiff,tc);
  }
}
/*************END SQUARE WAVE VOLTAMMETRY**************************/
