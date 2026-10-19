
#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <stdio.h>
#include <stdlib.h>

#define MEM_VRAM 0x06000000
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 160

typedef u16		M3LINE[SCREEN_WIDTH];
#define m3_mem	((M3LINE*)MEM_VRAM)

//colours
#define MAKE_COLOR(r,g,b) ((r) | ((g)<<5) | ((b)<<10))
#define BLACK MAKE_COLOR(0,0,0)
#define WHITE MAKE_COLOR(31,31,31)

//game feel for the paddles and ball size etc
#define PADDLE_W 8
#define PADDLE_H 24
#define BALL_SIZE 8
#define PLAYER_SPEED 2
#define BALL_SPEED_X  2
#define SERVE_DELAY 60
#define AI_SPEED 1
#define DIGIT_SCALE 3
#define WIN_SCORE 5

/* player and ball ,ball pins*/

struct rect {

	int x;
	int y;
	int prevX;
	int prevY;
	int width;
	int height;
	int velocityX;
	int velocityY;


};

// 10 digits × 5 rows × 3 dots. 1 = dot on, 0 = dot off
const u8 digitFont[10][5][3] = {
    {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},   // 0
    {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}},   // 1
    {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},   // 2
    {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},   // 3
    {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},   // 4
    {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},   // 5
    {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},   // 6
    {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},   // 7
    {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},   // 8
    {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}    // 9
};

// helpers here -----------
/* single pixel*/
void drawPixel(int x,int y, int color){
	m3_mem[y][x]=color;
}

void fillRect(int x,int y,int w,int h,u16 color){
	for(int row =y;row<y+h;row++)
	{
		for(int col =x;col<x+w;col++)
		{
			drawPixel(col,row,color);
		}
	}
}
/*entire paddle pixels */
void drawRect(struct rect* r){
	fillRect(r->x, r->y, r->width, r->height, WHITE);
}

/* to  hold the paddle in the boundaries of the screen*/

void clampToScreen(struct rect* r) {
	if (r->y < 0)
		r->y = 0;                               // ceiling
	if (r->y > SCREEN_HEIGHT - r->height)
		r->y = SCREEN_HEIGHT - r->height;       // floor
}
/* frame buffer used to clear the previous instance of pixels generated for the paddle*/
void clearPrevious(struct rect* r){
	fillRect(r->prevX, r->prevY, r->width, r->height, BLACK);
}




void savePrevious(struct rect* r) { 
		r->prevX = r->x; 
		r->prevY = r->y; 
	}

void drawCenterLine(){
	for(int y=0;y<SCREEN_HEIGHT;y+=8){
		fillRect(SCREEN_WIDTH/2,y,1,4,WHITE);
	}
}

//check for overlap
int overlaps(struct rect* a, struct rect* b)
		{
			return a->x<b->x+b->width && 
			a->x + a->width > b->x &&
			a->y < b->y + b->height &&
			a->y + a->height > b->y;



		}
//the ball speec change for the direction of y
		int hitAngle(struct rect* ball, struct rect* paddle){
			int offset =(ball->y + ball->height/2)-(paddle->y + paddle->height/2);
			return offset/6;
		}


//ball starts in the center of the screen
	void resetBall(struct rect* ball) {
    ball->x = SCREEN_WIDTH / 2 - ball->width / 2;
    ball->y = SCREEN_HEIGHT / 2 - ball->height / 2;
    ball->velocityX = 0;
    ball->velocityY = 0;
	}

	// fills the digits onto the screen
	void drawDigit(int digit, int x, int y) {
    fillRect(x, y, 3 * DIGIT_SCALE, 5 * DIGIT_SCALE, BLACK);   // wipe old digit
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 3; col++)
            if (digitFont[digit][row][col])                     // ← was: digitFont[digit][row] & (4 >> col)
                fillRect(x + col * DIGIT_SCALE, y + row * DIGIT_SCALE,
                         DIGIT_SCALE, DIGIT_SCALE, WHITE);
}

