#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "parser.h"
#include <malloc.h>

//-------------- VARIABLES --------------------

// Con cada ejecución del while saldrá prompt como si fuera una terminal
char prompt[] = "msh> ";
// String para la finalización
char stop[] = "fin\n";

// Buffer que recoge la secuencia de comnados escritas por consola
char buf[1024];
char bufaux[1024];

// Cadena de caracteres para guardar direccion que utilizamos en cd
char *aux;

// Número de comandos introducidos
tline *cadena;

int i;
int j;
int k;
int fd_entrada, fd_salida, fd_error;
int p[2];
int **pipes=NULL;
pid_t hijos_bk[20];
static int contador=0;
char buf_comandos[1024][1024];
int estado;

// Comprueba que si el mandato introducido es un cd, fg o jobs no salte el error de que no existe el mandato
int comprobador=0; 

//-------------- FIN VARIABLES --------------------

//-------------- DECLARACION DE METODOS --------------------
void manejador_hijo(int sig);
void esperar();
void printPrompt ();
//-------------- FIN DECLARACION DE METODOS --------------------

//-------------- MAIN --------------------
int main (int argc, char *argv[]){   
	//Proceso padre
	pid_t pid; 
	
	//-------------- CONTROL DE SEÑALES --------------------	
	//Invocamos al manejador
	signal(SIGUSR2, manejador_hijo);
	
	// Para evitar los atajos de teclado como indica el enunciado de la práctica
	// Evita que el programa acabe al pulsar control+c
	if(signal(SIGINT, SIG_IGN)==SIG_ERR){
		exit(EXIT_FAILURE);
	}
	// Evita que el programa acabe al pulsar control+\.
	if(signal(SIGQUIT, SIG_IGN)==SIG_ERR){
		exit(EXIT_FAILURE);
	}
	//-------------- FIN CONTROL DE SEÑALES --------------------
	
	//-------------- BUCLE DOWHILE --------------------
	// Esto es para que se ejecute de manera indefinida hasta que se teclea fin
	do{
		// Metodo para imprimir el prompt
		printPrompt ();
		
		// Aqui es donde se recoge la linea de comandos escrita
		fgets(buf,1024,stdin);
		
		strcpy(bufaux,buf);
		esperar();		
		
		//-------------- APARTADO 5 cd --------------------
		// Se cogerán los tres primeros caracteres de la cadena de mandatos, y si son “cd “,
		// el programa entra en el if de cd, efectuando el comando
		if(buf[0]=='c' && buf[1]=='d' && buf[2]==' '){
			comprobador = 1;
			// Reservo en memoria el espacio para una cadena de caracteres que contendrá el nombre del directorio introducido
			aux=(char *)malloc(sizeof(char)*strlen(buf));	
			// Cadena que copia el directorio introducido en caso de que se detecte el salto de linea que mete automaticamente la funcion fgets  		// se interrumpe la copia de manera que el resultado es un string "limpio" sin saltos de linea.
			for(j=0;j<strlen(buf)-4;j++){  
				// Si se detecta un salto de línea, se interrumpe la copia de 
				// directorio de manera que se adquiere un String limpio
				if(buf[j+3] == 'n' && buf[j+4] == '\\'){
					break;
				}	
				aux[j] = buf[j+3];		
			} 
			//Cambia de directorio
			chdir(aux); 
			// Se reutilizo el buffer ya que no se va a usar hasta que se introduzca otro mandato
			getcwd(buf,1024); 
			printf("%s\n", buf);
			// Se libera el espacio que ocupa aux de manera que cuando se vuelva a hacer un cd se vuelva a reservar y el buffer este limpio de nuevo
			free(aux);
				
		}else{
			// En caso de que quede vacio el comando se imprime la direccion actual
			if (buf[0]=='c' && buf[1]=='d'){ 
				getcwd(buf,1024);
				printf("%s\n",buf);
				
		//-------------- FIN APARTADO 5 cd --------------------
			}else{
				
				//-------------- APARTADO VARIOS COMANDOS --------------------
				
				// Se obtiene el numero de mandatos de line
				cadena = tokenize(buf);
				// Se crea un array con tamaño definido por el número de comandos de la cadena
				pid_t hijos[cadena->ncommands];
				
				// Hay que reservar en memoria dinamica el espacio donde almacenaré los pipes	
				pipes = (int **)malloc(sizeof(int)*cadena->ncommands); 
				
				// Creo los pipes segun cadena y los almaceno en un array
				for(k=0;k<cadena->ncommands-1;k++){ 
					int p[2];
					pipes[k]=p;
					pipe(pipes[k]);
				}
				// Bucle for el cual crea los procesos hijos y los almacena en el array 
				for(i=0; i < cadena->ncommands; i++){
					pid = fork();
					if(pid==0){			
						pause();
					}else{
						hijos[i]=pid; 
					}		
				}
				
				//-------------- APARTADO 7 BACKGROUND --------------------
				// Para añadir los procesos hijos al array de procesos en segundo plano
				if(cadena->background==1 && hijos!=NULL){
					hijos_bk[contador]=hijos[cadena->ncommands-1];
					strcpy(buf_comandos[contador],bufaux); 
					contador ++;
					
					// Reseteo el contador
					if(contador==19){ 
						contador=0;
					}
				}
				
				// Bucle for el cual inicializa los hijos
				for(j=0;j<cadena->ncommands;j++){
					kill(hijos[j], SIGUSR2); 
				}   
				
				//-------------- FIN APARTADO VARIOS COMANDOS --------------------
				
				// Si no se ha ejecutado el comando en background, sigue su proceso normal
				if(cadena->background==0){
					waitpid(hijos[cadena->ncommands-1], NULL, 0);
				}
				//-------------- FIN APARTADO 7 BACKGROUND --------------------
			}
		}
		
		//-------------- APARTADO 6 jobs,fg --------------------
		
		// Comando fg sin contenido
		// En caso de carecer de índice en el mandato, 
		// se implementa de manera que en vez de buscar el índice, se obtenga como índice el contador-1 actual.
		if(strcmp(buf, "fg\n")==0){ 
			comprobador=1;
			if(hijos_bk[contador-1]>0){
				waitpid(hijos_bk[contador-1], NULL, 0);
				hijos_bk[contador-1]=-1;		
			}else{	
				printf("No hay procesos en segundo plano\n");	
			}
		}
		
		// Comando fg con contenido
		// Se extrae el número introducido del mandato, y en caso de existir un hijo activo señalizado 
		// por dicho número en el array de procesos hijo en segundo plano, se hace un wait bloqueante.
		if(buf[0]=='f' && buf[1]=='g' && buf[2]==' '){		
			comprobador=1;
			aux=(char *)malloc(sizeof(char)*strlen(buf));
			for(j=0;j<strlen(buf)-4;j++){ 
				if(buf[j+3] == 'n' && buf[j+4] == '\\'){
					break;
				}	
				// Reutilizo la variable aux para conseguir el  
				// numero introducido sinsalto de linea
				aux[j] = buf[j+3];
			}
		
			// Compruebo si hay proceso background en la posición dada
			if(hijos_bk[atoi(aux)]>0){
				waitpid(hijos_bk[atoi(aux)], NULL, 0);
				hijos_bk[atoi(aux)]=-1;
			}else{	
				printf("No hay procesos en segundo plano\n");	
			}
		}	
		
		// Comando jobs
		//Mandato que muestra los procesos ejecutándose en segundo plano. 
		//Para efectuar el comando se recorre el array de hijos ejecutándose 
		//en segundo plano y se imprime por consola.
		if(strcmp(buf, "jobs\n")==0){	
			comprobador=1;
			for(j=0;j<contador;j++){	
				if(hijos_bk[j]!=-1){
					printf("[%d] running               %s", j,buf_comandos[j]);
				}
			}
		}
		//-------------- FIN APARTADO 6 jobs,fg --------------------
		
	// Se comprueba si el comando tecleado es fin, y en dicho caso se sale de él
	}while(strcmp(buf,stop) != 0);
	
	printf("minishell finalizada\n");
	
	return 0;
}
//-------------- FIN MAIN --------------------

