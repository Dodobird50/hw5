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
    int x;
    int y;
    Player* prev;
    Player* next;
} Player;
int numPlayers = 0;

typedef enum {
    TILE_GRASS,
    TILE_TOMATO
} TILETYPE;

Player* firstPlayer;
TILETYPE grid[GRIDSIZE][GRIDSIZE];

// get a random value in the range [0, 1]
double rand01() {
    return (double) rand() / (double) RAND_MAX;
}

void initGrid() {
    for (int i = 0; i < GRIDSIZE; i++) {
        for (int j = 0; j < GRIDSIZE; j++) {
            double r = rand01();
            if (r < 0.1) {
                grid[i][j] = TILE_TOMATO;
                numTomatoes++;
            }
            else
                grid[i][j] = TILE_GRASS;
        }
    }

    // force player's position to be grass
    // if (grid[Player.x][Player.y] == TILE_TOMATO) {
    //     grid[Player.x][Player.y] = TILE_GRASS;
    //     numTomatoes--;
    // }

    // ensure grid isn't empty
    while (numTomatoes == 0)
        initGrid();
}

sem_t modifyGrid;   // appleis for addPlayer, removePlayer, and movePlayer

Player* getPlayer( int x, int y ) {
    Player* player = firstPlayer;
    while ( player ) {
        if ( player->x == x && player->y == y ) {
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
    if ( !firstPlayer ) {
        // If there is no firstPlayer, create firstPlayer
        firstPlayer = malloc( sizeof( firstPlayer ) );
        firstPlayer->x = x;
        firstPlayer->y = y;
        firstPlayer->next = NULL;
        firstPlayer->prev = NULL;
    }
    else {
        // Otherwise, add new player to beginning of the list
        Player* newPlayer = malloc( sizeof( Player ) );
        newPlayer->x = x;
        newPlayer->y = y;
        newPlayer->prev = NULL;
        newPlayer->next = firstPlayer;
        firstPlayer = newPlayer;
    }
    
    // TODO: tell clients to update display
    notifyPlayersOfUpdate();
    sem_post( modifyGrid );
}

void removePlayer( int x, int y ) {
    if ( players == 0 ) {
        return;
    }
    
    sem_wait( modifyGrid );

    Player* toRemove;
    Player* player = firstPlayer;
    while ( player ){
        if ( player->x == x && player->y == y){
          toRemove = player;
        }
    }

    Player* nextPlayer = toRemove->next;
    nextPlayer->prev = toRemove->prev;

    Player* prevPlayer = toRemove->prev;
    prevPlayer->next = toRemove->next;

    free( toRemove );
    
    // TODO: tell clients to update display
    notifyPlayersOfUpdate();
    sem_post( modifyGrid );
}

// direction == 0 -> up
// direction == 1 -> right
// direction == 2 -> down
// direction == 3 -> left
void movePlayer( int x, int y, int direction ) {
    sem_wait( modifyGrid );
    
    // getPlayer without sem_wait( modifyGrid ) and sem_post( modifyGrid )
    Player* playerToMove = firstPlayer;
    while ( playerToMove ) {
        if ( playerToMove->x == x && playerToMove->y == y ) {
            break;
        }
    }
    
    int success = 0;
    if ( playerToMove ) {
        if ( direction == 0 && player->y > 0 && !getPlayer( x, y - 1 ) ) {
            player->y--;
            success = 1;
        }
        else if ( direction == 1 && player->x < GRIDSIZE - 1 && !getPlayer( x + 1, y ) ) {
            player->x++;
            success = 1;
        }
        else if ( direction == 2 && player->y < GRIDSIZE - 1 && !getPlayer( x, y - 1 ) ) {
            player->y--;
            success = 1;
        }
        else if ( direction == 3 && player->x > 0 && !getPlayer( x, y - 1 ) ) {
            player->x--;
            success = 1;
        }
        
        if ( success && grid[player->x][player->y] == TILE_TOMATO ) {
            grid[player->x][player->y] = TILE_GRASS;
        }
    }
    
    // TODO: tell clients to update display
    notifyPlayersOfUpdate();
    
    sem_post( modifyGrid );
}

char* gridToString() {
    char output[GRIDSIZE * GRIDSIZE];
    for ( int i = 0; i < GRIDSIZE; i++ ) {
        for ( int j = 0; j < GRIDSIZE; j++ ) {
            if ( grid[i][j] == TILE_GRASS ) {
                output[i * GRIDSIZE + j] = '0';
            }
            else {
                output[i * GRIDSIZE + j] = '1';
            }
        }
    }
    
    Player* player = firstPlayer;
    while ( player ) {
        output[player->x * GRIDSIZE + player->y] = '2';
        player = player->next;
    }
    
    return output;
}

void notifyPlayersOfUpdate() {
    for ( int i = 0; i < 8; i++ ) {
        if ( isActive[i] ) {
            rio_writen( connfds[i], gridToString(), )
        }
    }
}

void* playerThread( void* index ) {
    while ( 1 ) {
        // accept connection
        listen( listenfd, 0 );
        struct sockaddr_storage clientaddr;
        int clientlen = sizeof( struct sockaddr_storage );
        int connfd = accept( listenfd, &clientaddr, &clientlen );
        connfds[(int)i] = connfd;
        
        rio_t rio;
        char buf[MAXLINE];
        rio_readinitb( &rio, connfd );
        rio_readlineb( &rio, buf, MAXLINE );
        
        // add new player
        int x = 0; y = 0;
        for ( int i = 0; i < GRIDSIZE * GRIDSIZE; i++ ) {
            if ( grid[i / GRIDSIZE][i % GRIDSIZE] == TILE_GRASS ) {
                x = i / GRIDSIZE;
                y = i % GRIDSIZE;
                break;
            }
        }
        addPlayer( x, y );
        isActive[(int)i] = 1;
        int direction = 0;
        while ( 1 ) {
            // receive input from client
            int n = rio_readlineb( &rio, buf, MAXLINE );
            if ( n == 0 ) {
                break;
            }
            x = atoi( buf );
            rio_readlineb( &rio, buf, MAXLINE );
            y = atoi( buf );
            rio_readlineb( &rio, buf, MAXLINE );
            direction = atoi( buf );
            
            movePlayer( x, y, direction );
        }
        
        isActive[(int)i] = 0;
        removePlayer( x, y );
        close( connfd );
        connfds[i] = -1;
    }
    return NULL;
}

typedef struct addrinfo addrinfo;

int listenfd;
pthread_t playerThreads[8];
int isActive[8];
int connfds[8];
int main() {
    sem_init( &modifyGrid, 0, 1 );
    initGrid();
    
    // setup server
    addrinfo* addrinfo;
    getaddrinfo( "localhost", "49494", NULL, &addrinfo );
    addrinfo* ptr;
    while ( ptr = addrinfo; ptr != NULL; ptr = addrinfo->ai_next ) {
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