//
//  main.c
//  servidor-http
//
//  Created by Carla Santos, Diego Carabajal & Matías Iglesias on 07/06/14.
//
//  Bassed on nweb IBM server
//  http://www.ibm.com/developerworks/systems/library/es-nweb/index.html?ca=dat#
//
// Para que en Mac no pinche
#ifndef SIGCLD
#   define SIGCLD SIGCHLD
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define VERSION     1
#define BUFSIZE  8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404

//Constantes
#define URI_MAX                     PATH_MAX //Definido en limits.h
#define CURRENT_DIRECTORY           "/Users/matiasiglesias/Desktop/raiz"
#define DEFAULT_PORT_NUMBER         8080
#define DEFAULT_STRATEGY            ITERATIVE
#define True                        1
#define False                       0
#define STRATEGY_MAX                20
#define	INTERVAL                    5
#define SIZE                        20


struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };

typedef enum{ITERATIVE, FORKED, THREADS, DAEMON} strategy_t;

//Variables globales
int             port_number =   DEFAULT_PORT_NUMBER;        //Número de Puerto de escucha
static  char    path_root[PATH_MAX] = CURRENT_DIRECTORY;
static  char    strategy_name[STRATEGY_MAX];
static  char    log_file[PATH_MAX];
int             hit;                                        //Contador de hits

struct { /* estructura para estadisticas */
	pthread_mutex_t	est_mutex;
	unsigned int	est_concount; 	/* contador de conexiones activas */
	unsigned int	est_contotal;	/* contador de conexiones completadas */
	unsigned long	est_contime;    /* tiempo de una conexion */
	unsigned long	est_byteincount;  /* contador de bytes de entrada */
    unsigned long	est_byteoutcount;  /* contador de bytes de salida */
} estadisticas;

/* imprime las estadisticas. */
void impestadisticas(void) {
	time_t	ahora;
    
    struct sockaddr_in dest;   /* info del socket acerca de la maquina que se conecta. */
    struct sockaddr_in serv;  /* info del socket del server                           */
    int mysocket, consocket, pid;
    int socksize = sizeof(struct sockaddr_in);
    char str[SIZE],revstr[SIZE];
    int n,i,j=0;



    memset(&serv, 0, sizeof(serv));       /* se popne a cero los campos de la estructura */
    serv.sin_family = AF_INET;           /* seteo a que el tipo de conexion sea TCP/IP  */
    serv.sin_addr.s_addr = INADDR_ANY;  /* seteo que escuche por cualquier interface   */
    serv.sin_port = htons(port_number + 1);    /* seteo el puerto del servidor                */

    mysocket = socket(AF_INET, SOCK_STREAM, 0);
    
    bind(mysocket, (struct sockaddr *)&serv, sizeof(struct sockaddr));
    
    listen(mysocket, 1);

	while (1) {
        //Me quedo escuchando
        
        
		//(void) sleep(INTERVAL);
		consocket = accept(mysocket, (struct sockaddr *)&dest, &socksize);
        n = recv(consocket, str, sizeof(str), 0);

		printf("Conexion desde %s\n", inet_ntoa(dest.sin_addr));

        
		(void) pthread_mutex_lock(&estadisticas.est_mutex);
		ahora = time(0);
		(void) printf("--- %s", ctime(&ahora));
		(void) printf("%-32s: %u\n", "Conexiones concurrentes", estadisticas.est_concount);
		(void) printf("%-32s: %u\n", "Conexiones completadas",	estadisticas.est_contotal);
        
		if (estadisticas.est_contotal) {
			(void) printf("%-32s: %.2f (secs)\n", "Promedio de conexiones completadas",
                          (float)estadisticas.est_contime / (float)estadisticas.est_contotal);
			(void) printf("%-32s: %.2f\n", "Promedio de bytes recibidos",
                          (float)estadisticas.est_byteincount / (float)(estadisticas.est_contotal +	estadisticas.est_concount));
            (void) printf("%-32s: %.2f\n", "Promedio de bytes enviados",
                          (float)estadisticas.est_byteoutcount / (float)(estadisticas.est_contotal +	estadisticas.est_concount));
		}
		(void) printf("%-32s: %lu\n\n", "Total de bytes recibidos", estadisticas.est_byteincount);
        (void) printf("%-32s: %lu\n\n", "Total de bytes enviados", estadisticas.est_byteoutcount);
		(void) pthread_mutex_unlock(&estadisticas.est_mutex);
        
        (void)write(consocket, "HTTP/1.1 200 OK\nContent-Length: 140\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>Estadisticas</title>\n</head><body>\n<h1>Estadisticas</h1>\nLas estadisticas ya se han mostrado por consola.\n</body></html>\n",231);
        
        sleep(1);	/* allow socket to drain before signalling the socket is closed */
        close(consocket);
        
	}
}

