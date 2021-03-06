/**********************************************************
Author:YouQing
Date:2018/1/27
Version：1.1

功能描述：使用STM32从A7模块获取GPS数据，并将数据通过GPRS上传至OneNet(经纬度数据分开传输)

接线说明：
STM32					A7模块
GND		<----->	GND
PA2/TX2------>	U_RXD
PA3/RX2<------	U_TXD
RX1/PA10<------ GPS_TXD

//用于调试可不接
STM32					USB-TTL模块
GND		------>	GND
TX1/PA9------>	RXD

***********************************************************/

#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "usart2.h"


//常量
#define Success 1U
#define Failure 0U

//定义变量
unsigned long  Time_Cont = 0;       //定时器计数器


char OneNetServer[] = "api.heclouds.com";       //不需要修改


char device_id[] = "24910453";    //修改为自己的设备ID
char API_KEY[] = "6w3YEj1eYlYvJtuPs5i=pKVZy5Y=";    //修改为自己的API_KEY
char sensor_gps[] = "location";				//不需要修改

unsigned int count = 0;

void errorLog(int num);
//void phone(char *number);
unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry);
//void sendMessage(char *number,char *msg);
void Sys_Soft_Reset(void);
void parseGpsBuffer(void);
void printGpsBuffer(void);
void postGpsDataToOneNet(char* API_VALUE_temp, char* device_id_temp, char* sensor_id_temp, char* lon_temp, char* lat_temp);
char* longitudeToOnenetFormat(char *lon_str_temp);
char* latitudeToOnenetFormat(char *lat_str_temp);
int Digcount(long num);

int main(void)
{	
	delay_init();
	
	NVIC_Configuration(); 	 //设置NVIC中断分组2:2位抢占优先级，2位响应优先级
	uart_init(9600);	 //串口初始化为9600
	USART2_Init(115200);	//串口2波特率9600
	Init_LEDpin();

	LED1 = 0;//开始初始化模块
	delay_ms(2000);//等待A7模块稳定
	if (sendCommand("AT+RST\r\n", "OK\r\n", 3000, 10) == Success);//复位
	else errorLog(1);
	delay_ms(10);

	if (sendCommand("AT\r\n", "OK\r\n", 3000, 10) == Success);
	else errorLog(2);
	delay_ms(10);

	if (sendCommand("AT+CPIN?\r\n", "READY", 10000, 10) == Success);
	else errorLog(3);
	delay_ms(10);

	if (sendCommand("AT+CREG?\r\n", "CREG: 1", 1000, 10) == Success);
	else errorLog(4);
	delay_ms(10);

	if (sendCommand("AT+GPS=1\r\n", "OK\r\n", 3000, 10) == Success);
	else errorLog(8);
	delay_ms(10);
	
	if (sendCommand("AT+CGATT=1\r\n", "OK\r\n", 5000, 10) == Success);
	else errorLog(5);
	delay_ms(10);

	if (sendCommand("AT+CGDCONT=1,\"IP\",\"CMNET\"\r\n", "OK\r\n", 1000, 10) == Success);
	else errorLog(6);
	delay_ms(10);


	if (sendCommand("AT+CGACT=1,1\r\n","OK\r\n", 5000, 10) == Success);
	else errorLog(7);
	delay_ms(10);
	LED1 = 1;//模块初始化完成
	
	clrStruct();//清空缓存结构体
	while(1)
	{
		parseGpsBuffer();//解析GPS数据
		printGpsBuffer();//打印并上传GPS数据
		delay_ms(5000); //这里延时时间仅供参考，因为在模块操作中存在延时，重发时间以实际为准
	}
}

