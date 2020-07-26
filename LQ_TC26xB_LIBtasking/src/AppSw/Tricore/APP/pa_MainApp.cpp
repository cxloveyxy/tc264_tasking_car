
extern "C"
{

#include "pa_MainApp.h"
#include "pa_CommonLib/pa_CrossCalc.h"
#include "pa_CommonLib/pa_MotorManager.h"
#include "pa_CommonLib/pa_app/pa_OLED/pa_oled.h"
#include "pa_CommonLib/pa_drv/pa_IIC.h"
#include "pa_CommonLib/pa_app/pa_RDA5804/pa_RDA5807.h"

#include "stdio.h"
#include "LQ_UART.h"
#include <LQ_ADC.h>
#include <LQ_STM.h>
#include <Ifx_FftF32.h>
#include <LQ_FFT.h>
#include <LQ_GPT12_ENC.h>
#include "string.h"

#include <LQ_Atom_Motor.h>
#include "math.h"
}

#include "pa_CommonLib/pa_PID.h"
#include "pa_CommonLib/pa_MecanumModel.h"
#include "pa_CommonLib/pa_UartManager.h"
#include "pa_CommonLib/pa_GlobalCpp.h"
#include "pa_CommonLib/pa_UltrasonicDistance.h"


#define adc_arrlen 2048
unsigned short adc_arr1[adc_arrlen];
unsigned short adc_arr2[adc_arrlen];
unsigned short adc_arr3[adc_arrlen];
unsigned short adc_arr4[adc_arrlen];

unsigned short adcValue1;
unsigned short adcValue2;
unsigned short adcValue3;
unsigned short adcValue4;

unsigned short step = 0;

int cnt = 0;
char flag_5ms = 1;
char flag_100ms = 1;
SpeedOfMotors speedOfMotors;

pa_MecanumModel mecanumModel;

char err_X = 0;
char err_Y = 0;
char pre_err_Y = 0;

short rightFftCount = 0;//符合信标阈值的计数。数量超过一定范围确定为信标响起
short lastrightFftCount = 0;

pa_GlobalCpp globalCpp;

pa_GlobalCpp *pa_GlobalCpp::getGlobalCpp()
{
	return &globalCpp;
}

pa_UltrasonicDistance ultrasonicDistance1;
pa_UltrasonicDistance ultrasonicDistance2;

void initVariable()
{ //初始化变量
	speedOfMotors.speedOfM1 = 0;
	speedOfMotors.speedOfM2 = 0;
	speedOfMotors.speedOfM3 = 0;
	speedOfMotors.speedOfM4 = 0;
	globalCpp.pid_Motor1.setPid(12, 0, 0);
	globalCpp.pid_Motor2.setPid(17, 0, 1);
	globalCpp.pid_Motor3.setPid(17, 0, 1);
	globalCpp.pid_Motor4.setPid(17, 0, 1);
	globalCpp.pid_Direction.setPid(500, 0, 1);
	ultrasonicDistance1.init(1);
	ultrasonicDistance2.init(2);
}

