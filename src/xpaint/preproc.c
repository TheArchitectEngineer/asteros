#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
FILE *fd;
char *ptr;
char buf[512];
int num, i, l;
static char i18n_file[] = "share/messages/Messages";
static char i18n_version_string[] = "# XPaint Messages version ";

    fd = fopen(i18n_file, "r");
    if (!fd) {
       fprintf(stderr, "Cannot open language file %s !!\n", i18n_file);
       exit(-1);
    }

    /* get version string from first line */
    ptr = fgets(buf, 510, fd);
    l = strlen(i18n_version_string);
    if (strncmp(ptr, i18n_version_string, l)) {
       fprintf(stderr, "Language file %s does not begin with required version "
         "string !!\n", i18n_file);
       exit(-1);
    }
    ptr += l;
    l = strlen(ptr)-1;
    if (ptr[l]=='\n') {
       ptr[l] = '\0';
       --l;
    }
    while(l>=0 && isspace(ptr[l])) {
       ptr[l] = '\0';
       --l;
    }
    printf("#define MESSAGES_VERSION \"%s\"\n", ptr);

    num = 0;
    while ((ptr = fgets(buf, 510, fd))) {
       l = strlen(ptr)-1;
       if (ptr[l]=='\n') {
          ptr[l] = '\0';
	  --l;
       }
       while(l>=0 && isspace(ptr[l])) {
	  ptr[l] = '\0';
	  --l;
       }
       if (l<0) continue;
       if (*ptr=='#') {
          for (i=0; i<=l; i++) {
	      if (isspace(ptr[i])) {
		 ptr[i] = '\0';
		 break;
	      }
	  }
          printf("#define %s %d\n", ptr+1, num);
       } else
	  ++num;
       if (!strncmp(ptr, "#END_OF_I18N_FILE", 17)) break;
    }
    fclose(fd);
    printf("\nextern String *msgText;\nextern int msgNum;\n");
    exit(0);
}
