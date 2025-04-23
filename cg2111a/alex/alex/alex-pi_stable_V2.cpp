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
#define BAUD_RATE      B9600

int exitFlag = 0;

// =============== TIME HELPERS ===============
long getCurrentTimeMS()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// =============== COMMAND THROTTLING ===============
static const long REPEAT_DELAY_MS = 200;   // Minimum gap between repeated same commands
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
    startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);
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
    keypad(stdscr, TRUE);  // If you want arrow keys, F-keys, etc. recognized

    // Print usage
    printw("NCURSES CONTROLS:\n");
    printw(" w=forward, a=left, d=right, r=reverse\n");
    printw(" s=stop, c=clear stats, g=get stats,\n");
    printw(" u=servo up, j=servo down, q=quit\n");
    printw(" Y = Open Front Claw, U = Close Front Claw \n");
    printw(" I = Deposit, O = Undeposit\n");

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
// SEND COMMAND
// ----------------------------------------------------------
void sendCommand(char c)
{
    long now = getCurrentTimeMS();

    // Throttle repeated commands for the same key
    if(c == lastCommandChar && (now - lastCommandTime) < REPEAT_DELAY_MS)
    {
        return;
    }
    lastCommandChar = c;
    lastCommandTime = now;

    // Build the TPacket to send
    TPacket commandPacket;
    commandPacket.packetType = PACKET_TYPE_COMMAND;

    switch(c)
    {
        // Movement
        case 'w': case 'W':
            commandPacket.params[0] = 1;   // distance or steps
            commandPacket.params[1] = 50;  // power
            commandPacket.command = COMMAND_FORWARD;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 'a': case 'A':
            commandPacket.params[0] = 1;   // e.g. 1 degree turn
            commandPacket.params[1] = 65;
            commandPacket.command = COMMAND_TURN_LEFT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 'd': case 'D':
            commandPacket.params[0] = 1;
            commandPacket.params[1] = 65;
            commandPacket.command = COMMAND_TURN_RIGHT;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 'r': case 'R':
            commandPacket.params[0] = 1;
            commandPacket.params[1] = 50;
            commandPacket.command = COMMAND_REVERSE;
            sendPacket(&commandPacket);
            movementActive = true;
            lastMovementCommandTime = now;
            break;

        case 's': case 'S':
            commandPacket.command = COMMAND_STOP;
            sendPacket(&commandPacket);
            movementActive = false;
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
        case 'y': case 'Y':
            commandPacket.command = COMMAND_CLAW_OPEN;
            sendPacket(&commandPacket);
            break;
        case 'u': case 'U':
            commandPacket.command = COMMAND_CLAW_CLOSE;
            sendPacket(&commandPacket);
            break;
         case 'i': case 'I':
            commandPacket.command = COMMAND_DEPOSIT;
            sendPacket(&commandPacket);
            break;
        case 'o': case 'O':
            commandPacket.command = COMMAND_UNDEPOSIT;
            sendPacket(&commandPacket);
            break;

        // Quit
        case 'q': case 'Q':
            exitFlag = 1;
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
                    printw("Command OK\n");
                    break;
                case RESP_STATUS:
                    printw("Status: LFT=%d RFT=%d ...\n",
                           packet->params[0],
                           packet->params[1]);
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