void initFuncs()
{
	// ATOM_PWM_InitConfig(ATOMPWM0, 4000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM1, 5000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM2, 6000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM3, 7000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM4, 8000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM5, 9000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM6, 3000, 15000);
	// ATOM_PWM_InitConfig(ATOMPWM7, 2000, 15000);
	//初始化功能
	UART_InitConfig(UART2_RX_P14_3, UART2_TX_P14_2, 115200); //2红色线
	//UART_InitConfig(UART1_RX_P02_3, UART1_TX_P02_2, 115200);
	{
		UART_PutStr(UART2, "init\r\n");
	}
	//pa_BNO055_init();

	ADC_InitConfig(ADC0, 100000); //初始化
	ADC_InitConfig(ADC1, 100000);
	ADC_InitConfig(ADC2, 100000);
	ADC_InitConfig(ADC4, 100000);

	//adc采集以及控制 定时器中断
	STM_InitConfig(STM0, STM_Channel_0, 100, []() { //微秒  //UART_PutStr(UART2,"ssss");
		getadc();									//getadc();
		ultrasonicDistance1.checkEcho();
		// if (cnt % 100 == 0)
		// {

		// }
		if (cnt % 50 == 0)
		{
			// cnt = 0;
			flag_5ms = 1;

			speedOfMotors.speedOfM1 = ENC_GetCounter(ENC3_InPut_P02_6);
			speedOfMotors.speedOfM2 = -ENC_GetCounter(ENC2_InPut_P33_7);
			speedOfMotors.speedOfM3 = -ENC_GetCounter(ENC6_InPut_P20_3);
			speedOfMotors.speedOfM4 = ENC_GetCounter(ENC5_InPut_P10_3);
		}
		if (cnt == 1000)
		{
			cnt = 0;
			flag_100ms = 1;
		}
		cnt++;
	});

	pa_initMotorPwm();

	ENC_InitConfig(ENC2_InPut_P33_7, ENC2_Dir_P33_6);
	//ENC_InitConfig(ENC3_InPut_P02_6, ENC3_Dir_P02_7);//摄像头冲突，不建议用
	ENC_InitConfig(ENC3_InPut_P02_6, ENC3_Dir_P02_7);
	ENC_InitConfig(ENC5_InPut_P10_3, ENC5_Dir_P10_1);
	ENC_InitConfig(ENC6_InPut_P20_3, ENC6_Dir_P20_0);

	ultrasonicDistance1.init(1);

	pa_IIC_init();
	OLED_Init(); //初始化OLED
	OLED_Clear();

	// OLED_ShowString(0,0,"helloWorld",8);
	UART_PutStr(UART2,"txt1");
	RDA5807_Init();	      //RDA5807初始化
	UART_PutStr(UART2,"txt2");
//   //显示芯片ID 0x5804
//   RXFreq = RDA5807_ReadReg(RDA_R00);
//   sprintf(txt,"Chip:0x%04X",RXFreq);
//   TFTSPI_P8X16Str(1,2,txt,u16WHITE,u16BLACK);
   
	unsigned short RXFreq = RDA5807_ReadReg(RDA_R00);//pa_IIC_read8(RDA_WRITE,RDA_R00);
	// char RSSI=RDA5807_GetRssi();//显示信号强度0~127
	// {
		char txt[30]={0};
		sprintf(txt,"Chip:0x%04X",RXFreq);
		UART_PutStr(UART2,txt);
		OLED_ShowString(0,0,txt,8);
		
 	// 	sprintf(txt,"RSSI:%02d  \r\n",RSSI);
	// 	UART_PutStr(UART2,txt);
	// 	//OLED_ShowString(0,2,txt,8);
	// }
	/*rotate box demo****************************/
	// {unsigned char a[128*8]={0};
	// unsigned char b[128*8]={0};
	// memset(b,1,128*8);
	// short angle=0;
	// while(1){
	// 	memset(a,0,128*8);
	// 	float realAngle=angle/180.0*PI;
	// 	int x=60+sin(realAngle)*20;
	// 	int y=30+cos(realAngle)*20;
	// 	for(int i=-3;i<4;i++){
	// 		for(int j=-3;j<4;j++){
	// 			int x1=x+i;
	// 			int y1=y+j;
	// 			a[(y1/8)*128+x1]|=0x01<<(y1%8);
	// 		}
	// 	}
		
	// 	fill_picture(a);

	// 	angle+=20;
	// 	if(angle==360)angle=0;
	// }}
	/****************************/
	//   TFTSPI_P8X16Str(1,2,txt,u16WHITE,u16BLACK);
}
short xMoveDelay = 0;
short err_Y_whenBlocked = 0;
void startMainTask()
{
	initVariable();
	initFuncs();
	int cnt5ms = 0;

	while (1)
	{
		//执行控制算法
		if (flag_5ms == 1)
		{
			flag_5ms = 0; //50ms清0
			cnt5ms = cnt5ms + 1;
			if (cnt5ms >= 1000)
			{
				globalCpp.targetSpeed = -globalCpp.targetSpeed;

				cnt5ms = 0;
			}
			// pa_updateMotorPwm(1, 1000);
			// pa_updateMotorPwm(2, 1000);
			// pa_updateMotorPwm(3, 1000);
			// pa_updateMotorPwm(4, 1000);
			if (!checkSignalStable())
			{
				//UART_PutStr(UART2, "signal Not Stable or Beacon Off!!\r\n");
			}else{

			}

			if (fabs(globalCpp.targetSpeed) < 50 || globalCpp.motorDisable || !checkSignalStable())
			{
				
				pa_updateMotorPwm(1, 0);
				pa_updateMotorPwm(2, 0);
				pa_updateMotorPwm(3, 0);
				pa_updateMotorPwm(4, 0);
				globalCpp.pid_Motor1.iSum = 0;
				globalCpp.pid_Motor2.iSum = 0;
				globalCpp.pid_Motor3.iSum = 0;
				globalCpp.pid_Motor4.iSum = 0;
			}
			else
			{
				float dirOut = getMotorRotationValueByErr(err_Y < 0 ? err_X : -err_X);
				float yVelocity = err_Y < 0 ? 1000 : -1000;
				float xVelocity = 0;
				if (ultrasonicDistance1.distance < 10)
				{
					err_Y_whenBlocked = err_Y;
					xMoveDelay = 100;
					yVelocity = 0;
					xVelocity = err_X > 0 ? 1000 : -1000;
					dirOut = 0;
				}
				else if (xMoveDelay > 0 && err_Y != err_Y_whenBlocked)
				{
					xMoveDelay--;
					yVelocity = 0;
					xVelocity = err_X > 0 ? 1000 : -1000;
					dirOut = 0;
				}
				else if (err_Y_whenBlocked == err_Y)
				{
					xMoveDelay = 0;
				}
				//float dirOut=pid_Direction.calcPid(err_Y>0?err_X:-err_X);
				// dirOut=300;

				// pa_updateMotorPwm(1, -globalCpp.pid_Motor1.calcPid(speedOfMotors.speedOfM1 + dirOut));
				// pa_updateMotorPwm(2, -globalCpp.pid_Motor2.calcPid(speedOfMotors.speedOfM2 - dirOut));
				// float out = -globalCpp.pid_Motor3.calcPid(speedOfMotors.speedOfM3 - dirOut);
				// pa_updateMotorPwm(3, out);
				// // pa_updateMotorPwm(2,300);
				// pa_updateMotorPwm(4, -globalCpp.pid_Motor4.calcPid(speedOfMotors.speedOfM4 + dirOut));
				#define speedPid
				#ifdef speedPid
					xVelocity=0;
					pa_updateMotorPwm(1,
								  -globalCpp.pid_Motor1.calcPid(speedOfMotors.speedOfM1 - (dirOut + yVelocity - xVelocity)));
					pa_updateMotorPwm(2,
									-globalCpp.pid_Motor2.calcPid(speedOfMotors.speedOfM2 - (-dirOut + yVelocity + xVelocity)));
					//float out = -globalCpp.pid_Motor3.calcPid(speedOfMotors.speedOfM3 - dirOut);
					pa_updateMotorPwm(3,
									-globalCpp.pid_Motor3.calcPid(speedOfMotors.speedOfM3 - (-dirOut + yVelocity - xVelocity)));
					// pa_updateMotorPwm(2,300);
					pa_updateMotorPwm(4,
									-globalCpp.pid_Motor4.calcPid(speedOfMotors.speedOfM4 - (dirOut + yVelocity + xVelocity)));
				#endif
				#ifndef speedPid
				//yVelocity = 0;
				
					pa_updateMotorPwm(1,
								  (dirOut + yVelocity - xVelocity));
					pa_updateMotorPwm(2,
									(-dirOut + yVelocity + xVelocity));
					//float out = -globalCpp.pid_Motor3.calcPid(speedOfMotors.speedOfM3 - dirOut);
					pa_updateMotorPwm(3,
									(-dirOut + yVelocity - xVelocity));
					// pa_updateMotorPwm(2,300);
					pa_updateMotorPwm(4,
									(dirOut + yVelocity + xVelocity));
				#endif
				{
					char buf[30] = {0};
					sprintf(buf, "%f\r\n",
							dirOut);
					// sprintf(buf, "%f %5d %2d %2d\r\n",
					// 		dirOut, globalCpp.targetSpeed,err_X,err_Y);

					UART_PutStr(UART2, buf);
				}
			}

			checkUartData();
		}

		if (flag_100ms)
		{
			flag_100ms = 0;
			//触发超声波
			ultrasonicDistance1.trig();
			//互相关计算
			err_X = crossCorrelation_noFFT(adc_arr1, adc_arr2);
			err_Y = crossCorrelation_noFFT(adc_arr3, adc_arr4);

			//计算fft并且检查信标打开
			checkBeaconOn();
		}

		// checkSignalStable();
		//输出内容的判断
		switch (globalCpp.micOutPutMode)
		{
		case OutputMode_cross:
		{
			char buf[30] = {0};
			sprintf(buf, "%5d %5d\r\n",
					err_X,
					err_Y);

			UART_PutStr(UART2, buf);
		}
		break;
		case OutputMode_Mic:
		{
			//adc值实时***************************************
			{
				char buf[30] = {0};
				sprintf(buf, "%5d %5d %5d %5d\r\n",
						adcValue1,
						adcValue2,
						adcValue3,
						adcValue4);

				UART_PutStr(UART2, buf);
			}
			//***************************************************
		}
		break;
		case OutputMode_UltrasonicDistance:
		{
			//adc值实时***************************************
			{
				char buf[30] = {0};
				sprintf(buf, "%5d\r\n",
						ultrasonicDistance1.distance);
				UART_PutStr(UART2, buf);
			}
			//***************************************************
			break;
		}
		case OutputMode_motor: //编码器速度
		{
			{
				char buf[30] = {0};
				sprintf(buf, "%5d %5d %5d %5d\r\n",
						speedOfMotors.speedOfM1,
						speedOfMotors.speedOfM2,
						speedOfMotors.speedOfM3,
						speedOfMotors.speedOfM4);
				// sprintf(buf, "%f %5d %2d %2d\r\n",
				// 		dirOut, globalCpp.targetSpeed,err_X,err_Y);

				UART_PutStr(UART2, buf);
			}
			break;
		}
		}
	}
}