void parseGpsBuffer()
{
	char *subString;
	char *subStringNext;
	char i = 0;
	if (Save_Data.isGetData)
	{
		Save_Data.isGetData = false;
		printf("**************\r\n");
		printf(Save_Data.GPS_Buffer);
	
		for (i = 0 ; i <= 6 ; i++)
		{
			if (i == 0)
			{
				if ((subString = strstr(Save_Data.GPS_Buffer, ",")) == NULL)
					errorLog(1);	//解析错误
			}
			else
			{
				subString++;
				if ((subStringNext = strstr(subString, ",")) != NULL)
				{
					char usefullBuffer[2]; 
					switch(i)
					{
						case 1:memcpy(Save_Data.UTCTime, subString, subStringNext - subString);break;	//获取UTC时间
						case 2:memcpy(usefullBuffer, subString, subStringNext - subString);break;	//获取UTC时间
						case 3:memcpy(Save_Data.latitude, subString, subStringNext - subString);break;	//获取纬度信息
						case 4:memcpy(Save_Data.N_S, subString, subStringNext - subString);break;	//获取N/S
						case 5:memcpy(Save_Data.longitude, subString, subStringNext - subString);break;	//获取经度信息
						case 6:memcpy(Save_Data.E_W, subString, subStringNext - subString);break;	//获取E/W

						default:break;
					}

					subString = subStringNext;
					Save_Data.isParseData = true;
					if(usefullBuffer[0] == 'A')
						Save_Data.isUsefull = true;
					else if(usefullBuffer[0] == 'V')
						Save_Data.isUsefull = false;
				}
				else
				{
					errorLog(2);	//解析错误
				}
			}
		}
	}
}

void printGpsBuffer()
{
	if (Save_Data.isParseData)
	{
		Save_Data.isParseData = false;
		
		printf("Save_Data.UTCTime = ");
		printf(Save_Data.UTCTime);
		printf("\r\n");

		if(Save_Data.isUsefull)
		{
			Save_Data.isUsefull = false;
			printf("Save_Data.latitude = ");
			printf(Save_Data.latitude);
			printf("\r\n");


			printf("Save_Data.N_S = ");
			printf(Save_Data.N_S);
			printf("\r\n");

			printf("Save_Data.longitude = ");
			printf(Save_Data.longitude);
			printf("\r\n");

			printf("Save_Data.E_W = ");
			printf(Save_Data.E_W);
			printf("\r\n");
			
			
			postGpsDataToOneNet(API_KEY, device_id, sensor_gps, Save_Data.longitude, Save_Data.latitude);		//发送数据到Onenet
			
			LED1 = 0; //LED闪烁指示
			delay_ms(100);
			LED1 = 1;
		}
		else
		{
			printf("GPS DATA is not usefull!\r\n");
		}
		
	}
}

int Digcount(long num)
{
	int i=0;	
	while(num>0)
	{ 
		i++;
		num=num/10;
	}
  return i;
}

char* longitudeToOnenetFormat(char *lon_str_temp) 		//经度
{
	unsigned long lon_Onenet = 0;
	unsigned int dd_int = 0;
	unsigned long mm_int = 0;
//	float lon_Onenet_double = 0;
	int i = 0;

	unsigned long tempInt = 0;
	unsigned long tempPoint = 0;
	char result[20];
	char point_result[15];
	int pointLength = 0;

	//分开整数和小数换算。
	sscanf(lon_str_temp, "%ld.%ld", &tempInt,&tempPoint);
	lon_Onenet = tempInt%100;
	pointLength = strlen(lon_str_temp) - 2 - Digcount(tempInt);		
	for( i = 0 ; i < pointLength ; i++)	//小数点几位，整数部分就放大10的几次方
	{
		lon_Onenet *= 10; 	
	}

	dd_int = tempInt / 100; //取出dd

	mm_int = lon_Onenet + tempPoint; //取出MM部分

	mm_int = mm_int*10/6;	 		//本来是除以60，这里*10/6为了多2位小数点有有效数字


   	sprintf(result,"%d.",dd_int);
	for( i = 0 ; i < pointLength + 2 - Digcount(mm_int) ; i++)
	{
		strcat(result, "0");	
	}
	sprintf(point_result,"%ld",mm_int);
	strcat(result, point_result);

//	SendString("\r\n==========ONENET FORMART==========\r\n");
//	SendString(result);
	return result;
}



