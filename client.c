#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <netdb.h>
#include <netinet/in.h>

// Dimensions for the drawn grid (should be GRIDSIZE * texture dimensions)
#define GRID_DRAW_WIDTH 640
#define GRID_DRAW_HEIGHT 640

#define WINDOW_WIDTH GRID_DRAW_WIDTH
#define WINDOW_HEIGHT (HEADER_HEIGHT + GRID_DRAW_HEIGHT)

// Header displays current score
#define HEADER_HEIGHT 50

// Number of cells vertically/horizontally in the grid
#define GRIDSIZE 10
#define TILE_GRASS 0
#define TILE_TOMATO 1

bool shouldExit = false;

TTF_Font* font;

void initSDL() {
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "Error initializing SDL: %s\n", SDL_GetError() );
        exit( EXIT_FAILURE );
    }

    int rv = IMG_Init( IMG_INIT_PNG );
    if ( ( rv & IMG_INIT_PNG ) != IMG_INIT_PNG ) {
        fprintf( stderr, "Error initializing IMG: %s\n", IMG_GetError() );
        exit(EXIT_FAILURE);
    }

    if ( TTF_Init() == -1 ) {
        fprintf( stderr, "Error initializing TTF: %s\n", TTF_GetError() );
        exit( EXIT_FAILURE );
    }
}


int clientfd;
// TODO
void handleKeyDown( SDL_KeyboardEvent* event )
{
    // // ignore repeat events if key is held down
    // if ( event->repeat ) {
    //     return;
    // }

    if ( event->keysym.scancode == SDL_SCANCODE_Q || event->keysym.scancode == SDL_SCANCODE_ESCAPE ) {
        shouldExit = true;
    }

    if ( event->keysym.scancode == SDL_SCANCODE_UP || event->keysym.scancode == SDL_SCANCODE_W ) {
        send( clientfd, "0", 2, 0 );
        // printf( "Player tried to move up\n" );
    }
    if ( event->keysym.scancode == SDL_SCANCODE_RIGHT || event->keysym.scancode == SDL_SCANCODE_D ) {
        send( clientfd, "1", 2, 0 );
        // printf( "Player tried to move right\n" );
    }
    if ( event->keysym.scancode == SDL_SCANCODE_DOWN || event->keysym.scancode == SDL_SCANCODE_S ) {
        send( clientfd, "2", 2, 0 );
        // printf( "Player tried to move down\n" );
    }
    if ( event->keysym.scancode == SDL_SCANCODE_LEFT || event->keysym.scancode == SDL_SCANCODE_A ) {
        send( clientfd, "3", 2, 0 );
        // printf( "Player tried to move left\n" );
    }        
}

void processInputs() {
	SDL_Event event;

	while ( SDL_PollEvent( &event ) ) {
		switch ( event.type ) {
			case SDL_QUIT: {
				shouldExit = true;
				break;
            }
            case SDL_KEYDOWN: {
                handleKeyDown( &event.key );
				break;
            }
			default: {
				break;
            }
		}
	}
}

// TODO
void drawGrid( SDL_Renderer* renderer, SDL_Texture* grassTexture, SDL_Texture* tomatoTexture, SDL_Texture* playerTexture, int* data ) {
    SDL_Rect dest;
    
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            dest.x = 64 * i;
            dest.y = 64 * j + HEADER_HEIGHT;
            
            SDL_Texture* texture;
            if ( data[i * GRIDSIZE + j] == TILE_GRASS ) {
                texture = grassTexture;
            }
            else if ( data[i * GRIDSIZE + j] == TILE_TOMATO ) {
                texture = tomatoTexture;
            }
            else {
                texture = playerTexture;
            }
            SDL_QueryTexture( texture, NULL, NULL, &dest.w, &dest.h );
            SDL_RenderCopy( renderer, texture, NULL, &dest );
        }
    }
}

void drawUI(SDL_Renderer* renderer, int* data ) {
    // largest score/level supported is 2147483647
    char scoreStr[18];
    char levelStr[18];
    int score = data[GRIDSIZE * GRIDSIZE + 1];
    int level = data[GRIDSIZE * GRIDSIZE];
    sprintf(scoreStr, "Score: %d", score);
    sprintf(levelStr, "Level: %d", level);

    SDL_Color white = {255, 255, 255};
    SDL_Surface* scoreSurface = TTF_RenderText_Solid(font, scoreStr, white);
    SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);

    SDL_Surface* levelSurface = TTF_RenderText_Solid(font, levelStr, white);
    SDL_Texture* levelTexture = SDL_CreateTextureFromSurface(renderer, levelSurface);

    SDL_Rect scoreDest;
    TTF_SizeText(font, scoreStr, &scoreDest.w, &scoreDest.h);
    scoreDest.x = 0;
    scoreDest.y = 0;

    SDL_Rect levelDest;
    TTF_SizeText(font, levelStr, &levelDest.w, &levelDest.h);
    levelDest.x = GRID_DRAW_WIDTH - levelDest.w;
    levelDest.y = 0;

    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreDest);
    SDL_RenderCopy(renderer, levelTexture, NULL, &levelDest);

    SDL_FreeSurface(scoreSurface);
    SDL_DestroyTexture(scoreTexture);

    SDL_FreeSurface(levelSurface);
    SDL_DestroyTexture(levelTexture);
}

