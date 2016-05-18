//
//  main.c
//  GameOfLife
//
//  Created by Fielding Johnston on 3/11/13.
//
//

#include <iostream>
#include <time.h>
#include <sstream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "timer.h"

#ifndef __MINGW32__
//#include "SDL/SDL_rotozoom.h"
#else
#include "Windows.h"
#endif

#ifdef EMSCRIPTEN
#include "emscripten.h"
#include "SDL_rotozoom.h"
#endif

using namespace std;

#define DEBUGINFO 0
#define TEXTURES 1

#define CELL_SIZE 16         // length and width (in pixels) for the square cells
#define BOARD_SIZE 1600     // Length and width for the square viewing area
#define SCREEN_BPP 32       // Screen bits per-pixels
const int SCREEN_FPS = 60;
const int SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

#ifdef APP
#define IMG_CELL "GameOfLife.app/Contents/Resources/img/pink.png"
#define BG_LIGHT "GameOfLife.app/Contents/Resources/img/bglight.png"
#define BG_DARK "GameOfLife.app/Contents/Resources/img/bgdark.png"
#elif __MINGW32__
#define IMG_CELL "pink.png"
#define BG_LIGHT "bglight.png"
#define BG_DARK "bgdark.png"
#else
#define IMG_CELL "assets/img/pink.png"
#define BG_LIGHT "assets/img/bglight.png"
#define BG_DARK "assets/img/bgdark.png"
#endif

bool running = true;
int grid[BOARD_SIZE / CELL_SIZE][BOARD_SIZE / CELL_SIZE];
int bufferGrid[BOARD_SIZE / CELL_SIZE][BOARD_SIZE / CELL_SIZE];
int generation = 0;
int framecount = 0;

bool init();
void quit();

bool loadAssets();

void spawn();

void handleEvents();
void update();
void draw(bool textures = false);

void mainloop(); // main loop wrapper for Emscripten