char* latitudeToOnenetFormat(char *lat_str_temp) 		//纬度
{
	unsigned long lat_Onenet = 0;
	int dd_int = 0;
	unsigned long mm_int = 0;

	int i = 0;

	unsigned long tempInt = 0;
	unsigned long tempPoint = 0;
	char result[20];
	char  point_result[15];
	int pointLength = 0;
//	char xdata debugTest[30];
	
	//分开整数和小数换算。
	sscanf(lat_str_temp, "%ld.%ld", &tempInt,&tempPoint);
	lat_Onenet = tempInt%100;
	
//	SendString("\r\n==========ONENET FORMART strlen(lat_str_temp)==========\r\n");
//	sprintf(debugTest,"%d",strlen(lat_str_temp));
//	SendString(debugTest);

	pointLength = strlen(lat_str_temp) - 2 - Digcount(tempInt);	

//	SendString("\r\n==========ONENET FORMART pointLength==========\r\n");
//	sprintf(debugTest,"%d",pointLength);
//	SendString(debugTest);
	for( i = 0 ; i < pointLength ; i++)	//小数点几位，整数部分就放大10的几次方
	{
		lat_Onenet *= 10; 	
	}

//	SendString("\r\n==========ONENET FORMART tempPoint==========\r\n");
//	sprintf(debugTest,"%ld",tempPoint);
//	SendString(debugTest);
//
//	SendString("\r\n==========ONENET FORMART tempInt==========\r\n");
//	sprintf(debugTest,"%ld",tempInt);
//	SendString(debugTest);
//
//	SendString("\r\n==========ONENET FORMART lat_Onenet==========\r\n");
//	sprintf(debugTest,"%ld",lat_Onenet);
//	SendString(debugTest);

	dd_int = tempInt / 100; //取出dd

	mm_int = lat_Onenet + tempPoint; //取出MM部分

	mm_int = mm_int*10/6;	 		//本来是除以60，这里*10/6为了多2位小数点有有效数字

//	SendString("\r\n==========ONENET FORMART mm_int==========\r\n");
//	sprintf(debugTest,"%ld",mm_int);
//	SendString(debugTest);

	
	sprintf(result,"%d.",dd_int);
	for( i = 0 ; i < pointLength + 2 - Digcount(mm_int) ; i++)
	{
		strcat(result, "0");	
	}
	sprintf(point_result,"%ld",mm_int);
	strcat(result, point_result);

//	SendString("\r\n==========ONENET FORMART==========\r\n");
//	SendString(result);
	return result;
}

void postGpsDataToOneNet(char* API_VALUE_temp, char* device_id_temp, char* sensor_id_temp, char* lon_temp, char* lat_temp)
{
	char send_buf[400] = {0};
	char text[200] = {0};
	char tmp[25] = {0};

	char lon_str_end[15] = {0};
	char lat_str_end[15] = {0};

	char sendCom[2] = {0x1A};

//	dtostrf(longitudeToOnenetFormat(lon_temp), 3, 6, lon_str_end); //转换成字符串输出
//	dtostrf(latitudeToOnenetFormat(lat_temp), 2, 6, lat_str_end); //转换成字符串输出

//	lon_temp = "11834.40925";
//	lat_temp = "3249.05362";

									//	sprintf(lon_str_end,"%s", longitudeToOnenetFormat(lon_temp)); 
									//	sprintf(lat_str_end,"%s", latitudeToOnenetFormat(lat_temp)); 

	//连接服务器
	memset(send_buf, 0, 400);    //清空
	strcpy(send_buf, "AT+CIPSTART=\"TCP\",\"");
	strcat(send_buf, OneNetServer);
	strcat(send_buf, "\",80\r\n");
	if (sendCommand(send_buf, "CONNECT", 10000, 3) == Success);
	else errorLog(7);

	//发送数据
	if (sendCommand("AT+CIPSEND\r\n", ">", 3000, 5) == Success);
	else errorLog(8);

	memset(send_buf, 0, 400);    //清空

	/*准备JSON串*/
//	sprintf(text, "{\"datastreams\":[{\"id\":\"%s\",\"datapoints\":[{\"value\":{\"lon\":%s,\"lat\":%s}}]}]}"
//	        , sensor_id_temp, lon_str_end, lat_str_end);
    sprintf(text, ",;%s,%s","latitude", lat_temp);     //采用分割字符串格式:type = 5
	/*准备HTTP报头*/
	send_buf[0] = 0;
	strcat(send_buf, "POST /devices/");
	strcat(send_buf, device_id_temp);
//	strcat(send_buf, "/datapoints HTTP/1.1\r\n"); //注意后面必须加上\r\n
    strcat(send_buf, "/datapoints?type=5 HTTP/1.1\r\n");	
	strcat(send_buf, "api-key:");
	strcat(send_buf, API_VALUE_temp);
	strcat(send_buf, "\r\n");
	strcat(send_buf, "Host:");
	strcat(send_buf, OneNetServer);
	strcat(send_buf, "\r\n");
	sprintf(tmp, "Content-Length:%d\r\n\r\n", strlen(text)); //计算JSON串长度
	strcat(send_buf, tmp);
	strcat(send_buf, text);

	if (sendCommand(send_buf, send_buf, 3000, 1) == Success);//上传数据
	else errorLog(9);
	delay_ms(100);

	memset(send_buf, 0, 400);    //清空
	/*准备JSON串*/
    sprintf(text, ",;%s,%s","longtitude", lon_temp);     //采用分割字符串格式:type = 5
	/*准备HTTP报头*/
	send_buf[0] = 0;
	strcat(send_buf, "POST /devices/");
	strcat(send_buf, device_id_temp);
    strcat(send_buf, "/datapoints?type=5 HTTP/1.1\r\n");	
	strcat(send_buf, "api-key:");
	strcat(send_buf, API_VALUE_temp);
	strcat(send_buf, "\r\n");
	strcat(send_buf, "Host:");
	strcat(send_buf, OneNetServer);
	strcat(send_buf, "\r\n");
	sprintf(tmp, "Content-Length:%d\r\n\r\n", strlen(text)); //计算JSON串长度
	strcat(send_buf, tmp);
	strcat(send_buf, text);

	if (sendCommand(send_buf, send_buf, 3000, 1) == Success);//上传数据
	else errorLog(9);
	delay_ms(100);
	
	if (sendCommand(sendCom, sendCom, 3000, 5) == Success);//发送0x1A,对于GPRS必须
	else errorLog(10);
	delay_ms(100);
	
	if (sendCommand("AT+CIPCLOSE\r\n", "OK\r\n", 3000, 10) == Success);
	else errorLog(11);
	delay_ms(100);
}


