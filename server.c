#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <semaphore.h>
#include <unistd.h>
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

typedef struct Player {
    int playerNumber;
    int x;
    int y;
    struct Player* next;
} Player;
int numPlayers = 0;
int nextPlayerNumber = 2;

#define TILE_GRASS 0
#define TILE_TOMATO 1

Player* firstPlayer;
int grid[GRIDSIZE][GRIDSIZE];
int score = 0;
int level = 1;
int numTomatoes = 0;

// get a random value in the range [0, 1]
double rand01() {
    return (double) rand() / (double) RAND_MAX;
}

void initGrid() {
    for ( int i = 0; i < GRIDSIZE; i++ ) {
        for ( int j = 0; j < GRIDSIZE; j++ ) {
            double r = rand01();
            if ( r < 0.1 ) {
                grid[i][j] = TILE_TOMATO;
                numTomatoes++;
            }
            else {
                grid[i][j] = TILE_GRASS;
            }
        }
    }

    if ( numTomatoes == 0 ) {
        initGrid();
    }
}

sem_t modifyGrid;   // appleis for addPlayer, removePlayer, and movePlayer

Player* getPlayerByPlayerNumber( int playerNumber ) {
    Player* player = firstPlayer;
    while ( player ) {
        if ( player->playerNumber == playerNumber ) {
            return player;
        }
    }
    return NULL;
}

char* gridToString() {
    int* output = calloc( 4, GRIDSIZE * GRIDSIZE + 2 );
    for ( int i = 0; i < GRIDSIZE; i++ ) {
        for ( int j = 0; j < GRIDSIZE; j++ ) {
            output[i * GRIDSIZE + j] = grid[i][j];
        }
    }
    
    output[GRIDSIZE * GRIDSIZE] = level;
    output[GRIDSIZE * GRIDSIZE] = score;
    
    return (char*)output;
}

#define MAX_CONNECTIONS 8
int isConnectionActive[MAX_CONNECTIONS];
int connfds[MAX_CONNECTIONS];
void notifyPlayersOfUpdate() {
    char* gridString = gridToString();
    for ( int i = 0; i < 8; i++ ) {
        if ( isConnectionActive[i] ) {
            send( connfds[i], gridString, ( GRIDSIZE * GRIDSIZE + 2 ) * 4, 0 );
        }
    }
    free( gridString );
}

void removePlayer( int playerNumber ) {
    sem_wait( &modifyGrid );

    Player* toRemove;
    Player* player = firstPlayer;
    Player* prevPlayer = NULL;
    while ( player ) {
        if ( player->playerNumber == playerNumber ) {
            toRemove = player;
            break;
        }
        prevPlayer = player;
        player = player->next;
    }

    if ( toRemove ) {
        if ( prevPlayer ) {
            prevPlayer->next = toRemove->next;
        }
        
        grid[toRemove->x][toRemove->y] = TILE_GRASS;
        
        free( toRemove );
        
        notifyPlayersOfUpdate();
    }
    
    sem_post( &modifyGrid );
}

void levelUp() {
    for ( int x = 0; x < GRIDSIZE; x++ ) {
        for ( int y = 0; y < GRIDSIZE; y++ ) {
            double r = rand01();
            if ( r < 0.1 && grid[x][y] == TILE_GRASS ) {
                grid[x][y] = TILE_TOMATO;
                numTomatoes++;
            }
        }
    }

    if ( numTomatoes == 0 ) {
        levelUp();
    }
}


#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3
void movePlayer( int playerNumber, int direction ) {
    sem_wait( &modifyGrid );
    
    Player* playerToMove = getPlayerByPlayerNumber( playerNumber );
    int success = 0;
    if ( playerToMove ) {
        int playerPreviousX = playerToMove->x;
        int playerPreviousY = playerToMove->y;
        if ( direction == UP && playerToMove->y > 0 && grid[playerToMove->x][playerToMove->y - 1] < 2 ) {
            playerToMove->y--;
            success = 1;
            printf( "Player moved up\n" );
        }
        else if ( direction == RIGHT && playerToMove->x < GRIDSIZE - 1 && grid[playerToMove->x + 1][playerToMove->y] < 2 ) {
            playerToMove->x++;
            success = 1;
            printf( "Player moved right\n" );
        }
        else if ( direction == DOWN && playerToMove->y < GRIDSIZE - 1 && grid[playerToMove->x][playerToMove->y + 1] < 2 ) {
            playerToMove->y++;
            success = 1;
            printf( "Player moved down\n" );
        }
        else if ( direction == LEFT && playerToMove->x > 0 && grid[playerToMove->x - 1][playerToMove->y] < 2 ) {
            playerToMove->x--;
            success = 1;
            printf( "Player moved left\n" );
        }
        
        if ( success ) {
            if ( grid[playerToMove->x][playerToMove->y] == TILE_TOMATO ) {
                score++;
                numTomatoes--;
            }
            grid[playerToMove->x][playerToMove->y] = playerToMove->playerNumber;
            grid[playerPreviousX][playerPreviousY] = TILE_GRASS;
            
            if ( numTomatoes == 0 ) {
                levelUp();
            }

            // tell clients to update display
            notifyPlayersOfUpdate();
        }
    }
    
    sem_post( &modifyGrid );
}