void* getPlayerInput( void* i ) {
    while( 1 ) {
        processInputs();
    }
}

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture *grassTexture;
SDL_Texture *tomatoTexture;
SDL_Texture *playerTexture;

void* getUpdatesFromServer( void* i ) {
    char buf[( GRIDSIZE * GRIDSIZE + 2 ) * 4];
    while ( 1 ) {
        SDL_SetRenderDrawColor( renderer, 0, 105, 6, 255 );
        SDL_RenderClear( renderer );

        recv( clientfd, buf, ( GRIDSIZE * GRIDSIZE + 2 ) * 4, 0 );
        
        if ( strcmp( buf, "Unable to accept connection from client." ) == 0 ) {
            printf( "Sorry! Current room is full.\n" );
            exit( 0 );
        }
        
        int* data = (int*) buf;
        drawGrid( renderer, grassTexture, tomatoTexture, playerTexture, data );
        drawUI( renderer, data );

        SDL_RenderPresent( renderer );
        SDL_Delay( 16 ); // 16 ms delay to limit display to 60 fps
    }
}

int connected = 0;
void* waitConnected( void* i ) {
    sleep( 5 );
    
    if ( !connected ) {
        printf( "Unable to connect to server\n" );
        exit( 0 );
    }
    
    return NULL;
}


typedef struct addrinfo addrinfo;
int main( int argc, char* argv[] ) {
    if ( argc == 1 ) {
        printf( "Please specify a port number\n" );
        exit( 0 );
    }
    
    srand(time(NULL));

    initSDL();

    font = TTF_OpenFont( "resources/Burbank-Big-Condensed-Bold-Font.otf", HEADER_HEIGHT );
    if ( font == NULL ) {
        fprintf( stderr, "Error loading font: %s\n", TTF_GetError() );
        exit( EXIT_FAILURE );
    }

    window = SDL_CreateWindow( "Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0 );

    if ( window == NULL ) {
        fprintf( stderr, "Error creating app window: %s\n", SDL_GetError() );
        exit( EXIT_FAILURE );
    }

    renderer = SDL_CreateRenderer( window, -1, 0 );

	if ( renderer == NULL ) {
		fprintf( stderr, "Error creating renderer: %s\n", SDL_GetError() );
        exit( EXIT_FAILURE );
	}

    grassTexture = IMG_LoadTexture( renderer, "resources/grass.png" );
    tomatoTexture = IMG_LoadTexture( renderer, "resources/tomato.png" );
    playerTexture = IMG_LoadTexture( renderer, "resources/player.png" );

    addrinfo hints;
    memset( &hints, 0, sizeof( addrinfo ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    pthread_t waitConnectedThread;
    pthread_create( &waitConnectedThread, NULL, waitConnected, NULL );

    addrinfo* addrinfo;
    getaddrinfo( "localhost", argv[1], &hints, &addrinfo );
    
    for ( struct addrinfo* ptr = addrinfo; ptr != NULL; ptr = ptr->ai_next ) {
        clientfd = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
        if ( clientfd < 0 ) {
            continue;
        }
        
        if ( connect( clientfd, ptr->ai_addr, ptr->ai_addrlen ) != -1 ) {
            break;
        }
        
        close( clientfd );
    }

    freeaddrinfo( addrinfo );
    if ( clientfd == -1 ) {
        printf( "Unable to connect to server\n" );
        exit( 0 );
    }

    connected = 1;
    
    pthread_t getUpdatesFromServerThread;
    pthread_create( &getUpdatesFromServerThread, NULL, getUpdatesFromServer, NULL );
    
    while ( !shouldExit ) {
        processInputs();
    }

    // clean up everything
    SDL_DestroyTexture( grassTexture );
    SDL_DestroyTexture( tomatoTexture );
    SDL_DestroyTexture( playerTexture );

    TTF_CloseFont( font );
    TTF_Quit();

    IMG_Quit();

    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    SDL_Quit();
}
