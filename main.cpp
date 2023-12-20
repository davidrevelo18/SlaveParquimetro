#include "Timeout.h"
#include "mbed.h"
#include <cstdio>
#include <string>
#include "Queue.h"

using namespace std;

#define     COMMAND_ON          0X70
#define     COMMAND_INIT        0X71    //113
#define     COMMAND_GPRS        0X72    //114
#define     COMMAND_DATA        0X73
#define     COMMAND_POST        0X74
#define     COMMAND_ANSWER      0X75
#define     COMMAND_SIGNAL      0X76
#define     COMMAND_CONNECT     0X77
#define     COMMAND_OFF         0X78    //120
#define     HOSTING_STEP1       0x79    //121
#define     HOSTING_STEP2       0x7A
#define     HOSTING_STEP3       0x7B
#define     HOSTING_STEP4       0x7C
#define     HOSTING_STEP5       0x7D
#define     HOSTING_STEP6       0x7E
#define     HOSTING_STEP7       0x7F
#define     HOSTING_STEP8       0x80
#define     HOSTING_STEP9       0x81
#define     HOSTING_STEP10      0x82
#define     HOSTING_STEP11      0x83
#define     HOSTING_OFF         0x84
#define     NEXT_STEP           0x85
#define     NEXT_SLEEP          0x86
#define     NO_DATA             0x87
#define     COMMAND_END         0X88

/* COMUNICACION SERIAL*/
BufferedSerial serial(PA_0,PA_1, 19200); // UART TX, RX Comunicacion con modem SIM900
static UnbufferedSerial Master(PC_1, PC_0,9600); //transmision de master a slave
ATCmdParser Gsm(&serial,"\r\n");

/* ENTRADAS Y SALIDAS DIGITALES */
DigitalIn   Button(BUTTON1);
DigitalOut  Led(LED1);
DigitalOut  EnableSIM900(PB_0);
DigitalOut  ResetSIM900(PA_10);

/* VARIABLES PARA ALMACENAR INSTRUCCIONES DE MAESTRO HACIA EL ESCLAVO*/
char Command;
Timer       BaseClock;
EventQueue eventQueue;
QUEUE MasterCommand;

/* VARIABLES PARA ALMACENAR MENSAJES DE MAESTRO A ESCLAVO*/
char Encrypt[174];  
// char HostingData[]="{\"municipio\":\"San Jose\",\"id\":\"1068\",\"alerts\":\"AAAAAA\"}\r\n";
char HostingData[]="{\"municipio\":\"xxxxxxxx\",\"id\":\"xxxx\",\"alerts\":\"xxxxxx\"}\r\n";
char HostingAnswer[]="OK+000+00:00";
char userAnswer[]="0000000+00:00+0000-00-00";

/* VARIABLES PARA INTERRUPCION SERIAL MAESTRO/ESCLAVO */
char c;
bool isSerialInterrupt = false;
char buff[10] = {};
char commandByte[1];
char commandByteUsr[1];

/* VARIABLES DE ESTATUS HTTP*/
bool okStatus = 0;
bool okStatusUsr = 0;

// INTERRUPCION DE RECEPCION SERIAL MAESTRO-ESCLAVO
void on_rx_interrupt()
{
    Master.read(&c, 1);
    buff[0] = c; 
    isSerialInterrupt = true;
    MasterCommand.Put(c);
}

