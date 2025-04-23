#include <ncurses.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include "constants.h"

// =============== RASPBERRY PI / ARDUINO SERIAL CONFIG ===============
#define PORT_NAME      "/dev/ttyACM0"
#define PORT_NAME2      "/dev/ttyACM1"

#define BAUD_RATE      B9600

int exitFlag = 0;
bool togglePrecision = false;                    
bool power = false;
bool ultra_precise = false;
bool max_precision = false;
// =============== TIME HELPERS ===============
long getCurrentTimeMS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// =============== COMMAND THROTTLING ===============
static const long REPEAT_DELAY_MS = 500;   // Minimum gap between repeated same commands
char lastCommandChar = 0;
long lastCommandTime = 0;

// =============== AUTO-STOP LOGIC ===============
static const long NO_MOVEMENT_STOP_DELAY_MS = 300; // If no new movement in 300ms => STOP
bool movementActive = false;
long lastMovementCommandTime = 0;

// =============== FUNCTION PROTOTYPES ===============
void *receiveThread(void *p);
void handlePacket(TPacket *packet);
void handleError(TResult error);
void sendCommand(char c);
void sendPacket(TPacket *packet);

// ----------------------------------------------------------
// MAIN
// ----------------------------------------------------------
int main()
{
    // 1) Initialize the serial connection to Arduino
    //startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 2);
    startSerial(PORT_NAME2, BAUD_RATE, 8, 'N', 1, 2); //uncomment if port changes

    printf("WAITING TWO SECONDS FOR ARDUINO TO REBOOT\n");
    sleep(2);
    printf("DONE\n");

    // 2) Spawn a thread to receive data from Arduino
    pthread_t recv;
    pthread_create(&recv, NULL, receiveThread, NULL);

    // 3) Send a hello packet to Arduino
    TPacket helloPacket;
    helloPacket.packetType = PACKET_TYPE_HELLO;
    sendPacket(&helloPacket);

    // 4) Initialize ncurses
    initscr();             // Start ncurses mode
    cbreak();              // cbreak mode => no buffering, character-by-character
    noecho();             // Do not echo pressed keys to the screen
    nodelay(stdscr, TRUE); // getch() becomes non-blocking
    scrollok(stdscr, TRUE);

    // Print usage
    printw("NCURSES CONTROLS:\n");
    printw(" w=forward, a=left, d=right, r=reverse\n");
    printw(" s=stop, c=clear stats, g=get stats,\n");
    printw(" 9 = 45 Degree Left,  0 = 45 Degree Right , q=quit\n");
    printw(" i = Close Front Claw, O = Open Front Claw \n");
    printw(" K = Close Back, L = Deposit/Open Back\n");
    printw(" G = Get Information, P = Precise Mode \n");
    printw(" [ = POWERRRRRR, ] = ULTRA PRECISE");
	printw(" M = Max Precision");
    printw("-------------------------------------\n");
    refresh();  // Update screen

    // 5) Main loop
    while(!exitFlag)
    {
        // a) Check if a key was pressed (non-blocking)
        int ch = getch();   // Returns ERR if no key was pressed
        if(ch != ERR)
        {
            // We got a character
            sendCommand((char)ch); // Our custom logic
            fflush(stdout);
        }

        // b) Check if we should auto-stop
        long now = getCurrentTimeMS();
        if(movementActive && (now - lastMovementCommandTime) > NO_MOVEMENT_STOP_DELAY_MS)
        {
            // Send STOP
            TPacket stopPacket;
            stopPacket.packetType = PACKET_TYPE_COMMAND;
            stopPacket.command = COMMAND_STOP;
            sendPacket(&stopPacket);
            movementActive = false;
        }

        // c) Sleep briefly to avoid maxing CPU
        usleep(5000); // 5ms
    }

    // 6) End ncurses mode
    endwin();

    // 7) Close serial
    printf("Closing connection to Arduino.\n");
    endSerial();

    return 0;
}

// ----------------------------------------------------------
// Precision Mode
// ----------------------------------------------------------