//void sendMessage(char *number,char *msg)
//{
//	char send_buf[20] = {0};
//	memset(send_buf, 0, 20);    //清空
//	strcpy(send_buf, "AT+CMGS=\"");
//	strcat(send_buf, number);
//	strcat(send_buf, "\"\r\n");
//	if (sendCommand(send_buf, ">", 3000, 10) == Success);
//	else errorLog(6);


//	if (sendCommand(msg, msg, 3000, 1) == Success);
//	else errorLog(7);
//	delay_ms(100);

//	memset(send_buf, 0, 100);    //清空
//	send_buf[0] = 0x1a;
//	if (sendCommand(send_buf, "OK\r\n", 10000, 5) == Success);
//	else errorLog(8);
//	delay_ms(100);
//}

void errorLog(int num)
{
	printf("ERROR%d\r\n",num);
	while (1)
	{
		if (sendCommand("AT\r\n", "OK", 100, 10) == Success)
		{
			Sys_Soft_Reset();
		}
		delay_ms(200);
	}
}

void Sys_Soft_Reset(void)
{  
    SCB->AIRCR =0X05FA0000|(u32)0x04;      
}

//void phone(char *number)
//{
//	char send_buf[20] = {0};
//	memset(send_buf, 0, 20);    //清空
//	strcpy(send_buf, "ATD");
//	strcat(send_buf, number);
//	strcat(send_buf, ";\r\n");

//	if (sendCommand(send_buf, "SOUNDER", 10000, 10) == Success);
//	else errorLog(4);
//}

unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry)
{
	unsigned char n;
	USART2_CLR_Buf();
	for (n = 0; n < Retry; n++)
	{
		u2_printf(Command); 		//发送GPRS指令
		
		printf("\r\n***************send****************\r\n");
		printf(Command);
		
		Time_Cont = 0;
		while (Time_Cont < Timeout)
		{
			delay_ms(100);
			Time_Cont += 100;
			if (strstr(USART2_RX_BUF, Response) != NULL)
			{				
				printf("\r\n***************receive****************\r\n");
				printf(USART2_RX_BUF);
				USART2_CLR_Buf();
				return Success;
			}
			
		}
		Time_Cont = 0;
	}
	printf("\r\n***************receive****************\r\n");
	printf(USART2_RX_BUF);
	USART2_CLR_Buf();
	return Failure;
}