void initRect(struct rect* r, int x, int y, int w, int h) {
    r->x = x;          
	r->y = y;
    r->prevX = x;      
	r->prevY = y;
    r->width = w;      
	r->height = h;
    r->velocityX = 0;  
	r->velocityY = 0;
}
//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------


	// the vblank interrupt must be enabled for VBlankIntrWait() to work
	// since the default dispatcher handles the bios flags no vblank handler
	// is required
	irqInit(); //------>fetches about the time vblank starts
	irqEnable(IRQ_VBLANK);//------>this keeps an alarm type system to notify when this vblank starts

	SetMode(MODE_3 | BG2_ON);
	fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BLACK);

	//init pixels onto the screen paddles, balls,etc
	struct rect humanPlayer,computerPlayer,ball;

	initRect(&humanPlayer, 1, SCREEN_HEIGHT / 2 - PADDLE_H / 2, PADDLE_W, PADDLE_H);
    initRect(&computerPlayer, SCREEN_WIDTH - PADDLE_W - 1,SCREEN_HEIGHT / 2 - PADDLE_H / 2, PADDLE_W, PADDLE_H);
    initRect(&ball, SCREEN_WIDTH / 2 - BALL_SIZE / 2,SCREEN_HEIGHT / 2 - BALL_SIZE / 2, BALL_SIZE, BALL_SIZE);
    

	
	
	//the initilization of ball speed
	//ball.velocityX = BALL_SPEED_X;
    //ball.velocityY = 1;
	int gameOver =0;
	int humanScore =0;
	int computerScore =0;
	int serveTimer = SERVE_DELAY;
	int serveDirection =-1;

	
	

	
	
	//consoleDemoInit();

	// ansi escape sequence to set print co-ordinates
	// /x1b[line;columnH
	//iprintf("\x1b[10;10Hpong gba test!\n");

	while (1) {
		VBlankIntrWait();//------->calls at the cpu at the vblank time for it to start, this is put into the gba for helping devs for screen tear

		/* inputs for the buttons*/

		clearPrevious(&ball);
        clearPrevious(&humanPlayer);
        clearPrevious(&computerPlayer);

		drawCenterLine();

		drawRect(&ball);
        drawRect(&humanPlayer);
        drawRect(&computerPlayer);

		savePrevious(&ball);
        savePrevious(&humanPlayer);
        savePrevious(&computerPlayer);

		scanKeys();
		// int key_pressed = keysDown();
		// int key_released = keysUp();
		u16 held = keysHeld();
		u16 pressed = keysDown();

		humanPlayer.velocityY = 0;                                  // ← stop by default, EVERY frame

		/*key press and release as the paddle should stop*/
		if(held & KEY_UP ){
			humanPlayer.velocityY=-PLAYER_SPEED;
		}

		/*velocity of movement for the paddle*/
		if(held & KEY_DOWN )
		{
			humanPlayer.velocityY =  PLAYER_SPEED;
		}
		
		
		//move the player (update the pos)
		humanPlayer.y+=humanPlayer.velocityY;
		clampToScreen(&humanPlayer);

			

		
		// move the ball (update the pos)
        ball.x += ball.velocityX;
        ball.y += ball.velocityY;



        // bounce off ceiling and floor
        if (ball.y < 0) {
            ball.y = 0;
            ball.velocityY = -ball.velocityY;
        }
        if (ball.y > SCREEN_HEIGHT - ball.height) {
            ball.y = SCREEN_HEIGHT - ball.height;
            ball.velocityY = -ball.velocityY;
        }

		// if (ball.x < 0) {
        //     ball.x = 0;
        //     ball.velocityX = -ball.velocityX;
        // }
        // if (ball.x > SCREEN_WIDTH - ball.width) {
        //     ball.x = SCREEN_WIDTH - ball.width;
        //     ball.velocityX = -ball.velocityX;
        // }
		
		int ballCenter = ball.y+ball.height/2;
	int aiCenter = computerPlayer.y+computerPlayer.height/2;


		if(aiCenter<ballCenter-2){
		computerPlayer.y+=AI_SPEED;
		
	}

	if(aiCenter>ballCenter+2){
		computerPlayer.y-=AI_SPEED;
	}
	clampToScreen(&computerPlayer);
		//collision check happrns here for the ball and paddles
		//human player collision
		if (ball.velocityX < 0 && overlaps(&ball, &humanPlayer)) {
            ball.x = humanPlayer.x + humanPlayer.width;          
            ball.velocityX = -ball.velocityX;
            ball.velocityY = hitAngle(&ball, &humanPlayer);
        }
		//computer player collision
        if (ball.velocityX > 0 && overlaps(&ball, &computerPlayer)) {
            ball.x = computerPlayer.x - ball.width;              
            ball.velocityX = -ball.velocityX;
            ball.velocityY = hitAngle(&ball, &computerPlayer);
        }
		
		 // serve countdown
        if (serveTimer > 0) {
            serveTimer--;
            if (serveTimer == 0) {
                ball.velocityX = BALL_SPEED_X * serveDirection;
                ball.velocityY = 1;
            }
        }


		  if (ball.x < 0) {                          
            computerScore++;
            serveDirection = -1;                   
            resetBall(&ball);
            serveTimer = SERVE_DELAY;
        }

		if(ball.x>SCREEN_WIDTH - ball.width){
			humanScore++;
			serveDirection =1;
			resetBall(&ball);
			serveTimer =SERVE_DELAY;
		}
		
		//calling the digits onthe screen on those positioins on the screen
		drawDigit(humanScore, 96, 6);
        drawDigit(computerScore, 135, 6);

		if(gameOver && (pressed & KEY_START)){
			humanScore=0;
			computerScore=0;
			gameOver=0;
			serveTimer = SERVE_DELAY;
		}

		if(humanScore>=WIN_SCORE || computerScore>=WIN_SCORE){
			gameOver=1;
			serveTimer=0;
		}

	}
}


