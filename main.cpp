

#include "Timeout.h"
#include "mbed.h"
#include <string>
using namespace std;


#define     COMMAND_ON          0X70
#define     COMMAND_INIT        0X71
#define     COMMAND_GPRS        0X72
#define     COMMAND_DATA        0X73
#define     COMMAND_POST        0X74
#define     COMMAND_ANSWER      0X75
#define     COMMAND_SIGNAL      0X76
#define     COMMAND_CONNECT     0X77
#define     COMMAND_OFF         0X78
#define     HOSTING_STEP1       0x79
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

BufferedSerial serial(PA_0,PA_1, 19200); // UART TX, RX
ATCmdParser Gsm(&serial,"\r\n");
BufferedSerial Master(PC_1, PC_0,9600); //transmision de master a slave

DigitalIn   Button(BUTTON1);
DigitalOut  Led(LED1);
DigitalOut  EnableSIM900(PB_0);
DigitalOut  ResetSIM900(PA_10);

char Command;
char Sim900_Buffer[1000];
Timer       BaseClock;
char buf[256];
EventQueue      eventQueue;
Thread t;
bool statusRcvSerial;
bool isSimOn = false;
bool isCallReady = true;

char powerDownResponse[] = "POWER";
char okResponse[] = "OK";
char callReady[] = "Call Ready";
char Encrypt[174];  
char HostingData[]="{\"municipio\":\"San Jose\",\"id\":\"1068\",\"alerts\":\"AAAAAA\"}\r\n";
char HostingAnswer[]="OK+000+00:00";
// QUEUE       MasterCommand;
// Queue MasterCommand;



void onSerialReceived(void)
{

    // printf("Serial Interrupt\n");
    char *p_buf = buf;

    memset(buf, 0, sizeof(buf));
    while(serial.readable())
    {
        p_buf += serial.read(p_buf, sizeof(buf) - (p_buf - buf));
    }
    if(p_buf > buf)
    {
        printf("Received: %s\r\n", buf);
 

        //  string str= buf;
        // if (str.find(powerDownResponse) != std::string::npos){
        //         printf("GSM Apagado\n");
        //         isSimOn = false;
        // }
        // else if(str.find(okResponse) != std::string::npos && isSimOn==false){
        //         isSimOn = true;
        //         printf("Modulo GSM ya se encuentra encendido\n");
        // }
        // else if(str.find(callReady) != std::string::npos){
        //         isCallReady = true;
        //         printf("Modulo GSM Call Ready\n");
        // }
        // else{
        //     statusRcvSerial = true;
        //     printf("Buffer %s,\n",buf);
        // }
    }

    // printf("End reception\n");
}

void onSigio(void)
{
    eventQueue.call(onSerialReceived);
}

void EncenderSIM900()
{
    bool result = false;
    bool TimeOut = false;

    Gsm.printf("AT\r\n");

    auto Transcurrido = 0;
    BaseClock.reset();
    BaseClock.start();

    while(isSimOn==false) 
    {      
        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        if(Transcurrido>10000) {
            isCallReady = false;
            printf("Error Timeout GSM\n");
            if (isSimOn==false){
                    printf("GSM Apagado\n");
                    EnableSIM900 = 1;
                    wait_us(2000000);
                    EnableSIM900 = 0;
                    wait_us(2000000);

                    Gsm.printf("AT\r\n");
            }

            Transcurrido=0;
            BaseClock.reset();
        }

        // if(statusRcvSerial){
        //     Transcurrido=0;
        //     BaseClock.reset();
        //     string str= buf;
        //     if (str.find(okResponse) != std::string::npos){
        //             isSimOn = true;
        //             printf("GSM Encendido correctamente\n");
        //             break;
        //     }

        //     statusRcvSerial = false;
        // }
            
    }
    while(isCallReady==false){
        ThisThread::sleep_for(chrono::milliseconds(1000));
        printf("Esperando Call ready");

    }


}