void checkBeaconOn()
{
	{
		//FFT变换结果，必须对齐256位，整数倍
		cfloat32 fftIn[adc_arrlen];
		cfloat32 fftOut[adc_arrlen];
		short count = 0;
		for (int i = 0; i < adc_arrlen; i++)
		{
			fftIn[i].imag = 0;
			fftIn[i].real = adc_arr1[i];
		}
		/* 初始化为 FFT  注意 这里FFT输出结果是实际结果的64倍  */
		Ifx_FftF32_radix2(fftOut, fftIn, adc_arrlen);
		// for(int i=60;i<300;i++){
		// 	if(fabs(fftOut[i].real)>1000)
		// 	{
		// 		count++;
		// 	}
		// 	{
		// 		char buf[30] = {0};
		// 		sprintf(buf, "%f\r\n",
		// 				fabs(fftOut[i].real));

		// 		UART_PutStr(UART2, buf);
		// 	}
		// }

		for (int i = 101; i < 300; i++)
		{
			if (fabs(fftOut[i + 62].real) > 1000)
			{
				count++;
			}
		}
		for (int i = 82; i < 101; i++)
		{
			if (fabs(fftOut[i + 62].real) > -52 * i + 6316)
			{
				count++;
			}
		}
		for (int i = 0; i < 82; i++)
		{
			if (fabs(fftOut[i + 62].real) > 24 * i)
			{
				count++;
			}
		}

		// for(int i=0;i<170;i++){
		// 	{
		// 		UART_PutStr(UART2, "0\r\n");
		// 	}
		// }

		// {
		// 	char buf[30] = {0};
		// 	sprintf(buf, "%d\r\n",
		// 			count);

		// 	UART_PutStr(UART2, buf);
		// }
		rightFftCount = count;
		if(lastrightFftCount!=rightFftCount){
			if(!checkSignalStable()){
				OLED_ShowString(0,0,"BeaconOff     ",8);	
			}else{
				OLED_ShowString(0,0,"BeaconOn      ",8);
			}
		}
		lastrightFftCount=rightFftCount;
	}
}