void manejador_hijo(int sig){
	
	//-------------- REDIRRECIONES --------------------
	// redirección de ficheros
	// redirección de error
	if(cadena->redirect_error  != NULL && i==cadena->ncommands-1){
		comprobador=1;
		fd_error = open(cadena->redirect_error,O_CREAT | O_RDWR);
		if(fd_error==-1){
			printf("Error. fallo al escribir en fichero de error\n");
		}else{
			dup2(fd_error,2);
		}
	}
	
	// redirección de entrada (se efectúa en el primer proceso siendo i = 0)
	if(cadena->redirect_input != NULL && i==0){  
		comprobador=1;
		fd_entrada = open(cadena->redirect_input,O_RDWR);
		if(fd_entrada==-1){
			printf("Error. fallo al leer de fichero\n");
		}else{
			dup2(fd_entrada,0);
		}
	}
	
	// redirección de salida (se efectúa en el último proceso siendo i = ncommands -1)
	if(cadena->redirect_output != NULL && i==cadena->ncommands-1){
		comprobador=1;
		fd_salida = open(cadena->redirect_output,O_CREAT | O_RDWR);
		if(fd_salida==-1){
			printf("Error. fallo al escribir en fichero\n");
		}else{
			dup2(fd_salida,1);
		}
	}
	//-------------- FIN REDIRRECIONES --------------------
	
	//-------------- APARTADO VARIOS COMANDOS --------------------
	// Primer pipe es la salida, el resto de pipes serán entradas.
	
	// redirección de mandatos
	// salida (primer mandato)
	if(cadena->ncommands >=0 && i!=cadena->ncommands-1){ 	
		close(pipes[i][0]);
		dup2(pipes[i][1],1);
	}
	
	// Entrada (todos excepto el primer mandato)
	if(cadena->ncommands >=0 && i!=0){
		close(pipes[i-1][1]);
		dup2(pipes[i-1][0],0);
	}
	
	// Para asegurar que el comando existe y lo ejecute
	if(cadena->commands[i].filename==NULL){
		if(comprobador==0){	
			printf("%s: mandato no encontrado\n",cadena->commands[i].argv[0]);
		}else{comprobador=0;}
	}else{
		execvp(cadena->commands[i].filename,cadena->commands[i].argv);//ejecuto el comando
		exit(0);
	}
	//-------------- FIN APARTADO VARIOS COMANDOS --------------------
	
	exit(0); 	
} 

//-------------- APARTADO 7 BACKGROUND --------------------
//metodo que lleva a cabo la espera de los procesos en segundo plano
void esperar(){ 
	//bucle de espera
	for(j=0;j<20;j++){
        //el padre espera a que acaben los hijos. El metodo waitpid(WNOHANG)devuelve el pid del hijo y se almacena en status
		estado=waitpid(hijos_bk[j], NULL, WNOHANG);
		//bucle que comprueba si el proceso que ha terminado esta en la lista
		for(k=0;k<20;k++){
		//si el proceso ha terminado adecuadamente lo elimino rellenandolo con -1 
			if(estado==hijos_bk[k]){ 
				hijos_bk[k]=-1;
			}
		}
	}
}
//-------------- FIN APARTADO 7 BACKGROUND --------------------

// Método que imprime el prompt
void printPrompt (){
	printf ("%s", prompt);
}