void logger(int type, char *s1, char *s2, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];
    
    //Fecha y hora actual
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char ahora[20];
    sprintf(ahora, "%d/%d/%d %d:%d:%d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,  tm.tm_hour, tm.tm_min, tm.tm_sec);
    
	switch (type) {
        case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid());
            break;
        case FORBIDDEN:
            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
            (void)sprintf(logbuffer,"%s: FORBIDDEN: %s:%s",ahora, s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
            (void)sprintf(logbuffer,"%s: NOT FOUND: %s:%s. Path %s",ahora, s1, s2, path_root);
            break;
        case LOG: (void)sprintf(logbuffer,"%s: INFO: %s:%s:%d", ahora, s1, s2, socket_fd); break;
	}
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open(log_file, O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd)
{
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;
	static char buffer[BUFSIZE+1]; /* static so zero filled */
    
    //Estadísticas
	time_t	comienzo;
    
	comienzo = time(0); /* retorna el tiempo actual en formato condensado */
    
	(void) pthread_mutex_lock(&estadisticas.est_mutex); /* solicita acceso al mutex, el hilo se bloquea hasta su obtención. */
	estadisticas.est_concount++;
	(void) pthread_mutex_unlock(&estadisticas.est_mutex);

    
    
	ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
    
    
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request","",fd);
	}
	if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	logger(LOG,"request",buffer,hit);
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
	}
	for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}
	for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
		}
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
		(void)strcpy(buffer,"GET /index.html");
    
	/* work out the file type and check we support it */
	buflen=(int) strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",buffer,fd);
    
	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
		logger(NOTFOUND, "failed to open file",&buffer[5],fd);
	}
	logger(LOG,"SEND",&buffer[5],hit);
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
    (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: servidor-http/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
	logger(LOG,"Header",buffer,hit);
	(void)write(fd,buffer,strlen(buffer));
    
    //Guardo estadística de bytes recibidos y enviados
	(void) pthread_mutex_lock(&estadisticas.est_mutex);
	estadisticas.est_byteincount += ret;
    estadisticas.est_byteoutcount += len;
	(void) pthread_mutex_unlock(&estadisticas.est_mutex);

    
	/* send file in 8KB block - last block may be smaller */
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}
    
    //Guardo estadística de tiempo
	(void) pthread_mutex_lock(&estadisticas.est_mutex);
	estadisticas.est_contime += time(0) - comienzo;
	estadisticas.est_concount--;
	estadisticas.est_contotal++;
	(void) pthread_mutex_unlock(&estadisticas.est_mutex);
    
	//sleep(1);	/* allow socket to drain before signalling the socket is closed */
	//close(fd);
	//exit(1);
}

//Muestra la ayuda
void show_help() {
    int i;

    (void) printf("servidor HTTP \t\tversion %d\n\n", VERSION);
    (void) printf("Modo de uso: servidor_http [-p puerto] [-d directorio] [estrategia]\n");
    (void) printf("Parámtros:\n");
    (void) printf("\t-?\t\t Muestra esta ayuda\n");
    (void) printf("\t-p\t\t Puerto debe ser mayor a 60000 por razones de seguridad\n");
    (void) printf("\t-d\t\t Directorio. NO soportados (por seguridad): directorios / /etc /bin /lib /tmp /usr /dev /sbin \n");
    (void) printf("\testrategia:\n");
    (void) printf("\t\t\t-i\tIterative (Iterativo)\n");
    (void) printf("\t\t\t-t\tThreads (Hilos)\n");
    (void) printf("\t\t\t-f\tForked (Procesos)\n");
    (void) printf("\t\t\t-e\tDaemon (Demonio)\n");

    (void) printf("Parámetros por defecto:\n");
    (void) printf("\tpuerto:\t\t\t %d\n", DEFAULT_PORT_NUMBER);
    (void) printf("\tdirectorio:\t\t %s\n", CURRENT_DIRECTORY);
    (void) printf("\testrategia:\t\t %s\n", "ITERATIVE");
    
    (void) printf("\nExtenciones Soportadas:\n");
    for(i=0;extensions[i].ext != 0;i++)
        (void)printf(" %s",extensions[i].ext);
    
    (void)printf("\n\tNO soportadas: URLs incluyendo \"..\", Java, Javascript, CGI\n"
                 "\tGracias a: tNigel Griffiths nag@uk.ibm.com\n"  );
}

//Chequea los parametros
//devuelve True si estan correctos o False si no lo estan
int check_settings() {

	if( !strncmp(path_root,"/"   ,2 ) || !strncmp(path_root,"/etc", 5 ) ||
       !strncmp(path_root,"/bin",5 ) || !strncmp(path_root,"/lib", 5 ) ||
       !strncmp(path_root,"/tmp",5 ) || !strncmp(path_root,"/usr", 5 ) ||
       !strncmp(path_root,"/dev",5 ) || !strncmp(path_root,"/sbin",6) ){
		(void)printf("ERROR: Directorio raiz %s invalido\n",path_root);
        return False;
	}
	if(chdir(path_root) == -1){
		(void)printf("ERROR: Error al cambiar al directorio %s\n",path_root);
		return False;
	}
    
	if(port_number <= 0 || port_number >60000) {
		(void)printf("Puerto %d invalido (Probar 1->60000)\n",port_number);
        return False;
    }
    return True;
}