char checkSignalStable()
{ 
	return rightFftCount >= 55;
}

void paLinearInterpolation(unsigned short to1, unsigned short to2, unsigned short count)
{

	// int from1 = adc_arr1[step];
	// int from2 = adc_arr2[step];

	// int deltaValue1 = to1 - from1;
	// int deltaValue2 = to2 - from2;
	// for (unsigned short i = 0; i < count; i++)
	// {
	// 	addValueToArr(
	// 		(unsigned short)(from1 + (deltaValue1 * (i + 1)) / count),
	// 		(unsigned short)(from2 + (deltaValue2 * (i + 1)) / count));
	// }
}

char endBuffer = 0;

void addValueToArr(unsigned short value1, unsigned short value2, unsigned short value3, unsigned short value4)
{
	if (endBuffer)
	{
		return;
	}
	// {
	// 	char buffer[30] = "";
	// 	sprintf(buffer, "%d\r\n",value1);
	// 	UART_PutStr(UART2, buffer);
	// }

	adc_arr1[step] = value1;
	adc_arr2[step] = value2;
	adc_arr3[step] = value3;
	adc_arr4[step] = value4;

	// adc_arr1[step] = value1==0?1:value1;
	// adc_arr2[step] = value2==0?1:value2;

	step += 1;
	if (step == adc_arrlen)
	{
		step = 0;
		endBuffer = 1;
	}
	//step %= adc_arrlen;
}

