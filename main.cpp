

#include "mbed.h"
#include <string>
using namespace std;

BufferedSerial serial(PA_0,PA_1, 9600); // UART TX, RX
ATCmdParser Gsm(&serial,"\r\n");

DigitalIn   Button(BUTTON1);
DigitalOut  Led(LED1);
DigitalOut  EnableSIM900(PB_0);
DigitalOut  ResetSIM900(PA_10);

char Sim900_Buffer[1000];
Timer       BaseClock;
char buf[64];
EventQueue      eventQueue;
Thread t;
bool statusRcvSerial;

char powerDownResponse[] = "POWER";

void onSerialReceived(void)
{
    printf("Serial Interrupt\n");
    char *p_buf = buf;

    memset(buf, 0, sizeof(buf));
    while(serial.readable())
    {
        p_buf += serial.read(p_buf, sizeof(buf) - (p_buf - buf));
    }
    if(p_buf > buf)
    {
        printf("Received: %s\r\n", buf);
        statusRcvSerial = true;

        string str= buf;
        if (str.find(powerDownResponse) != std::string::npos){
            printf("GSM Apagado\n");
            statusRcvSerial = false;
        }
    }

    printf("End reception\n");
}

void onSigio(void)
{
    eventQueue.call(onSerialReceived);
}

void EncenderSIM900()
{
    bool result = false;
    bool TimeOut = false;
    char okResponse[] = "OK";

    statusRcvSerial = false;
    BaseClock.start();
    Gsm.printf("AT\r\n");

    while(true) 
    {      
        BaseClock.stop();
        auto Transcurrido = chrono::duration_cast<chrono::milliseconds>(BaseClock.elapsed_time()).count();  
        if(Transcurrido>5000) {
            printf("Error Timeout GSM\n");  
            break;
        }
        if(statusRcvSerial){
            string str= buf;
            if (str.find(okResponse) != std::string::npos){
                    printf("GSM OK Correct Response\n");
                    statusRcvSerial = false;
                    break;
            }

            if (str.find(powerDownResponse) != std::string::npos){
                    printf("GSM Apagado\n");
                    statusRcvSerial = false;
            }
        }
            
    }
}

int main()
{
    printf("Paquimetro v2.0\n");
    t.start(callback(&eventQueue, &EventQueue::dispatch_forever));
    serial.sigio(callback(onSigio));

    Led=0;
    ResetSIM900=0;
    Gsm.debug_on(false);

    while (true) 
    {
        if(Button==0) 
        {
            Led=1;
            BaseClock.start();
            BaseClock.reset();
            printf("Button\n");
            EncenderSIM900();
            BaseClock.stop();
            Led=0;
        }   
        wait_us(2000000);
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