int checkNeighbors( int x, int y );
void drawCell( Sint16 x, Sint16 y, Uint16 w, Uint16 h, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
SDL_Texture* loadTexture(std::string path);

// SDL Objects
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *cell = NULL;
SDL_Texture *bg_light = NULL;
SDL_Texture *bg_dark = NULL;
SDL_Event event;
Timer fpsTimer;
Timer frameticks;

bool init()
{
  bool initialized = true;
  if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
  {
    printf( "SDL failed to initialize! SDL_Error: %s\n", SDL_GetError() );
    initialized = false;
  }
  else
  {
    if ( !SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "1" ) )
    {
      printf( "Warning: Linear texture filtering not enabled!" );
    }
    
    SDL_CreateWindowAndRenderer( BOARD_SIZE, BOARD_SIZE, SDL_WINDOW_RESIZABLE | SDL_RENDERER_ACCELERATED, &window, &renderer);
    SDL_SetWindowTitle( window, "Conway's Game of Life by Fielding");
    
    if ( window == NULL )
    {
      printf( "SDL window could not be created! SDL_Error: %s\n", SDL_GetError() );
      initialized = false;
    }
    else
    {
      if ( renderer == NULL )
      {
        printf( "SDL renderer could not be created! SDL_Error: %s\n", SDL_GetError() );
        initialized = false;
      }
      else
      {
        SDL_SetRenderDrawColor( renderer, 0xFF, 0xFF, 0xFF, 0xFF );
        
        int imgFlags = IMG_INIT_PNG;
        if( !( IMG_Init( imgFlags ) & imgFlags ) )
        {
          printf( "SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError() );
          initialized = false;
        }
      }
    }
  }
  return initialized;
}

void quit()
{
  SDL_DestroyTexture( cell );
  SDL_DestroyTexture( bg_light );
  SDL_DestroyTexture( bg_dark );
  
  SDL_DestroyRenderer( renderer );
  SDL_DestroyWindow( window );
  renderer = NULL;
  window = NULL;
  
  IMG_Quit();
  SDL_Quit();
}

bool loadAssets()
{
  bool loaded = true;
  
  cell = loadTexture( IMG_CELL );
  if ( cell == NULL )
  {
    printf( "Failed to load %s\n", IMG_CELL );
    loaded = false;
  }
  
  bg_light = loadTexture( BG_LIGHT );
  if ( bg_light == NULL )
  {
    printf( "Failed to load %s\n", BG_LIGHT );
    loaded = false;
  }
  
  bg_dark = loadTexture( BG_DARK);
  if ( bg_dark == NULL )
  {
    printf( "Failed to load %s\n", BG_DARK );
    loaded = false;
  }
  return loaded;
}

void spawn()
{
  // fill the array with random cells
  int rando = 0;
  //#ifndef __MINGW32__
  srand(( unsigned )time( NULL ));
  //#else
  //  srand( ( unsigned ) GetSystemTime() );
  //#endif
  
  for ( int x = 0; x < BOARD_SIZE / CELL_SIZE; x++ )
  {
    for ( int y = 0; y < BOARD_SIZE / CELL_SIZE; y++ )
    {
      rando = rand() % 2;
      grid[x][y] = rando;
      if ( DEBUGINFO ){ cout<<"Setting square at "<<x<<","<<y<<" to "<<grid[x][y]<<"\n"; }
    }
  }
}

void mainloop()
{
  float avgFPS = framecount / ( fpsTimer.getTicks() / 1000.f );
  if ( avgFPS > 2000000)
  {
    avgFPS = 0;
  }
  
  printf( "Average Frames Per Second %f\n", avgFPS);
  
    handleEvents();
    update();
    draw(TEXTURES);
}

void handleEvents()
{
    while ( SDL_PollEvent( &event ) )
    {
      if ( event.type == SDL_KEYDOWN )   // If a Key was pressed
      {
        switch ( event.key.keysym.sym )
        {
          case SDLK_ESCAPE:   // If key pressed was escape
            running = false;      // exit the program
            break;
          
          case SDLK_UP:       // If key pressed was up arrow
            break;
          
          case SDLK_DOWN:     // If key pressed was down arrow
            break;
          
          case SDLK_f:
            //  toggleFPS();
            break;
          default:            // default for undefined keys
            break;
        }
      } else if ( event.type == SDL_QUIT )
      {
        running = false;    // exit the program
      }
    }
  
}

void update()
{
    // Rules:
    // 1. Any live cell with fewer than two live neighbours dies, as if caused by under-population.
    // 2. Any live cell with two or three live neighbours lives on to the next generation.
    // 3. Any live cell with more than three live neighbours dies, as if by overcrowding.
    // 4. Any dead cell with exactly three live neighbours becomes a live cell, as if by reproduction
  
    for ( int x = 0; x < BOARD_SIZE / CELL_SIZE; x++ )
    {
        for ( int y = 0; y < BOARD_SIZE / CELL_SIZE; y++ )
        {
            int neighbors = checkNeighbors( x, y );
            
            if ( neighbors < 2 && grid[x][y] == 1 )
            {
                if( DEBUGINFO ){ cout<<x<<","<<y<<" has died from under-population!\n"; }
                bufferGrid[x][y] = 0;
            } else if ( (neighbors == 2 || neighbors == 3) && grid[x][y] == 1)
            {
                if( DEBUGINFO ){ cout<<x<<","<<y<<" has a healthy amount of neighbors and lives on!\n"; }
                bufferGrid[x][y] = 1;
            } else if ( neighbors > 3 && grid[x][y] == 1)
            {
                if( DEBUGINFO ){ cout<<x<<","<<y<<" has died from overcrowding!\n"; }
                bufferGrid[x][y] = 0;
                
            } else if ( neighbors == 3 && grid[x][y] == 0)
            {
                if( DEBUGINFO ){ cout<<"A new cell has been born at "<<x<<","<<y<<"!\n"; }
                bufferGrid[x][y] = 1;
            }
        }
    }
    
    for ( int x = 0; x < BOARD_SIZE / CELL_SIZE; x++ )
    {
        for ( int y = 0; y < BOARD_SIZE / CELL_SIZE; y++ )
        {
            grid[x][y] = bufferGrid[x][y];
        }
    }
  generation++;
}

void draw(bool textures)
{
  SDL_SetRenderDrawColor( renderer, 0x00, 0x00, 0x00, 0x00 );
  SDL_RenderClear( renderer );
  
  if (textures)
  {
  
    for ( int x = 0; x < BOARD_SIZE / CELL_SIZE; x++ )
    {
      for ( int y = 0; y < BOARD_SIZE / CELL_SIZE; y++ )
      {
        SDL_Rect rect = {x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
        if ( grid[x][y] == 1)
        {
          SDL_RenderCopy(renderer, cell, NULL, &rect);
        }
        else
        {
        if ( y % 2 != 0 && x % 2 == 0 )
          SDL_RenderCopy(renderer, bg_dark, NULL, &rect);
        else
          SDL_RenderCopy(renderer, bg_light, NULL, &rect);
        }
      }
    }
  }
  else
  {

    
    for ( int x = 0; x < BOARD_SIZE / CELL_SIZE; x++ )
    {
      for ( int y = 0; y < BOARD_SIZE / CELL_SIZE; y++ )
      {
        if ( grid[x][y] == 1 )
        {
          drawCell(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, 0xFF, 0x00, 0x00, 0xFF);
        }
      }
    }
  }

  SDL_RenderPresent( renderer );
  framecount++;
}

int checkNeighbors( int x, int y )
{
    int ncount = 0;
    if ( DEBUGINFO ){ cout<<"Checking neighbor cells for " << x << "," << y << ".\n"; }
    for ( int a = x - 1; a <= x + 1; a++ )
    {
        for ( int b = y - 1; b <= y + 1; b++)
        {
            if ( grid[a][b] == 1 && a >= 0 && a <= ( BOARD_SIZE / CELL_SIZE - 1 ) && b >= 0 && b <= ( BOARD_SIZE / CELL_SIZE - 1 ) )
            {
                ncount++;
            }
        }
    }
    
    if ( grid[x][y] ) { ncount--; } // subtract self from neighbor count if currently living
    if ( DEBUGINFO ){ cout<<x<<","<<y<<" has "<< ncount << " current neighbor cells.\n"; }
    
    return ncount;
}

void drawCell( Sint16 x, Sint16 y, Uint16 w, Uint16 h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
  
  SDL_Rect rect = {x, y, w, h};
  SDL_SetRenderDrawColor( renderer, r, g, b, a );
  SDL_RenderFillRect( renderer, &rect );

}

SDL_Texture* loadTexture( std::string path )
{
  //The final texture
  SDL_Texture* newTexture = NULL;
  
  //Load image at specified path
  SDL_Surface* loadedSurface = IMG_Load( path.c_str() );
  if( loadedSurface == NULL )
  {
    printf( "Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError() );
  }
  else
  {
    //Create texture from surface pixels
    newTexture = SDL_CreateTextureFromSurface( renderer, loadedSurface );
    if( newTexture == NULL )
    {
      printf( "Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError() );
    }
    
    //Get rid of old loaded surface
    SDL_FreeSurface( loadedSurface );
  }
  
  return newTexture;
}

int main ( int argc, char **argv )
{
  if ( !init() )
  {
    printf( "Failed to initialize!\n" );
  }
  else
  {
    if ( !loadAssets() )
    {
      printf ( "Failed to load assets!\n" );
    }
    else
    {
      spawn();;
      std::stringstream fpsText;
      fpsTimer.start();
      framecount = 0;
  
#ifdef EMSCRIPTEN
      emscripten_set_main_loop(mainloop, 10, 1);;
#else
      while ( running )
      {
        frameticks.start();
        mainloop();
        int ticks = frameticks.getTicks();
        if ( ticks < SCREEN_TICKS_PER_FRAME )
        {
          SDL_Delay( SCREEN_TICKS_PER_FRAME - ticks );
        }
      }
#endif
    }
  }
  quit();
  return 0;
}