// METODO PARA ENCENCED SIM Y ASEGURAR CONEXION A LA RED
void EncenderSIM9002(int timeoutEncender)
{
    bool result = false;
    bool resultCSQ = false;
    auto Transcurrido = 0;
    char valueCSQ[100];
    int size;
    char * p;
    bool signal;

    BaseClock.reset();
    BaseClock.start();

    printf("Inicia bucle encencder \n");

    EnableSIM900 = 1;
    ThisThread::sleep_for(chrono::milliseconds(1500));
    EnableSIM900 = 0;
    ThisThread::sleep_for(chrono::milliseconds(timeoutEncender));
    printf("Envio GSM encendido \n");


    while(result==false)
    {
        char valueRec[100];
        printf("Envia AT encender \n");
        Gsm.send("AT");

        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        result =  Gsm.recv("OK");
        // Gsm.recv("%s\r\n", valueRec);

        printf("encender result. %d\n",result);


        if(Transcurrido>15000) 
        {
            Transcurrido=0;
            BaseClock.reset();
            EnableSIM900 = 1;
            ThisThread::sleep_for(chrono::milliseconds(1500));
            EnableSIM900 = 0;
        }

    }

    Transcurrido=0;
    BaseClock.reset();
    BaseClock.start();

    while(true)
    {

        signal = true;
        Gsm.send("AT+CSQ");
        printf("Confirmar calidad de la senal\n");

        if(Gsm.recv("OK"))
        {
            Gsm.send("AT+CSQ");
            Gsm.read(valueCSQ, size);
            printf("---Calidad de la senal %s:---\n",valueCSQ);
            p = strtok(valueCSQ, "\n");

            while(p)
            {
        
                p = strtok(NULL, "\n");

                string s = p;

                while(s.find("+CSQ: 0,0") != string::npos)
                {
                    printf("MODEM SIN SENAL \n");
                    // ThisThread::sleep_for(chrono::milliseconds(1000));
                    signal = false;
                    break;
                }
                // printf("+++senal %s:+++\n",s.c_str());
            }
            if(signal)
            {
                printf("Fin Encender, Modem con senal > 0\n");
                break;
            }
                
        }
        else
        {
            EnableSIM900 = 1;
            ThisThread::sleep_for(chrono::milliseconds(1500));
            EnableSIM900 = 0;
        }

        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        printf("Current time %d\n",Transcurrido);
        if(Transcurrido>60000) 
        {
            printf("Timeout conexion CSQ\n");
            Transcurrido=0;
            BaseClock.reset();
            BaseClock.stop();
            EnableSIM900 = 1;
            ThisThread::sleep_for(chrono::milliseconds(1500));
            EnableSIM900 = 0;
            MasterCommand.Flush();
        }
    }
}


void IniciarSIM900()
{
    printf("Modem iniciado\n");
}

// CONFIGURACION GPRS Y APN DE LA CONEXION
void ConexionGPRS()
{
    char valueRecv[64];

    Gsm.send("AT");
    printf("AT %i\r\n",Gsm.recv("OK")); 
    // ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+CGSOCKCONT=1,\"IP\",\"internet.comcel.com.co\"");
    printf("AT+CGSOCKCONT %i\r\n",Gsm.recv("OK")); 
    // ThisThread::sleep_for(chrono::milliseconds(1000));

    Gsm.send("AT+CGPADDR"); 
    Gsm.recv("+CGPADDR:%s\r\nOK", valueRecv);
    printf("response AT+CGPADDR %s\n",valueRecv); 
    // ThisThread::sleep_for(chrono::milliseconds(1000));

    printf("Next Step\n");
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);                          
}

// APAGA SIM POR SOFTWARE
void ApagarSIM900()
{
    Gsm.send("AT+CPOF\r\n");
    printf("AT+CPOF %i\n",Gsm.recv("OK"));
    ThisThread::sleep_for(chrono::milliseconds(1000));
}


// CONEXION A ENDPOINT parkPQ
void PostEncryptHTTP() 
{
    char Len[24];    
    char value2[100];
    int val;
    char * caract;

    Gsm.send("AT+HTTPINIT\r\n"); 
    printf("AT+HTTPINIT %i\r\n",Gsm.recv("OK")); 
    // ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+HTTPPARA=\"URL\",\"http://34.211.174.1/AIGRest/AIGService/parkPQ\"\r\n"); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 
    // ThisThread::sleep_for(chrono::milliseconds(500));
   
    Gsm.send("AT+HTTPPARA=\"CONTENT\",\"application/json\""); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK"));
    // ThisThread::sleep_for(chrono::milliseconds(1000));

    sprintf(Len,"AT+HTTPDATA=%i,10000\r\n",(strlen(Encrypt)));// AT+HTTPDATA=AT+HTTPDATA/i  bytes 10s
    Gsm.send("%s",Len);
    Gsm.recv("OK");
    // ThisThread::sleep_for(chrono::milliseconds(200));
    Gsm.send("%s",Encrypt);
    ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+HTTPACTION=1\r\n"); 
    Gsm.recv("OK");
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    ThisThread::sleep_for(chrono::milliseconds(1000));
    Gsm.read(value2, val);
    printf("value action %s\n", value2);

    caract = strtok(value2, "\n");

    while(caract)
    {

        caract = strtok(NULL, "\n");
        string s = caract;
        int index = s.find(",");
        if(index!=-1)
        {

            string str2 = s.substr(index+1, 3);
            printf("Status  code %s \n",str2.c_str());

            okStatusUsr = str2.compare("200");
            printf("OKSTATuS %d\n",okStatus);
            break;

        }

    }

}