void EncenderSIM9002()
{
    bool result2 = false;
    bool TimeOut = false;
    auto Transcurrido = 0;
    char valueCSQ[100];
    int size;
    char * p;
    char * pp;

    Gsm.send("AT");
    // ThisThread::sleep_for(chrono::milliseconds(500));

    BaseClock.reset();
    BaseClock.start();

    while(result2==false){

        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        result2 =  Gsm.recv("OK");
        printf("Resultado2 %s", result2);

        if(Transcurrido>10000) {
            printf("GSM Apagado\n");
            EnableSIM900 = 1;
            wait_us(2500000);
            EnableSIM900 = 0;
            wait_us(2500000);
            Gsm.send("AT");
            Transcurrido=0;
            BaseClock.reset();
        }

    }

 
    bool signal;
    while(true){

        signal = true;
        Gsm.send("AT+CSQ");
        printf("Confirmar calidad de la senal\n");
        Gsm.read(valueCSQ, size);
        printf("Calidad de la senal %s:\n",valueCSQ);
        p = strtok(valueCSQ, "\n");

        while(p)
        {
    

            p = strtok(NULL, "\n");

            string s = p;

            while(s.find("+CSQ: 0,0") != string::npos){
                printf("MODEM SIN SENAL \n");
                ThisThread::sleep_for(chrono::milliseconds(1000));
                signal = false;
                break;
            }
            
        }
        if(signal)
            break;

    }
}

void PrintSerialRepeat(const char*  Command,const char* Answer,uint32_t Espera,uint32_t Delay)//
{
    // memset(buf, 0, sizeof(buf));
    ThisThread::sleep_for(chrono::milliseconds(Delay));
    Gsm.printf("%s",Command);


    bool TimeOut=0;
    auto Transcurrido = 0;
    BaseClock.reset();
    BaseClock.start();

    while(true) {               // Esta funcion detiene el programa y espera una respuesta
        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        if(Transcurrido>10000) {
           printf("Error Timeout GSM: %s\n",Command);  
           break;
        }
        if(statusRcvSerial){
            string stri= buf;

            if (stri.find(Answer) != std::string::npos){
                    printf("GSM OK Respuesta correcta, %s\n", Command);
                    statusRcvSerial = false;
                    break;
            }

        }
            ThisThread::sleep_for(chrono::milliseconds(Delay));
    }

}

void IniciarSIM900()
{
    printf("Modem iniciado\n");

}

void PrintSerialRepeat2(const char*  Command,const char* Answer,uint32_t Delay)//
{


    // memset(buf, 0, sizeof(buf));
    ThisThread::sleep_for(chrono::milliseconds(Delay));
    Gsm.send(Command);

    bool result2 = false;
    bool TimeOut=0;
    auto Transcurrido = 0;
    BaseClock.reset();
    BaseClock.start();

    while(result2==false) {               // Esta funcion detiene el programa y espera una respuesta
        BaseClock.stop();
        Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();
        BaseClock.start();
        if(Transcurrido>10000) {
           printf("Error Timeout GSM: %s\n",Command);  
           break;
        }

        result2 =  Gsm.recv(Answer);
        printf("Resultado2 %s\n", result2);
        ThisThread::sleep_for(chrono::milliseconds(Delay));
    }

}

void IniciarSIM9002()
{
    printf("Envio AT+CPIN? verifica si PIN es correcto\n");
    PrintSerialRepeat2("AT+CPIN?\r\n","OK",500);                                       // Respuesta +CPIN: READY\n\nOK verifica si PIN ES CORRECTO
    printf("Envio AT+CSQ Calidad de la senal\n");    
    PrintSerialRepeat2("AT+CSQ\r\n","OK",500);                                        // Respuesta +Señal Disponible   OK

    // char value[10];
    // Gsm.send("AT+CSQ\r\n");
    // ThisThread::sleep_for(chrono::milliseconds(500));
    // bool result2 =  Gsm.recv("+CSQ:%s\r\nOK", value);
    // printf("value %s\n", value);
    // printf("Envio AT+CREG=1 habilita network\n");
    // PrintSerialRepeat2("AT+CREG=1\r\n","OK",500);                                     // Peticion de conexion GPRS   OK
}