int listenfd;
pthread_t playerThreads[MAX_CONNECTIONS];
void* playerThread( void* index ) {
    while ( 1 ) {
        // accept connection
        listen( listenfd, 1000 );
        struct sockaddr clientaddr;
        socklen_t clientlen = sizeof( struct sockaddr );
        int connfd = accept( listenfd, &clientaddr, &clientlen );
        isConnectionActive[(long)index] = 1;
        printf( "Successfully accepted connection from client\n" );
        connfds[(long)index] = connfd;
        
        char buf[10];
        read( connfd, buf, 10 );
        
        sem_wait( &modifyGrid );
        
        // add new player
        int x = 0, y = 0;
        for ( int i = 0; i < GRIDSIZE * GRIDSIZE; i++ ) {
            if ( grid[i / GRIDSIZE][i % GRIDSIZE] == TILE_GRASS ) {
                x = i / GRIDSIZE;
                y = i % GRIDSIZE;
                break;
            }
        }
        
        int playerNumber;
        if ( !firstPlayer ) {
            // If there is no firstPlayer, create firstPlayer
            firstPlayer = malloc( sizeof( firstPlayer ) );
            firstPlayer->playerNumber = nextPlayerNumber;
            playerNumber = nextPlayerNumber;
            nextPlayerNumber++;
            firstPlayer->x = x;
            firstPlayer->y = y;
            firstPlayer->next = NULL;
        }
        else {
            // Otherwise, add new player to beginning of the list
            Player* newPlayer = malloc( sizeof( Player ) );
            newPlayer->playerNumber = nextPlayerNumber;
            playerNumber = nextPlayerNumber;
            nextPlayerNumber++;
            newPlayer->x = x;
            newPlayer->y = y;
            newPlayer->next = firstPlayer;
            firstPlayer = newPlayer;
        }
        grid[x][y] = nextPlayerNumber;
        
        notifyPlayersOfUpdate();
        
        sem_post( &modifyGrid );
        while ( 1 ) {
            // receive input from client
            int n = recv( connfd, buf, 5, 0 );
            if ( n == 0 ) {
                break;
            }
            
            int direction = atoi( buf );
            movePlayer( playerNumber, direction );
        }
        printf( "Connection dropped\n" );
        
        isConnectionActive[(long)index] = 0;
        removePlayer( playerNumber );
        close( connfd );
        connfds[(long)index] = -1;
    }
    return NULL;
}

typedef struct addrinfo addrinfo;

int main( int argc, char** argv ) {
    if ( argc == 1 ) {
        printf( "Please specify a port number\n" );
        exit( 0 );
    }
    
    sem_init( &modifyGrid, 0, 1 );
    initGrid();
    
    addrinfo hints;
    memset( &hints, 0, sizeof( addrinfo ) );
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_flags |= AI_NUMERICSERV;
    
    // setup server
    addrinfo* addrinfo;
    getaddrinfo( NULL, argv[1], &hints, &addrinfo );
    struct addrinfo* ptr;
    for ( ptr = addrinfo; ptr != NULL; ptr = ptr->ai_next ) {
        listenfd = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
        if ( listenfd < 0 ) {
            continue;
        }
        
        if ( bind( listenfd, ptr->ai_addr, ptr->ai_addrlen ) == 0 ) {
            break;
        }
        close( listenfd );
    }
    freeaddrinfo( addrinfo );
    if ( listenfd < 0 ) {
        printf( "server setup failed\n" );
        exit( 0 );
    }
    
    for ( long i = 0; i < 8; i++ ) {
        connfds[i] = -1;
        pthread_create( &playerThreads[i], NULL, playerThread, (void*) i );
        pthread_join( playerThreads[i], NULL );
    }
}