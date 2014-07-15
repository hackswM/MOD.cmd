#include "str.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef DEBUG
#define debug_msg(format,...)   \
    fprintf(stdout,format,##__VA_ARGS__)
#else
    #define debug_msg(format,...)
#endif   /* DEBUG */


int init(char *buff)
{
	if (buff){	
	    char tmp[] = {0x02,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
        memcpy(buff,tmp,16);
		return 0;
	}else{
	    return -1;
	}
}




/* The cJSON structure: */
typedef struct cJSON {
	struct cJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *child;		/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;					/* The type of the item, as above. */

	char *valuestring;			/* The item's string, if type==cJSON_String */
	int valueint;				/* The item's number, if type==cJSON_Number */
	double valuedouble;			/* The item's number, if type==cJSON_Number */

	char *string;				/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} cJSON;


#pragma pack(1)
typedef struct{
	uint32_t RHeaderSize;
	char     Tid[32];
	void *   RData;
	uint32_t RDataSize;
    uint32_t RStatus;
	void *   ExecCallFunc;
	void *   ResponseSender;
	void *   ResponseStreamSender;
	cJSON *  cJSON_GetObjectItem;
	cJSON *  lpJsonRoot;
}REQUEST_HEADER;
#pragma pack()

//ret :  0 ok
//      -1 fail
//      -2 no enough memory
int exec_cmd(const char * cmd,char * buffer,int bufferSize){
	int ret = -1;
	int fd[2];
	if (0 == pipe((int *)&fd)){
		pid_t pid = fork();
		if (0 == pid){ //;child process
			close(fd[0]);
			if (STDOUT_FILENO != fd[1]){
				if (STDOUT_FILENO == dup2(fd[1],STDOUT_FILENO)){
				    close(fd[1]);	    	
			        ret = execl("/bin/sh","sh","-c",cmd,NULL);
				}
			}
		}else if (-1 == pid){
		
		}else{         //;parent process 
			close(fd[1]);

			int dwRead = 0;
			do {
				dwRead = read (fd[0],buffer,bufferSize);
				buffer     += dwRead;
				bufferSize -= dwRead;				
			}while ((dwRead > 0) && (bufferSize > 0));
				
			close (fd[0]);

			if (0 <= waitpid(pid,0,0)){
				ret = 1;
			}
		}
	}

	if (bufferSize <= 0){
		ret = -2;
	}
	return ret;
}

int exec(const char *buff){
	REQUEST_HEADER * Buf = (REQUEST_HEADER *)buff;

    debug_msg ("Mod.exec: request len: %i contents: %s  addr: %p \n",Buf->RDataSize,(char *)Buf->RData,Buf->RData);
	debug_msg ("Mod.exec: request ID: %.*s \n",32,Buf->Tid);
	debug_msg ("Mod.exec: ExecFunc: %p \n",Buf->ExecCallFunc);
	debug_msg ("Mod.exec: cJSON_GetObjectItem: %p \n",Buf->cJSON_GetObjectItem);
	debug_msg ("Mod.exec: lpJsonRoot: %p \n",Buf->lpJsonRoot);
	
    char * cmd = 0;
	cJSON * jsonRoot = 0;

    cJSON * (* cJSON_GetObjectItem) (cJSON *object,const char *string);
    cJSON_GetObjectItem = Buf->cJSON_GetObjectItem;
    if (Buf->lpJsonRoot){		 		
		jsonRoot = Buf->lpJsonRoot;

		cJSON * data = cJSON_GetObjectItem(jsonRoot,"cmd");
		if ((data)&&(data->valuestring)){
			cmd = data->valuestring;
		}	
    }

	int (* responseSender) (REQUEST_HEADER * , int,char*); 
	responseSender = Buf->ResponseSender;
    	
	int bufferSize = 1024 * 1024;
	char *buffer   = 0;

	if (cmd){	
		debug_msg ("Command: %s \n",cmd);

		buffer = malloc(bufferSize);

		if (buffer){
			memset (buffer,0,bufferSize);
			strcpy (buffer,"$cmd=");
            bufferSize -= sizeof("$cmd=") + sizeof("$d=") + 1 + 100;
			bufferSize -= strlen(cmd);
			if (bufferSize > 0){
				strcat (buffer,cmd);
				strcat (buffer,"\x04");
				strcat (buffer,"$d=");
				int ret = exec_cmd(cmd,buffer + strlen(buffer),bufferSize);
				if (-1 == ret){
				    strcpy (buffer,"$e=1\x04");
				}else{
					strcat (buffer,"\x04");
					if (-2 == ret){
                        strcat (buffer,"$w=1\x04");
					}
				}				
				debug_msg ("cmd ret: %s \n",buffer);
			}else{
			    strcpy (buffer,"$e=2\x04");
			}			
		}

		if (buffer){
			responseSender(Buf,strlen(buffer),buffer);
			free(buffer);
        }else{
			responseSender(Buf,5,"$e=4\x04");
		}
	}else{
		responseSender(Buf,5,"$e=0\x04");
	}	

	return 0;
}