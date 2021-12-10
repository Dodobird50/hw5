#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include <sys/socket.h>
#include <netdb.h>

// Dimensions for the drawn grid (should be GRIDSIZE * texture dimensions)
#define GRID_DRAW_WIDTH 640
#define GRID_DRAW_HEIGHT 640

#define WINDOW_WIDTH GRID_DRAW_WIDTH
#define WINDOW_HEIGHT (HEADER_HEIGHT + GRID_DRAW_HEIGHT)

// Header displays current score
#define HEADER_HEIGHT 50

// Number of cells vertically/horizontally in the grid
#define GRIDSIZE 10

typedef struct {
    int playerNumber;
    int x;
    int y;
    Player* next;
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

Player* getPlayerByPlayerNumber( Player* playerNumber ) {
    Player* player = firstPlayer;
    while ( player ) {
        if ( player->playerNumber == playerNumber ) {
            return player;
        }
    }
    return NULL;
}

Player* addPlayer( int x, int y ) {
    if ( players == 8 ) {
        return;
    }
    
    sem_wait( modifyGrid );
    
    if ( grid[x][y] != TILE_GRASS ) {
        sem_post( modifyGrid );
        return NULL;
    }
    
    if ( !firstPlayer ) {
        // If there is no firstPlayer, create firstPlayer
        firstPlayer = malloc( sizeof( firstPlayer ) );
        firstPlayer = nextPlayerNumber;
        nextPlayerNumber++;
        firstPlayer->x = x;
        firstPlayer->y = y;
        firstPlayer->next = NULL;
    }
    else {
        // Otherwise, add new player to beginning of the list
        Player* newPlayer = malloc( sizeof( Player ) );
        newPlayer = nextPlayerNumber;
        nextPlayerNumber++;
        newPlayer->x = x;
        newPlayer->y = y;
        newPlayer->next = firstPlayer;
        firstPlayer = newPlayer;
    }
    grid[x][y] = nextPlayerNumber;
    
    notifyPlayersOfUpdate();
    
    sem_post( modifyGrid );
}

void removePlayer( int playerNumber ) {
    if ( players == 0 ) {
        return;
    }
    
    sem_wait( modifyGrid );

    Player* toRemove;
    Player* player = firstPlayer;
    while ( player ) {
        if ( player->playerNumber == playerNumber ) {
            toRemove = player;
            break;
        }
    }

    if ( toRemove ) {
        Player* nextPlayer = toRemove->next;
        prevPlayer->next = toRemove->next;
        grid[toRemove->x][grid->toRemove->y] = TILE_GRASS;
        
        free( toRemove );
        
        notifyPlayersOfUpdate( 0 );
    }
    
    sem_post( modifyGrid );
}

void levelUp() {
    for ( int x = 0; x < GRIDSIZE; x++ ) {
        for ( int y = 0; y < GRIDSIZE; y++ ) {
            double r = rand01();
            if ( r < 0.1 && grid[x][y] == TILE_GRASS ) {
                grid[x][y] = TILE_TOMATO;
                numTomatoes++;
            }
            else {
                grid[x][y] = TILE_GRASS;
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
    sem_wait( modifyGrid );
    
    Player* playerToMove = getPlayerByPlayerNumber( playerNumber );
    int success = 0;
    if ( playerToMove ) {
        int playerPreviousX = player->x;
        int playerPreviousY = player->y;
        if ( direction == UP && player->y > 0 && grid[x][y - 1] < 2 ) {
            player->y--;
            success = 1;
        }
        else if ( direction == RIGHT && player->x < GRIDSIZE - 1 && grid[x + 1][y] < 2 ) {
            player->x++;
            success = 1;
        }
        else if ( direction == DOWN && player->y < GRIDSIZE - 1 && grid[x][y + 1] < 2 ) {
            player->y--;
            success = 1;
        }
        else if ( direction == LEFT && player->x > 0 && grid[x - 1][y] < 2 ) {
            player->x--;
            success = 1;
        }
        
        if ( success ) {
            if ( grid[player->x][player->y] == TILE_TOMATO ) {
                score++;
                numTomatoes--;
            }
            grid[player->x][player->y] = player->playerNumber;
            grid[playerPreviousX][playerPreviousY] = TILE_GRASS;
            
            if ( numTomatoes == 0 ) {
                levelUp();
            }

            // tell clients to update display
            notifyPlayersOfUpdate( 0 );
        }
    }
    
    sem_post( modifyGrid );
}

char* gridToString() {
    int output[GRIDSIZE * GRIDSIZE + 2];
    for ( int i = 0; i < GRIDSIZE; i++ ) {
        for ( int j = 0; j < GRIDSIZE; j++ ) {
            output[i][j] = grid[i][j];
        }
    }
    
    output[GRIDSIZE * GRIDSIZE] = level;
    output[GRIDSIZE * GRIDSIZE] = score;
    
    return (char*)output;
}

void notifyPlayersOfUpdate() {
    char* gridToString = gridToString();
    for ( int i = 0; i < 8; i++ ) {
        if ( isActive[i] ) {
            send( connfds[i], gridToString(), ( GRIDSIZE * GRIDSIZE + 2 ) * 4 );
        }
    }
}

#define MAX_CONNECTIONS 8
int listenfd;
pthread_t playerThreads[MAX_CONNECTIONS];
int isConnectionActive[MAX_CONNECTIONS];
int connfds[MAX_CONNECTIONS];

void* playerThread( void* index ) {
    while ( 1 ) {
        // accept connection
        listen( listenfd, 0 );
        struct sockaddr_storage clientaddr;
        int clientlen = sizeof( struct sockaddr_storage );
        int connfd = accept( listenfd, &clientaddr, &clientlen );
        connfds[(int)index] = connfd;
        
        char buf[MAXLINE];
        read( connfd, buf, MAXLINE );
        
        sem_wait( modifyGrid );
        
        // add new player
        int x = 0; y = 0;
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
            firstPlayer = nextPlayerNumber;
            playerNumber = nextPlayerNumber;
            nextPlayerNumber++;
            firstPlayer->x = x;
            firstPlayer->y = y;
            firstPlayer->next = NULL;
        }
        else {
            // Otherwise, add new player to beginning of the list
            Player* newPlayer = malloc( sizeof( Player ) );
            newPlayer = nextPlayerNumber;
            playerNumber = nextPlayerNumber;
            nextPlayerNumber++;
            newPlayer->x = x;
            newPlayer->y = y;
            newPlayer->next = firstPlayer;
            firstPlayer = newPlayer;
        }
        grid[x][y] = nextPlayerNumber;
        notifyPlayersOfUpdate();
        
        sem_post( modifyGrid );
        
        isConnectionActive[(int)i] = 1;
        int direction = 0;
        while ( 1 ) {
            // receive input from client
            int n = recv( connfd, buf, MAXLINE );
            if ( n == 0 ) {
                break;
            }
            
            recv( connfd, buf, MAXLINE );
            int direction = atoi( buf );
            movePlayer( playerNumber, direction );
        }
        
        isConnectionActive[(int)index] = 0;
        removePlayer( x, y );
        close( connfd );
        connfds[(int)index] = -1;
    }
    return NULL;
}

typedef struct addrinfo addrinfo;

int main() {
    sem_init( &modifyGrid, 0, 1 );
    initGrid();
    
    // setup server
    addrinfo* addrinfo;
    getaddrinfo( "localhost", "49494", NULL, &addrinfo );
    addrinfo* ptr;
    for ( ptr = addrinfo; ptr != NULL; ptr = addrinfo->ai_next ) {
        listenfd = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol );
        if ( listenfd < 0 ) {
            continue;
        }
        
        if ( bind( listenfd, ptr->ai_addr, p->ai_addrlen ) == 0 ) {
            break;
        }
        close( listenfd );
    }
    freeaddrinfo( addrinfo );
    if ( listenfd < 0 ) {
        printf( "server setup failed\n" );
        exit( 0 );
    }
    
    for ( int i = 0; i < 8; i++ ) {
        connfds[i] = -1;
        pthread_create( &playerThreads[i], NULL, playerThread, (void*) i );
    }
}