// RECIBE RESPUESTA HTTP SI STATUS ES 200
void RespuestaHTTP()
{

    char value2[100];
    int val;
    char * caract;
    bool okUserResponse = 0;

    if(okStatusUsr==0)
    {
        Gsm.send("AT+HTTPREAD"); 
        ThisThread::sleep_for(chrono::milliseconds(100));
        Gsm.read(value2, val);
        ThisThread::sleep_for(chrono::milliseconds(100));

        printf("value %s\n", value2);
        printf("size read=%d \n",val);

        printf("<--Hosting Answer User--%s-->\n",value2);
        caract = strtok(value2, "\n");

           
        while(caract)
        {
            caract = strtok(NULL, "\n");
            string s = caract;
            int index = s.find("OK+");
            if(index!=-1)
            {
                userAnswer[0]=value2[32];
                userAnswer[1]=value2[33];
                userAnswer[2]=value2[34];
                userAnswer[3]=value2[35];
                userAnswer[4]=value2[36];
                userAnswer[5]=value2[37];
                userAnswer[6]=value2[38];

                userAnswer[7]=value2[39];

                userAnswer[8]=value2[40];
                userAnswer[9]=value2[41];
                userAnswer[10]=value2[42];
                userAnswer[11]=value2[43];
                userAnswer[12]=value2[44];

                userAnswer[13]=value2[45];

                userAnswer[14]=value2[46];
                userAnswer[15]=value2[47];
                userAnswer[16]=value2[48];
                userAnswer[17]=value2[49];
                userAnswer[18]=value2[50];
                userAnswer[19]=value2[51];
                userAnswer[20]=value2[52];
                userAnswer[21]=value2[53];
                userAnswer[22]=value2[54];
                userAnswer[23]=value2[55];

                Master.write(userAnswer,sizeof(userAnswer));
                commandByteUsr[0]='A';
                Master.write(&commandByteUsr[0],1);
                okUserResponse = 1;

                break;

            }

        }
 
         if(okUserResponse==0) {
            printf("Rechazada\n");
            commandByteUsr[0]='R';
            Master.write(&commandByteUsr[0],1);
        }
    }
    else{
        printf("Rechazada\n");
        commandByteUsr[0]='R';
        Master.write(&commandByteUsr[0],1);
    }


    // Gsm.send("AT+HTTPTERM"); 
    // ThisThread::sleep_for(chrono::milliseconds(500));
    // printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
    // Gsm.send("AT+SAPBR=0,1"); 
    // ThisThread::sleep_for(chrono::milliseconds(500));
    // printf("AT+SAPBR=0,1 %i\r\n",Gsm.recv("OK")); 
}

// TERMINA CONEXION HTTP CON MODEM
void FinalizarComunicacion()
{
    Gsm.send("AT+HTTPTERM"); 
    // ThisThread::sleep_for(chrono::milliseconds(500));
    printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
}

// LEE INFORNACION DEL USUARIO A TRAVES DEL MAESTRO
void ReadUsuario()
{
    printf("Read Usuario\n");
    ThisThread::sleep_for(chrono::milliseconds(500));
    for(int i=0; i<172; i++) {
        Encrypt[i]=MasterCommand.Get();
        printf("-%c-",Encrypt[i]);
    }
    Encrypt[173]='\r';
    commandByteUsr[0]=NEXT_STEP;
    Master.write(&commandByteUsr[0],1);
}

// VERIFIC CONEXION CON SIM900
void ConexionSIM900()
{
    char ConectionEnable;
    auto Transcurrido = 0;
    ConectionEnable='A';

    Gsm.send("AT");

    if(Gsm.recv("OK")==0){
        printf("At = 0");
        EnableSIM900 = 1;
        ThisThread::sleep_for(chrono::milliseconds(1000));
        EnableSIM900 = 0;

        Gsm.send("AT");
        BaseClock.reset();
        BaseClock.start();


        while(true){

            BaseClock.stop();
            Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
            BaseClock.start();

            if(Gsm.recv("OK")==1){
                printf("AT OK\n");
                break;
            }
            else{
                printf("Esperando conexion \n");
                Gsm.send("AT");
                ThisThread::sleep_for(chrono::milliseconds(1000));
            }

            if(Transcurrido>30000) 
            {
                Transcurrido=0;
                BaseClock.stop();
                BaseClock.reset();
                ConectionEnable='E';
                printf("Timeout SIM sin conexion\n");
                break;
            }
        }

    }
    else{
        printf("At = 1\n");
        ThisThread::sleep_for(chrono::milliseconds(6000));
    }

    printf("Enviando estado de conexion %s\n",&ConectionEnable);
    commandByteUsr[0]=ConectionEnable;
    Master.write(&commandByteUsr[0],1);

    ThisThread::sleep_for(chrono::milliseconds(1000));
    ApagarSIM900();
}