void ConexionGPRS()
{

    char valueRecv[64];

    Gsm.send("AT\r\n");
    printf("AT %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"APN\",\"internet.comcel.com.co\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"USER\",\"COMCELWEB\"\r\n");
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK"));  
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"PWD\",\"COMCELWEB\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=1,1\r\n"); 
    printf("AT+SAPBR=1,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=2,1\r\n"); 
    Gsm.recv("+SAPBR:%s\r\nOK", valueRecv);
    printf("response AT+SAPBR=2,1 %s\n", valueRecv); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    printf("Next Step\n");
    // if(Master.writeable()) {
    //    Master.putc(NEXT_STEP);
    // }                                
}


void ReadUsuario()
{
    printf("Read Usuario\n");
    wait_us(1000000);
    for(int i=0; i<172; i++) {
        // Encrypt[i]=MasterCommand.Get();
        printf("-%c-",Encrypt[i]);
    }
    Encrypt[173]='\r';
    // if(Master.writeable()) {
    Master.write("0x85",sizeof("0x85"));
        // Master.putc(NEXT_STEP);
    // }
}

void PostEncryptHTTP() //Servicio de conexión
{
    char Len[24];    
    char value2[100];
    int val;

    Gsm.send("AT+HTTPINIT\r\n"); 
    printf("AT+HTTPINIT %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+HTTPPARA=\"CID\",1\r\n"); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+HTTPPARA=\"URL\",\"http://34.211.174.1/AIGRest/AIGService/alertPQ\"\r\n"); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 

    sprintf(Len,"AT+HTTPDATA=%i,10000\r\n",(strlen(HostingData)));// AT+HTTPDATA=AT+HTTPDATA/i  bytes 10s
    Gsm.send("%s",Len);
    printf("AT+HTTPDATA %i\r\n",Gsm.recv("OK"));
    ThisThread::sleep_for(chrono::milliseconds(500));
    Gsm.send("%s",HostingData);
    ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("AT+HTTPACTION=1\r\n"); 
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    ThisThread::sleep_for(chrono::milliseconds(1000));
    Gsm.read(value2, val);
    printf("value %s\n", value2);
}