// ----------------------------------------------------------
// SEND COMMAND
// ----------------------------------------------------------
void sendCommand(char c)
{
    long now = getCurrentTimeMS();

    // Throttle repeated commands for the same key
    if(now - lastCommandTime < REPEAT_DELAY_MS)
    {
        return;
    }
    lastCommandChar = c;
    lastCommandTime = now;

    // Build the TPacket to send
    TPacket commandPacket {0};
    commandPacket.packetType = PACKET_TYPE_COMMAND;

    switch(c)
    {
        // Movement
        case 'w': case 'W':
            if(togglePrecision){
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 38;  // power
            }
            else if(power){
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 75;  // power
            } else if (ultra_precise) {
		commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 34;  // power
	    } else if (max_precision) {
		commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 30;  // power
	    }
            else{
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 50;  // power
            }
            commandPacket.command = COMMAND_FORWARD;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            
            break;

        case 'a': case 'A':
            if(togglePrecision){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 57;  // power
            }

	    else if(ultra_precise){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 52;  // power
            }
	    else if(max_precision){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 45;  // power
            } else{
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 65;  // power
            }
            commandPacket.command = COMMAND_TURN_LEFT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 'd': case 'D':
            if(togglePrecision){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 57;  // power
            }

            else if(ultra_precise){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 52;  // power
	}   
	    else if(max_precision){
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 45;  // power
            }
            else{
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 65;  // power
            }
            commandPacket.command = COMMAND_TURN_RIGHT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 'r': case 'R':
            if(togglePrecision){
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 38;  // power
            } else if (ultra_precise) {
		commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 34;  // power
	    } else if (max_precision) {
		commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 30;  // power
	    }else if(power){
                commandPacket.params[0] = 1;   // distance or steps
                commandPacket.params[1] = 75;  // power
            }
            else{
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 50;  // power
            }
            commandPacket.command = COMMAND_REVERSE;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 's': case 'S':
            commandPacket.command = COMMAND_STOP;
            sendPacket(&commandPacket);
            movementActive = false;
            lastMovementCommandTime = now;
            break;

        case '9':                          // 90 degree left
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 100;  // power
            commandPacket.command = COMMAND_TURN_LEFT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;
        
        case '0':                          //90 degree right
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 255;  // power
            commandPacket.command = COMMAND_TURN_RIGHT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        // Stats
        case 'c': case 'C':
            commandPacket.command = COMMAND_CLEAR_STATS;
            sendPacket(&commandPacket);
            break;
        case 'g': case 'G':
            commandPacket.command = COMMAND_GET_STATS;
            sendPacket(&commandPacket);
            break;

        // Servo
        case 'o': case 'O':
            printw("Opening Front Claw\n");
            commandPacket.command = COMMAND_CLAW_OPEN;
            sendPacket(&commandPacket);
            break;
        case 'i': case 'I':
            printw("Closing Front Claw\n");
            commandPacket.command = COMMAND_CLAW_CLOSE;
            sendPacket(&commandPacket);
            break;
         case 'l': case 'L':
            printw("Depositing, Remeber to Move After Medpack Drops \n");
            commandPacket.command = COMMAND_DEPOSIT;
            sendPacket(&commandPacket);
            break;
        case 'k': case 'K':
            printw("UnDepositing, Closing back servo \n");
            commandPacket.command = COMMAND_UNDEPOSIT;
            sendPacket(&commandPacket);
            break;

        // Quit
        case 'q': case 'Q':
            exitFlag = 1;
            break;
            
        case 'p': case 'P':
            power = false;
            togglePrecision = !togglePrecision;
	    ultra_precise = false;
            max_precision = false;
	    printw("Toggle Precision: %d \n", togglePrecision);
            refresh();
            break;
        
        case '[':
            power = !power;
            togglePrecision = false;
            ultra_precise = false;
            max_precision = false;
	    printw("Power Mode: %d \n", power);
            refresh();
            break;
 
                       
       case ']':
            ultra_precise = !ultra_precise;
            togglePrecision = false;
	    power = false;
            max_precision = false;
	printw("Ultra Precise Mode!: %d \n", ultra_precise);
            refresh();
            break;        
	case 'm':
		
		ultra_precise = false;
		togglePrecision = false;
		power = false;
		max_precision = !max_precision;
		printw("MAX PRECISION MODE!: %d \n", max_precision);
            	refresh();
            	break;
	default:
            // Unrecognized command
            printw("Unknown command: %c\n", c);
            refresh();
            break;
    }
}

// ----------------------------------------------------------
// SENDING / RECEIVING PACKETS
// ----------------------------------------------------------
void sendPacket(TPacket *packet)
{
    char buffer[PACKET_SIZE];
    int len = serialize(buffer, packet, sizeof(TPacket));
    serialWrite(buffer, len);
    fflush(stdout);
}

// Thread that continuously reads from Arduino
void *receiveThread(void *p)
{
    char buffer[PACKET_SIZE];
    int len;
    TPacket packet;
    TResult result;
    int counter=0;

    while(1)
    {
        len = serialRead(buffer);
        counter += len;
        if(len > 0)
        {
            result = deserialize(buffer, len, &packet);
            if(result == PACKET_OK)
            {
                counter=0;
                handlePacket(&packet);
            }
            else if(result != PACKET_INCOMPLETE)
            {
                printw("PACKET ERROR\n");
                refresh();
                handleError(result);
            }
        }
    }
}

// ----------------------------------------------------------
// HANDLE INCOMING PACKET FROM ARDUINO
// ----------------------------------------------------------
void handlePacket(TPacket *packet)
{
    switch(packet->packetType)
    {
        case PACKET_TYPE_COMMAND:
            // We only send commands; ignore
            break;

        case PACKET_TYPE_RESPONSE:
        {
            switch(packet->command)
            {
                case RESP_OK:
                    printw("Command OK, Precision: %d \n", togglePrecision);
                    break;
                case RESP_STATUS:
                    printw("Object Distance: %d | Colors: ",
                           packet->params[0]);
                           
                    switch(packet->params[1]){
                        case 1:  printw("RED \n");      break;
                        case 2:  printw("GREEN \n");    break;
                        default: printw("No Color \n");
                    }
                    
                    if(packet-> params[0] >= 8) printw("Get Closer to the object!!!\n");
                    break;
                default:
                    printw("Arduino is confused\n");
            }
            refresh();
            break;
        }

        case PACKET_TYPE_ERROR:
        {
            switch(packet->command)
            {
                case RESP_BAD_PACKET:    printw("Bad magic number\n");       break;
                case RESP_BAD_CHECKSUM:  printw("Bad checksum\n");           break;
                case RESP_BAD_COMMAND:   printw("Bad command\n");            break;
                case RESP_BAD_RESPONSE:  printw("Unexpected response\n");     break;
                default:                 printw("Unknown error\n");
            }
            refresh();
            break;
        }

        case PACKET_TYPE_MESSAGE:
            // If Arduino is sending sensor messages periodically
            printw("Arduino Message: %s\n", packet->data);
            refresh();
            break;
    }
}

void handleError(TResult error)
{
    switch(error)
    {
        case PACKET_BAD:
            printw("ERROR: Bad Magic Number\n");
            break;
        case PACKET_CHECKSUM_BAD:
            printw("ERROR: Bad checksum\n");
            break;
        default:
            printw("ERROR: Unknown\n");
    }
    refresh();
}