// VERIFICA SI HAY COBERTURA DE SIM900 CON LA RED DE DATOS 
void CoberturaSIM900()
{
    char coberturaCSQ;
    char valueCSQ[100];
    int size;
    char * p;
    bool signal;
    
    Gsm.send("AT");
    if(Gsm.recv("OK")==0){
        printf("At = 0");
        EnableSIM900 = 1;
        ThisThread::sleep_for(chrono::milliseconds(1500));
        EnableSIM900 = 0;

    }

    while(true)
    {

        Gsm.send("AT+CSQ");
        printf("Confirmar calidad de la senal\n");

        if(Gsm.recv("OK"))
        {
            Gsm.send("AT+CSQ");
            Gsm.read(valueCSQ, size);
            printf("---Calidad de la senal %s:---\n",valueCSQ);

            string ss = valueCSQ;

            int index1 = ss.find(":");
            int index2 = ss.find(",");
            printf("index 1 %d\n",index1);
            printf("index 2 %d\n",index2);
            if(index1!=-1 && index2!=-1)
                {
                    if(index2-index1 == 4){
                        string str2 = ss.substr(index1+2, 4);

                        printf("Status  code %s \n",str2.c_str());
                        printf("1er %c\n", str2.at(0));
                        printf("2 %c\n", str2.at(1));
                        printf("3 %c\n", str2.at(2));
                        printf("4 %c\n", str2.at(3));

                        char digit[1];

    
                        digit[0] = str2.at(0);
                        Master.write(&digit[0],1);


                        digit[0] = str2.at(1);
                        Master.write(&digit[0],1);


                        digit[0] = str2.at(3);
                        Master.write(&digit[0],1);

                        coberturaCSQ='A';
                        commandByteUsr[0]=coberturaCSQ;
                        Master.write(&commandByteUsr[0],1);
                        ThisThread::sleep_for(chrono::milliseconds(200));

                        break;
        
                    }
                    else if(index2-index1 == 3){
                        string str2 = ss.substr(index1+2, 3);

                        printf("Status  code %s \n",str2.c_str());
                        printf("1er %c\n", str2.at(index2-1));
                        printf("2 %c\n", str2.at(1));
                        printf("3 %c\n", str2.at(2));


                        char digit[1];

                        digit[0] = str2.at(0);
                        Master.write(&digit[0],1);


                        digit[0] = str2.at(1);
                        Master.write(&digit[0],1);


                        digit[0] = '0';
                        Master.write(&digit[0],1);

                        coberturaCSQ='A';
                        commandByteUsr[0]=coberturaCSQ;
                        Master.write(&commandByteUsr[0],1);
                        ThisThread::sleep_for(chrono::milliseconds(200));
                        break;
        
                    }
                    else{   
                        coberturaCSQ='E';
                        commandByteUsr[0]=coberturaCSQ;
                        Master.write(&commandByteUsr[0],1);
                        ThisThread::sleep_for(chrono::milliseconds(200));
                        break;
                    }
            }
            else{
                    coberturaCSQ='E';
                    commandByteUsr[0]=coberturaCSQ;
                    Master.write(&commandByteUsr[0],1);
                    ThisThread::sleep_for(chrono::milliseconds(200));
                    break;
                }  
        }

    }

    ApagarSIM900();
    ThisThread::sleep_for(chrono::milliseconds(1000));
}


/* SECUENCIA PARA ENVIO DE ESTATUS A LA BASE DE DATOS*/