void Step1()
{
    printf("Encender\n");
    EncenderSIM9002();
    ThisThread::sleep_for(chrono::milliseconds(1000));
    printf("Next Step\n");
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step2(){
    Gsm.send("AT+CPIN?\r\n");                                       // Respuesta +CPIN: READY\n\nOK
    Gsm.send("AT+CSQ\r\n"); 
    Gsm.send("AT+CREG=1\r\n"); 
    Gsm.send("AT+CIPSHUT\r\n");                                         // Respuesta OK
    printf("Next Step\n");
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step3()
{
    Gsm.send("AT+CIPSTATUS\r\n");                                   // Respuesta OK  STATE: IP INITIAL
    Gsm.send("AT+CIPMUX=0\r\n");                                    // Respuesta OK
    ThisThread::sleep_for(chrono::milliseconds(3000));
    Gsm.send("AT+CGATT=1\r\n");
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step4()
{
    ThisThread::sleep_for(chrono::milliseconds(3000));
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step5()
{
    Gsm.send("AT\r\n");
    printf("AT %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"Contype\",\"GPRS\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"APN\",\"internet.comcel.com.co\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"USER\",\"COMCELWEB\"\r\n");
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK"));  
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=3,1,\"PWD\",\"COMCELWEB\"\r\n"); 
    printf("AT+SAPBR=3,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step6()
{
    char valueRecv[64];

    Gsm.send("AT+SAPBR=1,1\r\n"); 
    printf("AT+SAPBR=1,1 %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+SAPBR=2,1\r\n"); 
    Gsm.recv("+SAPBR:%s\r\nOK", valueRecv);
    printf("response AT+SAPBR=2,1 %s\n", valueRecv); 
    ThisThread::sleep_for(chrono::milliseconds(2000));
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP); 
    // }
}

void Step7()
{
    Gsm.send("AT+HTTPINIT\r\n"); 
    printf("AT+HTTPINIT %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+HTTPPARA=\"CID\",1\r\n"); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 
    ThisThread::sleep_for(chrono::milliseconds(2000));

    Gsm.send("AT+HTTPPARA=\"URL\",\"http://34.211.174.1/AIGRest/AIGService/alertPQ\"\r\n"); 
    printf("AT+HTTPPARA %i\r\n",Gsm.recv("OK")); 
   // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step8()
{
    //char HostingData[]="{\"municipio\":\"Santa Ana\",\"id\":0000, \"alerts\":\"000000\"}\r\n";
    // printf("Data hosting %s\n",HostingData);
    ThisThread::sleep_for(chrono::milliseconds(100));
    // Municipio
    // HostingData[14]=MasterCommand.Get();
    // HostingData[15]=MasterCommand.Get();
    // HostingData[16]=MasterCommand.Get();
    // HostingData[17]=MasterCommand.Get();
    // HostingData[18]=MasterCommand.Get();
    // HostingData[19]=MasterCommand.Get();
    // HostingData[20]=MasterCommand.Get();
    // HostingData[21]=MasterCommand.Get();
    // Alarmas
    // HostingData[46]=MasterCommand.Get();
    // HostingData[47]=MasterCommand.Get();
    // HostingData[48]=MasterCommand.Get();
    // HostingData[49]=MasterCommand.Get();    
    // HostingData[50]=MasterCommand.Get();
    // HostingData[51]=MasterCommand.Get();
    // Identificador
    // HostingData[30]=MasterCommand.Get();
    // HostingData[31]=MasterCommand.Get();
    // HostingData[32]=MasterCommand.Get();
    // HostingData[33]=MasterCommand.Get();
    printf("Data hosting 2 %s\n",HostingData);
    // PrintSerialRepeat("AT+HTTPPARA=\"CONTENT\",\"application/json\"\r\n","OK",500,75);    // AT+HTTPPARA="CONTENT","application/json"
    // if(Master.writeable()) {        
    //     Master.putc(NEXT_STEP);
    // }
}

void Step9()
{
    char value2[100];
    int val;

    printf("-1-\n");
    char Len[23];
 
    sprintf(Len,"AT+HTTPDATA=%i,10000\r\n",(strlen(HostingData)));// AT+HTTPDATA=AT+HTTPDATA/i  bytes 10s
    Gsm.send("%s",Len);

    printf("-2-\n");                   
    ThisThread::sleep_for(chrono::milliseconds(500));

    Gsm.send("%s",HostingData);
                                        // Envio de informacion al web service
    ThisThread::sleep_for(chrono::milliseconds(500));
    
    printf("-3-\n");
    Gsm.send("AT+HTTPACTION=1\r\n"); 
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    ThisThread::sleep_for(chrono::milliseconds(1000));
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    // PrintSerialRepeat("AT+HTTPSCONT?\r\n","OK",8000,1800);

    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
    printf("-4-\n");

}
void Step10()
{
    char value2[100];
    int val;

    Gsm.send("AT+HTTPREAD\r\n"); 
    Gsm.read(value2, val);
    printf("value %s\n", value2);
    ThisThread::sleep_for(chrono::milliseconds(100));
    

    if(true) {
        // HostingAnswer[0]=Sim900_Buffer[36];
        // HostingAnswer[1]=Sim900_Buffer[37];
        // HostingAnswer[2]=Sim900_Buffer[38];
        // HostingAnswer[3]=Sim900_Buffer[39];
        // HostingAnswer[4]=Sim900_Buffer[40];
        // HostingAnswer[5]=Sim900_Buffer[41];         
        // HostingAnswer[6]=Sim900_Buffer[42];
        // HostingAnswer[7]=Sim900_Buffer[43];
        // HostingAnswer[8]=Sim900_Buffer[44];
        // HostingAnswer[9]=Sim900_Buffer[45];
        // HostingAnswer[10]=Sim900_Buffer[46];
        // HostingAnswer[11]=Sim900_Buffer[47];
        
        printf("Aceptada\n");
        printf("<--%s-->\n",value2);
    } else {
        printf("Rechazada\n");
    }
    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
}

void Step11()
{
    // if(Master.writeable()) {
    //     Master.printf("%s",HostingAnswer);
    //     Computer.printf("<--%s-->",HostingAnswer);
    // }
    // PrintSerialRepeat("AT+HTTPSCONT?\r\n","OK",2000,2000);
    // PrintSerialRepeat("AT+HTTPPARA?\r\n","OK",2000,2000);
    // PrintSerialRepeat("AT+SAPBR=4,1\r\n","OK",500,50);

    Gsm.send("AT+HTTPTERM\r\n"); 
    ThisThread::sleep_for(chrono::milliseconds(2000));
    printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
    Gsm.send("AT+SAPBR=0,1\r\n"); 
    ThisThread::sleep_for(chrono::milliseconds(2000));
    printf("AT+SAPBR=0,1 %i\r\n",Gsm.recv("OK")); 



    // if(Master.writeable()) {
    //     Master.putc(NEXT_STEP);
    // }
    HostingAnswer[0]='0';
    HostingAnswer[1]='0';
    HostingAnswer[2]='0';
    HostingAnswer[3]='0';
    HostingAnswer[4]='0';
    HostingAnswer[5]='0';
    ThisThread::sleep_for(chrono::milliseconds(4000));
}

// AT
// AT+CPIN?
// AT+CSQ
// AT+CREG=1
// AT+CGATT=1 
// AT+SAPBR=3,1,"Contype","GPRS"
// AT+SAPBR=3,1,"APN","internet.comcel.com.co"
// AT+SAPBR=3,1,"USER","COMCELWEB"
// AT+SAPBR=3,1,"PWD","COMCELWEB"
// AT+SAPBR=1,1
// AT+SAPBR=2,1
// AT+HTTPINIT
// AT+HTTPPARA="CID",1
// AT+HTTPPARA="REDIR",1
// AT+HTTPPARA="URL","http://34.211.174.1/AIGRest/AIGService/parkPQ"
// AT+HTTPPARA="URL","http://34.211.174.1/AIGRest/AIGService/alertPQ"
// AT+HTTPDATA=56,100000
// {"municipio":"San Jose","id":"1068","alerts":"AAAAAA"}
// AT+HTTPACTION=1
// AT+HTTPREAD=0,9000

// AT+HTTPTERM
// AT+SAPBR=0,1

 
int main()
{
    printf("\n-----------Paquimetro/Slave v2.0\n");
    t.start(callback(&eventQueue, &EventQueue::dispatch_forever));
    Master.sigio(callback(onSigio));
    // serial.sigio(callback(onSigio));
    BaseClock.start();
    Led=0;
    ResetSIM900=0;
    Gsm.debug_on(false);
    Gsm.set_timeout(1000);


    // printf("Encender\n");
    // EncenderSIM9002();
    // ThisThread::sleep_for(chrono::milliseconds(500));
    // printf("Ok Encencido\n");


// ThisThread::sleep_for(chrono::milliseconds(500));
// printf("AT+CPIN\n");
// Gsm.printf("AT+CPIN?\r\n");
// ThisThread::sleep_for(chrono::milliseconds(500));
// printf("AT+CSQ\n");
// Gsm.printf("AT+CSQ\r\n");
// ThisThread::sleep_for(chrono::milliseconds(500));
// printf("AT+CREG=1\\n");
// Gsm.printf("AT+CREG=1\r\n");


    while (true) 
    {
        if(Button==0) 
        {
            Led=1;
            BaseClock.start();
            BaseClock.reset();
            printf("Button\n");
            EncenderSIM9002();
            
            BaseClock.stop();
            Led=0;
        }   
        wait_us(2000000);


            // Command = COMMAND_ON;
            // Command = COMMAND_INIT;
            // Command = COMMAND_GPRS;
        EncenderSIM9002();
        ThisThread::sleep_for(chrono::milliseconds(2000));
        IniciarSIM9002();
        ThisThread::sleep_for(chrono::milliseconds(2000));
        ConexionGPRS();
        ThisThread::sleep_for(chrono::milliseconds(2000));
         PostEncryptHTTP();
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



            switch (Command) {
                case COMMAND_ON:
                   printf("-> -> Encender\n");
                    BaseClock.start();
                    BaseClock.reset();
                    EncenderSIM900();
                    Led = 0;
                    break;

                case COMMAND_INIT:
                    printf("-> -> Iniciar\n");
                    IniciarSIM9002();
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
                    // RespuestaHTTP();
                    break;

                case COMMAND_END :
                    printf("-> -> finalizar tiempo\n");
                    // FinalizarComunicacion();
                    break;

                case COMMAND_SIGNAL:
                    printf("Cobertura Sim900\n");
                    BaseClock.start();
                    BaseClock.reset();
                    // CoberturaSIM900();
                    BaseClock.stop();
                    break;

                case COMMAND_CONNECT:
                    printf("Conexion Sim900\n");
                    BaseClock.start();
                    BaseClock.reset();
                    // ConexionSIM900();
                    BaseClock.stop();
                    break;

                case COMMAND_OFF:
                    printf("-> -> Apagar\n");
                    // MasterCommand.Flush();
                    // ApagarSIM900();
                    wait_us(4000000);
                    BaseClock.stop();
                    break;

                case HOSTING_STEP1:
                    printf("Paso 1!\n");
                    BaseClock.start();
                    BaseClock.reset();
                    Step1();
                    break;

                case HOSTING_STEP2:
                    printf("Paso 2!\n");                                // Respuesta SHUT OK
                    Step2();
                    break;

                case HOSTING_STEP3:
                    printf("Paso 3!\n");
                    Step3();
                    break;

                case HOSTING_STEP4:
                    printf("Paso 4!\n");
                    Step4();                                                    // Respuesta OK
                    break;

                case HOSTING_STEP5:
                    printf("Paso 5!\n");
                    Step5();
                    break;

                case HOSTING_STEP6:
                    printf("Paso 6!\n");
                    Step6();
                    break;

                case HOSTING_STEP7:
                    printf("Paso 7!\n");
                    Step7();
                    break;

                case HOSTING_STEP8:
                    printf("Paso 8!\n");
                    Step8();
                    break;

                case HOSTING_STEP9:
                    printf("Paso 9!\n");
                    Step9();
                    break;

                case HOSTING_STEP10:
                    printf("Paso 10!\n");
                    Step10();
                    break;

                case HOSTING_STEP11:
                    printf("Paso 11!\n");
                    // MasterCommand.Flush();
                    Step11();
                    BaseClock.stop();
                    break;

                case HOSTING_OFF:
                    printf("Paso salida!\n");
                    Gsm.send("AT+HTTPTERM\r\n"); 
                    ThisThread::sleep_for(chrono::milliseconds(2000));
                    printf("AT+HTTPTERM %i\r\n",Gsm.recv("OK")); 
                    // PrintSerialRepeat("AT+HTTPTERM\r\n","OK",500,50);
                    break;
            }
        break;
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
