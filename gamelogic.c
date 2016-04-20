/*
 * cs120b_project.c
 *  Author: Tien Tran CS120B Winter 2015
 * Thanks to
 * http://uzebox.org/files/NES-controller-Hydra-Ch6All-v1.0.pdf
 * http://www.circuitvalley.com/2012/02/lcd-custom-character-hd44780-16x2.html
 * for showing me how to use the SNES and custom characters on LCD.
 * Also got help the various documents on iLearn on shift registers and the LED matrix
 */ 

#define F_CPU 1000000UL
#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include <math.h>
#include <avr/interrupt.h>
volatile unsigned char TimerFlag=0; // ISR raises, main() lowers

unsigned long _avr_timer_M = 1; // Start count from here, down to 0. Default 1 ms.
unsigned long _avr_timer_cntcurr = 0; // Current internal count of 1ms ticks


//****************
//Timer functions
//****************

void TimerOn() {
	TCCR1B = 0x0B;
	OCR1A = 125;
	TIMSK1 = 0x02;
	TCNT1=0;
	_avr_timer_cntcurr = _avr_timer_M;
	SREG |= 0x80;
}

void TimerOff() {
	
	TCCR1B = 0x00;
}

ISR(TIMER1_COMPA_vect) {
	
	_avr_timer_cntcurr--;
	if (_avr_timer_cntcurr == 0) {
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

void TimerSet(unsigned long M) {
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}


//Struct for tasks
typedef struct task{
	int state;
	unsigned long period;
	unsigned long elapsedtime;
	int(*TickFct)(int);
} task;

//Finds the gcd of 2 numbers
unsigned long getGCD ( unsigned long a, unsigned long b )
{
	unsigned long c;
	while(a != 0) {
		c = a; a = b%a;  b = c;
	}
	return b;
}

//Number of tasks
unsigned long tasknum = 4;
task tasks[4];

//Player's position on the LED matrix (blue)
unsigned char playerCol = 0xF7;
unsigned char playerRow = 0x08;

//Middle
unsigned char midRow = 0x18;
unsigned char midCol = 0xE7;

//Enemy's position on the LED matrix (green)
unsigned char enemyCol[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned char enemyRow[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
	
//Columns
unsigned char columns[] = {0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, 0xFE};

//Number of enemies, starting with 1
unsigned char numEnemies = 1;
unsigned char enemyInc = 0;

//Total Points
unsigned char points = 0;

//Status of game
unsigned char gameStatus = 1;

//Enemies flag
unsigned char first = 0;
unsigned char cnt1 = 0;
unsigned char form = 0;

//Set bit
unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b) {
	return (b ? x | (0x01 << k) : x & ~(0x01 << k));
}

//Get bit
unsigned char GetBit(unsigned char x, unsigned char k) {
	return ((x & (0x01 << k)) != 0);
}

//Custome LCD characters
void writeChar(){
		LCD_WriteCommand(0x40);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x0A);
		LCD_WriteData(0x0E);
		LCD_WriteData(0x1F);
		LCD_WriteData(0x04);
		LCD_WriteData(0x04);
		LCD_WriteData(0x0A);
		LCD_WriteData(0x11);
		//LCD_WriteCommand(0x80);
}

//Display the LCD sceen points during game
void LCDprogress(){
	LCD_Cursor(28);
	LCD_WriteData( (points / 10) + '0');
	LCD_Cursor(29);
	LCD_WriteData( (points % 10) + '0');
}

void LCDend(){
	LCD_DisplayString(1, "   GAME OVER!       POINTS:");
	LCD_Cursor(17);
	LCD_WriteData(0x00);
	LCD_Cursor(32);
	LCD_WriteData(0x00);
	LCDprogress();
}
//Transfers data to the blue leds
void transmit_data(unsigned char data) {
	int i;
	for (i = 0; i < 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTC = 0x08;
		// set SER = next bit of data to be sent.
		PORTC |= ((data >> i) & 0x01);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTC |= 0x02;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTC |= 0x04;
	// clears all lines in preparation of a new transmission
	PORTC = 0x00;
}

//Transfers data to the green led
void transmit_data2(unsigned char data) {
	int i;
	unsigned char ex = 0;
	for (i = 0; i < 8 ; ++i) {
		// Sets SRCLR to 1 allowing data to be set
		// Also clears SRCLK in preparation of sending data
		PORTC = 0x80;
		// set SER = next bit of data to be sent.
		if((3-i) < 0){
			PORTC |= ((data >> (ex)) & 0x10);
			ex++;
		}
		else
		PORTC |= ((data << (4-i)) & 0x10);
		// set SRCLK = 1. Rising edge shifts next bit of data into the shift register
		PORTC |= 0x20;
	}
	// set RCLK = 1. Rising edge copies data from “Shift” register to “Storage” register
	PORTC |= 0x40;
	// clears all lines in preparation of a new transmission
	PORTC = 0x00;
}

//reset game variables
void resetGame(){
	numEnemies = 1;
	points = 0;
	gameStatus = 0;
	first = 0;
	cnt1 = 0;
	form = 0;
	playerCol = 0xF7;
	playerRow = 0x08;
	for(unsigned i = 0; i < 8; i++){
		enemyCol[i] = 0xFF;
		enemyRow[i] = 0x80;
	}
	LCD_DisplayString(1, "Game in progress    Points:");
	
	LCD_Cursor(17);
	LCD_WriteData(0x00);
	LCD_Cursor(32);
	LCD_WriteData(0x00);
	LCD_Cursor(28);
	
}

//generateEnemies creates and moves enemies generated on the LED matrix
unsigned char generateEnemies(unsigned char num){
	//Create enemies
	
	if(cnt1 < 8){
		if(form == 0){ //UP -> DOWN pattern
			//Initialize
			if(cnt1 == 0)
				for(unsigned char i = 0; i < num; i++){
					enemyRow[i] = 0x80;
				}

			//Create
			for(unsigned long i = 0; i < num; i++){
				if(enemyRow[i] == 0x80){
					enemyRow[i] = 0x01;
					enemyCol[i] = columns[rand() % 8];
					first = 0;
				}
				else
					first = 1;
			}
	
			//Move enemies
			for(unsigned long i = 0; i < num; i++){
				if(enemyRow[i] != 0x80 && first != 0){
					enemyRow[i] = enemyRow[i] << 1;
				}
			}
		}
		else if(form == 1){ //DOWN -> UP pattern
			//Initialize
			if(cnt1 == 0)
				for(unsigned char i = 0; i < num; i++){
					enemyRow[i] = 0x01;
				}
			
			//Create
			for(unsigned long i = 0; i < num; i++){
				if(enemyRow[i] == 0x01){
					enemyRow[i] = 0x80;
					enemyCol[i] = columns[rand() % 8];
					first = 0;
				}
				else
					first = 1;
			}
			
			//Move enemies
			for(unsigned long i = 0; i < num; i++){
				if(enemyRow[i] != 0x01 && first != 0)
					enemyRow[i] = enemyRow[i] >> 1;
			}
		}
		//Collision
		for(unsigned long i = 0; i < num; i++){
			if(playerRow == enemyRow[i] && playerCol == enemyCol[i])
				return 1;
		}
		cnt1++;
	}
	else{
		form = rand() % 2;
		cnt1 = 0;
	}
	return 0;
}

//Gets the buttons of the SNES controller
unsigned short getButtons() {
	
	unsigned short val = 0x00;
	PORTB = SetBit(PINB, 3, 1);
	//_delay_us(12);
	PORTB = SetBit(PINB, 3, 0);
	//_delay_us(6);
	
	for(unsigned char i = 0; i < 16; i++){
		if(GetBit(~PINB, 2))
		val = SetBit(val, i, 1);
		else
		val = SetBit(val, i, 0);
		
		PORTB = SetBit(PINB, 4, 1);
		//_delay_ms(6);
		PORTB = SetBit(PINB, 4, 0);
		//_delay_ms(6);
	}
	
	return val; //Returns value of
}

//Controls the user movement
enum user_states{user_start, user_move};
int user_tick(int state);

//Controls enemy movement and collision
enum enemy_states{enemy_start, moveEnemy, EnemyWin};
int enemy_tick(int state);

//Controls the middle
enum mid_states{mid_start, mid_chk, mid_gameover};
int mid_states(int state);

//Outputs to the LED matrix
enum display_states{display_start, display_user, display_enemy, display_mid, display_gameover};
int display_tick(int state);

//Keeps track of time

enum timerMid_states{timer_start, timerINC, timerEnd};
int timerMid_tick(int state);

int user_tick(int state){
static unsigned char startbutton = 0;
	switch(state){
		case user_start:
			//resetGame();
			state = user_move;
			break;
		case user_move:
			if(startbutton == 1){
				//resetGame();
				state = user_start;
				startbutton = 0;
			}
			else
				state = user_move;
		default:
			state = user_move;
			break;
	}
	
	switch(state){
		case user_start:
			break;
		case user_move:
			//Move up
			if(getButtons() == 16){
				if(playerRow != 0x01){
					playerRow = (playerRow >> 1);
					playerRow = playerRow;
				}
			}
			//Move down
			else if (getButtons() == 32){
				if(playerRow != 0x80){
					playerRow = (playerRow << 1);
					playerRow = playerRow;
				}
			}
			//Move left
			else if(getButtons() == 64){
				if(playerCol != 0x7F){
					playerCol = (playerCol << 1) | 0x01;
				}
			}
			//Move right
			else if(getButtons() == 128){
				if(playerCol != 0xFE){
					playerCol = (playerCol >> 1) | 0x80;
				}
			}
			//Start button
			else if(getButtons() == 8){
				startbutton = 1;
				resetGame();
			}
			break;
		default:
			break;
	}
	return state;
}


int enemy_tick(int state){
	switch(state){
		case enemy_start:
			state = moveEnemy;
			break;
		case moveEnemy:
			if(gameStatus == 1)
				state = EnemyWin;
			else{
				if(generateEnemies(numEnemies) == 1){
					gameStatus = 1;
					state = EnemyWin;
				}
				else
					state = moveEnemy;
			}
			break;
		case EnemyWin:
			if(gameStatus == 1)
				state = EnemyWin;
			else
				state = enemy_start;
			break;
		default:
			break;
	}
	
	switch(state){
		case enemy_start:
			break;
		case moveEnemy:
			break;
		case EnemyWin:
			break;
		default:
			break;
	}
	return state;
}

//Keeps track of the points and collision on the 4 illuminated dots in middle
int timerMid_tick(int state){
	static cnt = 0;
	switch(state){
		case timer_start:
			state = timerINC;
			break;
		case timerINC:
			if(gameStatus == 1)
				state = timerEnd;
			else
				state = timerINC;
			break;
		case timerEnd:
			if(gameStatus == 0)
				state = timer_start;
			else
				state = timerEnd;
			break;
	}
	
	switch(state){
		case timer_start:
			break;
		case timerINC:
			if(cnt < 2 && ((playerCol == 0xEF && playerRow == 0x08) ||	
						  (playerCol == 0xF7 && playerRow == 0x08) ||
						  (playerCol == 0xEF && playerRow == 0x10) ||
						  (playerCol == 0xF7 && playerRow == 0x10)))  
				cnt++;
			else if(cnt >= 2){
				points++;
				LCDprogress();
				if(points == 3){
					numEnemies++;
				}
				else if(points == 6){
					numEnemies++;
				}
				else if(points == 9){
					numEnemies++;
				}
				else if(points == 11){
					numEnemies++;
				}
				else if(points == 15){
					numEnemies++;
				}
				else if(points == 20){
					numEnemies++;
				}
				cnt = 0;
			}
			else
				cnt = 0;
			break;
		case timerEnd:
			break;	
	}
	return state;
};

int display_tick(int state){
	static unsigned long chk = 0;
	
	switch(state){
		case display_start:
			if(gameStatus == 1){
				LCDend();
				state = display_gameover;
			}
			else
				state = display_user;
			break;
		case display_user:
			if(gameStatus == 1){
				LCDend();
				state = display_gameover;
			}
			else if(chk < 1){
				state = display_user;
			}
			else{
				state = display_enemy;
				chk = 0;
			}
			break;
		case display_enemy:
			if(gameStatus == 1){
				LCDend();
				state = display_gameover;
			}
			else if(chk < numEnemies)
				state = display_enemy;
			else{
				state = display_mid;
				chk = 0;
			}
			break;
		case display_mid:
			if(gameStatus == 1){
				LCDend();
				state = display_gameover;
			}
			else if(chk < 1)
				state = display_mid;
			else{
				state = display_start;
				chk = 0;
			}
			break;
		case display_gameover:
			if(gameStatus == 1)
				state = display_gameover;
			else
				state = display_start;
			break;
		default:
			state = display_start;
			break;
	}
	
	switch(state){
		case display_start:
			break;
		case display_user:
			transmit_data2(0xFF);
			transmit_data(playerCol);
			PORTD = playerRow;
			chk++;
			break;

		case display_enemy:
			transmit_data(0xFF);
			transmit_data2(enemyCol[chk]);
			PORTD = enemyRow[chk];
			chk++;
			break;
		case display_mid:
			transmit_data(0xFF);
			transmit_data2(midCol);
			PORTD = midRow;
			chk++;
			break;
		case display_gameover:
			transmit_data2(0xFF);
			transmit_data(0xAA);
			PORTD = 0xFF;
			break;
		default:
			break;
	}
	return state;
};

//Schedulers periods
const unsigned long displayPeriod = 1;
const unsigned long userPeriod = 100;
const unsigned long timerMidPeriod = 500;
const unsigned long enemyPeriod = 250;	
const unsigned long globalGCD = 1;

void TimerISR() {
	unsigned char i;
	for (i = 0; i < tasknum; ++i) {
		if ( tasks[i].elapsedtime >= tasks[i].period ) {
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedtime = 0;
		}
		tasks[i].elapsedtime += globalGCD;
	}
}


int main(void)
{
	//srand(0);
	DDRA = 0xFF; PORTA = 0x00;
	DDRB = 0x1B; PORTB = 0xE4;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;
	
	LCD_init();
	writeChar(); //Writes custom char to memory
	LCD_DisplayString(1, "Press START      to play!");
	
	//tasks
	unsigned long i = 0;

	tasks[i].state = user_start;
	tasks[i].period = userPeriod;
	tasks[i].elapsedtime = userPeriod;
	tasks[i].TickFct = &user_tick;
	i++;

	tasks[i].state = EnemyWin;
	tasks[i].period = enemyPeriod;
	tasks[i].elapsedtime = enemyPeriod;
	tasks[i].TickFct = &enemy_tick;
	i++;

	tasks[i].state = timer_start;
	tasks[i].period = timerMidPeriod;
	tasks[i].elapsedtime = timerMidPeriod;
	tasks[i].TickFct = &timerMid_tick;
	i++;

	tasks[i].state = display_gameover;
	tasks[i].period = displayPeriod;
	tasks[i].elapsedtime = displayPeriod;
	tasks[i].TickFct = &display_tick;
	
	TimerSet(globalGCD);
	TimerOn();

    while(1)
    {
    }
}