// ENCENDER SIM Y ASEGURARSE DE COBERTURA DE RED DE DATOS
void Step1()
{
    printf("Encender\n");
    BaseClock.start();
    BaseClock.reset();
    EncenderSIM9002(10000);
    BaseClock.stop();
    // ThisThread::sleep_for(chrono::milliseconds(200));

    MasterCommand.Flush();
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// BYPASS
void Step2(){
    
    // Gsm.send("AT+CPIN?\r\n");                                       // Respuesta +CPIN: READY\n\nOK
    // Gsm.send("AT+CSQ\r\n"); 
    
    // Gsm.send("AT+CREG=1\r\n"); 
    // Gsm.send("AT+CIPSHUT\r\n");                                         // Respuesta OK
    // printf("Next Step\n");
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// BYPASS
void Step3()
{
    // Gsm.send("AT+CIPSTATUS\r\n");                                   // Respuesta OK  STATE: IP INITIAL
    // Gsm.send("AT+CIPMUX=0\r\n");                                    // Respuesta OK
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Gsm.send("AT+CGATT=1\r\n");

    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// BYPASS
void Step4()
{
    // ThisThread::sleep_for(chrono::milliseconds(200));
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// CONFIGURACION APN
void Step5()
{
    Gsm.send("AT");
    printf("AT %i\r\n",Gsm.recv("OK")); 
    // ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+CGSOCKCONT=1,\"IP\",\"internet.comcel.com.co\"");
    printf("AT+CGSOCKCONT %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(1500));

    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// SE ASEGURA DE ABRIR BEARER Y DE OBTENER UNA IP PARA LA CONEXION
void Step6()
{
    char valueRecv[100];

    Gsm.send("AT+CGPADDR"); 
    Gsm.recv("+CGPADDR:%s\r\nOK", valueRecv);
    printf("response AT+CGPADDR %s\n",valueRecv); 
    ThisThread::sleep_for(chrono::milliseconds(1500));


    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// INICIA COMUNICACION HTTP
void Step7()
{
    Gsm.send("AT+HTTPINIT"); 
    printf("AT+HTTPINIT %i\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+HTTPPARA=\"URL\",\"http://34.211.174.1/AIGRest/AIGService/alertPQ\""); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 

    ThisThread::sleep_for(chrono::milliseconds(500));
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// RECIBE DATOS DEL MAESTRO PARA LA DATA DEL POST
void Step8()
{
    // char HostingData[]="{\"municipio\":\"Santa Ana\",\"id\":0000, \"alerts\":\"000000\"}\r\n";
    // printf("Data hosting %s\n",HostingData);
    // ThisThread::sleep_for(chrono::milliseconds(100));
    // Municipio
    HostingData[14]=MasterCommand.Get();
    HostingData[15]=MasterCommand.Get();
    HostingData[16]=MasterCommand.Get();
    HostingData[17]=MasterCommand.Get();
    HostingData[18]=MasterCommand.Get();
    HostingData[19]=MasterCommand.Get();
    HostingData[20]=MasterCommand.Get();
    HostingData[21]=MasterCommand.Get();
    // Alarmas
    HostingData[46]=MasterCommand.Get();
    HostingData[47]=MasterCommand.Get();
    HostingData[48]=MasterCommand.Get();
    HostingData[49]=MasterCommand.Get();    
    HostingData[50]=MasterCommand.Get();
    HostingData[51]=MasterCommand.Get();
    // Identificador
    HostingData[30]=MasterCommand.Get();
    HostingData[31]=MasterCommand.Get();
    HostingData[32]=MasterCommand.Get();
    HostingData[33]=MasterCommand.Get();
    printf("Data hosting 2 %s\n",HostingData);

    Gsm.send("AT+HTTPPARA=\"CONTENT\",\"application/json\""); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK"));
    ThisThread::sleep_for(chrono::milliseconds(500));
    // PrintSerialRepeat("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n","OK",500,75);    // AT+HTTPPARA="CONTENT","application/json"
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// SETEA LA DATA Y REALIZA EL POST A LA URL
void Step9()
{
    char value2[100];
    int val;
    char * caract;
    char Len[23];
 
    sprintf(Len,"AT+HTTPDATA=%i,10000\r\n",(strlen(HostingData)));// AT+HTTPDATA=AT+HTTPDATA/i  bytes 10s
    Gsm.send("%s",Len);
    Gsm.recv("OK");
                
    ThisThread::sleep_for(chrono::milliseconds(200));

    Gsm.send("%s",HostingData);
                                        
    ThisThread::sleep_for(chrono::milliseconds(500));
    

    Gsm.send("AT+HTTPACTION=1"); 
    Gsm.recv("OK");
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    ThisThread::sleep_for(chrono::milliseconds(4000));
    Gsm.read(value2, val);
    printf("----value action %s-----\n", value2);
    // PrintSerialRepeat("AT+HTTPSCONT?\r\n","OK",8000,1800);

    caract = strtok(value2, "\n");

    while(caract)
    {

        caract = strtok(NULL, "\n");
        string s = caract;
        int index = s.find(",");
        if(index!=-1)
        {

            string str2 = s.substr(index+1, 3);
            printf("Status  code %s \n",str2.c_str());

            okStatus = str2.compare("200");
            printf("OKSTATuS %d\n",okStatus);
            break;

        }

    }

    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

}

// SI EL ESTATUS ES 200 RECIBE LA DATA DEL SERVIDOR
void Step10()
{
    char value2[100];
    int val;

    if(okStatus==0)
    {
        Gsm.send("AT+HTTPREAD"); 
        ThisThread::sleep_for(chrono::milliseconds(100));
        Gsm.read(value2, val);
        ThisThread::sleep_for(chrono::milliseconds(100));

        printf("value %s\n", value2);
        printf("size read=%d \n",val);

        if(true) {
            // HostingAnswer[0]=value2[36];
            // HostingAnswer[1]=value2[37];
            // HostingAnswer[2]=value2[38];
            // HostingAnswer[3]=value2[39];
            // HostingAnswer[4]=value2[40];
            // HostingAnswer[5]=value2[41];         
            // HostingAnswer[6]=value2[42];
            // HostingAnswer[7]=value2[43];
            // HostingAnswer[8]=value2[44];
            // HostingAnswer[9]=value2[45];
            // HostingAnswer[10]=value2[46];
            // HostingAnswer[11]=value2[47];

            HostingAnswer[0]=value2[28];
            HostingAnswer[1]=value2[29];
            HostingAnswer[2]=value2[30];
            HostingAnswer[3]=value2[31];
            HostingAnswer[4]=value2[32];
            HostingAnswer[5]=value2[33];         
            HostingAnswer[6]=value2[34];
            HostingAnswer[7]=value2[35];
            HostingAnswer[8]=value2[36];
            HostingAnswer[9]=value2[37];
            HostingAnswer[10]=value2[38];
            HostingAnswer[11]=value2[39];

            HostingAnswer[12]=value2[40];

            // for(int i=0;i<47;i++){
            // printf("CARACETER %d %c\n",i,value2[i]);
            // }

            // printf("Aceptada\n");
            // printf("<--Hosting Answer--%s-->\n",HostingAnswer);
        } else {
            printf("Rechazada\n");
        }
    }
    
    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);
}

// ENVIA INFORMACION DE RESPUESTA AL MAESTRO Y TERMINA LA CONEXION HTTP
void Step11()
{
    printf("<--Hosting Answer 11--%s-->\n",HostingAnswer);
    Master.write(HostingAnswer,sizeof(HostingAnswer));
    // PrintSerialRepeat("AT+HTTPSCONT?\r\n","OK",2000,2000);
    // PrintSerialRepeat("AT+HTTPPARA?\r\n","OK",2000,2000);
    // PrintSerialRepeat("AT+SAPBR=4,1\r\n","OK",500,50);

    Gsm.send("AT+HTTPTERM"); 
    ThisThread::sleep_for(chrono::milliseconds(500));
    printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
    // Gsm.send("AT+SAPBR=0,1"); 
    // ThisThread::sleep_for(chrono::milliseconds(500));
    // printf("AT+SAPBR=0,1 %i\r\n",Gsm.recv("OK")); 

    commandByte[0]=NEXT_STEP;
    Master.write(&commandByte[0],1);

    HostingAnswer[0]='0';
    HostingAnswer[1]='0';
    HostingAnswer[2]='0';
    HostingAnswer[3]='0';
    HostingAnswer[4]='0';
    HostingAnswer[5]='0';
    // ThisThread::sleep_for(chrono::milliseconds(1000));

    Gsm.send("AT+CPOF\r\n");//Apaga modulo
    ThisThread::sleep_for(chrono::milliseconds(1000));
    printf("AT+CPOF %i\n",Gsm.recv("OK"));
}

int main()
{
    printf("\n-----------Paquimetro/Slave v1.2\n");
    // t.start(callback(&eventQueue, &EventQueue::dispatch_forever));
    // Master.sigio(callback(onSigio));
   
    Master.attach(&on_rx_interrupt, SerialBase::RxIrq);

    // serial.sigio(callback(onSigio));
    // BaseClock.start();
    Led=0;
    ResetSIM900=0;
    // Gsm.debug_on(false);
    Gsm.set_timeout(1000);

    while (true) 
    {


    // Step1();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step2();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step3();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step4();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step5();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step6();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step7();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step8();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step9();
    // ThisThread::sleep_for(chrono::milliseconds(6000));
    // Step10();
    // ThisThread::sleep_for(chrono::milliseconds(3000));
    // Step11();
    // ThisThread::sleep_for(chrono::milliseconds(120000));

        // if(isSerialInterrupt){

        //     printf("Serial Interrupt %d\n",buff[0]);
        //     isSerialInterrupt=false;
        // }

        if(Button==0) 
        {
            Led=1;
            BaseClock.start();
            BaseClock.reset();
            printf("Button\n");
            EncenderSIM9002(10000);
            
            BaseClock.stop();
            Led=0;
        }   
        // wait_us(2000000);
      
        while(MasterCommand.Available()>0) {
            printf("Get command \n");
            Command=MasterCommand.Get();
            printf("Current command %d\n",Command);

            switch (Command) {
                case COMMAND_ON:
                   printf("-> -> Encender\n");
                    BaseClock.start();
                    BaseClock.reset();
                    EncenderSIM9002(10000);
                    BaseClock.stop();
                    Led = 0;
                    break;

                case COMMAND_INIT:
                    printf("-> -> Iniciar\n");
                    IniciarSIM900();
                    break;

                case COMMAND_GPRS:
                    printf("-> -> GPRS\n");
                    ConexionGPRS();
                    break;

                case COMMAND_DATA:
                    printf("-> -> Datos\n");
                    ReadUsuario();
                    break;

                case COMMAND_POST:
                    printf("-> -> Post HTTP\n");
                    PostEncryptHTTP();
                    break;

                case COMMAND_ANSWER:
                    printf("-> -> Respuesta\n");
                    RespuestaHTTP();
                    break;

                case COMMAND_END :
                    printf("-> -> finalizar tiempo\n");
                    
                    FinalizarComunicacion();
                    break;

                case COMMAND_SIGNAL:
                    printf("Cobertura Sim900\n");
                    BaseClock.start();
                    BaseClock.reset();
                    CoberturaSIM900();
                    MasterCommand.Flush();
                    BaseClock.stop();
                    break;

                case COMMAND_CONNECT:
                    printf("Conexion Sim900\n");
                    BaseClock.start();
                    BaseClock.reset();
                    ConexionSIM900();
                    MasterCommand.Flush();
                    BaseClock.stop();
                    break;

                case COMMAND_OFF:
                    printf("-> -> Apagar\n");
                    MasterCommand.Flush();
                    ApagarSIM900();
                    ThisThread::sleep_for(chrono::milliseconds(1000));
                    // BaseClock.stop();

                    // // SOLO PARA PRUEBAS
                    // Command = HOSTING_STEP1;
                    // ThisThread::sleep_for(chrono::milliseconds(1*60*1000));
                    break;

                case HOSTING_STEP1:
                    printf("------------Paso 1!\n");
                    // BaseClock.start();
                    // BaseClock.reset();
                    // MasterCommand.Flush();
                    Led=1;
                    Step1();

                    // Command = HOSTING_STEP2;
                    break;

                case HOSTING_STEP2:
                    printf("------------Paso 2!\n");                                // Respuesta SHUT OK
                    Led=0;
                    Step2();

                    // Command = HOSTING_STEP3;
                    break;

                case HOSTING_STEP3:
                    printf("------------Paso 3!\n");
                    Led=1;
                    Step3();

                    // Command = HOSTING_STEP4;
                    break;

                case HOSTING_STEP4:
                    printf("------------Paso 4!\n");
                    Led=0;
                    Step4();                                                    // Respuesta OK

                    // Command = HOSTING_STEP5;
                    break;

                case HOSTING_STEP5:
                    printf("------------Paso 5!\n");
                    Led=1;
                    Step5();

                    // Command = HOSTING_STEP6;
                    break;

                case HOSTING_STEP6:
                    printf("------------Paso 6!\n");
                    Led=0;
                    Step6();

                    // Command = HOSTING_STEP7;
                    break;

                case HOSTING_STEP7:
                    printf("------------Paso 7!\n");
                    Led=1;
                    Step7();

                    // Command = HOSTING_STEP8;
                    break;

                case HOSTING_STEP8:
                    printf("------------Paso 8!\n");
                    Led=0;
                    Step8();

                    // Command = HOSTING_STEP9;
                    break;

                case HOSTING_STEP9:
                    printf("------------Paso 9!\n");
                    Led=1;
                    Step9();
                    // Command = HOSTING_STEP10;
                    break;

                case HOSTING_STEP10:
                    printf("------------Paso 10!\n");
                    Led=0;
                    Step10();
                    // Command = HOSTING_STEP11;
                    break;

                case HOSTING_STEP11:
                    printf("------------Paso 11!\n");
                    Led=1;
                    MasterCommand.Flush();
                    Step11();
                    Led=0;
                    // BaseClock.stop();

                    // Command = HOSTING_OFF;
                    break;

                case HOSTING_OFF:
                    printf("------------Paso salida!\n");
                    // Gsm.send("AT+HTTPTERM\r\n"); 
                    ThisThread::sleep_for(chrono::milliseconds(1000));
                    // printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
                    // PrintSerialRepeat("AT+HTTPTERM\r\n","OK",500,50);

                    // SOLO PARA PRUEBAS
                    // Command = COMMAND_OFF;

                    break;
            }
        }
    }
}

// Proceso Usuario, lista de comandos que se deben enviar:
// 1.COMMAND_ON      ---> Encerder el modulo.
// 2.COMMAND_INIT    ---> Cargar configuracion del modulo.
// 3.COMMAND_GPRS    ---> Iniciar conexion GPRS.
// 4.COMMAND_DATA    ---> Enviar datos [Tiempo,Identificador,Espacio] y se espera confirmacion.
// 5.COMMAND_POST    ---> Realizar post HTTP.
// 6.COMMAND_ANSWER  ---> Recibir respuesta del servicio.
// 7.COMMAND_OFF     ---> Apagar el modulo.

// Proceso Hosting, lista de comando que se deben enviar:
// 1.HOSTING_STEP1   ---> Encerder el modulo.
// 2.HOSTING_STEP2   ---> Comandos CREG Y CIPSHUT.
// 3.HOSTING_STEP3   ---> Comandos CIPSTATUS Y CIPMUX.
// 4.HOSTING_STEP4   ---> Comando CGATT.
// 5.HOSTING_STEP5   ---> Comandos SAPBR GPRS y APN.
// 6.HOSTING_STEP6   ---> Comando SAPBR 1,1.
// 7.HOSTING_STEP7   ---> Comandos HTTPINIT y HTTPPARA.
// 8.HOSTING_STEP8   ---> Se envian los datos [Indentificador, Alarmas] y HTTPPARA JSON.
// 9.HOSTING_STEP9   ---> Comandos HTTPDATA y HTTPACTION.
// 10.HOSTING_STEP10 ---> Comando HTTPREAD y se espera respuesta.
// 11.HOSTING_STEP11 ---> Comandos HTTPTERM y se enviar respuesta, y se apaga modulo.
// 12.HOSTING_OFF    ---> Comando HTTPTERM en caso de choque con usuario.

// AT
// AT+CSQ
// AT+CGSOCKCONT=1,"IP","internet.comcel.com.co"
// AT+CGPADDR
// AT+HTTPINIT
// AT+HTTPPARA="URL","http://34.211.174.1/AIGRest/AIGService/parkPQ"
// AT+HTTPPARA="URL","http://34.211.174.1/AIGRest/AIGService/alertPQ"
// AT+HTTPPARA="CONTENT","application/json"
// AT+HTTPDATA=56,10000
// {"municipio":"San Jose","id":"1068","alerts":"AAAAAA"}
// AT+HTTPACTION=1
// AT+HTTPREAD=0,9000
// AT+HTTPTERM


 

/**************************************/
        // char valueRecv[64];
        // char Len[24];    
        // char value2[100];
        // int val;
        // Gsm.send("AT\r\n");
        // printf("AT %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));


        // Gsm.send("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n"); 
        // printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+SAPBR=3,1,\"APN\",\"internet.comcel.com.co\"\r\n"); 
        // printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+SAPBR=3,1,\"USER\",\"COMCELWEB\"\r\n");
        // printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK"));  
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+SAPBR=3,1,\"PWD\",\"COMCELWEB\"\r\n"); 
        // printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+SAPBR=1,1\r\n"); 
        // printf("AT+SAPBR=1,1 %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+SAPBR=2,1\r\n"); 
        // Gsm.recv("+SAPBR:%s\r\nOK", valueRecv);
        // printf("response AT+SAPBR=2,1 %s\n", valueRecv); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+HTTPINIT\r\n"); 
        // printf("AT+HTTPINIT %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+HTTPPARA=\"CID\",1\r\n"); 
        // printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));

        // Gsm.send("AT+HTTPPARA=\"URL\",\"http://34.211.174.1/AIGRest/AIGService/alertPQ\"\r\n"); 
        // printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 

        // sprintf(Len,"AT+HTTPDATA=%i,10000\r\n",(strlen(HostingData)));// AT+HTTPDATA=AT+HTTPDATA/i  bytes 10s
        // Gsm.send("%s",Len);
        // printf("AT+HTTPDATA %i\r\n",Gsm.recv("OK"));
        // ThisThread::sleep_for(chrono::milliseconds(500));
        // Gsm.send("%s",HostingData);
        // ThisThread::sleep_for(chrono::milliseconds(500));

        // Gsm.send("AT+HTTPACTION=1\r\n"); 
        // Gsm.read(value2, val);
        // printf("value %s\n", value2);
        // ThisThread::sleep_for(chrono::milliseconds(1000));
        // Gsm.read(value2, val);
        // printf("value %s\n", value2);

        // Gsm.send("AT+HTTPREAD\r\n"); 

        // Gsm.read(value2, val);
        // printf("value %s\n", value2);

        // Gsm.send("AT+HTTPTERM\r\n"); 
        // ThisThread::sleep_for(chrono::milliseconds(2000));
        // printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 

/**************************************/