void getadc()
{
	adcValue1 = ADC_Read(ADC0) / 20;
	adcValue2 = ADC_Read(ADC4) / 20;

	adcValue3 = ADC_Read(ADC1) / 20;
	adcValue4 = ADC_Read(ADC2) / 20;
	addValueToArr(adcValue1, adcValue2, adcValue3, adcValue4);
	//paLinearInterpolation(adc1_val,adc2_val,3);
	// adc_arr1[step] = adc1_val;
	// adc_arr2[step] = adc2_val;
	// {
	// 	char buffer[30];
	// 	sprintf(buffer,"%d %d\r\n",adc_arr1[step],adc_arr2[step]);
	// 	UART_PutStr(UART2,buffer);
	// }
	// step+=1;
	// step %= adc_arrlen;
}
// unsigned short adc_arr1_copy[adc_arrlen];
// unsigned short adc_arr2_copy[adc_arrlen];
bool lockRead = false;
char crossCorrelation_noFFT(unsigned short arr1[], unsigned short arr2[])
{
	long max = 0, maxpos = 0;
#define detectRange 10
	int i = -detectRange;
	if (endBuffer)
	{

		// 发送收集到的adc*************************************************
		// for (int a = 0; a < adc_arrlen; a++)
		// {
		// 	{
		// 		char buffer[30] = "";
		// 		sprintf(buffer, "%d %d\r\n", adc_arr1[a], adc_arr2[a]);
		// 		UART_PutStr(UART2, buffer);
		// 	}
		// }
		// UART_PutStr(UART2, "done\r\n");
		// **********************************************
		endBuffer = 0;
	}

	for (; i < detectRange; i++)
	{
		long sum = 0;
		for (int t = 0; t < adc_arrlen; t++)
		{
			int first_pos = t;
			int second_pos = t + i;
			if (second_pos < 0)
			{
				second_pos += adc_arrlen;
			}
			else if (second_pos >= adc_arrlen)
			{
				second_pos -= adc_arrlen;
			}
			// if (t + i < 0 || t + i > adc_arrlen - 1)
			// 	continue;
			sum += (long)arr1[first_pos] * arr2[second_pos];
		}
		if (sum > max)
		{
			max = sum;
			maxpos = i;
		}
		if (globalCpp.micOutPutMode == OutputMode_crossDetail)
		{
			char buffer[30] = "";
			sprintf(buffer, "%d\r\n", sum);
			UART_PutStr(UART2, buffer);
		}
	}
	if (globalCpp.micOutPutMode == OutputMode_crossDetail)
	{
		UART_PutStr(UART2, "0\r\n");
	}
	// UART_PutStr(UART2, "0\r\n");
	// {
	// 	char buffer[30] = "";
	// 	sprintf(buffer, "%d\r\n", maxpos);
	// 	UART_PutStr(UART2, buffer);
	// }
	return maxpos;
}
