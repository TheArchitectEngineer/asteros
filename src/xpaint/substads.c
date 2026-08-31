#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
FILE *fd_in, *fd_out;
char buf_in[1024], buf_out[1024];
char file_out[80];
char *ptr;
int i, doline;
struct dirent *file;
DIR *appdef;

    if (argc>3 && !strcmp(argv[1], "-ad2c")) {
	fd_in = fopen(argv[2], "r");
	fd_out = fopen(argv[3], "w");
	if (fd_in && fd_out) {
	    doline = 1;
	    while ((ptr = fgets(buf_in, 510, fd_in))) {
	        if (*ptr == '!' || *ptr == '\n') continue;
	        for (i=0; i<strlen(ptr); i++) if (!isspace(ptr[i])) break;
                if (i==strlen(ptr)) continue;
	        if (doline) fprintf(fd_out, "\"");
	        for (i=0; i<strlen(ptr); i++) {
		    if (ptr[i]=='\\') {
		        if (ptr[i+1]=='\n') {
			    fprintf(fd_out, "\\\n");
			    doline = 0;
                            break;
			} else
			    fprintf(fd_out, "\\\\");
		    } else
		    if (ptr[i]=='"') {
			fprintf(fd_out, "\\\"");
		    } else
		    if (ptr[i]=='\n') {
	                fprintf(fd_out, "\",\n");
			doline = 1;
		    } else
			fprintf(fd_out, "%c", ptr[i]);			
		}
	    }
	}
	if (fd_in) fclose(fd_in);
	if (fd_out) fclose(fd_out);
	exit(0);
    }

    if (argc>3 && !strcmp(argv[1], "-single")) {
	fd_in = fopen(argv[2], "r");
	fd_out = fopen(argv[3], "w");
	if (fd_in && fd_out) {
	    while ((ptr = fgets(buf_in, 510, fd_in))) {
	        strcpy(buf_out, buf_in);
		if (strstr(buf_in, "XPAINT_"))
		    for (i=4; i<argc-1; i+=2)
		    if ((ptr = strstr(buf_in, argv[i]))) {
		        buf_out[(long)ptr-(long)buf_in] = '\0';
		        strcat(buf_out, argv[i+1]);
			strcat(buf_out, ptr+strlen(argv[i]));
		    }
		fprintf(fd_out, "%s", buf_out);
		}
	    }

	if (fd_in) fclose(fd_in);
	if (fd_out) fclose(fd_out);
	exit(0);
    }

    if (argc>4 && !strcmp(argv[1], "-appdefs")) {
        appdef = opendir(".");
        while((file = readdir(appdef)) != NULL) {
            if (!strncmp(file->d_name, "XPaint",6) && strlen(file->d_name)>6 &&
    	    !strcmp(file->d_name+strlen(file->d_name)-6, ".ad.in")) {
    	    sprintf(file_out, "out/%s", file->d_name);
    	    file_out[strlen(file->d_name)-2] = '\0';
                printf("Converting %s -> %s\n", file->d_name, file_out);
    	    fd_in = fopen(file->d_name, "r");
    	    fd_out = fopen(file_out, "w");
    	    if (fd_in && fd_out) {
    	        while ((ptr = fgets(buf_in, 510, fd_in))) {
    		    strcpy(buf_out, buf_in);
    		    if (strstr(buf_in, "XPAINT_"))
    		        for (i=2; i<argc-1; i+=2)
    		        if ((ptr = strstr(buf_in, argv[i]))) {
    		            buf_out[(long)ptr-(long)buf_in] = '\0';
    			    strcat(buf_out, argv[i+1]);
    			    strcat(buf_out, ptr+strlen(argv[i]));
    		        }
    		    fprintf(fd_out, "%s", buf_out);
    		}
    	    }
    	    if (fd_in) fclose(fd_in);
    	    if (fd_out) fclose(fd_out);
            }
        }
        closedir(appdef);
    }
    exit(0);
}
    
    
    
