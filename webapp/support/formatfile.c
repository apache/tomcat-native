#include <stdio.h>

int main(void) {
    char buf[1024];
    char *ret=NULL;
    int x=0;
    int y=0;
    int k=0;
    int s=0;
    int l=0;

    while ((ret=fgets(buf,1024,stdin))!=NULL) {
        for (x=strlen(ret); x>=0; x--) {
            if((ret[x]=='\0')||(ret[x]=='\t')||(ret[x]=='\n')||(ret[x]==' '))
                ret[x]='\0';
            else break;
        }

        k=0;
        for (x=0; x<strlen(ret); x++) {
            if (ret[x]=='\t') {
                fprintf(stdout," ");
                k++;
                s=((((k/4)+1)*4)-k);
                if (s==4) s=0;
                for (y=0; y<s; y++) fprintf(stdout," ");
                k+=s;
            } else {
                fprintf(stdout,"%c",ret[x]);
                k++;
            }
        }
        fprintf(stdout,"\n");
        l++;
        if (k>=80) fprintf(stderr,"line %4d (%4d) [%s]\n",l,k,ret);
    }
}