strategy_t configure_server(int argc,char *argv[])
{
	int option,option_count=0;
	strategy_t operation = ITERATIVE; //Operacion por defecto
	//s_start(&total_uptime);
    
    //Seteo archivo de log
    strcpy(log_file, (argv[0]));
    strcat(log_file,".log");
	while((option = getopt(argc,argv,"?:p:d:itfe:"))!=-1)
	{
		switch (option)
		{
            case '?':
                show_help();
                exit(0);
			case 'p':
				port_number = atoi(optarg);
				break;
			case 'd':
				strcpy(path_root,optarg);
				break;
			case 'i':
				strcpy(strategy_name,"Iterative");
                operation = ITERATIVE;
				option_count++;
				break;
			case 't':
				strcpy(strategy_name,"Threads");
                operation = THREADS;
				option_count++;
				break;
			case 'f':
				strcpy(strategy_name,"Forked");
				operation = FORKED;
				option_count++;
				break;
			case 'e':
				strcpy(strategy_name,"Daemon");
				operation = DAEMON;
				option_count++;
				break;
			default:
				printf("Error en los Parametros\n");
				break;
		}
	}
	if(option_count > 1)
	{
		printf("\nNo pases argumentos de mas de una Estrategia.\n");
		exit(0);
	}
	
	//if(!check_folder_exists(path_root)) exit(0);
	
	if(option_count ==0)
	{
		operation =ITERATIVE;
		strcpy(strategy_name,"Iterative");
	}
	return operation;
}


int main(int argc, char **argv)
{
	int pid, listenfd, socketfd;
    strategy_t server_operation;
	socklen_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    
    //Hilos
    pthread_t thread_id; //Hilo para atención en Estrategia Threads

	pthread_t       th; /* identificador del hilo */
	pthread_attr_t	ta; /* define los atributos del hilo */
    
	(void) pthread_attr_init(&ta); /* inicializa los atributos del hilo a su valor por defecto. */
	(void) pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED); /* establece el atributo detachstate al valor: PTHREAD_CREATE_DETACHED. */
    /* PTHREAD_CREATE_JOINABLE (no desconectado)
     PTHREAD_CREATE_DETACHED (desconectado)  */
    
    (void) pthread_mutex_init(&estadisticas.est_mutex, 0); /* crea un mutex. */
    
	if (pthread_create(&th, &ta, (void * (*)(void *))impestadisticas, 0) < 0)
		printf("ERROR pthread_create(infestadistica).\n");



    
    
	server_operation = (strategy_t)configure_server(argc,argv);
    if (!check_settings()) {
        show_help();
        exit(0);
    } else {
        logger(LOG,"Iniciando servidor",strategy_name,getpid());
        //Verifico si tengo que correr como demonio
        
        if (server_operation == DAEMON) {
            /* Become deamon + unstopable and no zombies children (= no wait()) */
            printf("Me vuelvo un demonio");
            if(fork() != 0)
                return 0; /* parent returns OK to shell */
            (void)signal(SIGCLD, SIG_IGN); /* ignore child death */
            (void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
            for(int i=0;i<32;i++)
                (void)close(i);		/* close open files */
            (void)setpgrp();		/* break away from process group */
        }

        
        
        /* setup the network socket */
        if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
            logger(ERROR, "system call","socket",0);
        
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        serv_addr.sin_port = htons(port_number);
        if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
            logger(ERROR,"system call","bind",0);
        if( listen(listenfd,64) <0)
            logger(ERROR,"system call","listen",0);
        for(hit=1; ;hit++) {
            length = sizeof(cli_addr);
            if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
                logger(ERROR,"system call","accept",0);
            logger(LOG, "Aceptando conexion", inet_ntoa(cli_addr.sin_addr),socketfd);
            switch (server_operation) {
                case ITERATIVE:
                    printf("Atiendo ITERATIVE\n");
                    web(socketfd); //Atiendo solicitud
                    break;
                case FORKED:
                    printf("Atiendo FORKED\n");
                    if((pid = fork()) < 0) {
                        logger(ERROR,"system call","fork",0);
                        exit(1);
                    } else {
                        if(pid == 0) { 	/* Hijo */
                            web(socketfd); //Atiendo solicitud
                        } else { 	/* Padre */
                            (void)close(socketfd);
                        }
                    }
                    break;
                case THREADS:
                    printf("Atiendo THREAD\n");
                    if (pthread_create(&thread_id, NULL, (void * (*)(void *))web, (void *)socketfd )<0)
                        printf("ERROR pthread_create.\n");
                    break;
                case DAEMON:
                    printf("Atiendo DAEMON\n");

                    if((pid = fork()) < 0) {
                        logger(ERROR,"system call","fork",0);
                    }
                    else {
                        if(pid == 0) { 	/* Hijo */
                            (void)close(listenfd);
                            web(socketfd); //Atiendo solicitud
                        } else { 	/* Padre */
                            (void)close(socketfd);
                        }
                    }
                    break;
                default:
                    printf("Estrategia %s (%d) aún no implementada\n", strategy_name, server_operation);
                    logger(ERROR,"Estrategia no implementada",strategy_name,server_operation);
                    exit(2);
                    break;
            }

        }
    }
    



}

